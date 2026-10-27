#!/usr/bin/env python3
"""
Converte as músicas para o 32X em IMA ADPCM (4 bits por amostra, mono), uma faixa por entrada.
O SH-2 secundário descomprime em tempo real, interpola para 22050 Hz e escala para o PWM.
O ADPCM ocupa metade do espaço de 8 bits por amostra, com qualidade parecida, e é o que deixa
caber três músicas e a cutscene nos 4 MB do cartucho.

Entrada: taxa e pares NOME ficheiro.raw (PCM 16 bits mono à mesma taxa, gerado pelo ffmpeg).
Uso:
  ffmpeg -i musica/Arcade_Rush.mp3 -ac 1 -ar 11025 -f s16le fortaleza.raw
  python3 tools/converter_musica.py 11025 FORTALEZA fortaleza.raw ALDEIA aldeia.raw INTRO intro.raw > src/musica.h
"""
import array
import sys

PICO = 0.80   # folga para os efeitos sonoros
INDEX = [-1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8]
STEP = [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
        107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
        876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428,
        4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350,
        22385, 24623, 27086, 29794, 32767]

def adpcm(amostras):
    pred, idx, out = 0, 0, []
    for s in amostras:
        step = STEP[idx]
        diff = s - pred
        n = 0
        if diff < 0: n = 8; diff = -diff
        if diff >= step: n |= 4; diff -= step
        if diff >= step >> 1: n |= 2; diff -= step >> 1
        if diff >= step >> 2: n |= 1
        d = step >> 3
        if n & 4: d += step
        if n & 2: d += step >> 1
        if n & 1: d += step >> 2
        pred = pred - d if n & 8 else pred + d
        pred = max(-32767, min(32767, pred))
        idx = max(0, min(88, idx + INDEX[n]))
        out.append(n)
    if len(out) & 1: out.append(0)
    return bytes(out[i] | (out[i + 1] << 4) for i in range(0, len(out), 2))

def main():
    rate = int(sys.argv[1])
    pares = sys.argv[2:]
    w = sys.stdout.write
    w("/* Gerado por tools/converter_musica.py — não editar à mão. Músicas em IMA ADPCM (4 bits). */\n")
    w("#ifndef MUSICA_H\n#define MUSICA_H\n#include <stdint.h>\n")
    w(f"#define MUSICA_RATE {rate}\n")
    w("static const int16_t ADPCM_STEP[89] = {" + ",".join(map(str, STEP)) + "};\n")
    w("static const int8_t ADPCM_INDEX[16] = {" + ",".join(map(str, INDEX)) + "};\n")
    total = 0
    for i in range(0, len(pares), 2):
        nome, raw = pares[i], pares[i + 1]
        a = array.array("h")
        a.frombytes(open(raw, "rb").read())
        if sys.byteorder == "big":
            a.byteswap()
        pico = max(1, max(abs(x) for x in a))
        g = 32767.0 * PICO / pico
        s = [max(-32767, min(32767, int(round(x * g)))) for x in a]
        dados = adpcm(s)
        w(f"#define MUSICA_{nome}_LEN {len(s)}\n")
        w(f"static const uint8_t MUSICA_{nome}[{len(dados)}] __attribute__((aligned(4))) = {{\n")
        for j in range(0, len(dados), 40):
            w(",".join(str(v) for v in dados[j:j + 40]) + ",\n")
        w("};\n")
        total += len(dados)
        print(f"{nome}: {len(s) / rate:.1f} s, {len(dados) / 1048576:.2f} MB", file=sys.stderr)
    w("#endif\n")
    print(f"total de música: {total / 1048576:.2f} MB", file=sys.stderr)

if __name__ == "__main__":
    main()
