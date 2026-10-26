#!/usr/bin/env python3
"""
Converte as músicas para o 32X: amostras de 8 bits com sinal, mono, uma faixa por mapa.
O SH-2 secundário interpola-as para 22050 Hz e escala-as para o PWM (8 bits chegam: o PWM a
22050 Hz tem cerca de 10 bits de resolução). Assim cabem duas músicas nos 4 MB do cartucho.

Entrada: taxa e pares NOME ficheiro.raw (PCM 16 bits mono à mesma taxa, gerado pelo ffmpeg).
Uso:
  ffmpeg -i musica/Arcade_Rush.mp3 -ac 1 -ar 11025 -f s16le fortaleza.raw
  ffmpeg -i musica/aldeia.mp3 -ac 1 -ar 11025 -f s16le aldeia.raw
  python3 tools/converter_musica.py 11025 FORTALEZA fortaleza.raw ALDEIA aldeia.raw > src/musica.h
"""
import array
import sys

PICO = 0.80   # folga para os efeitos sonoros

def main():
    rate = int(sys.argv[1])
    pares = sys.argv[2:]
    w = sys.stdout.write
    w("/* Gerado por tools/converter_musica.py — não editar à mão. */\n#ifndef MUSICA_H\n#define MUSICA_H\n#include <stdint.h>\n")
    w(f"#define MUSICA_RATE {rate}\n")
    total = 0
    for i in range(0, len(pares), 2):
        nome, raw = pares[i], pares[i + 1]
        a = array.array("h")
        a.frombytes(open(raw, "rb").read())
        if sys.byteorder == "big":
            a.byteswap()
        pico = max(1, max(abs(x) for x in a))
        g = 127.0 * PICO / pico
        d = [max(-127, min(127, int(round(x * g)))) for x in a]
        w(f"#define MUSICA_{nome}_LEN {len(d)}\n")
        w(f"static const int8_t MUSICA_{nome}[{len(d)}] __attribute__((aligned(4))) = {{\n")
        for j in range(0, len(d), 32):
            w(",".join(str(v) for v in d[j:j + 32]) + ",\n")
        w("};\n")
        total += len(d)
        print(f"{nome}: {len(d) / rate:.1f} s, {len(d) / 1048576:.2f} MB", file=sys.stderr)
    w("#endif\n")
    print(f"total de música: {total / 1048576:.2f} MB", file=sys.stderr)

if __name__ == "__main__":
    main()
