#!/usr/bin/env python3
"""
Corrige o código de arranque do 68000 (m68k_crt1.s) do exemplo 32x-skeleton do Marsdev, para o comando
funcionar no hardware real e no MiSTer:

1. Retira a procura de rato no arranque. A rotina get_mky tem esperas sem limite de tempo pelo sinal
   de resposta do periférico; com um comando normal no hardware real ou no MiSTer o 68000 pode ficar
   lá preso para sempre, e então a interrupção de fim de imagem (que lê o comando) nunca é ativada.
2. Põe mais esperas em get_input entre mudar a linha TH do comando e ler o valor (eram só dois nop).

Uso: python3 tools/corrigir_crt1.py build/m68k_crt1.s
Não falha a compilação se não encontrar o código esperado: avisa no registo.
"""
import re
import sys

caminho = sys.argv[1]
s = open(caminho).read()
original = s

# 1. procura de rato: desde o comentário "look for mouse" até antes de limpar o contador de imagens
m = re.search(r"\|\s*look for mouse.*?\n(?=[ \t]*move\.l[ \t]+#0,[ \t]*0xA1512C)", s, re.S | re.I)
if m:
    s = s[:m.start()] + "| (procura de rato retirada: prendia o 68000 no hardware real e no MiSTer)\n" + s[m.end():]
    print("crt1: procura de rato retirada")
else:
    print("AVISO crt1: não encontrei a procura de rato")

# 2. get_input com esperas maiores
espera = "\n".join(["        nop"] * 8)
novo = ("get_input:\n"
        "        move.b  #0x00,(a0)\n" + espera + "\n"
        "        move.b  (a0),d0\n"
        "        move.b  #0x40,(a0)\n" + espera + "\n"
        "        lsl.w   #8,d0\n"
        "        move.b  (a0),d0\n"
        "        rts\n")
m = re.search(r"^get_input:[ \t]*\n.*?^[ \t]*rts[ \t]*\n", s, re.S | re.M)
if m:
    s = s[:m.start()] + novo + s[m.end():]
    print("crt1: get_input com mais esperas")
else:
    print("AVISO crt1: não encontrei get_input")

if s != original:
    open(caminho, "w").write(s)
