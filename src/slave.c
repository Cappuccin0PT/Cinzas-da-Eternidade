/*
 * SH-2 secundário: toca a música do mapa atual e mistura os efeitos sonoros.
 * Substitui o src/slave.c do exemplo 32x-skeleton do Marsdev (o arranque chama slave() e nunca volta).
 *
 * O PWM corre a 22050 Hz (a esta frequência a portadora do PWM não se ouve no hardware real).
 * Música: src/musica.h, IMA ADPCM (4 bits) a MUSICA_RATE, descomprimida em tempo real e interpolada
 *         para 22050 Hz; faixas: 0 fortaleza e título, 1 aldeia, 2 intro, 3 sem música.
 * Efeitos: src/sfx.h, 16 bits com sinal à mesma taxa, em duas vozes (golpe da espada e restantes),
 * para o golpe, que é frequente, não cortar os outros sons.
 *
 * Partilhado com o SH-2 principal (acesso sem cache):
 *   som_pedido = contador << 4 | efeito   (novo efeito sempre que o valor muda)
 *   som_golpe  = contador do golpe da espada (pedido à parte para nunca apagar outro efeito)
 *   som_musica = faixa a tocar (FAIXA_* em mundo.h)
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

static const uint8_t *const FAIXA[3] = {MUSICA_FORTALEZA, MUSICA_ALDEIA, MUSICA_INTRO};
static const uint32_t FAIXA_LEN[3] = {MUSICA_FORTALEZA_LEN, MUSICA_ALDEIA_LEN, MUSICA_INTRO_LEN};

/* Descodificador IMA ADPCM: devolve a próxima amostra de 16 bits da faixa */
typedef struct { const uint8_t *d; uint32_t len, pos; int32_t pred; int idx; } Adpcm;
static void adpcm_inicia(Adpcm *a, uint32_t faixa) { a->d = FAIXA[faixa]; a->len = FAIXA_LEN[faixa]; a->pos = 0; a->pred = 0; a->idx = 0; }
static int32_t adpcm_proxima(Adpcm *a) {
    if (a->pos >= a->len) { a->pos = 0; a->pred = 0; a->idx = 0; }   /* fim da faixa: volta ao início */
    uint32_t b = a->d[a->pos >> 1]; uint32_t n = (a->pos & 1) ? (b >> 4) : (b & 15);
    a->pos++;
    int32_t step = ADPCM_STEP[a->idx], d = step >> 3;
    if (n & 4) d += step;
    if (n & 2) d += step >> 1;
    if (n & 1) d += step >> 2;
    a->pred += (n & 8) ? -d : d;
    if (a->pred > 32767) a->pred = 32767;
    if (a->pred < -32767) a->pred = -32767;
    a->idx += ADPCM_INDEX[n];
    if (a->idx < 0) a->idx = 0;
    if (a->idx > 88) a->idx = 88;
    return a->pred;
}

typedef struct { const int16_t *d; uint32_t len, idx, frac; } Voz;

static void voz_inicia(Voz *v, uint32_t id) {
    switch (id) {
    case SFX_ID_HURT:     v->d = SFX_HURT;     v->len = SFX_HURT_LEN;     break;
    case SFX_ID_POWERUP:  v->d = SFX_POWERUP;  v->len = SFX_POWERUP_LEN;  break;
    case SFX_ID_ATTACK:   v->d = SFX_ATTACK;   v->len = SFX_ATTACK_LEN;   break;
    case SFX_ID_TELEPORT: v->d = SFX_TELEPORT; v->len = SFX_TELEPORT_LEN; break;
    case SFX_ID_MONSTER:  v->d = SFX_MONSTER;  v->len = SFX_MONSTER_LEN;  break;
    case SFX_ID_TROVAO:   v->d = SFX_TROVAO;   v->len = SFX_TROVAO_LEN;   break;
    case SFX_ID_FOGO:     v->d = SFX_FOGO;     v->len = SFX_FOGO_LEN;     break;
    case SFX_ID_OSSOS:    v->d = SFX_OSSOS;    v->len = SFX_OSSOS_LEN;    break;
    case SFX_ID_PORTA:    v->d = SFX_PORTA;    v->len = SFX_PORTA_LEN;    break;
    case SFX_ID_IMPACTO:  v->d = SFX_IMPACTO;  v->len = SFX_IMPACTO_LEN;  break;
    case SFX_ID_CONFIRMAR:v->d = SFX_CONFIRMAR;v->len = SFX_CONFIRMAR_LEN;break;
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
    uint32_t ultimo = UNCACHED(som_pedido), ultGolpe = UNCACHED(som_golpe), faixa = UNCACHED(som_musica), frac = 0;
    Adpcm mus; int32_t s0 = 0, s1 = 0;
    if (faixa < 3) { adpcm_inicia(&mus, faixa); s0 = adpcm_proxima(&mus); s1 = adpcm_proxima(&mus); }
    for (;;) {
        uint32_t ped = UNCACHED(som_pedido), gp = UNCACHED(som_golpe);
        if (ped != ultimo) { ultimo = ped; voz_inicia(&outro, ped & 15); }
        if (gp != ultGolpe) { ultGolpe = gp; voz_inicia(&golpe, SFX_ID_ATTACK); }
        uint32_t f = UNCACHED(som_musica);
        if (f != faixa) {   /* mudou de faixa: começa do início */
            faixa = f; frac = 0;
            if (faixa < 3) { adpcm_inicia(&mus, faixa); s0 = adpcm_proxima(&mus); s1 = adpcm_proxima(&mus); }
        }
        int32_t mix = 0;
        if (faixa < 3) {
            /* música: interpolação linear entre duas amostras descomprimidas */
            mix = s0 + (((s1 - s0) * (int32_t)(frac >> 4)) >> 12);
            frac += passo;
            while (frac >= 65536u) { frac -= 65536u; s0 = s1; s1 = adpcm_proxima(&mus); }
        }
        mix += voz_amostra(&golpe, passo) + voz_amostra(&outro, passo);
        if (mix > 32767) mix = 32767;
        if (mix < -32767) mix = -32767;
        pwm_put((uint16_t)(meio + ((mix * amp) >> 15)));
    }
}
