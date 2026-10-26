/*
 * Cinzas da Eternidade — Sega 32X
 * Sobrevivência por hordas: o herói ataca sozinho, os inimigos chegam em vagas cada vez maiores,
 * os cristais dão experiência e cada nível novo dá a escolher uma de três melhorias.
 * Sobrevive 10 minutos até ao amanhecer.
 *
 * Arte, tiles, mapa (96x96 tiles) e fonte vêm de assets.h, gerado no PC por tools/gerar_assets.c.
 * A lógica corre a 60 passos por segundo medidos pelo temporizador FRT do SH-2.
 *
 * Técnicas 32X usadas:
 *  - modo de 256 cores; scroll em palavras de 16 bits + registo de deslocamento de 1 px (SFT)
 *  - sprites escritos na área de sobreposição (bytes a 0 são transparentes), já pré-deslocados e espelhados
 *  - ciclos de paleta para água, runas e luz dos braseiros; relâmpagos trocando a paleta inteira
 *
 * Som: o SH-2 secundário (src/slave.c) toca a música e mistura os efeitos (dano e subida de nível).
 *
 * Controlos: direcional = mover, START = começar/pausa, A/B/C = escolher melhoria
 *            (e atacar com a espada, se estiver em modo manual no menu do título).
 */
#include <stdint.h>
#include "32x.h"
#include "hw_32x.h"
#include "mundo.h"
#include "assets.h"
#include "logo.h"

#ifndef SEGA_CTRL_UP
#define SEGA_CTRL_UP     0x0001
#define SEGA_CTRL_DOWN   0x0002
#define SEGA_CTRL_LEFT   0x0004
#define SEGA_CTRL_RIGHT  0x0008
#define SEGA_CTRL_B      0x0010
#define SEGA_CTRL_C      0x0020
#define SEGA_CTRL_A      0x0040
#define SEGA_CTRL_START  0x0080
#endif
#ifndef MARS_VDP_MODE_256
#define MARS_VDP_MODE_256 1
#endif
#define BTN_OK (SEGA_CTRL_A | SEGA_CTRL_B | SEGA_CTRL_C)

/* ===== Hardware ===== */
#ifndef PREVIEW
#define FB_LINES  ((volatile uint16_t *)0x24000000)
#define FB8       ((volatile uint8_t  *)0x24000200)
#define FBW       ((volatile uint16_t *)0x24000200)
#define OWW       ((volatile uint16_t *)0x24020200)
#define CRAM      ((volatile uint16_t *)0x20004200)
#define VDP_FBCTL (*(volatile uint16_t *)0x2000410A)
#define VDP_SFT   (*(volatile uint16_t *)0x20004102)
#define FRT_TCR   (*(volatile uint8_t  *)0xFFFFFE16)
#define FRT_FRCH  (*(volatile uint8_t  *)0xFFFFFE12)
#define FRT_FRCL  (*(volatile uint8_t  *)0xFFFFFE13)
#define PEN_READY() (VDP_FBCTL & 0x2000)
#define RD16(p)   (*(const uint16_t *)(p))
#define OW_PUT(i, v) (OWW[i] = (v))
static uint16_t frt_read(void) { uint8_t h = FRT_FRCH; uint8_t l = FRT_FRCL; return (uint16_t)((h << 8) | l); }
static void frt_init(void) { FRT_TCR = (uint8_t)((FRT_TCR & ~3) | 2); }   /* Pφ/128 */
static void set_shift(int s) { VDP_SFT = (uint16_t)s; }
#else
extern uint16_t pv_lines[256]; extern uint8_t pv_pix[SCR_W * SCR_H]; extern uint16_t pv_cram[256]; extern int pv_sft;
#define FB_LINES  pv_lines
#define FB8       pv_pix
#define FBW       ((uint16_t *)pv_pix)
#define CRAM      pv_cram
#define PEN_READY() 1
#define RD16(p)   ((uint16_t)(((p)[0] << 8) | (p)[1]))
static inline void OW_PUT(int i, uint16_t v) { uint8_t a = (uint8_t)(v >> 8), b = (uint8_t)v; if (a) pv_pix[i * 2] = a; if (b) pv_pix[i * 2 + 1] = b; }
extern uint16_t pv_frt(void);
static uint16_t frt_read(void) { return pv_frt(); }
static void frt_init(void) {}
static void set_shift(int s) { pv_sft = s; }
#endif
#define TICKS_PER_STEP 2996   /* 23,01 MHz / 128 / 60 */
#include "sincronia.h"
#ifndef PREVIEW
extern volatile uint32_t som_pedido, som_golpe, som_musica;   /* partilhados com o SH-2 secundário (slave.c) */
#define SOM_LER(v) (*(volatile uint32_t *)((uintptr_t)&(v) | 0x20000000))   /* acesso sem cache */
#else
static uint32_t som_pedido, som_golpe, som_musica;
#define SOM_LER(v) (v)
#endif
/* Pede um efeito ao SH-2 secundário; o contador garante que o mesmo efeito repetido também toca. */
static void tocar_sfx(int id) {
    if (id == SFX_ID_ATTACK) SOM_LER(som_golpe) = SOM_LER(som_golpe) + 1u;   /* voz própria do golpe */
    else SOM_LER(som_pedido) = ((SOM_LER(som_pedido) + 16u) & ~15u) | (uint32_t)id;
}

#define LINEW (SCR_W / 2)

#ifndef PREVIEW
/* O exemplo do Marsdev liga sem a biblioteca C (-nostdlib). O compilador pode gerar chamadas a memcpy
   para copiar estruturas, por isso o jogo traz a sua. O atributo impede o compilador de transformar
   este ciclo numa chamada a memcpy (o que daria recursão infinita) e de a remover na otimização LTO. */
__attribute__((used, noinline, optimize("no-tree-loop-distribute-patterns")))
void *memcpy(void *dst, const void *src, __SIZE_TYPE__ n) {
    uint8_t *d = (uint8_t *)dst; const uint8_t *s = (const uint8_t *)src;
    while (n--) *d++ = *s++;
    return dst;
}
#endif

static uint32_t seed = 12345;
static int rndi(int n) { seed = seed * 1103515245u + 12345u; return (int)((seed >> 16) % (uint32_t)n); }
static int iabs(int v) { return v < 0 ? -v : v; }
static int dist_approx(int dx, int dy) { int a = iabs(dx), b = iabs(dy); return a > b ? a + (b * 3 >> 3) : b + (a * 3 >> 3); }
static const uint8_t BAYER[16] = {0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};

/* ===== Dados copiados para a SDRAM no arranque ===== */
static uint16_t tilesRam[NTILES * 128];
static uint8_t mapRam[MAP_W * MAP_H], attrRam[MAP_W * MAP_H];
typedef struct { const uint8_t *t, *a; const Prop *props; int nprops; const char *nome, *sub; int sx, sy; } MapaDef;
static const MapaDef MAPAS[NMAPAS] = {
    {MAP0_T, MAP0_A, PROPS0, NPROPS0, "PÁTIO DA FORTALEZA", "AS RUÍNAS DE CINZA", 48 * 16 + 8, 50 * 16 + 8},
    {MAP1_T, MAP1_A, PROPS1, NPROPS1, "ALDEIA DE HINOSATO", "A ALDEIA EM CHAMAS", 48 * 16 + 8, 86 * 16 + 8},
};
static int mapa;
static int musicaOn = 1;   /* opção do menu: música ligada ou desligada */
static const Prop *PROPSP; static int NPROPSP;   /* adereços do mapa atual */
static void load_tiles(void) {
    const uint16_t *s = (const uint16_t *)TILES;
    for (int i = 0; i < NTILES * 128; i++) tilesRam[i] = s[i];
}
static void load_map(int m) {
    mapa = m;
    for (int i = 0; i < MAP_W * MAP_H; i++) { mapRam[i] = MAPAS[m].t[i]; attrRam[i] = MAPAS[m].a[i]; }
    PROPSP = MAPAS[m].props; NPROPSP = MAPAS[m].nprops;
    SOM_LER(som_musica) = musicaOn ? (uint32_t)m : 2u;   /* música do mapa (2 = sem música) */
}
static int attr_at(int px, int py) {   /* px,py em píxeis do mundo */
    if (px < 0 || py < 0 || px >= MAP_W * TILE || py >= MAP_H * TILE) return A_SOLID;
    return attrRam[(py >> 4) * MAP_W + (px >> 4)];
}

/* ===== Paleta: base fixa + ciclos ===== */
static int cyWater, cyRune, fireA, fireB, lightning;
static const uint8_t WATER_SEQ[4] = {M0, M1, M2, M1};
static const uint8_t FIRE_N[4] = {S0, S1, E1, E2}, FIRE_B[4] = {S1, S2, E2, E3};
static void palette_write(void) {
    uint8_t idx[NPAL];
    for (int i = 0; i < NPAL; i++) idx[i] = i < NCOL ? (uint8_t)i : M0;
    for (int i = 0; i < 4; i++) idx[CY_WATER + i] = WATER_SEQ[(i + cyWater) & 3];
    idx[CY_RUNE] = (cyRune & 1) ? C1 : C0; idx[CY_RUNE + 1] = (cyRune % 3 == 0) ? BONE : C1;
    for (int i = 0; i < 4; i++) { idx[CY_FIRE_A + i] = fireA ? FIRE_B[i] : FIRE_N[i]; idx[CY_FIRE_B + i] = fireB ? FIRE_B[i] : FIRE_N[i]; }
    static const uint8_t LAKE_SEQ[4] = {C0, E0, E1, E0};
    for (int i = 0; i < 4; i++) idx[CY_LAKE + i] = LAKE_SEQ[(i + cyWater) & 3];
    idx[CY_MOON] = (cyRune & 2) ? E2 : E1; idx[CY_MOON + 1] = (cyRune & 2) ? E1 : E0;
    for (int i = 0; i < NPAL; i++) {
        uint8_t c = idx[i];
        if (lightning > 0 && (lightning & 3) != 2 && i) c = PAL_LIGHTER[PAL_LIGHTER[c]];
        while (!PEN_READY()) {}
        CRAM[i] = PAL_CRAM[c];
    }
}

/* ===== Desenho ===== */
static void draw_tiles(int cx, int cy) {   /* cx par */
    for (int y = 0; y < SCR_H; y++) {
        int wy = cy + y;
        const uint8_t *mrow = mapRam + (wy >> 4) * MAP_W;
        const uint16_t *trow = tilesRam + (wy & 15) * 8;
        volatile uint16_t *dst = FBW + y * LINEW;
        int mx = cx >> 4, tx = (cx & 15) >> 1, left = LINEW;
        while (left > 0) {
            const uint16_t *s = trow + mrow[mx] * 128 + tx;
            int n = 8 - tx; if (n > left) n = left;
            if (n == 8) { dst[0]=s[0]; dst[1]=s[1]; dst[2]=s[2]; dst[3]=s[3]; dst[4]=s[4]; dst[5]=s[5]; dst[6]=s[6]; dst[7]=s[7]; }
            else for (int i = 0; i < n; i++) dst[i] = s[i];
            dst += n; left -= n; mx++; tx = 0;
        }
    }
}
/* v = flash*4 + espelhado*2; o deslocamento de 1 px escolhe-se aqui */
static void spr(int id, int v, int sx, int sy) {
    const SprDesc *d = &SPR[id];
    int x0 = sx - d->ax, y0 = sy - d->ay;
    if (x0 >= SCR_W || y0 >= SCR_H || x0 + d->w <= 0 || y0 + d->h <= 0) return;
    int sh = x0 & 1; x0 &= ~1;
    const uint8_t *src = SPR_DATA + d->off + (uint32_t)((v & 6) + sh) * d->w * d->h;
    int w2 = d->w >> 1, i0 = 0, i1 = w2;
    if (x0 < 0) i0 = (-x0) >> 1;
    if (x0 + d->w > SCR_W) i1 = (SCR_W - x0) >> 1;
    for (int j = 0; j < d->h; j++) {
        int y = y0 + j; if (y < 0) continue; if (y >= SCR_H) break;
        const uint8_t *s = src + j * d->w;
        int base = y * LINEW + (x0 >> 1);
        for (int i = i0; i < i1; i++) { uint16_t px = RD16(s + i * 2); if (px) OW_PUT(base + i, px); }
    }
}
static inline void pset(int x, int y, uint8_t c) { if ((unsigned)x < SCR_W && (unsigned)y < SCR_H) FB8[y * SCR_W + x] = c; }
static void rect(int x, int y, int w, int h, uint8_t c) {
    for (int j = y; j < y + h; j++) for (int i = x; i < x + w; i++) pset(i, j, c);
}
static void panel(int x, int y, int w, int h) {
    rect(x, y, w, h, D1); rect(x, y, w, 1, INK); rect(x, y + h - 1, w, 1, INK); rect(x, y, 1, h, INK); rect(x + w - 1, y, 1, h, INK);
    rect(x + 1, y + 1, w - 2, 1, D2); rect(x + 2, y + h, w - 2, 2, INK); rect(x + w, y + 2, 2, h - 2, INK);
}
/* Texto: UTF-8 com as letras acentuadas do português */
static int next_glyph(const char **ps) {
    const uint8_t *s = (const uint8_t *)*ps; int c = *s;
    if (!c) return -1;
    if (c == 0xC3 && s[1]) { *ps += 2; switch (s[1]) {
        case 0x81: return 64; case 0x82: return 65; case 0x83: return 66; case 0x80: return 67;
        case 0x89: return 68; case 0x8A: return 69; case 0x8D: return 70; case 0x93: return 71;
        case 0x94: return 72; case 0x95: return 73; case 0x9A: return 74; case 0x87: return 75; default: return 0; } }
    *ps += 1;
    if (c >= 'a' && c <= 'z') c -= 32;
    return (c >= 32 && c < 96) ? c - 32 : 0;
}
static int text_w(const char *s, int sc) { int n = 0; while (next_glyph(&s) >= 0) n++; return n ? (n * 6 - 1) * sc : 0; }
static int hud_dx;   /* compensa o deslocamento SFT para a interface ficar parada */
static void text(const char *s, int x, int y, uint8_t c, int sc) {
    x += hud_dx; int g;
    while ((g = next_glyph(&s)) >= 0) {
        for (int pass = 0; pass < 2; pass++) {
            int o = pass ? 0 : sc; uint8_t col = pass ? c : INK;
            for (int r = 0; r < GLYPH_H; r++) { uint8_t m = FONT[g * GLYPH_H + r]; if (!m) continue;
                for (int b = 0; b < 5; b++) if (m & (16 >> b)) rect(x + b * sc + o, y + (r - 2) * sc + o, sc, sc, col); }
        }
        x += 6 * sc;
    }
}
static void text_c(const char *s, int y, uint8_t c, int sc) { text(s, (SCR_W - text_w(s, sc)) / 2, y, c, sc); }
static void num(char *b, int v, int digits) { for (int i = digits - 1; i >= 0; i--) { b[i] = (char)('0' + v % 10); v /= 10; } b[digits] = 0; }
static void numstr(char *b, int v) { char t[12]; int n = 0; do { t[n++] = (char)('0' + v % 10); v /= 10; } while (v); for (int i = 0; i < n; i++) b[i] = t[n - 1 - i]; b[n] = 0; }

/* Vinheta pontilhada nos cantos (lista de píxeis calculada uma vez) */
#define MAXVIG 9000
static uint32_t vig[MAXVIG]; static int nVig;
static void build_vignette(void) {
    for (int y = 0; y < SCR_H; y++) for (int x = 0; x < SCR_W - 1; x++) {
        int vx = x - 160, vy = y - 112, vd = vx * vx / 100 + vy * vy / 50;
        if (vd > 230 && BAYER[(y & 3) * 4 + (x & 3)] < (vd - 230) / 14 && nVig < MAXVIG) vig[nVig++] = (uint32_t)(y * SCR_W + x);
    }
}

/* ===== Jogo ===== */
enum { ST_TITLE, ST_PLAY, ST_LEVELUP, ST_PAUSE, ST_OVER, ST_WIN, ST_INTRO, ST_TRANSIT, ST_CHARSEL, ST_OPTIONS, ST_CREDITS, ST_BYE };
#define MENU_STATE(s) ((s) == ST_TITLE || (s) == ST_CHARSEL || (s) == ST_OPTIONS || (s) == ST_CREDITS || (s) == ST_BYE)
static int state;
#define FP 8   /* posições em 1/256 de píxel */

/* Personagens: o ninja usa Lâmina, Brasas e Faúlha; a feiticeira usa Bola de Fogo, Mísseis Arcanos,
   Buraco Negro e Relâmpago em Cadeia. As melhorias passivas são comuns.
   As magias da feiticeira têm 5 níveis; o 6.º é a evolução. */
enum { HERO_NINJA, HERO_MAGO };
static int heroi = HERO_NINJA;
enum { U_LAMINA, U_BRASAS, U_FAULHA, U_FOGO, U_MISSEIS, U_BURACO, U_RAIO, U_CORACAO, U_BOTAS, U_IMAN, U_FURIA, NUP };
static const char *UP_NAME[NUP] = {"LÂMINA", "BRASAS ORBITAIS", "FAÚLHA", "BOLA DE FOGO", "MÍSSEIS ARCANOS", "BURACO NEGRO", "RELÂMPAGO EM CADEIA",
                                   "CORAÇÃO DE PEDRA", "BOTAS DE VENTO", "ÍMAN DE BRASA", "FÚRIA"};
static const char *UP_NEW[NUP]  = {"CORTE AUTOMÁTICO À FRENTE", "BRASAS RODAM À TUA VOLTA", "DISPARA CONTRA O MAIS PERTO",
                                   "BOLAS QUE EXPLODEM EM ÁREA", "PERSEGUEM OS INIMIGOS", "PUXA E FERE OS INIMIGOS", "SALTA DE INIMIGO EM INIMIGO", "", "", "", ""};
static const char *UP_DESC[NUP] = {"MAIS DANO E MAIS ALCANCE", "MAIS UMA BRASA A RODAR", "MAIS UM PROJÉTIL", "", "", "", "",
                                   "+20 DE VIDA MÁXIMA E CURA", "+10% DE VELOCIDADE", "RECOLHE CRISTAIS DE MAIS LONGE", "-8% DE TEMPO DE RECARGA"};
static const uint8_t UP_MAX[NUP] = {6, 5, 5, 6, 6, 6, 6, 5, 5, 4, 4};
#define IS_MAGIA(u) ((u) >= U_FOGO && (u) <= U_RAIO)
static const char *MAGIA_NV[4][4] = {   /* descrição do nível seguinte (2 a 5) */
    {"EXPLOSÃO MAIOR", "+1 BOLA DE FOGO", "QUEIMA OS INIMIGOS", "+DANO E +1 BOLA DE FOGO"},
    {"+1 MÍSSIL", "MÍSSEIS MAIS RÁPIDOS", "RICOCHETEIAM 1 VEZ", "+2 MÍSSEIS E +1 RICOCHETE"},
    {"RAIO MAIOR", "ATRAI COM MAIS FORÇA", "DURA MAIS TEMPO", "+RAIO E +DANO"},
    {"+1 SALTO", "MAIS ALCANCE", "+1 SALTO E CRÍTICOS", "+2 SALTOS"}};
static const char *EVO_NAME[4] = {"CHUVA DE METEOROS", "BARRAGEM ARCANA", "SINGULARIDADE", "DEUS DO TROVÃO"};
static const char *EVO_DESC[4] = {"METEOROS CAEM PELO MAPA", "DEZENAS DE MÍSSEIS SEGUIDOS", "ABSORVE E EXPLODE NO FIM", "RAIOS CAEM À TUA VOLTA"};
static int upgrade_do_heroi(int u) {   /* esta melhoria está disponível para o personagem escolhido? */
    if (u <= U_FAULHA) return heroi == HERO_NINJA;
    if (IS_MAGIA(u)) return heroi == HERO_MAGO;
    return 1;
}
static uint8_t lvl[NUP];

static struct { int32_t x, y; int dir, flip, fx, fy, anim, moving, atkT, hp, maxhp, inv, flash; } H;
static int level, xp, xpNext, kills, steps, shake, cdLamina, cdFaulha, orbAng, choice[3], nChoice, sel;
static int camX, camY, spawnT, hordeT, lightT, swingSide;
static int menuSel, autoEspada = 1, swordQueued, charSel, optSel, byeT, skyT;
static int cdFogo, cdMis, cdBuraco, cdRaio, meteorT, barrageT, barrageLeft, thunderT;   /* magias da feiticeira */   /* menu do título; espada automática (1) ou manual (0) */
static uint16_t padNow, padPressed;   /* comando: botões carregados e acabados de carregar */

#define MAXE 96
typedef struct { int32_t x, y, kx, ky; int16_t hp; uint8_t type, on, flash, cdOrb, face, anim, burn, burnT; int16_t next; } Enemy;
static Enemy en[MAXE];
static const int E_SPD[3] = {220, 110, 150}, E_HP[3] = {8, 32, 1800}, E_DMG[3] = {4, 10, 20}, E_R[3] = {6, 10, 22};
/* Boss do primeiro mapa: Gorgomusgo. Aparece no nível 15; ao morrer abre um portal no centro do mapa. */
#define BOSS_NIVEL 15
static const char *BOSS_NOME = "GORGOMUSGO", *BOSS_SUB = "O TRONCO QUE DEVORA RUNAS";
static int bossIdx = -1, bossState, bossT, bossCycles, bossVx, bossVy, bossDone, bossMaxHp;
static int portalOn, portalX, portalY, introT, transT, mapNameT, bannerT, mapSteps;
static const char *bannerTxt = "";
#define MAXG 160
typedef struct { int16_t x, y; uint8_t on, tier, pull; } Gem;
static Gem gems[MAXG]; static int gemNext;
#define MAXB 48
/* Projéteis: kind 0 faúlha, 1 bola de fogo (bounce = raio da explosão), 2 míssil arcano (bounce = ricochetes, tgt = alvo) */
typedef struct { int32_t x, y, vx, vy; int16_t life; uint8_t on, dmg, kind, bounce; int16_t tgt; } Bolt;
#define MAXMET 12
typedef struct { int16_t x, y, h; uint8_t on; } Meteor;
static Meteor met[MAXMET];
static struct { int32_t x, y; int16_t t, life, R, pull; uint8_t on; } bh;   /* buraco negro */
#define MAXRAIO 8
typedef struct { int16_t px[10], py[10]; uint8_t n, t; } Raio;   /* relâmpago desenhado em linhas durante alguns passos */
static Raio raios[MAXRAIO];
static Bolt bolts[MAXB];
#define MAXFX 40
typedef struct { int16_t x, y; uint8_t on, base, n, rate, t, v; } Fx;
static Fx fx[MAXFX];
#define MAXPT 120
typedef struct { int32_t x, y, vx, vy; int16_t life; uint8_t on, kind, c; } Part;
static Part pt[MAXPT];

static void add_fx(int base, int n, int rate, int x, int y, int v) {
    for (int i = 0; i < MAXFX; i++) if (!fx[i].on) { fx[i] = (Fx){(int16_t)x, (int16_t)y, 1, (uint8_t)base, (uint8_t)n, (uint8_t)rate, 0, (uint8_t)v}; return; }
}
static void add_part(int kind, int32_t x, int32_t y, int32_t vx, int32_t vy, int life, int c) {
    for (int i = 0; i < MAXPT; i++) if (!pt[i].on) { pt[i] = (Part){x, y, vx, vy, (int16_t)life, 1, (uint8_t)kind, (uint8_t)c}; return; }
}
static int hero_speed(void) { return 320 * (10 + lvl[U_BOTAS]) / 10; }
static int cd_factor(void) { return 100 - 8 * lvl[U_FURIA]; }
static int magnet(void) { return 36 + 16 * lvl[U_IMAN]; }

static void new_game(void) {
    for (int i = 0; i < MAXE; i++) en[i].on = 0;
    for (int i = 0; i < MAXG; i++) gems[i].on = 0;
    for (int i = 0; i < MAXB; i++) bolts[i].on = 0;
    for (int i = 0; i < MAXFX; i++) fx[i].on = 0;
    for (int i = 0; i < NUP; i++) lvl[i] = 0;
    if (heroi == HERO_NINJA) lvl[U_LAMINA] = 1; else lvl[U_FOGO] = 1;
    cdFogo = 30; cdMis = 50; cdBuraco = 120; cdRaio = 60; meteorT = 150; barrageT = 240; barrageLeft = 0; thunderT = 120;
    for (int i = 0; i < MAXMET; i++) met[i].on = 0;
    for (int i = 0; i < MAXRAIO; i++) raios[i].t = 0;
    bh.on = 0;
    H.x = (48 * 16 + 8) << FP; H.y = (50 * 16 + 8) << FP; H.dir = DOWN; H.flip = 0; H.fx = 0; H.fy = 1;
    H.maxhp = heroi == HERO_MAGO ? 85 : 100; H.hp = H.maxhp; H.inv = 0; H.flash = 0; H.atkT = 0;
    level = 1; xp = 0; xpNext = 5; swingSide = 0; swordQueued = 0; kills = 0; steps = 0; shake = 0; cdLamina = 30; cdFaulha = 60; orbAng = 0;
    spawnT = 60; hordeT = 60 * 60; lightT = 60 * 50;
    load_map(0);
    H.x = MAPAS[0].sx << FP; H.y = MAPAS[0].sy << FP;
    bossIdx = -1; bossDone = 0; bossCycles = 0; portalOn = 0; mapNameT = 240; bannerT = 0; mapSteps = 0;
}

/* ----- Inimigos ----- */
static int free_enemy(void) {
    for (int i = 0; i < MAXE; i++) if (!en[i].on) return i;
    int far = 0, fd = -1;   /* sem espaço: recicla o mais afastado */
    for (int i = 0; i < MAXE; i++) { if (en[i].type == 2) continue;   /* o boss nunca é reciclado */
        int d = dist_approx((en[i].x - H.x) >> FP, (en[i].y - H.y) >> FP); if (d > fd) { fd = d; far = i; } }
    return far;
}
static void spawn_at(int type, int px, int py) {
    if (attr_at(px, py) & A_SOLID) return;
    int i = free_enemy(); Enemy *e = &en[i];
    int t = steps / 60;
    e->x = px << FP; e->y = py << FP; e->kx = e->ky = 0; e->type = (uint8_t)type; e->on = 1; e->flash = 0; e->cdOrb = 0; e->anim = (uint8_t)rndi(16); e->burn = 0; e->burnT = 0;
    e->hp = (int16_t)(type == 2 ? E_HP[2] : E_HP[type] * (100 + t / 2) / 100);
}
static void spawn_ring(int type, int rmin, int rmax) {
    for (int tries = 0; tries < 6; tries++) {
        int a = rndi(256), r = rmin + rndi(rmax - rmin + 1);
        int px = (H.x >> FP) + (SINT[(a + 64) & 255] * r >> 8), py = (H.y >> FP) + (SINT[a] * r >> 8);
        if (!(attr_at(px, py) & A_SOLID)) { spawn_at(type, px, py); return; }
    }
}
static void kill_enemy(Enemy *e) {
    int ex = e->x >> FP, ey = e->y >> FP;
    e->on = 0; kills++;
    if (e->type == 2) {   /* boss derrotado: chuva de cristais e portal no centro do mapa */
        bossIdx = -1; bossDone = 1; portalOn = 1;
        portalX = MAP_W * TILE / 2; portalY = MAP_H * TILE / 2 + 8;
        for (int k = 0; k < 5; k++) add_fx(SPR_PUFF, 4, 6, ex - 20 + k * 10, ey - (k & 1) * 16, 0);
        for (int k = 0; k < 16; k++) { Gem *g = &gems[gemNext]; gemNext = (gemNext + 1) % MAXG;
            g->on = 1; g->tier = 2; g->pull = 0; g->x = (int16_t)(ex + (SINT[(k * 16 + 64) & 255] * 40 >> 8)); g->y = (int16_t)(ey - 10 + (SINT[k * 16] * 28 >> 8)); }
        shake = 12; lightning = 9; tocar_sfx(SFX_ID_POWERUP);
        bannerTxt = "O PORTAL ABRIU-SE NO CENTRO DO MAPA"; bannerT = 300;
        return;
    }
    add_fx(SPR_PUFF, 4, 6, ex, ey + 2, 0);
    Gem *g = &gems[gemNext]; gemNext = (gemNext + 1) % MAXG;
    g->on = 1; g->x = (int16_t)ex; g->y = (int16_t)(ey - 2); g->pull = 0;
    g->tier = (uint8_t)(e->type == 1 ? (rndi(100) < 15 ? 2 : 1) : (rndi(100) < 4 ? 1 : 0));
}
static void hurt_enemy(Enemy *e, int dmg, int kx, int ky) {
    e->hp = (int16_t)(e->hp - dmg); e->flash = e->type == 2 ? 2 : 6;   /* boss: flash curto para não piscar sem parar */
    int k = e->type == 2 ? 0 : e->type ? 1 : 3; e->kx = kx * k; e->ky = ky * k;
    add_fx(SPR_SPARK, 3, 4, e->x >> FP, (e->y >> FP) - (e->type == 2 ? 28 : e->type ? 12 : 6), 0);
    if (e->hp <= 0) kill_enemy(e);
}
/* Boss: persegue, avisa (olhos vermelhos), investe, recupera; a cada duas investidas chama rastejantes.
   É um tronco gigante: passa por cima de muros e água, mas fica sempre dentro do mapa. */
static void boss_clamp(Enemy *e) {
    if (e->x < (4 * TILE) << FP) e->x = (4 * TILE) << FP;
    if (e->x > ((MAP_W - 4) * TILE) << FP) e->x = ((MAP_W - 4) * TILE) << FP;
    if (e->y < (6 * TILE) << FP) e->y = (6 * TILE) << FP;
    if (e->y > ((MAP_H - 4) * TILE) << FP) e->y = ((MAP_H - 4) * TILE) << FP;
}
static void boss_move(Enemy *e, int dx, int dy, int d) {
    bossT++;
    switch (bossState) {
    case 0: {
        e->x += dx * E_SPD[2] / d; e->y += dy * E_SPD[2] / d;
        e->face = (uint8_t)(dx < 0);
        if (bossT > 200) { bossState = 1; bossT = 0; shake = 3; }
        break; }
    case 1:
        if (bossT > 50) { bossState = 2; bossT = 0; bossVx = dx * 700 / d; bossVy = dy * 700 / d; e->face = (uint8_t)(dx < 0); }
        break;
    case 2: {
        e->x += bossVx; e->y += bossVy;
        if (bossT > 36) { bossState = 3; bossT = 0; shake = 6; }
        break; }
    case 3:
        if (bossT > 40) { bossState = 0; bossT = 0;
            if (++bossCycles % 2 == 0) for (int k = 0; k < 6; k++) { int a = k * 43;
                spawn_at(0, (e->x >> FP) + (SINT[(a + 64) & 255] * 50 >> 8), (e->y >> FP) + (SINT[a] * 36 >> 8)); } }
        break;
    }
    boss_clamp(e);
}
static void start_boss(void) {
    static const int off[4][2] = {{0, -100}, {0, 100}, {-110, 0}, {110, 0}};
    int hx = H.x >> FP, hy = H.y >> FP, k = 0;
    while (k < 3 && (attr_at(hx + off[k][0], hy + off[k][1]) & A_SOLID)) k++;
    int i = free_enemy(); Enemy *e = &en[i];
    e->x = (hx + off[k][0]) << FP; e->y = (hy + off[k][1]) << FP; e->kx = e->ky = 0;
    e->type = 2; e->on = 1; e->flash = 0; e->cdOrb = 0; e->anim = 0; e->face = 0; e->burn = 0; e->burnT = 0;
    e->hp = (int16_t)E_HP[2]; bossMaxHp = E_HP[2]; boss_clamp(e);
    bossIdx = i; bossState = 0; bossT = 0;
    state = ST_INTRO; introT = 0; lightning = 9; shake = 8;
    tocar_sfx(SFX_ID_MONSTER);
}
#define GC 32
#define GW (MAP_W * TILE / GC)
static int16_t ghead[GW * GW];
static void update_enemies(void) {
    int hx = H.x >> FP, hy = H.y >> FP;
    for (int i = 0; i < GW * GW; i++) ghead[i] = -1;
    for (int i = 0; i < MAXE; i++) {
        Enemy *e = &en[i]; if (!e->on) continue;
        int ex = e->x >> FP, ey = e->y >> FP, dx = hx - ex, dy = hy - ey, d = dist_approx(dx, dy);
        if (d < 1) d = 1;
        if (e->type == 2) { boss_move(e, dx, dy, d); e->anim++; }
        else {
            if (d > 420) { e->on = 0; spawn_ring(e->type, 200, 230); continue; }   /* reaparece perto */
            int vx = dx * E_SPD[e->type] / d + e->kx, vy = dy * E_SPD[e->type] / d + e->ky;
            e->kx = e->kx * 3 / 4; e->ky = e->ky * 3 / 4;
            int32_t nx = e->x + vx, ny = e->y + vy;
            if (!(attr_at(nx >> FP, ey) & A_SOLID)) e->x = nx;
            if (!(attr_at(e->x >> FP, ny >> FP) & A_SOLID)) e->y = ny;
            e->face = (uint8_t)(dx < 0); e->anim++;
            if (bh.on) {   /* buraco negro: puxa para o centro */
                int bx = (bh.x >> FP) - (e->x >> FP), by = (bh.y >> FP) - (e->y >> FP), bd = dist_approx(bx, by);
                if (bd < bh.R && bd > 2) { e->x += bx * bh.pull / bd; e->y += by * bh.pull / bd; }
            }
        }
        if (e->burn && ++e->burnT >= 20) {   /* queimadura: dano a cada 20 passos */
            e->burnT = 0; e->burn--; e->hp = (int16_t)(e->hp - (3 + lvl[U_FOGO])); e->flash = 2;
            add_part(1, e->x, e->y - (8 << FP), rndi(64) - 32, -90, 20, rndi(2) ? E3 : E2);
            if (e->hp <= 0) { kill_enemy(e); continue; }
        }
        if (e->flash) e->flash--;
        if (e->cdOrb) e->cdOrb--;
        int r = E_R[e->type] + 6;
        if (iabs(dx) < r && iabs(dy) < r && H.inv == 0) {
            H.hp -= E_DMG[e->type]; H.inv = 45; H.flash = 8; shake = 6; tocar_sfx(SFX_ID_HURT);
            if (H.hp <= 0) { H.hp = 0; state = ST_OVER; }
        }
        int c = ((e->y >> FP) / GC) * GW + (e->x >> FP) / GC;
        if (c >= 0 && c < GW * GW) { e->next = ghead[c]; ghead[c] = (int16_t)i; }
    }
    if (steps & 1) return;   /* separação a 30 Hz chega */
    for (int i = 0; i < MAXE; i++) {
        Enemy *a = &en[i]; if (!a->on || a->type == 2) continue;
        int ax = a->x >> FP, ay = a->y >> FP, cx = ax / GC, cy = ay / GC;
        for (int gy = cy - 1; gy <= cy + 1; gy++) for (int gx = cx - 1; gx <= cx + 1; gx++) {
            if (gx < 0 || gy < 0 || gx >= GW || gy >= GW) continue;
            for (int j = ghead[gy * GW + gx]; j >= 0; j = en[j].next) {
                if (j <= i) continue;
                Enemy *b = &en[j]; if (!b->on || b->type == 2) continue;
                int dx = (b->x >> FP) - ax, dy = (b->y >> FP) - ay, rs = E_R[a->type] + E_R[b->type];
                if (iabs(dx) >= rs || iabs(dy) >= rs) continue;
                int push = (rs - (iabs(dx) > iabs(dy) ? iabs(dx) : iabs(dy))) << (FP - 1);
                int sx = dx >= 0 ? 1 : -1, sy = dy >= 0 ? 1 : -1;
                if (iabs(dx) > iabs(dy)) { a->x -= sx * push; b->x += sx * push; } else { a->y -= sy * push; b->y += sy * push; }
            }
        }
    }
}

/* ----- Armas ----- */
static void lamina_swing(int fx, int fy, int R, int dmg) {   /* corte em arco de ~120° na direção (fx,fy) */
    int hx = H.x >> FP, hy = H.y >> FP;
    int rot = (!fx && fy < 0) ? 1 : (!fx && fy > 0) ? 2 : 0;
    add_fx(SPR_SMEAR + rot * 3, 3, 3, hx + fx * 14, hy + fy * 12 - 12, (rot == 0 && fx < 0) ? 2 : 0);
    for (int i = 0; i < MAXE; i++) { Enemy *e = &en[i]; if (!e->on) continue;
        int dx = (e->x >> FP) - hx, dy = (e->y >> FP) - (hy - 8), d = dist_approx(dx, dy);
        if (d > R) continue;
        int dot = dx * fx + dy * fy, fl = (fx && fy) ? 362 : 256;
        if (d > 10 && dot * 512 < d * fl) continue;
        hurt_enemy(e, dmg, fx * 512, fy * 512);
    }
}
static void weapons(void) {
    int hx = H.x >> FP, hy = H.y >> FP, cf = cd_factor();
    /* Lâmina: corte em arco à frente */
    if (!autoEspada && (padPressed & BTN_OK)) swordQueued = 1;   /* manual: cada clique vale um golpe */
    if (cdLamina > 0) cdLamina--;
    if (cdLamina <= 0 && (autoEspada || swordQueued)) {
        int L = lvl[U_LAMINA], R = 38 + 4 * L, dmg = 10 + 4 * (L - 1);
        swordQueued = 0;
        cdLamina = (64 - 5 * (L - 1)) * cf / 100; H.atkT = 10;
        if (!autoEspada) cdLamina = cdLamina * 3 / 5;   /* em manual a recarga é mais curta */
        tocar_sfx(SFX_ID_ATTACK);
        swingSide ^= 1;
        if (L >= 4) { lamina_swing(H.fx, H.fy, R, dmg); lamina_swing(-H.fx, -H.fy, R, dmg); }
        else if (!autoEspada || swingSide) lamina_swing(H.fx, H.fy, R, dmg);   /* manual: sempre para a frente */
        else lamina_swing(-H.fx, -H.fy, R, dmg);
        shake = shake < 2 ? 2 : shake;
    }
    /* Brasas orbitais */
    if (lvl[U_BRASAS]) {
        int n = lvl[U_BRASAS], R = 34 + 2 * n, dmg = 5 + 2 * n;
        orbAng = (orbAng + 5) & 255;
        for (int k = 0; k < n; k++) {
            int a = (orbAng + k * 256 / n) & 255;
            int ox = hx + (SINT[(a + 64) & 255] * R >> 8), oy = hy - 10 + (SINT[a] * R * 3 >> 10);
            for (int i = 0; i < MAXE; i++) { Enemy *e = &en[i]; if (!e->on || e->cdOrb) continue;
                int dx = (e->x >> FP) - ox, dy = (e->y >> FP) - E_R[e->type] - oy;
                if (iabs(dx) < 9 + E_R[e->type] && iabs(dy) < 9 + E_R[e->type]) { e->cdOrb = 20; hurt_enemy(e, dmg, dx > 0 ? 256 : -256, dy > 0 ? 256 : -256); }
            }
        }
    }
    /* Faúlha: projéteis para os inimigos mais perto */
    if (lvl[U_FAULHA] && --cdFaulha <= 0) {
        int L = lvl[U_FAULHA]; cdFaulha = (90 - 8 * L) * cf / 100;
        for (int k = 0; k < L; k++) {
            int best = -1, bd = 220;
            for (int i = 0; i < MAXE; i++) { Enemy *e = &en[i]; if (!e->on) continue;
                int d = dist_approx((e->x >> FP) - hx, (e->y >> FP) - hy) + k * 12 * (i & 1);
                if (d < bd) { bd = d; best = i; } }
            if (best < 0) break;
            for (int b = 0; b < MAXB; b++) if (!bolts[b].on) {
                int dx = (en[best].x >> FP) - hx, dy = (en[best].y >> FP) - 6 - (hy - 12), d = dist_approx(dx, dy); if (d < 1) d = 1;
                bolts[b] = (Bolt){H.x, H.y - (12 << FP), dx * 900 / d, dy * 900 / d, 90, 1, (uint8_t)(7 + 3 * L), 0, 0, -1};
                break; }
        }
    }
}

/* ----- Magias da feiticeira ----- */
static int nearest_enemy(int x, int y, int maxd, int excl1, int excl2) {
    int best = -1, bd = maxd;
    for (int i = 0; i < MAXE; i++) { Enemy *e = &en[i]; if (!e->on || i == excl1 || i == excl2) continue;
        int d = dist_approx((e->x >> FP) - x, (e->y >> FP) - E_R[e->type] - y); if (d < bd) { bd = d; best = i; } }
    return best;
}
static void explode(int x, int y, int R, int dmg, int burn) {   /* explosão em área (bola de fogo, meteoro, singularidade) */
    add_fx(SPR_EXPLO, 4, 4, x, y, 0);
    for (int i = 0; i < MAXE; i++) { Enemy *e = &en[i]; if (!e->on) continue;
        int dx = (e->x >> FP) - x, dy = (e->y >> FP) - E_R[e->type] - y;
        if (dist_approx(dx, dy) > R + E_R[e->type]) continue;
        if (burn && e->type != 2) e->burn = (uint8_t)burn;
        hurt_enemy(e, dmg, dx > 0 ? 256 : -256, dy > 0 ? 256 : -256);
    }
    if (shake < 2) shake = 2;
}
static Raio *novo_raio(void) { for (int i = 0; i < MAXRAIO; i++) if (!raios[i].t) { raios[i].n = 0; raios[i].t = 8; return &raios[i]; } return 0; }
static void fire_bolt(int kind, int32_t x, int32_t y, int tx, int ty, int speed, int dmg, int bounce, int tgt, int life) {
    for (int b = 0; b < MAXB; b++) if (!bolts[b].on) {
        int dx = tx - (x >> FP), dy = ty - (y >> FP), d = dist_approx(dx, dy); if (d < 1) d = 1;
        bolts[b] = (Bolt){x, y, dx * speed / d, dy * speed / d, (int16_t)life, 1, (uint8_t)dmg, (uint8_t)kind, (uint8_t)bounce, (int16_t)tgt};
        return; }
}
static void magias(void) {
    int hx = H.x >> FP, hy = H.y >> FP, cf = cd_factor();
    int32_t mx = H.x, my = H.y - (16 << FP);   /* mãos da feiticeira */
    /* Bola de Fogo: explode em área; nível 4+ queima; evolução: Chuva de Meteoros */
    if (lvl[U_FOGO] && --cdFogo <= 0) {
        int L = lvl[U_FOGO], n = 1 + (L >= 3) + (L >= 5), R = 22 + (L >= 2 ? 10 : 0) + (L >= 5 ? 6 : 0), dmg = 12 + (L >= 5 ? 8 : 0);
        cdFogo = 70 * cf / 100;
        int last = -1;
        for (int k = 0; k < n; k++) {
            int t = nearest_enemy(hx, hy - 12, 200, last, -1);
            int tx = t >= 0 ? en[t].x >> FP : hx + H.fx * 80 + (k - 1) * 20, ty = t >= 0 ? (en[t].y >> FP) - 6 : hy - 12 + H.fy * 80 + (k - 1) * 20;
            fire_bolt(1, mx, my, tx, ty, 768, dmg, R, -1, 75); last = t;
        }
        H.atkT = 10;
    }
    if (lvl[U_FOGO] >= 6 && --meteorT <= 0) {   /* meteoros caem à volta do herói */
        meteorT = 150 * cf / 100;
        for (int k = 0; k < 6; k++) for (int i = 0; i < MAXMET; i++) if (!met[i].on) {
            met[i].on = 1; met[i].x = (int16_t)(hx - 140 + rndi(281)); met[i].y = (int16_t)(hy - 90 + rndi(181)); met[i].h = (int16_t)(120 + k * 18);
            break; }
    }
    for (int i = 0; i < MAXMET; i++) if (met[i].on && (met[i].h -= 4) <= 0) {
        met[i].on = 0; explode(met[i].x, met[i].y, 34, 30, 4); shake = 5; }
    /* Mísseis Arcanos: perseguem; nível 4+ ricocheteiam; evolução: Barragem Arcana */
    if (lvl[U_MISSEIS] && --cdMis <= 0) {
        int L = lvl[U_MISSEIS], n = 2 + (L >= 2) + (L >= 5 ? 2 : 0), sp = L >= 3 ? 1000 : 700, rb = (L >= 4) + (L >= 5);
        cdMis = 80 * cf / 100;
        for (int k = 0; k < n; k++) {
            int a = (k * 256 / n + steps) & 255;
            int t = nearest_enemy(hx, hy - 12, 260, -1, -1);
            fire_bolt(2, mx, my, hx + (SINT[(a + 64) & 255] >> 3), hy - 16 + (SINT[a] >> 3), sp, 8 + 2 * L, rb, t, 120);
        }
        H.atkT = 10;
    }
    if (lvl[U_MISSEIS] >= 6) {
        if (--barrageT <= 0) { barrageT = 240 * cf / 100; barrageLeft = 24; }
        if (barrageLeft && (steps % 3) == 0) { barrageLeft--;
            int a = (barrageLeft * 37) & 255, t = nearest_enemy(hx, hy - 12, 300, -1, -1);
            fire_bolt(2, mx, my, hx + (SINT[(a + 64) & 255] >> 3), hy - 16 + (SINT[a] >> 3), 1000, 14, 2, t, 120); }
    }
    /* Buraco Negro: atrai e fere; evolução: Singularidade (explode no fim) */
    if (lvl[U_BURACO]) {
        int L = lvl[U_BURACO];
        if (!bh.on && --cdBuraco <= 0) {
            int t = nearest_enemy(hx, hy - 12, 180, -1, -1);
            if (t < 0) cdBuraco = 15;   /* sem inimigos por perto: espera por um alvo */
            else {
            bh.on = 1; bh.t = 0;
            bh.x = en[t].x; bh.y = en[t].y;
            bh.R = (int16_t)(40 + (L >= 2 ? 12 : 0) + (L >= 5 ? 10 : 0) + (L >= 6 ? 14 : 0));
            bh.pull = (int16_t)(L >= 6 ? 300 : L >= 3 ? 220 : 130);
            bh.life = (int16_t)(150 + (L >= 4 ? 90 : 0));
            H.atkT = 10;
            }
        }
        if (bh.on) {
            bh.t++;
            if (bh.t % 12 == 0) for (int i = 0; i < MAXE; i++) { Enemy *e = &en[i]; if (!e->on) continue;
                if (dist_approx((e->x >> FP) - (bh.x >> FP), (e->y >> FP) - (bh.y >> FP)) < bh.R) hurt_enemy(e, 4 + (L >= 5 ? 4 : 0), 0, 0); }
            if (rndi(2) == 0) { int a = rndi(256);
                add_part(1, bh.x + (SINT[(a + 64) & 255] * bh.R / 6), bh.y + (SINT[a] * bh.R / 8), -(SINT[(a + 64) & 255] >> 1), -(SINT[a] >> 1), 20, rndi(2) ? C1 : M3); }
            if (bh.t >= bh.life) {
                bh.on = 0; cdBuraco = 300 * cf / 100;
                if (L >= 6) { explode(bh.x >> FP, bh.y >> FP, bh.R * 3 / 2, 60, 0); shake = 10; lightning = 3; }
            }
        }
    }
    /* Relâmpago em Cadeia: salta entre inimigos; nível 4+ críticos; evolução: Deus do Trovão */
    if (lvl[U_RAIO] && --cdRaio <= 0) {
        int L = lvl[U_RAIO], jumps = 2 + (L >= 2) + (L >= 4) + (L >= 5 ? 2 : 0), jr = 60 + (L >= 3 ? 30 : 0), crit = L >= 4 ? 25 : 10;
        cdRaio = 90 * cf / 100;
        int t = nearest_enemy(hx, hy - 12, 150, -1, -1);
        if (t >= 0) {
            Raio *r = novo_raio(); int prev = -1, cx = hx, cy = hy - 18;
            if (r) { r->px[0] = (int16_t)cx; r->py[0] = (int16_t)cy; r->n = 1; }
            for (int j = 0; j <= jumps && t >= 0; j++) {
                Enemy *e = &en[t]; int ex = e->x >> FP, ey = (e->y >> FP) - E_R[e->type];
                int dmg = 14 + 3 * L; if (rndi(100) < crit) { dmg = dmg * 5 / 2; add_fx(SPR_SPARK, 3, 3, ex, ey - 6, 0); }
                if (r && r->n < 10) { r->px[r->n] = (int16_t)ex; r->py[r->n] = (int16_t)ey; r->n++; }
                int keep = t; hurt_enemy(e, dmg, 0, 0);
                int nt = nearest_enemy(ex, ey, jr, keep, prev); prev = keep; t = nt;
            }
            H.atkT = 10;
        }
    }
    if (lvl[U_RAIO] >= 6 && --thunderT <= 0) {   /* raios caem do céu à volta do herói */
        thunderT = 120 * cf / 100;
        for (int k = 0; k < 5; k++) {
            int t = -1, tries = 0;
            while (tries++ < 12) { int i = rndi(MAXE); if (en[i].on && dist_approx((en[i].x >> FP) - hx, (en[i].y >> FP) - hy) < 160) { t = i; break; } }
            if (t < 0) continue;
            int ex = en[t].x >> FP, ey = (en[t].y >> FP) - E_R[en[t].type];
            Raio *r = novo_raio(); if (r) { r->px[0] = (int16_t)ex; r->py[0] = (int16_t)(ey - 150); r->px[1] = (int16_t)ex; r->py[1] = (int16_t)ey; r->n = 2; }
            hurt_enemy(&en[t], 30, 0, 0); add_fx(SPR_SPARK, 3, 3, ex, ey, 0);
        }
        shake = 3;
    }
    for (int i = 0; i < MAXRAIO; i++) if (raios[i].t) raios[i].t--;
}
static void update_bolts(void) {
    for (int b = 0; b < MAXB; b++) { Bolt *o = &bolts[b]; if (!o->on) continue;
        if (o->kind == 2) {   /* míssil arcano: persegue o alvo, procura outro se este morrer */
            if (o->tgt < 0 || !en[o->tgt].on) o->tgt = (int16_t)nearest_enemy(o->x >> FP, o->y >> FP, 220, -1, -1);
            if (o->tgt >= 0) { Enemy *e = &en[o->tgt];
                int dx = (e->x >> FP) - (o->x >> FP), dy = (e->y >> FP) - E_R[e->type] - (o->y >> FP), d = dist_approx(dx, dy); if (d < 1) d = 1;
                int sp = dist_approx(o->vx, o->vy); if (sp < 600) sp = 600;
                o->vx += (dx * sp / d - o->vx) / 4; o->vy += (dy * sp / d - o->vy) / 4; }
        }
        o->x += o->vx; o->y += o->vy;
        int solid = (attr_at(o->x >> FP, (o->y >> FP) + 12) & A_SOLID) && !(attr_at(o->x >> FP, (o->y >> FP) + 12) & A_WATER);
        if (--o->life <= 0 || (o->kind != 2 && solid)) {
            if (o->kind == 1) explode(o->x >> FP, o->y >> FP, o->bounce, o->dmg, lvl[U_FOGO] >= 4 ? 4 : 0);
            o->on = 0; continue;
        }
        if (o->life % 3 == 0) add_part(1, o->x, o->y, 0, -40, 12, o->kind == 2 ? C1 : E2);
        for (int i = 0; i < MAXE; i++) { Enemy *e = &en[i]; if (!e->on) continue;
            int dx = (e->x >> FP) - (o->x >> FP), dy = (e->y >> FP) - E_R[e->type] - (o->y >> FP);
            if (iabs(dx) >= E_R[e->type] + 4 || iabs(dy) >= E_R[e->type] + 4) continue;
            if (o->kind == 1) { explode(o->x >> FP, o->y >> FP, o->bounce, o->dmg, lvl[U_FOGO] >= 4 ? 4 : 0); o->on = 0; }
            else if (o->kind == 2) {
                hurt_enemy(e, o->dmg, 0, 0);
                if (o->bounce) { o->bounce--; o->tgt = (int16_t)nearest_enemy(o->x >> FP, o->y >> FP, 140, i, -1); o->life = 60; if (o->tgt < 0) o->on = 0; }
                else o->on = 0;
            }
            else { hurt_enemy(e, o->dmg, o->vx > 0 ? 384 : -384, 0); o->on = 0; }
            break;
        }
    }
}

/* ----- Cristais e níveis ----- */
static void open_levelup(void) {
    int pool[NUP], n = 0;
    for (int i = 0; i < NUP; i++) if (lvl[i] < UP_MAX[i] && upgrade_do_heroi(i)) pool[n++] = i;
    nChoice = 0;
    while (n > 0 && nChoice < 3) { int k = rndi(n); choice[nChoice++] = pool[k]; pool[k] = pool[--n]; }
    sel = 0; state = ST_LEVELUP;
    tocar_sfx(SFX_ID_POWERUP);
}
static void apply_upgrade(int u) {
    lvl[u]++;
    if (u == U_CORACAO) { H.maxhp += 20; H.hp += 30; if (H.hp > H.maxhp) H.hp = H.maxhp; }
    if (u == U_FAULHA && lvl[u] == 1) cdFaulha = 20;
    if (u == U_MISSEIS && lvl[u] == 1) cdMis = 20;
    if (u == U_BURACO && lvl[u] == 1) cdBuraco = 30;
    if (u == U_RAIO && lvl[u] == 1) cdRaio = 20;
}
static void update_gems(void) {
    int hx = H.x >> FP, hy = (H.y >> FP) - 8, mg = magnet();
    static const int VAL[3] = {1, 3, 10};
    for (int i = 0; i < MAXG; i++) { Gem *g = &gems[i]; if (!g->on) continue;
        int dx = hx - g->x, dy = hy - g->y, d = dist_approx(dx, dy);
        if (d < mg) g->pull = 1;
        if (g->pull) { int sp = 4 + (g->pull > 20 ? 3 : g->pull / 7); if (g->pull < 255) g->pull++;
            if (d > 0) { g->x = (int16_t)(g->x + dx * sp / d); g->y = (int16_t)(g->y + dy * sp / d); } }
        if (d < 8) { g->on = 0; xp += VAL[g->tier]; }
    }
    if (xp >= xpNext && state == ST_PLAY) { xp -= xpNext; level++; xpNext = 1 + level * 4; open_levelup(); }
}

/* ----- Partículas e ambiente ----- */
static int brazOnScreen[24], nBrazOn;
static void update_ambient(void) {
    /* ciclos de paleta */
    if (steps % 10 == 0) cyWater++;
    if (steps % 18 == 0) cyRune++;
    if (rndi(6) == 0) fireA = rndi(3) == 0;
    if (rndi(7) == 0) fireB = rndi(3) == 0;
    /* relâmpago de tempos a tempos */
    if (lightning > 0) lightning--;
    if (--lightT <= 0) { lightT = 60 * (40 + rndi(50)); lightning = 7; shake = 3; }
    /* brasas a subir dos braseiros visíveis */
    for (int k = 0; k < nBrazOn; k++) {
        const Prop *pr = &PROPSP[brazOnScreen[k]];
        if (pr->flags & PF_FIRE) { if (rndi(2) == 0) add_part(1, (pr->x - 24 + rndi(48)) << FP, (pr->y - 46 - rndi(10)) << FP, (rndi(96) - 48), -(120 + rndi(140)), 50 + rndi(50), rndi(3) ? E3 : E4); }
        else if (rndi(5) == 0) add_part(1, (pr->x - 3 + rndi(7)) << FP, (pr->y - ((pr->flags & PF_RUINED) ? 22 : 26)) << FP, (rndi(64) - 32), -(100 + rndi(120)), 40 + rndi(40), rndi(3) ? E3 : E4);
    }
    if (portalOn && rndi(2) == 0)
        add_part(1, (portalX - 14 + rndi(29)) << FP, (portalY - 10 - rndi(44)) << FP, rndi(64) - 32, -(60 + rndi(80)), 30 + rndi(30), rndi(2) ? C1 : M3);
    /* cinza a cair e pirilampos na relva, sempre à volta da câmara */
    int ash = 0, fly = 0;
    for (int i = 0; i < MAXPT; i++) { Part *q = &pt[i]; if (!q->on) continue;
        q->x += q->vx; q->y += q->vy;
        if (q->kind == 1) { q->vx += (rndi(9) - 4) * 4; if (--q->life <= 0) q->on = 0; }
        else {
            int sx = (q->x >> FP) - camX, sy = (q->y >> FP) - camY;
            if (sx < -16 || sx > SCR_W + 16 || sy < -16 || sy > SCR_H + 16) q->on = 0;
            if (q->kind == 2) { ash++; q->vx = -60 + (SINT[(steps * 2 + i * 37) & 255] >> 3); }
            if (q->kind == 3) { fly++; if (--q->life <= 0) q->on = 0; if (rndi(20) == 0) { q->vx = rndi(81) - 40; q->vy = rndi(81) - 40; } }
        }
    }
    for (; ash < 40; ash++) {
        int edge = rndi(3), x = edge == 2 ? SCR_W + 8 : rndi(SCR_W + 120), y = edge == 2 ? rndi(SCR_H) : -8;
        add_part(2, (camX + x) << FP, (camY + y) << FP, -60, 90 + rndi(60), 0, rndi(3) ? G1 : G2);
    }
    for (; fly < 10; fly++) {
        int x = camX + rndi(SCR_W), y = camY + rndi(SCR_H);
        if (attr_at(x, y) & A_GRASS) add_part(3, x << FP, y << FP, rndi(81) - 40, rndi(81) - 40, 200 + rndi(300), V2);
    }
}

/* ----- Passo de lógica ----- */
static void step_play(void) {
    steps++;
    int mx = ((padNow & SEGA_CTRL_RIGHT) != 0) - ((padNow & SEGA_CTRL_LEFT) != 0);
    int my = ((padNow & SEGA_CTRL_DOWN) != 0) - ((padNow & SEGA_CTRL_UP) != 0);
    H.moving = mx || my;
    if (H.moving) {
        H.fx = mx; H.fy = my;
        if (mx) { H.dir = SIDE; H.flip = mx < 0; } else { H.dir = my < 0 ? UP : DOWN; H.flip = 0; }
        int sp = hero_speed(); if (mx && my) sp = sp * 181 >> 8;
        int32_t nx = H.x + mx * sp, ny = H.y + my * sp;
        int hy = H.y >> FP;
        if (!(attr_at((nx >> FP) - 5, hy) & A_SOLID) && !(attr_at((nx >> FP) + 5, hy) & A_SOLID)) H.x = nx;
        int hx = H.x >> FP;
        if (!(attr_at(hx - 5, ny >> FP) & A_SOLID) && !(attr_at(hx + 5, ny >> FP) & A_SOLID)) H.y = ny;
    }
    H.anim++; if (H.inv) H.inv--; if (H.flash) H.flash--; if (H.atkT) H.atkT--;
    if ((padPressed & SEGA_CTRL_START)) { state = ST_PAUSE; return; }
    /* aparecimento: cada vez mais depressa; hordas a cada minuto */
    int t = steps / 60;
    if (--spawnT <= 0) {
        spawnT = 60 - t / 5; if (spawnT < 6) spawnT = 6;
        if (bossIdx >= 0) spawnT *= 2;   /* com o boss vivo, menos inimigos normais */
        int huskPct = t < 40 ? 0 : (t - 40) / 4; if (huskPct > 45) huskPct = 45;
        spawn_ring(rndi(100) < huskPct ? 1 : 0, 200, 230);
    }
    if (--hordeT <= 0) { hordeT = 60 * 60; for (int k = 0; k < 16; k++) { int a = k * 16;
        spawn_at(0, (H.x >> FP) + (SINT[(a + 64) & 255] * 170 >> 8), (H.y >> FP) + (SINT[a] * 130 >> 8)); } }
    if (heroi == HERO_NINJA) weapons(); else magias();
    update_bolts();
    update_enemies();
    update_gems();
    for (int i = 0; i < MAXFX; i++) if (fx[i].on && ++fx[i].t >= fx[i].n * fx[i].rate) fx[i].on = 0;
    if (shake) shake--;
    mapSteps++;
    if (mapNameT) mapNameT--;
    if (bannerT) bannerT--;
    /* boss do primeiro mapa no nível 15 */
    if (mapa == 0 && !bossDone && bossIdx < 0 && level >= BOSS_NIVEL && state == ST_PLAY) start_boss();
    /* entrar no portal */
    if (portalOn) { int dx = (H.x >> FP) - portalX, dy = (H.y >> FP) - portalY;
        if (iabs(dx) < 14 && iabs(dy) < 10) { state = ST_TRANSIT; transT = 0; lightning = 14; shake = 6; tocar_sfx(SFX_ID_TELEPORT); } }
    /* aldeia: sobrevive 5 minutos até ao amanhecer */
    if (mapa == 1 && mapSteps >= 60 * 300) state = ST_WIN;
}
static void enter_map(int m) {
    for (int i = 0; i < MAXE; i++) en[i].on = 0;
    for (int i = 0; i < MAXG; i++) gems[i].on = 0;
    for (int i = 0; i < MAXB; i++) bolts[i].on = 0;
    for (int i = 0; i < MAXFX; i++) fx[i].on = 0;
    for (int i = 0; i < MAXPT; i++) pt[i].on = 0;
    load_map(m);
    H.x = MAPAS[m].sx << FP; H.y = MAPAS[m].sy << FP; H.fx = 0; H.fy = -1; H.dir = UP; H.inv = 60;
    portalOn = 0; bossIdx = -1; mapNameT = 240; bannerT = 0; mapSteps = 0; spawnT = 90; hordeT = 60 * 60;
    for (int i = 0; i < MAXMET; i++) met[i].on = 0;
    for (int i = 0; i < MAXRAIO; i++) raios[i].t = 0;
    bh.on = 0;
}
/* Animação do título: nuvens a deslizar, lago e lua a pulsar, brasas a subir, relâmpagos ocasionais */
static void title_fx(void) {
    skyT++;
    if (steps % 8 == 0) cyWater++;
    if (steps % 24 == 0) cyRune++;
    if (lightning > 0) lightning--;
    if (--lightT <= 0) { lightT = 60 * (9 + rndi(10)); lightning = 7; }
    if (rndi(2) == 0) add_part(1, rndi(320) << FP, 226 << FP, rndi(64) - 32, -(60 + rndi(110)), 110 + rndi(90), rndi(3) ? E3 : E2);
    for (int i = 0; i < MAXPT; i++) { Part *q = &pt[i]; if (!q->on) continue;
        q->x += q->vx; q->y += q->vy; q->vx += (rndi(9) - 4) * 3;
        if (--q->life <= 0 || q->y < 0) q->on = 0; }
}
static void step(void) {
    switch (state) {
    case ST_TITLE:   /* menu principal: JOGAR, OPÇÕES, CRÉDITOS, SAIR */
        steps++; title_fx();
        if (padPressed & SEGA_CTRL_UP) menuSel = (menuSel + 3) % 4;
        if (padPressed & SEGA_CTRL_DOWN) menuSel = (menuSel + 1) % 4;
        if (padPressed & (BTN_OK | SEGA_CTRL_START)) {
            if (menuSel == 0) { state = ST_CHARSEL; charSel = heroi; }
            else if (menuSel == 1) { state = ST_OPTIONS; optSel = 0; }
            else if (menuSel == 2) state = ST_CREDITS;
            else { state = ST_BYE; byeT = 0; }
        }
        break;
    case ST_CHARSEL:   /* escolha do personagem */
        steps++; title_fx();
        if (padPressed & (SEGA_CTRL_LEFT | SEGA_CTRL_RIGHT)) charSel ^= 1;
        if (padPressed & SEGA_CTRL_B) state = ST_TITLE;
        else if (padPressed & (SEGA_CTRL_A | SEGA_CTRL_C | SEGA_CTRL_START)) {
            heroi = charSel; for (int i = 0; i < MAXPT; i++) pt[i].on = 0;
            new_game(); state = ST_PLAY; }
        break;
    case ST_OPTIONS:
        steps++; title_fx();
        if (padPressed & SEGA_CTRL_UP) optSel = (optSel + 2) % 3;
        if (padPressed & SEGA_CTRL_DOWN) optSel = (optSel + 1) % 3;
        if (padPressed & (SEGA_CTRL_A | SEGA_CTRL_C | SEGA_CTRL_LEFT | SEGA_CTRL_RIGHT)) {
            if (optSel == 0) autoEspada ^= 1;
            else if (optSel == 1) { musicaOn ^= 1; SOM_LER(som_musica) = musicaOn ? 0u : 2u; }
            else if (padPressed & (SEGA_CTRL_A | SEGA_CTRL_C)) state = ST_TITLE;
        }
        if (padPressed & (SEGA_CTRL_B | SEGA_CTRL_START)) state = ST_TITLE;
        break;
    case ST_CREDITS:
        steps++; title_fx();
        if (padPressed & (BTN_OK | SEGA_CTRL_START)) state = ST_TITLE;
        break;
    case ST_BYE:   /* "sair": numa consola não há para onde sair, por isso despede-se e volta ao título */
        steps++; title_fx();
        if (++byeT > 180) state = ST_TITLE;
        break;
    case ST_PLAY: step_play(); break;
    case ST_LEVELUP:
        if (padPressed & SEGA_CTRL_UP) sel = (sel + nChoice - 1) % nChoice;
        if (padPressed & SEGA_CTRL_DOWN) sel = (sel + 1) % nChoice;
        if (padPressed & (BTN_OK | SEGA_CTRL_START)) {
            if (nChoice) apply_upgrade(choice[sel]); else { H.hp += 20; if (H.hp > H.maxhp) H.hp = H.maxhp; }
            state = ST_PLAY;
            if (xp >= xpNext) { xp -= xpNext; level++; xpNext = 1 + level * 4; open_levelup(); }
        }
        break;
    case ST_PAUSE: if (padPressed & SEGA_CTRL_START) state = ST_PLAY; break;
    case ST_INTRO: if (++introT >= 200) state = ST_PLAY; break;   /* apresentação do boss */
    case ST_TRANSIT:   /* entrar no portal: clarão, troca de mapa */
        if (++transT == 30) enter_map(mapa + 1 < NMAPAS ? mapa + 1 : 0);
        if (transT >= 50) state = ST_PLAY;
        break;
    case ST_OVER: case ST_WIN:
        if (padPressed & SEGA_CTRL_START) { state = ST_TITLE; steps = 0; menuSel = 0;
            for (int i = 0; i < MAXPT; i++) pt[i].on = 0;
            SOM_LER(som_musica) = musicaOn ? 0u : 2u; }
        break;
    }
    if (state != ST_PAUSE && state != ST_LEVELUP && !MENU_STATE(state)) update_ambient();
    padPressed = 0;
}

/* ===== Imagem ===== */
typedef struct { int16_t y; uint8_t kind; int16_t idx; } Item;
#define MAXITEMS 220
static Item items[MAXITEMS];
static int first_prop(int y) { int lo = 0, hi = NPROPSP; while (lo < hi) { int m = (lo + hi) >> 1; if (PROPSP[m].y < y) lo = m + 1; else hi = m; } return lo; }

/* Relâmpago: linhas em zigue-zague entre os pontos, com um brilho à volta */
static void bolt_line(int x0, int y0, int x1, int y1) {
    int dx = iabs(x1 - x0), sx = x0 < x1 ? 1 : -1, dy = -iabs(y1 - y0), sy = y0 < y1 ? 1 : -1, err = dx + dy;
    for (int guard = 0; guard < 400; guard++) {
        pset(x0 + 1, y0, M3); pset(x0, y0 + 1, M3); pset(x0, y0, BONE);
        if (x0 == x1 && y0 == y1) break;
        int e2 = 2 * err; if (e2 >= dy) { err += dy; x0 += sx; } if (e2 <= dx) { err += dx; y0 += sy; }
    }
}
static void draw_raio(const Raio *r, int ce, int cy) {
    for (int k = 0; k + 1 < r->n; k++) {
        int x0 = r->px[k] - ce, y0 = r->py[k] - cy, x1 = r->px[k + 1] - ce, y1 = r->py[k + 1] - cy;
        int px = x0, py = y0;
        for (int s = 1; s <= 3; s++) {
            int nx = x0 + (x1 - x0) * s / 3 + (s < 3 ? rndi(9) - 4 : 0), ny = y0 + (y1 - y0) * s / 3 + (s < 3 ? rndi(9) - 4 : 0);
            bolt_line(px, py, nx, ny); px = nx; py = ny;
        }
    }
}
static void draw_world(void) {
    int hx = H.x >> FP, hy = H.y >> FP;
    if (state != ST_TITLE) {
        int fx0 = hx, fy0 = hy;
        if (state == ST_INTRO && bossIdx >= 0) { fx0 = en[bossIdx].x >> FP; fy0 = (en[bossIdx].y >> FP) - 10; }   /* boss com os pés a y = 130 */
        camX = fx0 - SCR_W / 2; camY = fy0 - 8 - SCR_H / 2;
        if (shake) { camX += rndi(shake + 1) - shake / 2; camY += rndi(shake + 1) - shake / 2; }
    }
    if (camX < 0) camX = 0;
    if (camY < 0) camY = 0;
    if (camX > MAP_W * TILE - SCR_W) camX = MAP_W * TILE - SCR_W;
    if (camY > MAP_H * TILE - SCR_H) camY = MAP_H * TILE - SCR_H;
    int ce = camX & ~1; hud_dx = camX & 1;
    draw_tiles(ce, camY);
    /* passagem plana: sombras e cristais */
    int n = 0; nBrazOn = 0;
    for (int i = first_prop(camY - 16); i < NPROPSP && PROPSP[i].y < camY + SCR_H + 72; i++) {
        const Prop *p = &PROPSP[i]; int sx = p->x - ce, sy = p->y - camY;
        if (sx < -40 || sx > SCR_W + 40) continue;
        if (p->flags & PF_SH_L) spr(SPR_SH_L, 0, sx, sy);
        else if (p->flags & PF_SH_M) spr(SPR_SH_M, 0, sx, sy);
        else if (p->flags & PF_SH_S) spr(SPR_SH_S, 0, sx, sy);
        if ((p->flags & (PF_BRAZIER | PF_FIRE | PF_RUINED)) && nBrazOn < 24) brazOnScreen[nBrazOn++] = i;
        if (n < MAXITEMS) items[n++] = (Item){(int16_t)p->y, 0, (int16_t)i};
    }
    if (bh.on) spr(SPR_HOLE + ((steps >> 2) & 3), 0, (bh.x >> FP) - ce, (bh.y >> FP) - camY);   /* buraco negro, por baixo dos inimigos */
    for (int i = 0; i < MAXMET; i++) if (met[i].on && ((steps >> 2) & 1)) spr(SPR_TARGET, 0, met[i].x - ce, met[i].y - camY);   /* aviso no chão */
    for (int i = 0; i < MAXG; i++) if (gems[i].on) spr(SPR_GEM + gems[i].tier, 0, gems[i].x - ce, gems[i].y - camY + ((steps >> 3) & 1));
    for (int i = 0; i < MAXE; i++) if (en[i].on) {
        int sx = (en[i].x >> FP) - ce, sy = (en[i].y >> FP) - camY;
        if (sx < -40 || sx > SCR_W + 40 || sy < -8 || sy > SCR_H + 72) continue;
        spr(en[i].type == 2 ? SPR_SH_L : en[i].type ? SPR_SH_M : SPR_SH_S, 0, sx, sy);
        if (n < MAXITEMS) items[n++] = (Item){(int16_t)(en[i].y >> FP), 1, (int16_t)i};
    }
    if (state != ST_TITLE) { spr(SPR_SH_S, 0, hx - ce, hy - camY); if (n < MAXITEMS) items[n++] = (Item){(int16_t)hy, 2, 0}; }
    if (portalOn && n < MAXITEMS) items[n++] = (Item){(int16_t)portalY, 3, 0};
    /* ordenar por y e desenhar */
    for (int i = 1; i < n; i++) { Item t = items[i]; int j = i - 1; while (j >= 0 && items[j].y > t.y) { items[j + 1] = items[j]; j--; } items[j + 1] = t; }
    for (int k = 0; k < n; k++) {
        Item *it = &items[k];
        if (it->kind == 0) { const Prop *p = &PROPSP[it->idx]; int sx = p->x - ce, sy = p->y - camY;
            spr(p->spr, 0, sx, sy);
            int fa = (steps / 5 + it->idx) & 3;
            if (p->flags & PF_BRAZIER) spr(SPR_FLAME + fa, 0, sx, sy - 18);
            if (p->flags & PF_FIRE) { spr(SPR_FLAME + fa, 0, sx - 16, sy - 44); spr(SPR_FLAME + ((fa + 1) & 3), 0, sx + 2, sy - 52); spr(SPR_FLAME + ((fa + 2) & 3), 0, sx + 18, sy - 40); }
            if (p->flags & PF_RUINED) spr(SPR_FLAME + fa, 0, sx - 6, sy - 20); }
        else if (it->kind == 3) spr(SPR_PORTAL + ((steps >> 3) & 3), 0, portalX - ce, portalY - camY);
        else if (it->kind == 1) { Enemy *e = &en[it->idx];
            int fr = e->type == 2 ? SPR_BOSS + (bossState == 1 ? 2 : (e->anim >> 4) & 1)
                   : e->type ? SPR_HUSK + ((e->anim >> 4) & 1) : SPR_CRAWL + ((e->anim >> 3) & 1);
            spr(fr, (e->flash ? 4 : 0) | (e->face ? 2 : 0), (e->x >> FP) - ce, (e->y >> FP) - camY); }
        else {
            int k2 = H.atkT > 6 ? 6 : H.atkT ? 7 : H.moving ? 2 + ((H.anim / 6) & 3) : (H.anim / 30) & 1;
            if (!(H.inv && (H.inv & 2) && !H.flash))
                spr((heroi == HERO_NINJA ? SPR_HERO : SPR_MAGO) + H.dir * 9 + k2, (H.flash ? 4 : 0) | (H.flip ? 2 : 0), hx - ce, hy - camY);
        }
    }
    /* efeitos por cima */
    for (int i = 0; i < MAXFX; i++) if (fx[i].on) spr(fx[i].base + fx[i].t / fx[i].rate, fx[i].v, fx[i].x - ce, fx[i].y - camY);
    if (state != ST_TITLE && lvl[U_BRASAS]) {
        int nO = lvl[U_BRASAS], R = 34 + 2 * nO;
        for (int k = 0; k < nO; k++) { int a = (orbAng + k * 256 / nO) & 255;
            spr(SPR_ORB + ((steps >> 2) & 1), 0, hx - ce + (SINT[(a + 64) & 255] * R >> 8), hy - 10 - camY + (SINT[a] * R * 3 >> 10)); }
    }
    if (portalOn && state == ST_PLAY) {   /* seta à volta do herói a apontar para o portal */
        int dx = portalX - hx, dy = portalY - hy, d = dist_approx(dx, dy);
        if (d > 48) {
            int ax = iabs(dx), ay = iabs(dy), dir;
            if (ax * 5 > ay * 12) dir = dx > 0 ? 0 : 4;
            else if (ay * 5 > ax * 12) dir = dy > 0 ? 2 : 6;
            else dir = dx > 0 ? (dy > 0 ? 1 : 7) : (dy > 0 ? 3 : 5);
            int bob = ((steps >> 3) & 1);
            spr(SPR_ARROW + dir, 0, hx - ce + dx * (30 + bob * 2) / d, hy - 12 - camY + dy * (30 + bob * 2) / d);
        }
    }
    for (int b = 0; b < MAXB; b++) if (bolts[b].on) { Bolt *o = &bolts[b]; int f = (o->life >> 1) & 1;
        if (o->kind == 1) spr(SPR_FIREBALL + f, 0, (o->x >> FP) - ce, (o->y >> FP) - camY);
        else if (o->kind == 2) spr(SPR_MISSILE + f, 0, (o->x >> FP) - ce, (o->y >> FP) - camY);
        else spr(SPR_BOLT + f, o->vx < 0 ? 2 : 0, (o->x >> FP) - ce, (o->y >> FP) - camY); }
    for (int i = 0; i < MAXMET; i++) if (met[i].on) spr(SPR_METEOR + ((steps >> 2) & 1), 0, met[i].x - ce, met[i].y - met[i].h - camY);
    for (int i = 0; i < MAXRAIO; i++) if (raios[i].t) draw_raio(&raios[i], ce, camY);
    for (int i = 0; i < MAXPT; i++) if (pt[i].on) {
        int sx = (pt[i].x >> FP) - ce, sy = (pt[i].y >> FP) - camY;
        if (pt[i].kind == 3) { if (((steps + i * 13) & 63) < 40) { pset(sx, sy, V2); pset(sx + 1, sy, V1); pset(sx - 1, sy, V1); pset(sx, sy - 1, V1); pset(sx, sy + 1, V1); } }
        else { pset(sx, sy, pt[i].c); if (pt[i].kind == 2 && (i & 3) == 0) pset(sx + 1, sy, pt[i].c); }
    }
    for (int i = 0; i < nVig; i++) FB8[vig[i] + hud_dx] = INK;
}
static void draw_hud(void) {
    char b[40], t[12];
    /* barra de experiência */
    rect(8 + hud_dx, 4, 304, 6, INK);
    rect(9 + hud_dx, 5, 302, 4, D1);
    int w = xpNext ? 302 * xp / xpNext : 0; rect(9 + hud_dx, 5, w, 4, M2); rect(9 + hud_dx, 5, w, 1, M3);
    b[0] = 'N'; b[1] = 'V'; b[2] = ' '; numstr(b + 3, level); text(b, 8, 16, BONE, 1);
    int s = steps / 60; num(t, s / 60, 2); t[2] = ':'; num(t + 3, s % 60, 2); text_c(t, 16, BONE, 1);
    numstr(t, kills); { const char *a = "ABATES "; int i = 0; while (a[i]) { b[i] = a[i]; i++; } int j = 0; while (t[j]) b[i++] = t[j++]; b[i] = 0; }
    text(b, SCR_W - 8 - text_w(b, 1), 16, BONE, 1);
    if (bossIdx >= 0 && state == ST_PLAY) {   /* vida do boss */
        text_c(BOSS_NOME, 28, C1, 1);
        rect(60 + hud_dx, 38, 200, 6, INK); rect(61 + hud_dx, 39, 198, 4, C0);
        int bw = 198 * en[bossIdx].hp / bossMaxHp; if (bw < 0) bw = 0;
        rect(61 + hud_dx, 39, bw, 4, C1); rect(61 + hud_dx, 39, bw, 1, BONE);
    }
    if (bannerT && state == ST_PLAY) text_c(bannerTxt, 52, E4, 1);
    if (mapNameT && state == ST_PLAY && mapNameT < 230) {   /* nome do mapa ao entrar */
        text_c(MAPAS[mapa].nome, 70, E3, 2);
        text_c(MAPAS[mapa].sub, 94, BONE, 1);
    }
    /* vida por baixo do herói */
    int hx = (H.x >> FP) - (camX & ~1) - 10, hy = (H.y >> FP) - camY + 5;
    rect(hx, hy, 20, 4, INK); rect(hx + 1, hy + 1, 18, 2, E0); rect(hx + 1, hy + 1, 18 * H.hp / H.maxhp, 2, H.flash ? RED : E2);
}
static void draw_screens(void) {
    char b[48], t[12];
    if (state == ST_INTRO) {   /* apresentação do boss: barras de cinema e nome a aparecer letra a letra */
        int h = introT * 2; if (h > 32) h = 32;
        rect(0, 0, SCR_W, h, INK); rect(0, SCR_H - h, SCR_W, h, INK);
        if (introT > 16) text_c("UM MAL ANTIGO DESPERTA", 42, G2, 1);
        int n = (introT - 24) / 5; if (n > 10) n = 10;
        if (n > 0) { char nb[16]; int i = 0; for (; i < n && BOSS_NOME[i]; i++) nb[i] = BOSS_NOME[i]; nb[i] = 0;
            text((const char *)nb, (SCR_W - text_w(BOSS_NOME, 3)) / 2, 142, C1, 3); }
        if (introT > 90) text_c(BOSS_SUB, 174, BONE, 1);
        return;
    }
    if (state == ST_TRANSIT && transT >= 20 && transT <= 40) {   /* clarão do portal */
        volatile uint32_t *d = (volatile uint32_t *)FB8; uint32_t c = 0x06060606u;   /* tudo a BONE */
        for (int i = 0; i < SCR_W * SCR_H / 4; i++) d[i] = c;
        return;
    }
    if (state == ST_LEVELUP) {
        panel(28, 40, 264, 150);
        text_c("SUBISTE DE NÍVEL!", 50, E4, 1);
        text_c("ESCOLHE UMA MELHORIA", 64, G2, 1);
        for (int i = 0; i < nChoice; i++) {
            int y = 80 + i * 34, u = choice[i];
            if (i == sel) { rect(36 + hud_dx, y - 3, 248, 30, D2); rect(36 + hud_dx, y - 3, 248, 1, E3); rect(36 + hud_dx, y + 26, 248, 1, E3); rect(36 + hud_dx, y - 3, 1, 30, E3); rect(283 + hud_dx, y - 3, 1, 30, E3); }
            int evo = IS_MAGIA(u) && lvl[u] == 5;
            int k = 0; const char *nm = evo ? EVO_NAME[u - U_FOGO] : UP_NAME[u]; while (nm[k]) { b[k] = nm[k]; k++; }
            if (evo) { const char *nv = " EVOLUÇÃO!"; for (int j = 0; nv[j]; j++) b[k++] = nv[j]; }
            else if (lvl[u]) { b[k++] = ' '; b[k++] = 'N'; b[k++] = 'V'; b[k++] = ' '; numstr(t, lvl[u] + 1); for (int j = 0; t[j]; j++) b[k++] = t[j]; }
            else { const char *nv = " NOVO!"; for (int j = 0; nv[j]; j++) b[k++] = nv[j]; }
            b[k] = 0;
            text(b, 44, y + 2, evo ? C1 : i == sel ? E4 : BONE, 1);
            const char *ds = lvl[u] == 0 && UP_NEW[u][0] ? UP_NEW[u]
                           : evo ? EVO_DESC[u - U_FOGO] : IS_MAGIA(u) ? MAGIA_NV[u - U_FOGO][lvl[u] - 1] : UP_DESC[u];
            text(ds, 44, y + 14, G2, 1);
        }
        if (!nChoice) text_c("TUDO NO MÁXIMO: +20 DE VIDA", 100, BONE, 1);
    } else if (state == ST_PAUSE) {
        panel(100, 90, 120, 40); text_c("PAUSA", 100, E4, 2); text_c("START PARA VOLTAR", 118, G2, 1);
    } else if (state == ST_OVER || state == ST_WIN) {
        panel(50, 60, 220, 100);
        text_c(state == ST_WIN ? "AMANHECEU!" : "A CINZA VENCEU", 72, state == ST_WIN ? E4 : RED, 2);
        int s = steps / 60; const char *a = "SOBREVIVESTE "; int i = 0; while (a[i]) { b[i] = a[i]; i++; }
        num(t, s / 60, 2); t[2] = ':'; num(t + 3, s % 60, 2); for (int j = 0; t[j]; j++) b[i++] = t[j]; b[i] = 0;
        text_c(b, 100, BONE, 1);
        i = 0; a = "NÍVEL "; while (*a) b[i++] = *a++; numstr(t, level); for (int j = 0; t[j]; j++) b[i++] = t[j];
        a = "  ABATES "; while (*a) b[i++] = *a++; numstr(t, kills); for (int j = 0; t[j]; j++) b[i++] = t[j]; b[i] = 0;
        text_c(b, 114, BONE, 1);
        if ((steps >> 5) & 1 || state == ST_OVER) text_c("PRIME START", 138, E4, 1);
    }
}
/* ===== Ecrã de título ===== */
static void diamond(int cx, int cy, uint8_t c) {
    for (int dy = -2; dy <= 2; dy++) { int w = 2 - iabs(dy); for (int dx = -w; dx <= w; dx++) pset(cx + dx, cy + dy, (iabs(dx) == w || iabs(dy) == 2) ? INK : c); }
}
static void menu_item(const char *s, int y, int sel, int w) {   /* opção do menu; a escolhida numa caixa vermelha com ornamentos */
    if (sel) {
        int x = (SCR_W - w) / 2, b = (steps >> 3) & 1;
        rect(x, y - 4, w, 15, E0); rect(x, y - 4, w, 1, E3); rect(x, y + 10, w, 1, E3); rect(x, y - 4, 1, 15, E3); rect(x + w - 1, y - 4, 1, 15, E3);
        rect(x + 1, y - 3, w - 2, 1, E1);
        diamond(x - 5 - b, y + 3, E4); diamond(x + w + 4 + b, y + 3, E4);
    }
    text_c(s, y, sel ? BONE : G2, 1);
}
static void draw_title(void) {
    hud_dx = 0;
    /* céu com nuvens a deslizar (cópia com volta) */
    int o = ((skyT >> 2) % 320) >> 1;
    for (int y = 0; y < TSKY_H; y++) {
        const uint16_t *src = (const uint16_t *)(TSKY + y * SCR_W); volatile uint16_t *dst = FBW + y * LINEW;
        int n1 = LINEW - o;
        for (int i = 0; i < n1; i++) dst[i] = src[o + i];
        for (int i = 0; i < o; i++) dst[n1 + i] = src[i];
    }
    for (int y = TSKY_H; y < SCR_H; y++) {   /* parte de baixo: cenário opaco */
        const uint16_t *src = (const uint16_t *)(TFG + y * SCR_W); volatile uint16_t *dst = FBW + y * LINEW;
        for (int i = 0; i < LINEW; i++) dst[i] = src[i];
    }
    for (int y = 0; y < TSKY_H; y++) {   /* cenário por cima do céu (0 = transparente) */
        const uint8_t *src = TFG + y * SCR_W; int base = y * LINEW;
        for (int i = 0; i < LINEW; i++) { uint16_t px = RD16(src + i * 2); if (px) OW_PUT(base + i, px); }
    }
    for (int i = 0; i < MAXPT; i++) if (pt[i].on) pset(pt[i].x >> FP, pt[i].y >> FP, pt[i].c);
    spr(SPR_CAPE + (steps >> 3) % 3, 0, 92, 170);   /* herói na falésia, capa ao vento */
    for (int j = 0; j < LOGO_H; j++) {   /* logótipo */
        const uint8_t *src = LOGO + j * LOGO_W; int base = (2 + j) * LINEW + 76 / 2;
        for (int i = 0; i < LOGO_W / 2; i++) { uint16_t px = RD16(src + i * 2); if (px) OW_PUT(base + i, px); }
    }
    { int t = steps % 90; if (t < 12) spr(SPR_SPARK + t / 4, 0, 76 + 83, 2 + 97); }   /* estrela do logótipo a brilhar */
    if (state == ST_TITLE) {
        static const char *OP[4] = {"JOGAR", "OPÇÕES", "CRÉDITOS", "SAIR"};
        for (int i = 0; i < 4; i++) menu_item(OP[i], 128 + i * 15, i == menuSel, 96);
        text("CRIADO POR CAPPUCCIN0PT", 8, 210, G2, 1);
        for (int dy = -6; dy <= 6; dy++) for (int dx = -6; dx <= 6; dx++) if (dx * dx + dy * dy <= 36) pset(244 + dx, 213 + dy, (dx * dx + dy * dy > 25) ? M2 : M1);
        text("A", 242, 210, BONE, 1); text("SELECIONAR", 254, 210, G2, 1);
    } else if (state == ST_CHARSEL) {
        panel(40, 120, 240, 98);
        text_c("ESCOLHE O PERSONAGEM", 126, E3, 1);
        static const char *NOME[2] = {"NINJA", "FEITICEIRA"}, *DESC[2] = {"LÂMINA E BRASAS", "MAGIAS EM ÁREA"};
        for (int k = 0; k < 2; k++) {
            int x0 = k ? 166 : 62, sel = k == charSel;
            rect(x0, 138, 92, 62, D1); uint8_t bc = sel ? E3 : D2;
            rect(x0, 138, 92, 1, bc); rect(x0, 199, 92, 1, bc); rect(x0, 138, 1, 62, bc); rect(x0 + 91, 138, 1, 62, bc);
            int base = k ? SPR_MAGO : SPR_HERO;
            spr(base + DOWN * 9 + (sel ? 2 + ((steps / 6) & 3) : (steps / 30) & 1), 0, x0 + 46, 176);
            text(NOME[k], x0 + (92 - text_w(NOME[k], 1)) / 2, 181, sel ? E4 : BONE, 1);
            text(DESC[k], x0 + (92 - text_w(DESC[k], 1)) / 2, 191, G2, 1);
        }
        text_c("ESQ/DIR: ESCOLHER  A: JOGAR  B: VOLTAR", 207, G1, 1);
    } else if (state == ST_OPTIONS) {
        panel(36, 122, 248, 84);
        text_c("OPÇÕES", 128, E3, 1);
        menu_item(autoEspada ? "ESPADA DO NINJA: AUTOMÁTICA" : "ESPADA DO NINJA: MANUAL", 146, optSel == 0, 196);
        menu_item(musicaOn ? "MÚSICA: LIGADA" : "MÚSICA: DESLIGADA", 162, optSel == 1, 196);
        menu_item("VOLTAR", 178, optSel == 2, 196);
        text_c("A OU ESQ/DIR: MUDAR  B: VOLTAR", 194, G1, 1);
    } else if (state == ST_CREDITS) {
        panel(36, 122, 248, 92);
        text_c("CINZAS DA ETERNIDADE", 128, E3, 1);
        text_c("CRIADO POR CAPPUCCIN0PT", 144, BONE, 1);
        text_c("MÚSICA E SONS: CAPPUCCIN0PT", 156, BONE, 1);
        text_c("FEITO PARA A SEGA 32X", 172, G2, 1);
        text_c("B: VOLTAR", 196, G1, 1);
    } else if (state == ST_BYE) {
        panel(60, 130, 200, 54);
        text_c("ATÉ À PRÓXIMA!", 140, E4, 2);
        text_c("OBRIGADO POR JOGARES", 166, BONE, 1);
    }
}
static void draw(void) {
    if (MENU_STATE(state)) { draw_title(); return; }
    draw_world();
    if (state != ST_TITLE) draw_hud();
    draw_screens();
}

static void setup_linetable(void) {
    for (int b = 0; b < 2; b++) {
        for (int y = 0; y < 256; y++) FB_LINES[y] = (uint16_t)(0x100 + (y < SCR_H ? y : 0) * LINEW);
        for (int i = 0; i < SCR_W * SCR_H; i++) FB8[i] = M0;
        Hw32xScreenFlip(1);
    }
}

#ifdef PREVIEW
void pv_cheat(void) { level = 14; xp = xpNext - 1; lvl[U_LAMINA] = 6; lvl[U_BRASAS] = 5; lvl[U_FAULHA] = 5; H.maxhp = H.hp = 999; }
void pv_magias(int n) { lvl[U_FOGO] = lvl[U_MISSEIS] = lvl[U_BURACO] = lvl[U_RAIO] = (uint8_t)n; H.maxhp = H.hp = 999; }
void pv_heroi(int h) { heroi = h; }
int pv_bh(void) { return bh.on ? bh.t : -1; }
void pv_boss_low(void) { if (bossIdx >= 0) en[bossIdx].hp = 30; }
void pv_goto_portal(void) { H.x = portalX << FP; H.y = (portalY + 40) << FP; }
int pv_portal(void) { return portalOn; }
int pv_mapa(void) { return mapa; }
int pv_boss(void) { return bossIdx; }
int pv_state(void) { return state; }
int pv_hp(void) { return H.hp; }
int pv_enemies(void) { int n = 0; for (int i = 0; i < MAXE; i++) n += en[i].on; return n; }
#endif

int main(void) {
    Hw32xInit(MARS_VDP_MODE_256, 0);
    palette_write();
    setup_linetable();
#ifdef LIBERTAR_SLAVE
    LIBERTAR_SLAVE();   /* dá ao SH-2 secundário a ordem de arranque que o exemplo do Marsdev espera */
#endif
    load_tiles();
    load_map(0);
    build_vignette();
    frt_init();
    state = ST_TITLE; new_game(); steps = 0;
    uint16_t prev = 0, last = frt_read();
    uint32_t acc = 0;
    for (;;) {
        uint16_t now = frt_read(), dt = (uint16_t)(now - last); last = now;
        int nsteps;
        if (dt == 0) nsteps = 1;
        else { acc += dt; nsteps = (int)(acc / TICKS_PER_STEP); acc %= TICKS_PER_STEP; if (nsteps > 3) nsteps = 3; }
        uint16_t pad = HwMdReadPad(0);
        padPressed |= (uint16_t)(pad & ~prev); prev = pad; padNow = pad;
        for (int i = 0; i < nsteps; i++) step();
        draw();
        Hw32xScreenFlip(1);
        set_shift(hud_dx);   /* desloca o ecrã 1 px quando a câmara está num píxel ímpar */
        palette_write();
    }
    return 0;
}
