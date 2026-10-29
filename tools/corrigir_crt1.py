#!/usr/bin/env python3
"""
Corrige o código de arranque do 68000 do exemplo 32x-skeleton do Marsdev (m68k_crt0.s e m68k_crt1.s),
para o comando funcionar no hardware real e no MiSTer.

O problema: depois de deixar os SH-2 arrancar, o arranque do exemplo salta para o main() em C
(src_md/main.c) e lê o comando na interrupção de fim de imagem. Esse main() e a entrada da interrupção
estão no cartucho (ROM). Quando os dois SH-2 do jogo começam a ler a ROM sem parar (gráficos e música),
o 68000 deixa de conseguir executar código da ROM e o comando nunca mais é lido. Os emuladores não
simulam esta disputa pela ROM.

A correção: o 68000 nunca mais sai da sua memória (Work RAM) depois de os SH-2 arrancarem.
1. Interrupções desligadas no processador (todas as escritas ao registo de estado) e no VDP da Mega
   Drive (sem interrupção de imagem nem de linha).
2. Logo a seguir, o código copiado para a Work RAM salta para um ciclo nosso (cinzas_ciclo), também na
   Work RAM, que: aceita pedidos dos SH-2 em COMM0 sem os executar (o jogo não usa nenhum), lê os dois
   comandos (get_pad do exemplo) e escreve-os em COMM8 e COMM10, e a cada ~3 ms incrementa COMM14
   (sinal de vida). COMM12 recebe uma marca de posição (A001 a A005). O jmp para o main() em C deixa de
   ser alcançado.
3. Procura de rato no arranque retirada (esperas sem limite de tempo).
4. Mais esperas em get_input entre mudar a linha TH do comando e ler o valor.

Uso:
  python3 tools/corrigir_crt1.py --so-interrupcoes build/m68k_crt0.s
  python3 tools/corrigir_crt1.py build/m68k_crt1.s
Não falha a compilação se não encontrar o código esperado: avisa no registo.
"""
import re
import sys

so_interrupcoes = "--so-interrupcoes" in sys.argv   # usado no m68k_crt0.s: só desliga as interrupções
caminho = [a for a in sys.argv[1:] if not a.startswith("--")][0]
s = open(caminho, newline="").read().replace("\r\n", "\n")   # os ficheiros do Marsdev usam fins de linha do Windows
nome = caminho.rsplit("/", 1)[-1]

def info(msg): print(f"{nome}: {msg}")
def aviso(msg): print(f"AVISO {nome}: {msg}")

# 1a. processador: qualquer escrita no registo de estado que baixe a máscara passa a 0x2700
padrao_sr = re.compile(r"(move(?:\.w)?[ \t]+)#(?:0x|\$)2[0-6]00,([ \t]*%?sr)", re.I)
n = len(padrao_sr.findall(s)); s = padrao_sr.sub(r"\g<1>#0x2700,\2", s)
padrao_andi = re.compile(r"^[ \t]*andi(?:\.w)?[ \t]+#(?:0x|\$)F[0-9A-F]FF,[ \t]*%?sr.*\n", re.I | re.M)
n2 = len(padrao_andi.findall(s)); s = padrao_andi.sub("        * (andi ao registo de estado retirado: não ligar interrupções)\n", s)
info(f"interrupções do processador desligadas ({n} move, {n2} andi)")
# 1b. VDP: registo 1 sem interrupção de imagem (bit 0x20), registo 0 sem a de linha (bit 0x10)
def r1(v): return "0x81%02X" % (int(v, 16) & ~0x20)
def r0(v): return "0x80%02X" % (int(v, 16) & ~0x10)
n3 = len(re.findall(r"#0x81[0-9A-Fa-f]{2}\b", s)) + len(re.findall(r"#0x80[0-9A-Fa-f]{2}\b", s))
s = re.sub(r"#0x81([0-9A-Fa-f]{2})\b", lambda m: "#" + r1(m.group(1)), s)
s = re.sub(r"#0x80([0-9A-Fa-f]{2})\b", lambda m: "#" + r0(m.group(1)), s)
info(f"interrupções do VDP desligadas ({n3} escritas)")

if not so_interrupcoes:
    # 2. ciclo nosso na Work RAM, logo a seguir a desligar as interrupções em __m68k_start
    ini = s.find("__m68k_start:")
    m = re.compile(r"^[ \t]*move(?:\.w)?[ \t]+#0x2700,[ \t]*%?sr[^\n]*\n", re.M | re.I).search(s, ini if ini >= 0 else 0)
    if ini >= 0 and m:
        s = s[:m.end()] + "        bra     cinzas_ciclo        /* Cinzas da Eternidade: fica na Work RAM a ler os comandos */\n" + s[m.end():]
        info("depois de desligar as interrupções, o 68000 salta para o ciclo cinzas_ciclo (Work RAM)")
    else:
        aviso("não encontrei __m68k_start ou a escrita ao registo de estado: os comandos NÃO vão ser lidos")
    ciclo = """
* Cinzas da Eternidade: ciclo principal do 68000, na Work RAM (sem interrupções e sem tocar na ROM)
cinzas_ciclo:
        move.w  #0x2700,sr
        move.w  #0xA001,0xA1512C    /* marca: início do ciclo */
        move.w  0xA15120,d0         /* pedido do SH-2 em COMM0? */
        beq.b   1f
        move.w  #0,0xA15120         /* aceita-o sem o executar (o jogo não usa nenhum) */
1:
        move.w  #0xA002,0xA1512C    /* marca: a ler a porta 1 */
        lea     0xA10003,a0
        move.w  #0xF000,d2
        bsr     get_pad
        move.w  d2,0xA15128         /* COMM8: porta 1 */
        move.w  #0xA003,0xA1512C    /* marca: a ler a porta 2 */
        lea     0xA10005,a0
        move.w  #0xF000,d2
        bsr     get_pad
        move.w  d2,0xA1512A         /* COMM10: porta 2 */
        addq.w  #1,0xA1512E         /* COMM14: sinal de vida */
        move.w  #0xA004,0xA1512C    /* marca: a esperar */
        move.w  #2400,d1
2:
        dbra    d1,2b               /* ~3 ms entre leituras (comandos de 6 botões) */
        move.w  #0xA005,0xA1512C    /* marca: leitura acabada */
        bra     cinzas_ciclo

"""
    m = re.search(r"^__m68k_end:", s, re.M)
    if m:
        s = s[:m.start()] + ciclo + s[m.start():]
        info("ciclo cinzas_ciclo acrescentado ao código copiado para a Work RAM")
    else:
        aviso("não encontrei __m68k_end")
    gp = s.find("get_pad:"); fim = s.find("__m68k_end:")
    if gp < 0 or fim < 0 or gp > fim:
        aviso("get_pad não está no código copiado para a Work RAM")

    # 3. procura de rato no arranque (antes de __m68k_start): get_mky responde logo "não há rato"
    chamada = re.compile(r"^([ \t]*)(?:bsr|jsr)(?:\.[bswl])?[ \t]+get_mky\b[^\n]*$", re.M | re.I)
    corte = ini if ini >= 0 else len(s)
    k = len(chamada.findall(s[:corte]))
    s = chamada.sub(r"\1moveq   #-1,d0              /* sem procura de rato: esperas sem limite */", s[:corte]) + s[corte:]
    info(f"procura de rato no arranque retirada ({k} chamadas)")

    # 4. get_input com esperas maiores
    espera = "\n".join(["        nop"] * 8)
    novo = ("get_input:\n        move.b  #0x00,(a0)\n" + espera + "\n        move.b  (a0),d0\n"
            "        move.b  #0x40,(a0)\n" + espera + "\n        lsl.w   #8,d0\n        move.b  (a0),d0\n        rts\n")
    m = re.search(r"^get_input:[ \t]*\n.*?^[ \t]*rts[ \t]*\n", s, re.S | re.M)
    if m:
        s = s[:m.start()] + novo + s[m.end():]
        info("get_input com mais esperas")
    else:
        aviso("não encontrei get_input")
    if s.count("bsr.b   get_input") and s.find("get_input:") - s.find("get_pad:") > 120:
        pass

    print(f"--- {nome}: __m68k_start depois da correção ---")
    i0 = s.find("__m68k_start:")
    print(s[i0:i0 + 900] if i0 >= 0 else "(não encontrado)")
    print("---")

open(caminho, "w").write(s)
