#!/usr/bin/env python3
"""
Converte efeitos sonoros para o 32X: amostras de 16 bits com sinal, mono, à mesma taxa da música.
O SH-2 secundário soma-os à música (duas vozes: golpe da espada e restantes efeitos).

Uso:
  ffmpeg -i sons/hurt.wav -ac 1 -ar 11025 -f s16le hurt.raw
  python3 tools/converter_sfx.py 11025 HURT hurt.raw POWERUP powerup.raw ... > src/sfx.h
"""
import array
import sys

VOLUME = 0.85   # pico do efeito em relação à escala total

def main():
    rate = int(sys.argv[1])
    pares = sys.argv[2:]
    w = sys.stdout.write
    w("/* Gerado por tools/converter_sfx.py — não editar à mão. */\n#ifndef SFX_H\n#define SFX_H\n#include <stdint.h>\n")
    for i in range(0, len(pares), 2):
        nome, raw = pares[i], pares[i + 1]
        a = array.array("h")
        a.frombytes(open(raw, "rb").read())
        if sys.byteorder == "big":
            a.byteswap()
        pico = max(1, max(abs(x) for x in a))
        g = 32767.0 * VOLUME / pico
        d = [int(round(x * g)) for x in a]
        w(f"#define SFX_{nome}_LEN {len(d)}\n")
        w(f"static const int16_t SFX_{nome}[{len(d)}] = {{\n")
        for j in range(0, len(d), 24):
            w(",".join(str(v) for v in d[j:j + 24]) + ",\n")
        w("};\n")
        print(f"{nome}: {len(d) / rate:.2f} s", file=sys.stderr)
    w("#endif\n")

if __name__ == "__main__":
    main()
