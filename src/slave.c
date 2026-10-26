/*
 * SH-2 secundário: toca a música do mapa atual e mistura os efeitos sonoros.
 * Substitui o src/slave.c do exemplo 32x-skeleton do Marsdev (o arranque chama slave() e nunca volta).
 *
 * O PWM corre a 22050 Hz (a esta frequência a portadora do PWM não se ouve no hardware real).
 * Música: src/musica.h, 8 bits com sinal a MUSICA_RATE, interpolada para 22050 Hz; uma faixa por mapa.
 * Efeitos: src/sfx.h, 16 bits com sinal à mesma taxa, em duas vozes (golpe da espada e restantes),
 * para o golpe, que é frequente, não cortar os outros sons.
 *
 * Partilhado com o SH-2 principal (acesso sem cache):
 *   som_pedido = contador << 4 | efeito   (novo efeito sempre que o valor muda)
 *   som_golpe  = contador do golpe da espada (pedido à parte para nunca apagar outro efeito)
 *   som_musica = faixa a tocar (0 fortaleza e título, 1 aldeia, 2 sem música)
 */
#include <stdint.h>
#include "mundo.h"
#include "musica.h"
#include "sfx.h"

#define PWM_CTRL  (*(volatile uint16_t *)0x20004030)
#define PWM_CYCLE (*(volatile uint16_t *)0x20004032)
#define PWM_MONO  (*(volatile uint16_t *)0x20004038)
#define PWM_FULL  0x8000
#define UNCACHED(v) (*(volatile uint32_t *)((uintptr_t)&(v) | 0x20000000))
#define CLOCK_NTSC 23011361u
#define TAXA_SAIDA 22050u

volatile uint32_t som_pedido, som_golpe, som_musica;

static const int8_t *const FAIXA[2] = {MUSICA_FORTALEZA, MUSICA_ALDEIA};
static const uint32_t FAIXA_LEN[2] = {MUSICA_FORTALEZA_LEN, MUSICA_ALDEIA_LEN};

typedef struct { const int16_t *d; uint32_t len, idx, frac; } Voz;

static void voz_inicia(Voz *v, uint32_t id) {
    switch (id) {
    case SFX_ID_HURT:     v->d = SFX_HURT;     v->len = SFX_HURT_LEN;     break;
    case SFX_ID_POWERUP:  v->d = SFX_POWERUP;  v->len = SFX_POWERUP_LEN;  break;
    case SFX_ID_ATTACK:   v->d = SFX_ATTACK;   v->len = SFX_ATTACK_LEN;   break;
    case SFX_ID_TELEPORT: v->d = SFX_TELEPORT; v->len = SFX_TELEPORT_LEN; break;
    case SFX_ID_MONSTER:  v->d = SFX_MONSTER;  v->len = SFX_MONSTER_LEN;  break;
    default:              v->d = 0; break;
    }
    v->idx = 0; v->frac = 0;
}
static int32_t voz_amostra(Voz *v, uint32_t passo) {
    if (!v->d) return 0;
    int32_t s = v->d[v->idx];
    v->frac += passo;
    while (v->frac >= 65536u) { v->frac -= 65536u; if (++v->idx >= v->len) { v->d = 0; break; } }
    return s;
}
static void pwm_put(uint16_t v) {
    while (PWM_MONO & PWM_FULL) {}   /* espera enquanto a fila está cheia */
    PWM_MONO = v;
}

void slave(void) {
    const int32_t ciclo = (int32_t)(CLOCK_NTSC / TAXA_SAIDA + 1u);   /* ~1044 */
    const int32_t meio = ciclo / 2, amp = meio - 2;
    const uint32_t passo = ((uint32_t)MUSICA_RATE << 16) / TAXA_SAIDA;
    PWM_MONO = 1; PWM_MONO = 1; PWM_MONO = 1;
    PWM_CYCLE = (uint16_t)ciclo;     /* taxa = 23,01 MHz / ciclo */
    PWM_CTRL = 0x0185;               /* esquerdo e direito ligados */
    for (int v = 1; v < meio; v += 2) pwm_put((uint16_t)v);   /* sobe até ao silêncio sem estalo */

    Voz golpe = {0, 0, 0, 0}, outro = {0, 0, 0, 0};
    uint32_t ultimo = UNCACHED(som_pedido), ultGolpe = UNCACHED(som_golpe), faixa = UNCACHED(som_musica), idx = 0, frac = 0;
    for (;;) {
        uint32_t ped = UNCACHED(som_pedido), gp = UNCACHED(som_golpe);
        if (ped != ultimo) { ultimo = ped; voz_inicia(&outro, ped & 15); }
        if (gp != ultGolpe) { ultGolpe = gp; voz_inicia(&golpe, SFX_ID_ATTACK); }
        uint32_t f = UNCACHED(som_musica);
        if (f != faixa) { faixa = f; idx = 0; frac = 0; }   /* mudou de mapa: começa a faixa nova */
        int32_t mix = 0;
        if (faixa < 2) {   /* 2 = música desligada nas opções */
            const int8_t *m = FAIXA[faixa]; uint32_t len = FAIXA_LEN[faixa];
            /* música: interpolação linear entre duas amostras, em escala de 16 bits */
            int32_t a = m[idx], b = m[(idx + 1 < len) ? idx + 1 : 0];
            mix = (a << 8) + (b - a) * (int32_t)(frac >> 8);
            frac += passo;
            while (frac >= 65536u) { frac -= 65536u; if (++idx >= len) idx = 0; }
        }
        mix += voz_amostra(&golpe, passo) + voz_amostra(&outro, passo);
        if (mix > 32767) mix = 32767;
        if (mix < -32767) mix = -32767;
        pwm_put((uint16_t)(meio + ((mix * amp) >> 15)));
    }
}
