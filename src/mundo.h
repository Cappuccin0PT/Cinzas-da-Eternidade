/* Constantes partilhadas entre o jogo (SH-2) e o gerador de arte (PC). */
#ifndef MUNDO_H
#define MUNDO_H
#include <stdint.h>

#define SCR_W 320
#define SCR_H 224
#define TILE  16                 /* 16 px = 1 metro */
#define MAP_W 96                 /* mapa de 96x96 tiles = 1536x1536 px */
#define MAP_H 96

/* ===== Paleta fixa (bíblia de arte), índices 1..24; 0 = transparente ===== */
enum { TR = 0, INK, D1, D2, G1, G2, BONE, E0, E1, E2, E3, E4,
       M0, M1, M2, M3, V0, V1, V2, S0, S1, S2, C0, C1, RED, NCOL };
static const uint32_t PAL_HEX[NCOL] = {
    0x000000, 0x0e0c12, 0x2a2533, 0x4a4250, 0x77707a, 0xb3aca6, 0xece6d8,
    0x3b1414, 0x7a2618, 0xc4461f, 0xf08a2e, 0xffd36b,
    0x13233a, 0x24496b, 0x3f7fa3, 0x84c3d6,
    0x1e3322, 0x3d6b3a, 0x7fa64a, 0x4b3a2c, 0x7d6345, 0xb0905f,
    0x5a2a6e, 0xa33a7a, 0xd9243c };

/* Índices extra para ciclos de paleta: os valores continuam a ser cores da paleta fixa,
   só mudam ao longo do tempo (água a ondular, runas a pulsar, luz dos braseiros a tremer). */
#define CY_WATER  32   /* 4 índices */
#define CY_RUNE   36   /* 2 índices */
#define CY_FIRE_A 40   /* 4 níveis de luz, grupo A */
#define CY_FIRE_B 44   /* 4 níveis de luz, grupo B */
#define CY_LAKE   48   /* 4 índices: reflexo da lua no lago do título */
#define CY_MOON   52   /* 2 índices: brilho da lua do título */
#define NPAL      56

/* Atributos dos tiles */
#define A_SOLID  1
#define A_GRASS  2
#define A_WATER  4
#define A_NOPROP 8

enum { SIDE, DOWN, UP };
enum { IDLE, RUN, ATK0, ATK1, DASH };

/* Sprites (cada um tem 8 variantes: flash*4 + espelhado*2 + deslocado 1 px) */
enum {
    SPR_HERO   = 0,   /* 3 direções x 9 frames */
    SPR_CRAWL  = 27,  /* 2 */
    SPR_HUSK   = 29,  /* 2: esqueleto a andar (o golpe está em SPR_ESQ_ATK) */
    SPR_FLAME  = 31,  /* 4 */
    SPR_SMEAR  = 35,  /* 3 rotações (direita, cima, baixo) x 3 frames */
    SPR_SPARK  = 44,  /* 3 */
    SPR_PUFF   = 47,  /* 4 */
    SPR_GEM    = 51,  /* 3 valores */
    SPR_ORB    = 54,  /* 2 */
    SPR_BOLT   = 56,  /* 2 */
    SPR_PILLAR = 58, SPR_TREE, SPR_STATUE, SPR_GRAVE, SPR_BRAZIER, SPR_BANNER, SPR_RUBBLE, SPR_BUSH,
    SPR_SH_S, SPR_SH_M, SPR_SH_L,
    SPR_BOSS,                 /* 3: andar 0, andar 1, aviso */
    SPR_PORTAL = SPR_BOSS + 3,  /* 4 */
    SPR_ARROW  = SPR_PORTAL + 4, /* 8 direções: dir, baixo-dir, baixo, baixo-esq, esq, cima-esq, cima, cima-dir */
    SPR_HOUSE  = SPR_ARROW + 8,  /* 2: em chamas, desabada */
    SPR_TORII  = SPR_HOUSE + 2, SPR_LANTERN, SPR_SAKURA, SPR_BEAMS,
    SPR_MAGO,                       /* feiticeira: 3 direções x 9 frames, como o ninja */
    SPR_FIREBALL = SPR_MAGO + 27,   /* 2 */
    SPR_EXPLO = SPR_FIREBALL + 2,   /* 4 */
    SPR_METEOR = SPR_EXPLO + 4,     /* 2 */
    SPR_TARGET = SPR_METEOR + 2,    /* 1: aviso no chão onde o meteoro vai cair */
    SPR_MISSILE,                    /* 2 */
    SPR_HOLE = SPR_MISSILE + 2,     /* 4 */
    SPR_CAPE = SPR_HOLE + 4,        /* 3: herói de capa no ecrã de título */
    SPR_HUD_LEFT = SPR_CAPE + 3,    /* moldura: avatar, vida, nível e experiência (130x36) */
    SPR_HUD_TIMER,                  /* moldura do relógio (52x18) */
    SPR_HUD_KILLS,                  /* moldura com caveira para os abates (64x18) */
    SPR_HUD_BOSS,                   /* barra do boss com caveira de chifres e placa do nome (224x32) */
    SPR_AVATAR,                     /* 2: retratos do ninja e da feiticeira (24x24) */
    SPR_AREN = SPR_AVATAR + 2,      /* cutscene: ninja ampliado (64x64) — deitado, a erguer-se, de pé, espada em chamas (2), a correr, a atacar */
    SPR_CARRASCO = SPR_AREN + 7,    /* cutscene: Gorgomusgo ampliado (128x128) — andar 0, andar 1, aviso */
    SPR_ESQ_ATK = SPR_CARRASCO + 3, /* esqueleto: lâmina erguida (aviso), golpe */
    SPR_HIST = SPR_ESQ_ATK + 2,     /* intro (64x64): feiticeira (parada, magia), caçador (parado, a apontar), padre (parado, a orar) */
    SPR_REI = SPR_HIST + 6,         /* intro: Rei das Cinzas, o boss final (128x128), 2 frames */
    SPR_CACADOR = SPR_REI + 2,      /* caçador jogável: 3 direções x 9 frames */
    SPR_PADRE = SPR_CACADOR + 27,   /* padre jogável: 3 direções x 9 frames */
    SPR_AVATAR2 = SPR_PADRE + 27,   /* 2: retratos do caçador e do padre (24x24) */
    SPR_CRUZ = SPR_AVATAR2 + 2,     /* 2: cruz a girar */
    SPR_FRASCO = SPR_CRUZ + 2,      /* 1: frasco de água benta */
    SPR_ARMADILHA = SPR_FRASCO + 1, /* 2: aberta, fechada */
    SPR_FALCAO = SPR_ARMADILHA + 2, /* 2: asas em cima, asas em baixo */
    SPR_COUNT = SPR_FALCAO + 2
};
typedef struct { uint16_t w, h, ax, ay; uint32_t off; } SprDesc;   /* w já inclui folga par */
typedef struct { uint16_t x, y; uint8_t spr, flags; } Prop;        /* x,y = pés, em px do mundo */
#define PF_BRAZIER 1
#define PF_SH_S    2
#define PF_SH_M    4
#define PF_SH_L    8
#define PF_FIRE   16   /* casa em chamas: chamas no telhado e brasas */
#define PF_RUINED 32   /* casa desabada: uma chama baixa */
#define NMAPAS 2

/* Fonte 5x7 em células de 6x10 (2 linhas para acentos, 1 para cedilha) */
#define NGLYPH 76      /* ASCII 32..95 + 12 letras acentuadas */
#define GLYPH_H 10
/* Efeitos sonoros (pedidos pelo SH-2 principal, misturados pelo secundário) */
#define SFX_ID_HURT    1
#define SFX_ID_POWERUP 2
#define SFX_ID_ATTACK   3
#define SFX_ID_TELEPORT 4
#define SFX_ID_MONSTER  5
#define SFX_ID_TROVAO   6
#define SFX_ID_FOGO     7
#define SFX_ID_OSSOS    8
#define SFX_ID_PORTA    9
#define SFX_ID_IMPACTO  10
#define SFX_ID_CONFIRMAR 11
#define FAIXA_INTRO 2   /* música da cutscene de abertura */
#define FAIXA_NENHUMA 3 /* música desligada */

#define TSKY_H 150
enum { AREN_DEITADO, AREN_JOELHOS, AREN_PE, AREN_CHAMAS, AREN_CHAMAS2, AREN_CORRE, AREN_ATAQUE };   /* céu do título com nuvens (desliza na horizontal) */
#endif
