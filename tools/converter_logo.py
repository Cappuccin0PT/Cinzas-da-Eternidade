#!/usr/bin/env python3
"""
Converte o logótipo (arte/logo.png) para o 32X: reduz ao tamanho do ecrã e mapeia cada cor para a paleta
fixa do jogo. O fundo (branco ou preto, detetado pelo canto da imagem) fica transparente (índice 0),
e a orla clara que sobra à volta do contorno ao reduzir a imagem é removida. Escreve src/logo.h.
Precisa do Pillow (pip install pillow). Só é preciso correr quando o logótipo mudar.
Uso: python3 tools/converter_logo.py arte/logo.png 168 > src/logo.h
"""
import sys
from PIL import Image

PAL = {1: 0x0e0c12, 2: 0x2a2533, 3: 0x4a4250, 4: 0x77707a, 5: 0xb3aca6, 6: 0xece6d8,
       7: 0x3b1414, 8: 0x7a2618, 9: 0xc4461f, 10: 0xf08a2e, 11: 0xffd36b, 12: 0x13233a,
       19: 0x4b3a2c, 20: 0x7d6345, 21: 0xb0905f, 22: 0x5a2a6e, 23: 0xa33a7a}

def rgb(c): return ((c >> 16) & 255, (c >> 8) & 255, c & 255)

def main():
    src, larg = sys.argv[1], int(sys.argv[2])
    orig = Image.open(src)
    if "A" in orig.mode:   # PNG com transparência: usa o canal alfa e ignora as cores escondidas no fundo
        rgba = orig.convert("RGBA")
        a = rgba.getchannel("A")
        bbox = a.point(lambda v: 255 if v > 40 else 0).getbbox()
        rgba = rgba.crop(bbox)
        alt = round(rgba.height * larg / rgba.width)
        pre = Image.new("RGBA", rgba.size)
        px = rgba.load(); pp = pre.load()
        for y in range(rgba.height):          # pré-multiplica pela transparência antes de reduzir
            for x in range(rgba.width):
                r, g, b, al = px[x, y]
                pp[x, y] = (r * al // 255, g * al // 255, b * al // 255, al)
        pre = pre.resize((larg, alt), Image.BOX)
        pal = [(i, rgb(c)) for i, c in PAL.items()]
        out = []
        for y in range(alt):
            for x in range(larg):
                r, g, b, al = pre.getpixel((x, y))
                if al < 120: out.append(0); continue
                r, g, b = r * 255 // al, g * 255 // al, b * 255 // al
                best = min(pal, key=lambda q: 2 * (q[1][0] - r) ** 2 + 4 * (q[1][1] - g) ** 2 + 3 * (q[1][2] - b) ** 2)
                out.append(best[0])
        escrever(out, larg, alt)
        return
    im = orig.convert("RGB")
    branco = sum(im.getpixel((2, 2))) > 600          # fundo branco (ou preto)
    if branco:
        mask = im.convert("L").point(lambda v: 255 if v < 235 else 0)
    else:
        mask = im.convert("L").point(lambda v: 255 if v > 22 else 0)
    im = im.crop(mask.getbbox())
    alt = round(im.height * larg / im.width)
    im = im.resize((larg, alt), Image.BOX)
    pal = [(i, rgb(c)) for i, c in PAL.items()]
    out = []
    for y in range(alt):
        for x in range(larg):
            r, g, b = im.getpixel((x, y))
            if branco and r + g + b > 600:            # fundo branco: transparente
                out.append(0); continue
            if not branco and r + g + b < 70 and r < 60:   # fundo preto: transparente
                out.append(0); continue
            best = min(pal, key=lambda p: 2 * (p[1][0] - r) ** 2 + 4 * (p[1][1] - g) ** 2 + 3 * (p[1][2] - b) ** 2)
            out.append(best[0])
    if branco:   # orla: píxeis claros encostados ao fundo são restos da mistura com o branco
        claros = {4, 5, 6, 21}
        for _ in range(2):
            novo = out[:]
            for y in range(alt):
                for x in range(larg):
                    i = y * larg + x
                    if out[i] in claros:
                        viz = [out[(y + dy) * larg + x + dx] for dx, dy in ((1,0),(-1,0),(0,1),(0,-1))
                               if 0 <= x + dx < larg and 0 <= y + dy < alt]
                        if any(v == 0 for v in viz) or len(viz) < 4: novo[i] = 0
            out = novo
    escrever(out, larg, alt)

def escrever(out, larg, alt):
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
