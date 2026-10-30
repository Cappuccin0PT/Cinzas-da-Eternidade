#!/usr/bin/env python3
"""
Corta uma música num ciclo perfeito (perfect loop) e junta o fim ao início com uma transição curta.

Os pontos de início e fim foram escolhidos por análise da música: ficam alinhados com os compassos
(um número inteiro de frases musicais) e no ponto em que o som a seguir ao fim é mais parecido com o
som a seguir ao início. A transição usa a música que vem logo depois do fim, por isso o ciclo continua
sem salto quando volta ao início.

Uso: python3 tools/fazer_loop.py entrada.raw saida.raw taxa inicio_s fim_s transicao_ms
     (PCM 16 bits mono, little-endian, gerado pelo ffmpeg)
"""
import array
import math
import sys

def main():
    ent, sai, taxa = sys.argv[1], sys.argv[2], int(sys.argv[3])
    ini, fim, xf = float(sys.argv[4]), float(sys.argv[5]), float(sys.argv[6])
    x = array.array("h"); x.frombytes(open(ent, "rb").read())
    if sys.byteorder == "big": x.byteswap()
    a, b, n = int(round(ini * taxa)), int(round(fim * taxa)), int(round(xf / 1000 * taxa))
    b = min(b, len(x) - n)
    out = array.array("h", x[a:b])
    for i in range(n):   # transição de igual potência: o início do ciclo entra enquanto o fim continua
        t = (i + 0.5) / n
        v = x[a + i] * math.sin(t * math.pi / 2) + x[b + i] * math.cos(t * math.pi / 2)
        out[i] = max(-32767, min(32767, int(round(v))))
    if sys.byteorder == "big": out.byteswap()
    open(sai, "wb").write(out.tobytes())
    print(f"ciclo de {len(out) / taxa:.2f} s (de {ini:.3f} s a {fim:.3f} s), transição de {xf:.0f} ms", file=sys.stderr)

if __name__ == "__main__":
    main()
