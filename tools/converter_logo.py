#!/usr/bin/env python3
"""
Converte o logótipo (arte/logo.png) para o 32X: reduz ao tamanho do ecrã e mapeia cada cor para a paleta
fixa do jogo. O fundo preto fica transparente (índice 0). Escreve src/logo.h.
Precisa do Pillow (pip install pillow). Só é preciso correr quando o logótipo mudar.
Uso: python3 tools/converter_logo.py arte/logo.png 168 > src/logo.h
"""
import sys
from PIL import Image

PAL = {1: 0x0e0c12, 2: 0x2a2533, 3: 0x4a4250, 4: 0x77707a, 5: 0xb3aca6, 6: 0xece6d8,
       7: 0x3b1414, 8: 0x7a2618, 9: 0xc4461f, 10: 0xf08a2e, 11: 0xffd36b, 12: 0x13233a,
       22: 0x5a2a6e, 23: 0xa33a7a}

def rgb(c): return ((c >> 16) & 255, (c >> 8) & 255, c & 255)

def main():
    src, larg = sys.argv[1], int(sys.argv[2])
    im = Image.open(src).convert("RGB")
    lum = im.convert("L").point(lambda v: 255 if v > 22 else 0)
    im = im.crop(lum.getbbox())
    alt = round(im.height * larg / im.width)
    im = im.resize((larg, alt), Image.LANCZOS)
    pal = [(i, rgb(c)) for i, c in PAL.items()]
    out = []
    for y in range(alt):
        for x in range(larg):
            r, g, b = im.getpixel((x, y))
            if r + g + b < 70 and r < 60:          # fundo preto: transparente
                out.append(0); continue
            best = min(pal, key=lambda p: 2 * (p[1][0] - r) ** 2 + 4 * (p[1][1] - g) ** 2 + 3 * (p[1][2] - b) ** 2)
            out.append(best[0])
    w = sys.stdout.write
    w("/* Gerado por tools/converter_logo.py a partir de arte/logo.png — não editar à mão. */\n")
    w(f"#ifndef LOGO_DADOS_H\n#define LOGO_DADOS_H\n#include <stdint.h>\n#define LOGO_W {larg}\n#define LOGO_H {alt}\n")
    w(f"static const uint8_t LOGO[{larg * alt}] __attribute__((aligned(4))) = {{\n")
    for i in range(0, len(out), 48):
        w(",".join(str(v) for v in out[i:i + 48]) + ",\n")
    w("};\n#endif\n")
    print(f"logótipo {larg}x{alt}", file=sys.stderr)

if __name__ == "__main__":
    main()
