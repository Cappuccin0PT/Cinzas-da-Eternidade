#!/usr/bin/env python3
"""
Corrige o código de arranque do 68000 (m68k_crt1.s) do exemplo 32x-skeleton do Marsdev, para o comando
funcionar no hardware real e no MiSTer.

O problema: o exemplo lê o comando na interrupção de fim de imagem, mas o código dessa interrupção está
no cartucho (ROM). Quando os dois SH-2 do jogo começam a ler a ROM sem parar (gráficos e música), o
hardware real e o MiSTer dão-lhes prioridade e o 68000 fica preso à espera da ROM dentro da interrupção:
o comando deixa de ser lido. Os emuladores não simulam esta disputa pela ROM.

A correção (tudo no ciclo principal do 68000, que corre na memória dele, sem tocar na ROM):
1. Desliga as interrupções do 68000 (deixa de saltar para o código na ROM).
2. Lê os dois comandos no ciclo principal, a cada ~3 ms, e escreve-os em COMM8 e COMM10.
   Incrementa COMM14 a cada leitura (sinal de vida, mostrado no título do jogo).
3. Retira a procura de rato no arranque (tem esperas sem limite de tempo).
4. Põe mais esperas em get_input entre mudar a linha TH do comando e ler o valor.

Uso: python3 tools/corrigir_crt1.py build/m68k_crt1.s
Não falha a compilação se não encontrar o código esperado: avisa no registo.
"""
import re
import sys

caminho = sys.argv[1]
s = open(caminho).read()
original = s

def aviso(msg): print("AVISO crt1: " + msg)

# 1. interrupções sempre desligadas no 68000
n = len(re.findall(r"#0x2000,\s*sr", s))
if n:
    s = re.sub(r"#0x2000,(\s*)sr", r"#0x2700,\1sr", s)
    print(f"crt1: interrupções do 68000 desligadas ({n} sítios)")
else:
    aviso("não encontrei onde as interrupções são ligadas")

# 2. leitura dos comandos no ciclo principal
rotina = """
| Cinzas da Eternidade: lê os dois comandos no ciclo principal (sem interrupções, sem tocar na ROM)
ler_comandos:
        movem.l d0-d2/a0,-(sp)
        lea     0xA10003,a0
        move.w  #0xF000,d2
        bsr     get_pad
        move.w  d2,0xA15128         /* COMM8: porta 1 */
        lea     0xA10005,a0
        move.w  #0xF000,d2
        bsr     get_pad
        move.w  d2,0xA1512A         /* COMM10: porta 2 */
        addq.w  #1,0xA1512E         /* COMM14: sinal de vida */
        move.w  #2400,d1
1:
        dbra    d1,1b               /* ~3 ms entre leituras (comandos de 6 botões) */
        movem.l (sp)+,d0-d2/a0
        rts

"""
m = re.search(r"^[ \t]*\|[^\n]*any other 68000 tasks here[^\n]*\n", s, re.M | re.I)
if m:
    s = s[:m.start()] + "        bsr     ler_comandos        /* Cinzas da Eternidade: lê os comandos */\n" + s[m.end():]
    print("crt1: ciclo principal passa a ler os comandos")
else:
    m = re.search(r"^main_loop:[ \t]*\n", s, re.M)
    if m:
        s = s[:m.end()] + "        bsr     ler_comandos        /* Cinzas da Eternidade: lê os comandos */\n" + s[m.end():]
        print("crt1: ciclo principal passa a ler os comandos (no início de main_loop)")
    else:
        aviso("não encontrei o ciclo principal (main_loop)")
m = re.search(r"^__m68k_end:", s, re.M)
if m:
    s = s[:m.start()] + rotina + s[m.start():]
    print("crt1: rotina ler_comandos acrescentada")
else:
    aviso("não encontrei __m68k_end")

# 3. procura de rato no arranque
m = re.search(r"\|\s*look for mouse.*?\n(?=[ \t]*move\.l[ \t]+#0,[ \t]*0xA1512C)", s, re.S | re.I)
if m:
    s = s[:m.start()] + "| (procura de rato retirada: tem esperas sem limite de tempo)\n" + s[m.end():]
    print("crt1: procura de rato retirada")
else:
    aviso("não encontrei a procura de rato")

# 4. get_input com esperas maiores
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
    aviso("não encontrei get_input")

if s != original:
    open(caminho, "w").write(s)
