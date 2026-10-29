#!/usr/bin/env python3
"""
Corrige o código de arranque do 68000 (m68k_crt1.s) do exemplo 32x-skeleton do Marsdev, para o comando
funcionar no hardware real e no MiSTer.

O problema: o exemplo lê o comando na interrupção de fim de imagem, mas o código dessa interrupção está
no cartucho (ROM). Quando os dois SH-2 do jogo começam a ler a ROM sem parar (gráficos e música), o
hardware real e o MiSTer dão-lhes prioridade e o 68000 fica preso à espera da ROM dentro da interrupção:
o comando deixa de ser lido. Os emuladores não simulam esta disputa pela ROM.

A correção (tudo no ciclo principal do 68000, que corre na memória dele, sem tocar na ROM):
1. Desliga as interrupções do 68000 (deixa de saltar para o código na ROM): no processador, em todas as
   escritas ao registo de estado e em cada volta do ciclo, e no VDP da Mega Drive (sem interrupção de
   imagem nem de linha), para nenhuma interrupção disparar mesmo que algum código a ligue.
2. Lê os dois comandos no ciclo principal, a cada ~3 ms, e escreve-os em COMM8 e COMM10.
   Incrementa COMM14 a cada leitura (sinal de vida, mostrado no título do jogo).
3. Aceita de imediato qualquer pedido dos SH-2 em COMM0 sem o executar: o jogo não usa nenhum
   (SRAM, rato, ecrã da Mega Drive) e algumas dessas rotinas têm esperas sem fim.
4. Retira a procura de rato no arranque (tem esperas sem limite de tempo).
5. Põe mais esperas em get_input entre mudar a linha TH do comando e ler o valor.
Diagnóstico: COMM12 recebe uma marca de posição (A001 início do ciclo, na etiqueta cinzas_ciclo, A002 a A005 dentro da leitura,
A006 pedido recebido), mostrada no título do jogo como PT.

Uso: python3 tools/corrigir_crt1.py build/m68k_crt1.s
Não falha a compilação se não encontrar o código esperado: avisa no registo.
"""
import re
import sys

so_interrupcoes = "--so-interrupcoes" in sys.argv   # usado no m68k_crt0.s: só desliga as interrupções
caminho = [a for a in sys.argv[1:] if not a.startswith("--")][0]
s = open(caminho).read()
original = s
nome = caminho.rsplit("/", 1)[-1]

def aviso(msg): print(f"AVISO {nome}: " + msg)

# 1. interrupções sempre desligadas no 68000
#    a) no processador: qualquer escrita no registo de estado que baixe a máscara passa a 0x2700
padrao_sr = re.compile(r"(move(?:\.w)?[ \t]+)#(?:0x|\$)2[0-6]00,([ \t]*%?sr)", re.I)
n = len(padrao_sr.findall(s))
s = padrao_sr.sub(r"\1#0x2700,\2", s)
padrao_andi = re.compile(r"^[ \t]*andi(?:\.w)?[ \t]+#(?:0x|\$)F[0-9A-F]FF,[ \t]*%?sr.*\n", re.I | re.M)
n2 = len(padrao_andi.findall(s))
s = padrao_andi.sub("        | (andi ao registo de estado retirado: não ligar interrupções)\n", s)
print(f"{nome}: interrupções do processador desligadas ({n} move, {n2} andi)")
#    b) no VDP da Mega Drive: registo 1 sem interrupção de imagem (bit 0x20), registo 0 sem a de linha (bit 0x10)
def vdp1(m): return "#0x81%02X" % (int(m.group(1), 16) & ~0x20)
def vdp0(m): return "#0x80%02X" % (int(m.group(1), 16) & ~0x10)
n3 = len(re.findall(r"#0x81([0-9A-Fa-f]{2})\b", s)); s = re.sub(r"#0x81([0-9A-Fa-f]{2})\b", vdp1, s)
n4 = len(re.findall(r"#0x80([0-9A-Fa-f]{2})\b", s)); s = re.sub(r"#0x80([0-9A-Fa-f]{2})\b", vdp0, s)
def tabela(m):   # tabelas de dados com valores de registos do VDP (dc.w 0x8174, ...)
    linha = m.group(0)
    linha = re.sub(r"\b0x81([0-9A-Fa-f]{2})\b", lambda k: "0x81%02X" % (int(k.group(1), 16) & ~0x20), linha)
    return re.sub(r"\b0x80([0-9A-Fa-f]{2})\b", lambda k: "0x80%02X" % (int(k.group(1), 16) & ~0x10), linha)
n5 = len(re.findall(r"^[ \t]*dc\.w[^\n]*\b0x8[01][0-9A-Fa-f]{2}\b[^\n]*$", s, re.M | re.I))
s = re.sub(r"^[ \t]*dc\.w[^\n]*$", tabela, s, flags=re.M | re.I)
n3 += n5
print(f"{nome}: interrupções do VDP desligadas ({n3} escritas no registo 1, {n4} no registo 0)")
for linha in s.splitlines():   # outras formas de ligar interrupções, para conferir no registo
    if re.search(r"\bsr\b|0x81|0x80|vblank|vert_blank|hint|vint", linha, re.I) and not linha.strip().startswith(("*", "|")):
        print(f"   {nome}: " + linha.strip())
if so_interrupcoes:
    if s != original:
        open(caminho, "w").write(s)
    sys.exit(0)

# 2. leitura dos comandos no ciclo principal
rotina = """
| Cinzas da Eternidade: lê os dois comandos no ciclo principal (sem interrupções, sem tocar na ROM)
ler_comandos:
        movem.l d0-d2/a0,-(sp)
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
1:
        dbra    d1,1b               /* ~3 ms entre leituras (comandos de 6 botões) */
        move.w  #0xA005,0xA1512C    /* marca: leitura acabada */
        movem.l (sp)+,d0-d2/a0
        rts

"""
CHAMADA = "        bsr     ler_comandos        /* Cinzas da Eternidade: lê os comandos */\n"
# comentários do assembly: '|' na versão do Chilly Willy, '*' no Marsdev
m = re.search(r"^[ \t]*[|*;#][^\n]*any other 68000 tasks here[^\n]*\n", s, re.M | re.I)
if m:
    s = s[:m.start()] + CHAMADA + s[m.end():]
    print("crt1: ciclo principal passa a ler os comandos")
else:
    # plano B: logo a seguir ao teste dos pedidos em COMM0 (bne para handle_req)
    m = re.search(r"^[ \t]*bne(?:\.[bsw])?[ \t]+handle_req\b[^\n]*\n", s, re.M | re.I)
    if m:
        s = s[:m.end()] + CHAMADA + s[m.end():]
        print("crt1: ciclo principal passa a ler os comandos (a seguir ao teste dos pedidos)")
    else:
        aviso("não encontrei o ciclo principal: os comandos NÃO vão ser lidos")
# Etiqueta própria no início do ciclo principal (o nome do ciclo muda entre versões do exemplo):
# é o ponto do código copiado para a RAM onde o ciclo lê os pedidos em COMM0.
ini = s.find("__m68k_start")
m = re.compile(r"^[ \t]*move\.w[ \t]+0xA15120,[ \t]*d0.*\n", re.M | re.I).search(s, ini if ini >= 0 else 0)
tem_ciclo = False
if m:
    s = s[:m.start()] + ("cinzas_ciclo:\n        move.w  #0x2700,sr          /* sem interrupções, sempre */\n"
                         "        move.w  #0xA001,0xA1512C    /* marca: início do ciclo principal */\n") + s[m.start():]
    tem_ciclo = True
    print("crt1: etiqueta cinzas_ciclo no início do ciclo principal")
    print("--- ciclo principal do 68000 depois da correção ---")
    i0 = m.start()
    print(s[max(0, s.rfind("\n", 0, i0 - 200)):i0 + 600])
    print("---")
else:
    aviso("não encontrei a leitura de COMM0 no ciclo principal")
m = re.search(r"^handle_req:[ \t]*\n", s, re.M)
if m and tem_ciclo:
    s = s[:m.end()] + ("        move.w  #0xA006,0xA1512C    /* marca: pedido recebido */\n"
                       "        move.w  #0,0xA15120         /* aceita o pedido sem o executar (o jogo não usa nenhum) */\n"
                       "        bra     cinzas_ciclo\n") + s[m.end():]
    print("crt1: pedidos em COMM0 aceites sem os executar")
else:
    aviso("não encontrei handle_req (os pedidos continuam a ser executados)")
m = re.search(r"^__m68k_end:", s, re.M)
if m:
    s = s[:m.start()] + rotina + s[m.start():]
    print("crt1: rotina ler_comandos acrescentada")
else:
    aviso("não encontrei __m68k_end")

# 3. procura de rato no arranque: antes do ciclo principal, cada chamada a get_mky passa a responder
#    logo "não há rato" (-1), sem as esperas sem limite de tempo da rotina original
fim = s.find("cinzas_ciclo:")
if fim < 0: fim = len(s)
chamada = re.compile(r"^([ \t]*)(?:bsr|jsr)(?:\.[bswl])?[ \t]+get_mky\b[^\n]*$", re.M | re.I)
n = len(chamada.findall(s[:fim]))
s = chamada.sub(r"\1moveq   #-1,d0              /* sem procura de rato: esperas sem limite */", s[:fim]) + s[fim:]
if n:
    print(f"crt1: procura de rato retirada ({n} chamadas)")
else:
    aviso("não encontrei chamadas a get_mky antes do ciclo principal")
for linha in s.splitlines():
    if re.search(r"mky|mouse", linha, re.I):
        print("   rato: " + linha.strip())

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
