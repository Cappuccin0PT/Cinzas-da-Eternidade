#!/usr/bin/env python3
"""
Sintetiza os efeitos sonoros da cutscene de abertura (trovão, fogo, ossos, porta da cripta, impacto e
confirmar) e grava-os como WAV em sons/. Só é preciso correr uma vez; os WAV ficam no repositório e podem
ser trocados por gravações próprias com o mesmo nome.
Uso: python3 tools/sintetizar_sfx.py
"""
import math
import random
import struct

TAXA = 22050
random.seed(32)

def grava(nome, s):
    pico = max(1e-6, max(abs(v) for v in s))
    dados = b"".join(struct.pack("<h", int(v / pico * 30000)) for v in s)
    with open(f"sons/{nome}.wav", "wb") as f:
        f.write(b"RIFF" + struct.pack("<I", 36 + len(dados)) + b"WAVEfmt " + struct.pack("<IHHIIHH", 16, 1, 1, TAXA, TAXA * 2, 2, 16))
        f.write(b"data" + struct.pack("<I", len(dados)) + dados)

def ruido_filtrado(n, corte):
    y, out = 0.0, []
    for _ in range(n):
        y += corte * (random.uniform(-1, 1) - y)
        out.append(y)
    return out

def trovao():
    n = int(TAXA * 2.2); r = ruido_filtrado(n, 0.03); r2 = ruido_filtrado(n, 0.2)
    return [(r[i] * 3 + (r2[i] if i < TAXA * 0.15 else 0)) * math.exp(-i / (TAXA * 0.7)) * (1 + 0.5 * math.sin(i / 900)) for i in range(n)]

def fogo():
    n = int(TAXA * 1.6); base = ruido_filtrado(n, 0.08); out = []
    for i in range(n):
        v = base[i] * 0.6
        if random.random() < 0.004: v += random.uniform(-1, 1) * 2
        env = min(1, i / (TAXA * 0.2)) * min(1, (n - i) / (TAXA * 0.4))
        out.append(v * env)
    return out

def ossos():
    n = int(TAXA * 0.9); out = [0.0] * n
    for k in range(9):
        t0 = int(random.uniform(0, 0.75) * TAXA); f = random.uniform(900, 1800)
        for i in range(int(TAXA * 0.04)):
            if t0 + i < n: out[t0 + i] += math.sin(i * f * 2 * math.pi / TAXA) * math.exp(-i / (TAXA * 0.008)) + random.uniform(-.3, .3) * math.exp(-i / 200)
    return out

def porta():
    n = int(TAXA * 1.8); out = []
    for i in range(n):
        t = i / TAXA
        rangido = math.sin(2 * math.pi * (70 + 30 * math.sin(t * 7)) * t) * (0.6 + 0.4 * math.sin(t * 53)) * math.exp(-t * 1.2) * (t < 1.3)
        baque = (random.uniform(-1, 1) * 0.8 + math.sin(2 * math.pi * 55 * t)) * math.exp(-(t - 1.3) * 9) if t >= 1.3 else 0
        out.append(rangido * 0.7 + baque)
    return out

def impacto():
    n = int(TAXA * 1.0); out = []
    for i in range(n):
        t = i / TAXA
        out.append(math.sin(2 * math.pi * (90 - 40 * t) * t) * math.exp(-t * 5) + random.uniform(-1, 1) * math.exp(-t * 18) + math.sin(2 * math.pi * 1900 * t) * 0.3 * math.exp(-t * 6))
    return out

def confirmar():
    n = int(TAXA * 0.7); out = []
    for i in range(n):
        t = i / TAXA; f = 660 if t < 0.12 else 990
        out.append((math.sin(2 * math.pi * f * t) + 0.4 * math.sin(4 * math.pi * f * t)) * math.exp(-max(0, t - 0.12) * 5) * min(1, t * 200))
    return out

for nome, fn in [("trovao", trovao), ("fogo", fogo), ("ossos", ossos), ("porta", porta), ("impacto", impacto), ("confirmar", confirmar)]:
    grava(nome, fn())
    print("sons/" + nome + ".wav")
