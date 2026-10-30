/*
 * Gerador de arte para a versão 32X (corre no PC durante a compilação).
 * Desenha sprites, tiles, o mapa de 96x96 tiles, adereços e a fonte, com a direção de arte do jogo,
 * e escreve tudo em src/assets.h como dados da ROM. Assim a consola não faz cálculos pesados no arranque.
 * Uso: gcc -O2 -o gerar tools/gerar_assets.c -lm && ./gerar > src/assets.h
 */
#include <stdio.h>
#include <stdint.h>
#include "../src/mundo.h"

static uint8_t LIGHTER[NCOL], DARKER[NCOL], WARM[NCOL];
static const uint8_t BAYER[16] = {0,8,2,10,12,4,14,6,3,11,1,9,15,7,13,5};
static void init_ramps(void) {
    for (int i = 0; i < NCOL; i++) LIGHTER[i] = DARKER[i] = WARM[i] = (uint8_t)i;
    LIGHTER[INK]=D1; LIGHTER[D1]=D2; LIGHTER[D2]=G1; LIGHTER[G1]=G2; LIGHTER[V0]=V1; LIGHTER[V1]=V2; LIGHTER[S0]=S1;
    DARKER[D1]=INK; DARKER[D2]=D1; DARKER[G1]=D2; DARKER[G2]=G1; DARKER[V1]=V0; DARKER[V2]=V1;
    DARKER[M1]=M0; DARKER[S1]=S0; DARKER[E1]=E0; DARKER[V0]=INK; DARKER[S0]=INK;
    WARM[INK]=E0; WARM[D1]=S0; WARM[D2]=E1; WARM[G1]=S1; WARM[V0]=S0; WARM[V1]=S1;
}
/* ===== Aleatório determinista ===== */
static uint32_t seed = 1337;
static int rndi(int n) { seed = seed * 1103515245u + 12345u; return (int)((seed >> 16) % (uint32_t)n); }
static int chance(int pct) { return rndi(100) < pct; }

/* ===== Imagens em memória e desenho em pixel ===== */
typedef struct { uint8_t *px; int w, h; } Img;
static void ip(Img *im, int x, int y, int w, int h, uint8_t c) {
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            if (i >= 0 && j >= 0 && i < im->w && j < im->h) im->px[j * im->w + i] = c;
}
static void ie(Img *im, float cx, float cy, float rx, float ry, uint8_t c) {
    for (int y = (int)(cy - ry) - 1; y <= (int)(cy + ry) + 1; y++)
        for (int x = (int)(cx - rx) - 1; x <= (int)(cx + rx) + 1; x++) {
            float dx = (x + .5f - cx) / rx, dy = (y + .5f - cy) / ry;
            if (dx * dx + dy * dy <= 1.f) ip(im, x, y, 1, 1, c);
        }
}
static uint8_t tmpbuf[224 * 96 > 128 * 128 ? 224 * 96 : 128 * 128];
static void outline(Img *im) { /* contorno exterior de 1 px a tinta */
    int n = im->w * im->h;
    for (int i = 0; i < n; i++) tmpbuf[i] = im->px[i];
    for (int y = 0; y < im->h; y++)
        for (int x = 0; x < im->w; x++) {
            if (tmpbuf[y * im->w + x]) continue;
            static const int dx[4] = {1,-1,0,0}, dy[4] = {0,0,1,-1};
            for (int k = 0; k < 4; k++) {
                int xx = x + dx[k], yy = y + dy[k];
                if (xx < 0 || yy < 0 || xx >= im->w || yy >= im->h) continue;
                uint8_t c = tmpbuf[yy * im->w + xx];
                if (c && c != INK) { im->px[y * im->w + x] = INK; break; }
            }
        }
}
#define p(x,y,w,h,c) ip(im,(x),(y),(w),(h),(c))
#define e(cx,cy,rx,ry,c) ie(im,(cx),(cy),(rx),(ry),(c))

/* ===== Herói: 32x32, 24 px de altura (mesmo desenho da versão PC) ===== */
static void draw_hero(Img *im, int dir, int pose, int f) {
    /* Ninja de cabeça grande: capuz e máscara, faixa vermelha com emblema dourado, katana às costas.
       32x32, pés em y = 29..30. Nos golpes a katana sai das costas para a mão. */
    int Y = ((pose == IDLE && f == 1) || (pose == RUN && (f == 1 || f == 3))) ? 1 : 0;
    int atk = (pose == ATK0 || pose == ATK1);
    if (dir == SIDE) {
        static const int rs[4] = {0, 2, 0, -2};
        int s = pose == RUN ? rs[f] : pose == DASH ? 3 : 0;
        if (pose == ATK0) { p(8,0,2,12,G2); p(8,0,1,12,BONE); p(6,12,6,1,V1); }          /* katana erguida atrás */
        if (!atk) {                                                                       /* katana às costas */
            for (int i = 0; i < 12; i++) p(11 - i / 2, 14 + i, 2, 1, i < 10 ? E1 : E2);
            p(5,26,2,1,BONE); p(9,13,4,1,V1); p(11,9,2,4,M2); p(11,9,1,4,M3);
        }
        p(14-s,24,3,5,D1); p(13-s,29,4,2,D1);                                             /* perna de trás */
        p(16+s,24,3,5,D2); p(16+s,25,3,1,E1); p(16+s,29,5,2,D1);                          /* perna da frente */
        p(12,16+Y,9,8,D2); p(12,16+Y,9,1,G1); p(17,17+Y,2,4,G1);
        p(12,22+Y,9,2,E2); p(12,23+Y,9,1,E1); p(10,22+Y,2,3,E2);
        if (pose == IDLE || pose == RUN) { p(16,17+Y,3,6,D2); p(16,17+Y,1,6,G1); p(16,22+Y,3,1,E1); p(17,23+Y,2,2,S2); }
        if (pose == DASH) { p(12,19,2,2,S2); p(11,18,2,1,E1); }
        if (pose == ATK0) { p(11,12,3,5,D2); p(9,12,2,2,S2); }
        if (pose == ATK1) { p(18,17,5,3,D2); p(22,17,2,2,S2); p(24,17,8,2,G2); p(24,17,8,1,BONE); p(23,15,1,6,V1); }
        e(16,9+Y,8,7.5f,G1); e(11,10+Y,3,5,D2); e(15,5+Y,4,2,G2);                       /* capuz */
        p(9,7+Y,15,2,E2); p(9,8+Y,15,1,E1);                                               /* faixa */
        { int tl = (pose == RUN || pose == DASH) ? 6 : 3; p(9-tl,8+Y,tl,2,E2); p(9-tl,10+Y,2,1,E1); }
        p(19,10+Y,5,3,S2); p(19,12+Y,5,1,S1); p(21,11+Y,2,1,M3); p(22,11+Y,1,1,INK);       /* cara */
        p(18,13+Y,6,3,D2); p(18,13+Y,6,1,G1);                                             /* máscara */
    } else if (dir == DOWN) {
        int la = (pose == RUN && f == 1), lb = (pose == RUN && f == 3);
        if (!atk) { p(23,3,2,5,M2); p(23,3,1,5,M3); p(21,8,5,1,V1); p(5,23,2,5,E1); p(5,27,2,1,BONE); }
        if (pose == ATK0) { p(23,0,2,10,G2); p(23,0,1,10,BONE); p(21,10,5,1,V1); }
        p(11,24,4,5-la,D2); p(17,24,4,5-lb,D2); p(11,25,4,1,E1); p(17,25,4,1,E1);
        p(10,29-la,5,2,D1); p(17,29-lb,5,2,D1);
        p(10,16+Y,12,8,D2); p(10,16+Y,12,1,G1); p(13,17+Y,2,3,G1); p(17,17+Y,2,3,G1);
        p(10,22+Y,12,2,E2); p(10,23+Y,12,1,E1); p(20,22+Y,2,3,E2);
        p(7,17+Y,3,6,D2); p(7,17+Y,1,6,G1); p(7,22+Y,3,1,E1); p(7,23+Y,3,2,S2);
        if (pose != ATK0) { p(22,17+Y,3,6,D2); p(24,17+Y,1,6,D1); p(22,22+Y,3,1,E1); p(22,23+Y,3,2,S2); }
        else { p(22,11,3,6,D2); p(22,11,2,2,S2); }
        if (pose == ATK1) { p(1,24,15,2,G2); p(1,24,15,1,BONE); p(16,22,1,6,V1); p(17,23,2,2,S2); }
        e(16,9+Y,8.5f,7.5f,G1); e(14,5+Y,4,2,G2); p(23,7+Y,2,6,D2);
        p(8,7+Y,16,2,E2); p(8,8+Y,16,1,E1); p(15,7+Y,2,2,E4);
        p(24,8+Y,3,2,E2); p(26,10+Y,2,2,E1);
        p(11,10+Y,10,3,S2); p(11,12+Y,10,1,S1);
        p(12,11+Y,2,1,M3); p(18,11+Y,2,1,M3); p(13,11+Y,1,1,INK); p(18,11+Y,1,1,INK);
        p(10,13+Y,12,3,D2); p(11,13+Y,10,1,G1);
    } else {
        int la = (pose == RUN && f == 1), lb = (pose == RUN && f == 3);
        if (pose == ATK1) { p(16,2,14,2,G2); p(16,2,14,1,BONE); p(15,0,1,6,V1); }
        if (pose == ATK0) { p(22,0,2,11,G2); p(22,0,1,11,BONE); p(20,11,5,1,V1); }
        p(11,24,4,5-la,D1); p(17,24,4,5-lb,D1); p(11,25,4,1,E1); p(17,25,4,1,E1);
        p(10,29-la,5,2,D1); p(17,29-lb,5,2,D1);
        p(10,16+Y,12,8,D2); p(11,16+Y,10,1,G1);
        p(10,22+Y,12,2,E2); p(10,23+Y,12,1,E1); p(15,21+Y,2,3,E2);
        p(7,17+Y,3,6,D2); p(7,23+Y,3,2,S2); p(22,17+Y,3,6,D2); p(22,23+Y,3,2,S2);
        if (!atk) {                                                                       /* katana na diagonal das costas */
            for (int i = 0; i < 15; i++) { p(8 + i, 27 - i, 2, 2, E1); p(8 + i, 27 - i, 1, 1, E2); }
            p(6,29,2,1,BONE); p(22,11,4,2,V1); p(24,5,2,6,M2); p(24,5,1,6,M3);
        }
        e(16,9+Y,8.5f,7.5f,G1); e(14,5+Y,5,3,G2); p(9,14+Y,14,2,D2);
        p(8,8+Y,16,2,E2); p(14,8+Y,4,3,E1); p(15,11+Y,2,4,E2); p(17,12+Y,1,3,E1);
        if (atk) p(14,6,2,2,S2);
    }
}
/* Rastejante (16x16), flame (16x24), golpe (48x48) */
static void draw_crawler(Img *im, int f) {
    int lf = f & 1;
    p(3,13,1,2,D1); p(6+lf,14,1,2,D1); p(9-lf,14,1,2,D1); p(12,13,1,2,D1);
    e(8,10.5f,6,4.5f,C0); e(8,9.5f,5,3,C1); p(5,7,3,1,G1);
    p(5,10,2,1,E4); p(10,10,2,1,E4);
}
static void draw_flame(Img *im, int f) {
    static const int oo[4] = {0, 1, 0, -1}; int o = oo[f];
    e(8,17,5,6,E2); e(8+o*.5f,14,3.5f,7,E3); e(8,18,2,3,E4);
    p(7+o,3+(f&1),2,5,E2); p(4-o,9,2,3,E3); p(11+o,8,1,3,E2); p(8,19,1,1,BONE);
}
static void draw_smear(Img *im, int f, int big) {
    static const int s0[3] = {-70,-55,-30}, s1[3] = {70,80,85};
    float rin = big ? 12 : 15, rout = big ? 23 : 20;
    /* ângulos aproximados com tabela: compara declive em vez de atan2 */
    for (int y = 0; y < 48; y++) for (int x = 0; x < 48; x++) {
        float dx = x + .5f - 14, dy = y + .5f - 24, r2 = dx*dx + dy*dy;
        float r = 0; { float g = r2 > 1 ? r2 / 2 : 1; for (int k = 0; k < 8; k++) g = .5f*(g + r2/g); r = g; }
        /* ângulo em graus por aproximação de atan2 */
        float ax = dx < 0 ? -dx : dx, ay = dy < 0 ? -dy : dy, a;
        float mn = ax < ay ? ax : ay, mx = ax < ay ? ay : ax;
        float t = mx > 0 ? mn / mx : 0, s = t*t;
        a = ((-0.0464964749f*s + 0.15931422f)*s - 0.327622764f)*s*t + t; /* rad */
        if (ay > ax) a = 1.5707963f - a;
        if (dx < 0) a = 3.1415927f - a;
        if (dy < 0) a = -a;
        a *= 57.29578f;
        if (a < s0[f] || a > s1[f]) continue;
        float tt = (a - s0[f]) / (float)(s1[f] - s0[f]);
        float sn = tt * (1 - tt) * 4; /* aproxima sin(pi*tt) */
        float th = (rout - rin) * (.25f + .75f*sn) * (f == 2 ? .5f : 1);
        if (r < rout - th || r > rout) continue;
        float k = (rout - r) / th;
        ip(im, x, y, 1, 1, f == 2 ? (k < .5f ? E2 : E1) : (k < .3f ? BONE : k < .6f ? E4 : E3));
    }
}

/* ===== Novos sprites (mesma direção de arte) ===== */
static uint32_t hash2(int x, int y) { uint32_t h = (uint32_t)x * 374761393u + (uint32_t)y * 668265263u; h = (h ^ (h >> 13)) * 1274126177u; return h ^ (h >> 16); }

static void draw_husk(Img *im, int f) {   /* esqueleto: elmo-capuz vermelho com chifres, costelas, lâmina em brasa.
                                              f: 0 e 1 a andar, 2 lâmina erguida (aviso, olhos vermelhos), 3 golpe */
    int lf = (f == 1) ? 1 : 0, warn = f == 2, slash = f == 3, lean = slash ? 1 : 0;
    p(13 - lf, 22, 2, 7, S1); p(12 - lf, 28, 4, 3, E0);                            /* perna de trás */
    p(17 + lf + lean, 22, 2, 7, S2); p(16 + lf + lean, 28, 5, 3, E1); p(16 + lf + lean, 28, 5, 1, E2);   /* perna da frente */
    p(10, 12 + 0, 2, 7, S1); p(10, 18, 2, 2, S2);                                  /* braço de trás */
    p(11 + lean, 11, 10, 9, D1); p(12 + lean, 12, 2, 7, C0);                       /* túnica */
    for (int r = 0; r < 4; r++) { p(12 + lean, 12 + r * 2, 8, 1, BONE); p(19 + lean, 12 + r * 2, 1, 1, S1); }   /* costelas */
    p(15 + lean, 12, 2, 8, S2);                                                   /* coluna */
    p(13 + lean, 19, 7, 2, S2); p(12, 20, 9, 3, E1); p(12, 20, 9, 1, E2); p(14, 23, 4, 3, E1);   /* bacia e cinto */
    e(18 + lean, 7, 3.5f, 3.5f, BONE); p(16 + lean, 8, 6, 2, S2);                  /* crânio */
    p(17 + lean, 10, 4, 2, S2); p(17 + lean, 11, 4, 1, S1); p(18 + lean, 11, 1, 1, INK); p(20 + lean, 11, 1, 1, INK);   /* maxilar */
    p(18 + lean, 6, 2, 2, INK); p(19 + lean, 6, 1, 1, warn ? RED : E2); p(21 + lean, 8, 1, 1, INK);   /* órbita com brilho e nariz */
    e(12 + lean, 5, 4.5f, 5, E0); e(11 + lean, 3, 3, 2, E1); p(9 + lean, 6, 3, 5, E0);   /* elmo-capuz na nuca e na testa */
    p(13 + lean, 2, 7, 2, E0); p(13 + lean, 2, 6, 1, E1); p(15 + lean, 4, 2, 1, E0);
    p(10 + lean, 0, 1, 3, C0); p(11 + lean, 1, 1, 3, C0); p(18 + lean, 0, 1, 3, C0); p(19 + lean, 1, 1, 2, C0); p(10 + lean, 0, 1, 1, C1); p(18 + lean, 0, 1, 1, C1);   /* chifres */
    if (warn) { p(17, 4, 2, 8, S2); p(17, 2, 2, 2, BONE);                         /* lâmina erguida para trás */
        for (int i = 0; i < 9; i++) { p(15 - i, 1 + i / 3, 2, 2, (i & 1) ? E2 : E3); p(15 - i, 3 + i / 3, 2, 1, INK); }
        p(16, 0, 1, 1, E4); }
    else if (slash) { p(20, 14, 7, 2, S2); p(26, 13, 2, 3, BONE);                  /* golpe: lâmina à frente */
        for (int i = 0; i < 6; i++) { p(27 + i, 14 - (i > 3), 2, 2, (i & 1) ? E3 : E2); p(27 + i, 16 - (i > 3), 2, 1, INK); }
        p(31, 12, 1, 1, E4); }
    else { p(19, 13, 2, 6, S2); p(20, 18, 2, 2, BONE);                              /* lâmina em baixo, à frente */
        for (int i = 0; i < 7; i++) { p(22 + i, 19 + i * 5 / 7, 2, 2, (i & 1) ? E2 : E3); p(22 + i, 21 + i * 5 / 7, 2, 1, INK); }
        p(28, 23, 1, 1, E4); }
}
static void draw_spark(Img *im, int f) {
    if (f == 0) { p(7,2,2,12,BONE); p(2,7,12,2,BONE); p(7,7,2,2,E4); }
    else if (f == 1) { p(7,0,2,4,E4); p(7,12,2,4,E4); p(0,7,4,2,E4); p(12,7,4,2,E4); p(3,3,2,2,E3); p(11,11,2,2,E3); p(11,3,2,2,E3); p(3,11,2,2,E3); }
    else { p(1,1,2,2,E2); p(13,13,2,2,E2); p(13,1,2,2,E2); p(1,13,2,2,E2); }
}
static void draw_puff(Img *im, int f) {           /* inimigo a desfazer-se em cinza */
    static const int b[6][3] = {{16,24,6},{10,25,4},{22,25,5},{16,18,4},{11,19,3},{21,17,3}};
    for (int i = 0; i < 6; i++) { float rr = b[i][2] * (.6f + .3f * f);
        e(b[i][0] + (b[i][0]-16)*.15f*f, b[i][1] - f*2, rr, rr, f < 2 ? G1 : D2); }
    if (f < 2) for (int i = 0; i < 3; i++) { float rr = b[i][2] * .4f; e(b[i][0]-1, b[i][1]-1-f*2, rr, rr, G2); }
}
static void draw_gem(Img *im, int t) {            /* cristal de experiência: lua, brasa, corrupção */
    uint8_t a = t == 0 ? M1 : t == 1 ? E1 : C0, b = t == 0 ? M2 : t == 1 ? E3 : C1, c = t == 0 ? M3 : t == 1 ? E4 : BONE;
    p(3,1,2,6,b); p(2,2,4,4,b); p(1,3,6,2,b); p(4,4,2,2,a); p(5,3,1,2,a); p(3,1,1,3,c); p(2,3,1,1,c);
}
static void draw_orb(Img *im, int f) {            /* brasa orbital */
    e(4,4,3.5f,3.5f,E1); e(4,4,2.5f,2.5f,E3); e(4,4,1.2f,1.2f,f ? BONE : E4);
}
static void draw_bolt(Img *im, int f) {           /* faúlha */
    e(4,4,3,2.5f,E2); e(4.5f,4,2,1.5f,E3); p(4,3,2,2,f ? BONE : E4); p(0,4-f,2,1,E2);
}
static void draw_pillar(Img *im) {
    p(3,8,10,36,D2); p(3,8,3,36,G1); p(11,8,2,36,D1);
    p(1,3,14,5,G1); p(1,3,14,1,G2); p(1,7,14,1,D2);
    p(1,42,14,5,G1); p(1,42,14,1,G2);
    p(7,14,1,6,INK); p(8,20,1,4,INK); p(5,30,1,5,INK);
    p(3,40,4,2,V0); p(10,39,2,3,V1); p(4,24,1,3,V1);
}
static void draw_tree(Img *im) {                  /* árvore morta com folhagem escura */
    p(20,40,8,22,S0); p(20,40,3,22,S1); p(16,58,16,4,S0); p(14,60,4,2,S0); p(30,60,4,2,S0);
    p(25,44,1,8,INK); p(18,36,4,6,S0); p(27,34,5,6,S0);
    e(24,25,21,16,V0); e(15,22,10,9,V1); e(33,19,10,9,V1); e(24,13,10,8,V1); e(12,30,7,5,V0); e(37,30,7,5,V0);
    for (int i = 0; i < 40; i++) { int x = 8 + rndi(32), y = 8 + rndi(20); p(x,y,2,1,V2); }
    for (int i = 0; i < 25; i++) { int x = 6 + rndi(36), y = 26 + rndi(10); p(x,y,1,1,V0); }
}
static void draw_statue(Img *im) {                /* estátua corrompida de um guardião */
    p(6,36,20,10,D2); p(6,36,20,2,G1); p(6,45,20,1,D1); p(8,40,16,1,D1);
    e(16,26,7,10,D2); e(15,24,5,8,G1);
    e(16,12,6,7,G1); p(10,10,12,3,D2); p(13,12,2,1,C1); p(18,12,2,1,C1);
    p(9,20,3,12,G1); p(21,20,3,12,D2); p(23,8,2,26,G2); p(22,8,1,26,G1);
    p(14,28,1,6,C1); p(15,33,3,1,C1); p(17,20,1,4,C0);
}
static void draw_grave(Img *im) {
    e(8,8,5,4,D2); p(3,8,10,14,D2); p(3,8,3,14,G1); e(6,6,2,2,G1);
    p(7,10,2,8,INK); p(5,12,6,2,INK); p(3,19,10,3,V0); p(4,18,3,1,V1); p(10,18,2,1,V1);
}
static void draw_brazier(Img *im) {
    p(2,4,12,4,D1); p(2,4,12,1,G1); p(3,8,10,1,D1);
    p(6,9,4,11,D2); p(6,9,1,11,G1);
    p(3,20,10,3,D1); p(3,20,10,1,D2);
}
static void draw_banner(Img *im) {
    p(7,0,2,40,S0); p(7,0,1,40,S1); p(4,2,8,2,S1);
    p(9,4,6,20,E1); p(9,4,6,2,E0); p(9,23,2,3,E1); p(13,23,2,2,E1);
    p(11,10,2,6,C1); p(10,12,4,2,C1);
}
static void draw_rubble(Img *im) {
    e(5,11,4,3,D2); e(4,10,2.5f,2,G1); e(11,12,3,2.5f,D2); e(11,11,2,1.5f,G1); p(8,6,3,2,D2); p(8,6,2,1,G1);
}
static void draw_bush(Img *im) {
    e(10,16,9,6.5f,V0); e(21,15,10,7.5f,V0); e(16,10,7,5.5f,V1); e(9,13,5,4,V1); e(24,10,5,4,V1);
    for (int i = 0; i < 18; i++) { int x = 4 + rndi(24), y = 5 + rndi(10); p(x,y,2,1,V2); }
}
static void draw_shadow(Img *im, int w, int h) {  /* sombra de contacto pontilhada */
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        float dx = (x + .5f - w/2.f) / (w/2.f), dy = (y + .5f - h/2.f) / (h/2.f);
        if (dx*dx + dy*dy <= 1.f && ((x + y) & 1) == 0) ip(im, x, y, 1, 1, INK);
    }
}

/* ===== Construção dos sprites com variantes ===== */
static uint8_t sprData[3 * 1024 * 1024];
static uint32_t sprLen;
static SprDesc desc[SPR_COUNT];
static uint8_t work[224 * 96], work2[64 * 64];
static Img blank(int w, int h) { for (int i = 0; i < w * h; i++) work[i] = 0; Img im = {work, w, h}; return im; }

/* full = 1: 8 variantes (flash, espelhado, deslocado); full = 0: só as 2 deslocadas (adereços, efeitos fixos).
   O jogo só pede flash ou espelhado a sprites completos. */
static void add_sprite_v(int id, const uint8_t *px, int w, int h, int ax, int ay, int full) {
    int W = (w % 2 == 0) ? w + 2 : w + 1;
    desc[id].w = (uint16_t)W; desc[id].h = (uint16_t)h; desc[id].ax = (uint16_t)ax; desc[id].ay = (uint16_t)ay; desc[id].off = sprLen;
    int nv = full ? 2 : 1;
    for (int flash = 0; flash < nv; flash++) for (int flip = 0; flip < nv; flip++) for (int sh = 0; sh < 2; sh++) {
        uint8_t *o = sprData + sprLen;
        for (int i = 0; i < W * h; i++) o[i] = 0;
        for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
            uint8_t c = px[y * w + x]; if (!c) continue;
            int xx = (flip ? w - 1 - x : x) + sh;
            if (flash && c != INK) c = BONE;
            o[y * W + xx] = c;
        }
        sprLen += (uint32_t)(W * h);
    }
}
#define add_sprite(id, px, w, h, ax, ay)      add_sprite_v(id, px, w, h, ax, ay, 0)
#define add_sprite_full(id, px, w, h, ax, ay) add_sprite_v(id, px, w, h, ax, ay, 1)

/* ===== Boss, portal, setas e adereços da aldeia ===== */
static void tooth(Img *im, int cx, int y0, int h, int down) {
    for (int r = 0; r < h; r++) { int w = (h - r) * 4 / h; if (w < 1) w = 1;
        ip(im, cx - w / 2, down ? y0 + r : y0 - r, w, 1, C1); }
}
static void branch_arm(Img *im, int right, int warn) {   /* braço de ramos com folhas */
#define AX(x, w) (right ? 64 - (x) - (w) : (x))
    if (!warn) {
        ip(im, AX(4,13), 32, 13, 2, S0); ip(im, AX(4,13), 32, 13, 1, S1);
        ip(im, AX(3,2), 26, 2, 7, S0); ip(im, AX(8,2), 34, 2, 5, S0); ip(im, AX(12,2), 28, 2, 4, S0);
        ie(im, AX(3,0), 25, 2.5f, 2, V1); ie(im, AX(9,0), 39, 2.5f, 2, V1); ie(im, AX(1,0), 32, 2, 2, V2); ie(im, AX(13,0), 27, 2, 1.5f, V1);
    } else {
        for (int i = 0; i < 11; i++) ip(im, AX(5 + i, 2), 30 - i, 2, 2, S0);
        ip(im, AX(3,2), 14, 2, 6, S0); ip(im, AX(9,2), 18, 2, 4, S0);
        ie(im, AX(3,0), 13, 2.5f, 2, V1); ie(im, AX(5,0), 18, 2.5f, 2, V2); ie(im, AX(10,0), 17, 2, 1.5f, V1);
    }
#undef AX
}
static void draw_boss(Img *im, int f) {   /* Gorgomusgo, 64x64; f: 0 e 1 andar, 2 aviso */
    int warn = f == 2;
    int l1 = (f == 1), l2 = (f == 0);
    p(17,50+l1,9,10-l1,C0); p(17,50+l1,2,10-l1,C1); e(20,61,7,2.5f,C0); p(14,61,5,1,D1);
    p(38,50+l2,9,10-l2,C0); p(38,50+l2,2,10-l2,C1); e(44,61,7,2.5f,C0); p(46,61,5,1,D1);
    for (int y = 16; y <= 54; y++) { int hw = 14 + (y - 16) * 11 / 38;
        p(32 - hw, y, hw * 2, 1, C0); p(32 - hw + 1, y, 3, 1, C1); p(32 + hw - 4, y, 3, 1, D1); }
    for (int k = 0; k < 6; k++) { int x = 15 + k * 7; for (int y = 30; y < 54; y++) if (((y + k * 3) % 7) < 4) p(x + ((y >> 3) & 1), y, 1, 1, D1); }
    for (int x = 11; x < 53; x++) { int y = 45 + (x - 11) / 16;   /* corda */
        p(x, y, 1, 4, M1); if ((x + y) % 3 == 0) p(x, y + 1, 1, 2, M2); if ((x + y) % 3 == 1) p(x, y, 1, 1, M3); }
    e(22,50,6,5,BONE); p(17,52,10,2,G2);   /* caveira */
    e(20,50,1.6f,1.6f,INK); e(25,50,1.6f,1.6f,INK); p(20,50,1,1,E2); p(25,50,1,1,E2); p(22,52,1,1,INK);
    p(19,54,7,3,BONE); p(20,55,1,2,INK); p(22,55,1,2,INK); p(24,55,1,2,INK);
    int mt = warn ? 22 : 24, mb = warn ? 45 : 42;   /* boca serrilhada */
    for (int y = mt; y <= mb; y++) { int hw = (y == mt || y == mb) ? 10 : 13; p(32 - hw, y, hw * 2, 1, INK); }
    for (int y = mt + 3; y <= mb - 3; y += 4) p(26, y, 12, 1, D1);
    for (int i = 0; i < 5; i++) tooth(im, 21 + i * 5, mt, 6 + (i & 1) * 3, 1);
    for (int i = 0; i < 4; i++) tooth(im, 23 + i * 5, mb, 5 + ((i + 1) & 1) * 2, 0);
    branch_arm(im, 0, warn); branch_arm(im, 1, warn);
    e(32,11,29,8.5f,V0); e(26,6,17,6,V0); e(55,17,8,4.5f,V0); e(8,15,6,4,V0);   /* chapéu de musgo */
    e(30,9,23,5.5f,V1); e(24,5,12,3.5f,V1);
    for (int i = 0; i < 30; i++) p(6 + rndi(52), 3 + rndi(13), 1 + rndi(2), 1, V2);
    static const int rp[7][2] = {{14,10},{22,6},{30,11},{38,7},{45,12},{52,15},{34,15}};
    for (int i = 0; i < 7; i++) { int x = rp[i][0], y = rp[i][1]; uint8_t c = (uint8_t)(CY_RUNE + (i & 1));   /* runas que pulsam */
        p(x, y, 3, 1, c); p(x, y + 1, 1, 1, c); p(x + 1, y + 2, 2, 1, c); p(x + 2, y + 3, 1, 1, c); p(x, y + 4, 3, 1, c); }
    p(9,19,46,1,D1);
    uint8_t ec = warn ? RED : E2, cc = warn ? BONE : E4;   /* olhos: vermelhos no aviso */
    for (int k = 0; k < 2; k++) { int cx = k ? 39 : 25;
        for (int r = -2; r <= 2; r++) { int w = 5 - 2 * (r < 0 ? -r : r); p(cx - w / 2, 20 + r, w, 1, ec); }
        p(cx, 20, 1, 1, cc); }
}
static void draw_portal(Img *im, int f) {
    static const uint8_t cols[5] = {C0, C1, M2, M3, BONE};
    e(24,60,20,3.5f,D2); e(24,59,17,2.5f,G1);
    for (int k = 0; k < 5; k++) e(24, 33, 20.f - k * 4, 28.f - k * 5, cols[(k + f) % 5]);
    for (int i = 0; i < 10; i++) { int a = (i * 26 + f * 9) & 63; int x = 24 + (int)(__builtin_cosf(a / 10.2f) * (14 + i % 5)), y = 33 + (int)(__builtin_sinf(a / 10.2f) * (20 + i % 6));
        p(x, y, 1, 1, (i & 1) ? BONE : M3); }
}
static void draw_arrow(Img *im, int diag) {
    if (!diag) { p(2,7,8,2,E4); p(2,7,8,1,BONE); for (int i = 0; i < 5; i++) p(9 + i, 3 + i, 1, 10 - 2 * i, E4); }
    else { for (int i = 0; i < 7; i++) p(3 + i, 3 + i, 2, 2, E4); for (int y = 6; y <= 13; y++) p(13 - (y - 6), y, (y - 6) + 1, 1, E4); p(3,3,1,1,BONE); }
}
static void draw_house(Img *im, int ruined) {   /* casa japonesa de madeira, 64x64 */
    p(6,55,3,7,S0); p(30,55,3,7,S0); p(55,55,3,7,S0);
    p(3,49,58,6,S0); p(3,49,58,1,S1); p(26,55,12,3,S1); p(26,55,12,1,S2);
    p(6,28,52,21,S1); for (int x = 6; x < 58; x += 6) p(x,28,1,21,S0);
    for (int k = 0; k < 2; k++) { int x0 = k ? 37 : 11;   /* portas shoji rasgadas */
        p(x0,32,16,14,BONE); for (int gx = x0; gx <= x0 + 16; gx += 4) p(gx,32,1,14,G2); for (int gy = 32; gy <= 46; gy += 4) p(x0,gy,16,1,G2);
        for (int i = 0; i < 3; i++) p(x0 + 1 + rndi(12), 33 + rndi(10), 2 + rndi(2), 2, INK); }
    for (int i = 0; i < 6; i++) e(8 + rndi(48), 40 + rndi(9), 2 + rndi(3), 1.5f, D1);
    if (!ruined) {
        for (int y = 8; y < 30; y++) { int hw = 20 + (y - 8) * 3 / 4, x0 = 32 - hw < 0 ? 0 : 32 - hw, x1 = 32 + hw > 64 ? 64 : 32 + hw;
            p(x0, y, x1 - x0, 1, ((y - 8) % 3 == 0) ? D1 : D2);
            for (int x = x0 + 2; x < x1; x += 4) if ((y - 8) % 3) p(x, y, 1, 1, G1); }
        p(0,29,64,1,INK); p(0,26,3,3,D2); p(61,26,3,3,D2); p(0,25,2,1,D2); p(62,25,2,1,D2);
        p(12,6,40,3,D1); p(12,6,40,1,G1); p(9,4,4,4,D1); p(51,4,4,4,D1);
        e(22,15,5,3,INK); e(22,16,3.5f,2,CY_FIRE_A + 3); p(19,13,2,1,CY_FIRE_A + 1); p(25,14,2,1,CY_FIRE_A + 2);   /* buracos em brasa */
        e(43,21,4,2.5f,INK); e(43,22,2.5f,1.5f,CY_FIRE_A + 2); p(40,19,2,1,CY_FIRE_A + 1);
    } else {
        for (int y = 20; y < 46; y++) { int x0 = 2 + (y - 20) / 3, x1 = 38 + (y - 20) * 2 / 3;
            p(x0, y, x1 - x0, 1, ((y % 3) == 0) ? D1 : D2); }
        for (int i = 0; i < 12; i++) p(38 + i, 10 + i * 2, 2, 2, D1);
        p(50,8,3,34,D1); p(50,8,1,34,INK); p(30,14,2,10,D1);
        for (int i = 0; i < 8; i++) e(6 + rndi(40), 24 + rndi(18), 2 + rndi(2), 1.5f, INK);
        for (int i = 0; i < 6; i++) p(6 + rndi(40), 26 + rndi(16), 2, 1, CY_FIRE_A + 1 + rndi(3));
    }
}
static void draw_torii(Img *im) {
    p(14,18,5,42,E2); p(17,18,2,42,E1); p(45,18,5,42,E2); p(48,18,2,42,E1);
    p(4,8,56,3,D1); p(2,6,4,3,D1); p(58,6,4,3,D1); p(4,8,56,1,D2);
    p(6,11,52,3,E2); p(6,13,52,1,E1);
    p(10,22,44,4,E2); p(10,25,44,1,E1); p(29,14,6,8,E1);
    p(12,58,9,4,D2); p(43,58,9,4,D2); p(12,58,9,1,G1); p(43,58,9,1,G1);
    for (int i = 0; i < 8; i++) e(6 + rndi(52), 8 + rndi(50), 1.5f, 1.5f, INK);
}
static void draw_lantern(Img *im) {   /* tōrō de pedra com luz a tremer */
    p(4,26,8,5,D2); p(4,26,8,1,G1); p(6,16,4,10,G1); p(9,16,1,10,D2);
    p(3,9,10,7,D2); p(5,10,6,4,CY_FIRE_A + 3); p(7,10,2,4,CY_FIRE_A + 2);
    p(1,5,14,4,G1); p(1,5,14,1,G2); p(6,2,4,3,G1);
}
static void draw_sakura(Img *im) {   /* cerejeira queimada */
    p(21,36,6,26,S0); p(24,36,3,26,D1); p(19,42,3,6,S0); p(26,48,3,5,S0); p(17,60,14,2,S0);
    for (int i = 0; i < 10; i++) { p(12 + i, 30 - i / 2, 2, 2, S0); p(26 + i, 30 - i, 2, 2, S0); }
    for (int i = 0; i < 6; i++) p(23, 30 - i * 2, 2, 2, S0);
    static const int cl[9][3] = {{10,20,5},{18,14,6},{28,10,6},{38,16,5},{36,26,5},{14,30,4},{24,22,5},{42,24,4},{7,27,4}};
    for (int k = 0; k < 9; k++) { float cx = cl[k][0], cy = cl[k][1], r = cl[k][2];
        e(cx, cy, r, r * .8f, C0); e(cx - 1, cy - 1, r * .6f, r * .5f, C1); }
    for (int i = 0; i < 18; i++) p(4 + rndi(40), 6 + rndi(26), 1, 1, BONE);
    for (int i = 0; i < 10; i++) p(4 + rndi(40), 6 + rndi(26), 2, 1, INK);
}
static void draw_beams(Img *im) {   /* vigas caídas e carbonizadas */
    for (int i = 0; i < 24; i++) p(4 + i, 3 + i / 3, 2, 3, D1);
    p(2,9,28,3,S0); p(2,9,28,1,S1); p(6,4,3,10,D1);
    for (int i = 0; i < 6; i++) p(2 + rndi(26), 4 + rndi(8), 1 + rndi(2), 1, INK);
    for (int i = 0; i < 4; i++) p(3 + rndi(25), 5 + rndi(7), 1, 1, CY_FIRE_A + 1 + rndi(3));
}
/* ===== Feiticeira e magias ===== */
static void witch_hat(Img *im, int dir, int Y) {   /* chapéu pontiagudo roxo, aba larga, fita laranja */
    int tipx = dir == SIDE ? 10 : 20;
    for (int y = 0; y < 9; y++) {
        int w = 2 + y * 13 / 10, cx = tipx + (16 - tipx) * y / 8;
        p(cx - w / 2, y + Y, w, 1, C0); p(cx - w / 2, y + Y, 1, 1, C1);
    }
    if (dir == SIDE) { e(17,10+Y,10,2.5f,C0); p(9,9+Y,16,1,C1); }
    else { e(16,10+Y,11.5f,2.5f,C0); p(6,9+Y,20,1,C1); }
    p(10,7+Y,12,2,S1); p(10,8+Y,12,1,E1);
    if (dir == DOWN) { p(15,7+Y,2,2,M3); p(15,7+Y,1,1,BONE); }
    if (dir == SIDE) { p(19,7+Y,2,2,M3); }
}
static void draw_mago(Img *im, int dir, int pose, int f) {
    /* Feiticeira: chapéu pontiagudo roxo, cabelo ruivo comprido, manto roxo com debrum laranja e gemas azuis. */
    int Y = ((pose == IDLE && f == 1) || (pose == RUN && (f == 1 || f == 3))) ? 1 : 0;
    int la = (pose == RUN && f == 1), lb = (pose == RUN && f == 3);
    if (dir == SIDE) {
        static const int rs[4] = {0, 2, 0, -2};
        int s = pose == RUN ? rs[f] : pose == DASH ? 3 : 0;
        int hl = (pose == RUN || pose == DASH) ? 3 : 0;
        p(8 - hl, 12 + Y, 6 + hl, 9, E2); p(8 - hl, 12 + Y, 2, 9, E1); p(10 - hl, 20 + Y, 3, 2, E1);   /* cabelo atrás */
        p(14 - s, 26, 3, 3, D1); p(13 - s, 29, 4, 2, D1);
        p(16 + s, 26, 3, 3, D1); p(16 + s, 29, 5, 2, INK);
        for (int y = 16; y <= 27; y++) { int hw = 3 + (y - 16) * 5 / 11; p(16 - hw, y + (y < 26 ? Y : 0), hw * 2, 1, C0); p(16 - hw, y + (y < 26 ? Y : 0), 1, 1, D1); }
        p(11,27,11,1,E3); p(16,17+Y,4,1,E3); p(17,19+Y,2,1,M3);
        if (pose == IDLE || pose == RUN) { p(17,17+Y,3,6,C0); p(17,17+Y,1,6,C1); p(17,22+Y,3,1,E3); p(18,23+Y,2,2,S2); }
        if (pose == DASH) { p(11,18,3,4,C0); p(10,21,2,2,S2); }
        if (pose == ATK0) { p(16,10,3,7,C0); p(16,10,3,1,E3); p(17,8,2,2,S2); e(18,5,3,3,M3); p(18,4,1,1,BONE); }
        if (pose == ATK1) { p(18,17,6,3,C0); p(23,17,1,3,E3); p(24,17,2,2,S2); e(28,18,3,3,E3); e(28,18,1.5f,1.5f,E4); }
        p(17,11+Y,6,5,S2); p(17,15+Y,6,1,S1); p(20,12+Y,2,1,M3); p(21,12+Y,1,1,INK); p(17,11+Y,3,2,E2);
        witch_hat(im, SIDE, Y);
    } else if (dir == DOWN) {
        p(8,11+Y,4,10,E2); p(8,11+Y,1,10,E1); p(20,11+Y,4,10,E2); p(23,11+Y,1,10,E1);   /* cabelo */
        p(12,26,3,3-la,D1); p(17,26,3,3-lb,D1); p(11,29-la,4,2,INK); p(17,29-lb,4,2,INK);
        for (int y = 16; y <= 27; y++) { int hw = 4 + (y - 16) * 6 / 11; int yy = y + (y < 26 ? Y : 0);
            p(16 - hw, yy, hw * 2, 1, C0); p(16 - hw, yy, 2, 1, C1); p(16 + hw - 2, yy, 2, 1, D1); }
        p(9,27,14,1,E3); p(15,17+Y,2,10,S1); p(15,18+Y,2,1,M3); p(15,22+Y,2,1,M3); p(12,21+Y,8,1,E1);
        if (pose != ATK0) { p(8,17+Y,3,6,C0); p(8,22+Y,3,1,E3); p(8,23+Y,3,2,S2); p(21,17+Y,3,6,C0); p(21,22+Y,3,1,E3); p(21,23+Y,3,2,S2); }
        else { p(7,10,3,7,C0); p(7,9,3,2,S2); p(22,10,3,7,C0); p(22,9,3,2,S2); e(16,3,3,3,M3); p(16,2,1,1,BONE); }
        if (pose == ATK1) { e(16,27,4,2.5f,E3); e(16,27,2,1.5f,E4); }
        p(12,11+Y,8,5,S2); p(12,15+Y,8,1,S1);
        p(13,13+Y,2,1,M3); p(17,13+Y,2,1,M3); p(13,13+Y,1,1,INK); p(18,13+Y,1,1,INK);
        p(11,11+Y,10,2,E2); p(12,11+Y,4,1,E3);
        witch_hat(im, DOWN, Y);
    } else {
        p(12,26,3,3-la,D1); p(17,26,3,3-lb,D1); p(11,29-la,4,2,INK); p(17,29-lb,4,2,INK);
        for (int y = 16; y <= 27; y++) { int hw = 4 + (y - 16) * 6 / 11; int yy = y + (y < 26 ? Y : 0);
            p(16 - hw, yy, hw * 2, 1, C0); p(16 - hw, yy, 1, 1, D1); }
        p(9,27,14,1,E3);
        p(8,17+Y,3,6,C0); p(8,23+Y,3,2,S2); p(21,17+Y,3,6,C0); p(21,23+Y,3,2,S2);
        p(10,11+Y,12,6,E2); e(16,18+Y,6.5f,5,E2);   /* cabelo comprido nas costas */
        p(10,11+Y,1,7,E1); p(21,11+Y,1,7,E1); p(13,15+Y,1,5,E1); p(19,16+Y,1,5,E1); p(16,19+Y,1,3,E1);
        p(12,12+Y,3,1,E3); p(17,12+Y,3,1,E3); p(14,14+Y,1,2,E3);
        if (pose == ATK0 || pose == ATK1) { e(16,2,3,3,M3); p(16,1,1,1,BONE); }
        witch_hat(im, UP, Y);
    }
}
static void draw_fireball(Img *im, int f) {
    e(6,6,4.5f,4.5f,E2); e(6.5f,5.5f,3,3,E3); e(6,5,1.5f,1.5f,f ? BONE : E4); p(1,7+f,2,1,E2); p(10,3-f,1,1,E3);
}
static void draw_explosion(Img *im, int f) {   /* 40x40 */
    if (f == 0) { e(20,22,9,7,E3); e(20,22,6,5,E4); e(20,21,3,2.5f,BONE); }
    else if (f == 1) { e(20,22,16,12,E2); e(20,22,12,9,E3); e(20,21,7,5,E4); for (int i = 0; i < 10; i++) p(4 + rndi(32), 10 + rndi(22), 2, 2, E4); }
    else if (f == 2) { e(20,22,18,14,D2); e(20,21,14,11,E2); e(20,20,8,6,E3); for (int i = 0; i < 12; i++) p(2 + rndi(36), 8 + rndi(26), 2, 1, E3); }
    else { for (int i = 0; i < 9; i++) { int x = 6 + rndi(28), y = 8 + rndi(22); e(x, y, 3 + rndi(3), 2 + rndi(2), i & 1 ? G1 : D2); } }
}
static void draw_meteor(Img *im, int f) {   /* 16x24, cauda de fogo para cima */
    for (int y = 0; y < 15; y++) { int w = 1 + y / 3; p(8 - w / 2 + ((y + f) & 1), y, w, 1, y < 6 ? E2 : E3); }
    e(8,18,5,5,S0); e(7,17,3.5f,3.5f,E1); e(7,16,2,2,E3); p(6,15,1,1,E4);
}
static void draw_target(Img *im) {   /* aviso de impacto: anel pontilhado vermelho */
    for (int y = 0; y < 12; y++) for (int x = 0; x < 32; x++) {
        float dx = (x + .5f - 16) / 16, dy = (y + .5f - 6) / 6, d = dx*dx + dy*dy;
        if (d <= 1.f && d >= .62f && ((x + y) & 1) == 0) ip(im, x, y, 1, 1, RED);
        if (d < .08f) ip(im, x, y, 1, 1, RED);
    }
}
static void draw_missile(Img *im, int f) {
    e(4,4,3.5f,3.5f,C1); e(4,4,2.2f,2.2f,M3); p(3,3,2,2,f ? BONE : M3); p(f ? 0 : 7, 4, 1, 1, C1);
}
static void draw_hole(Img *im, int f) {   /* buraco negro 48x48, achatado em perspetiva */
    e(24,26,21,15,C0);
    for (int arm = 0; arm < 3; arm++) for (int k = 0; k < 28; k++) {
        float t = k / 28.f, a = t * 9.4f + arm * 2.094f + f * 0.52f, r = 7 + t * 15;
        int x = 24 + (int)(__builtin_cosf(a) * r), y = 26 + (int)(__builtin_sinf(a) * r * .72f);
        p(x, y, 2, 1, t < .35f ? BONE : t < .7f ? C1 : M2);
    }
    e(24,26,7.5f,5.5f,INK); e(24,25,4,3,M0); p(22,23,2,1,C1);
}
/* ===== Ecrã de título: céu com nuvens (horizontalmente contínuo), cenário fixo e herói de capa ===== */
static int iabs2(int v);
static uint8_t tsky[TSKY_H][320], tfg[224][320];
static int iabs2(int v);
#define TPI 6.2831853f
static float sn(float v) { return __builtin_sinf(v); }
static void build_title(void) {
    seed = 9090;
    /* céu: degradê noturno e nuvens vermelhas em faixas */
    for (int y = 0; y < TSKY_H; y++) for (int x = 0; x < 320; x++) {
        int b = BAYER[(y & 3) * 4 + (x & 3)];
        uint8_t c = y < 34 ? (b < (34 - y) / 2 ? INK : D1) : y < 100 ? (b < (100 - y) / 5 ? D1 : C0) : (b < (y - 100) / 4 ? E0 : C0);
        float fx = x / 320.f;
        float cv = .55f * sn(TPI * 2 * fx + y * .09f) + .35f * sn(TPI * 5 * fx - y * .21f + 1.3f) + .25f * sn(TPI * 9 * fx + y * .33f);
        float band = sn(y * .19f + cv * 1.4f);
        float dens = (band - .5f) / .5f;
        if (y > 12 && y < 128 && dens > 0) {
            int lvl = (int)(dens * 3.f + b / 16.f);
            if (lvl >= 3) c = E1; else if (lvl == 2) c = E0; else if (lvl == 1 && (b & 1)) c = E0;
            if (lvl >= 2 && y > 0 && sn((y - 1) * .19f + cv * 1.4f) < .62f && b < 8) c = C1;
        }
        tsky[y][x] = c;
    }
    for (int y = 0; y < 224; y++) for (int x = 0; x < 320; x++) tfg[y][x] = 0;
#define FG(x, y, c) do { int _x = (x), _y = (y); if (_x >= 0 && _y >= 0 && _x < 320 && _y < 224) tfg[_y][_x] = (uint8_t)(c); } while (0)
    /* lua vermelha com brilho pulsante */
    int mx = 268, my = 56, mr = 34;
    for (int y = my - mr - 9; y <= my + mr + 9; y++) for (int x = mx - mr - 9; x <= mx + mr + 9; x++) {
        float dx = x - mx + .5f, dy = y - my + .5f, d = __builtin_sqrtf(dx * dx + dy * dy); int b = BAYER[(y & 3) * 4 + (x & 3)];
        if (d <= mr) {
            uint8_t c = E1;
            if (dx + dy < -20 && b < 10) c = E2;
            if (d > mr - 2) c = CY_MOON;
            float cr1 = (dx - 10) * (dx - 10) + (dy + 6) * (dy + 6), cr2 = (dx + 12) * (dx + 12) + (dy - 12) * (dy - 12), cr3 = (dx - 4) * (dx - 4) + (dy - 20) * (dy - 20);
            if (cr1 < 49 || cr2 < 30 || cr3 < 20) c = (b < 9) ? E0 : E1;
            FG(x, y, c);
        } else if (d < mr + 9 && b < (int)((mr + 9 - d) * 1.6f)) FG(x, y, (d < mr + 4) ? CY_MOON + 1 : E0);
    }
    /* montanhas ao longe com neblina */
    for (int x = 0; x < 320; x++) {
        int h = 124 + (int)(9 * sn(x * .05f) + 6 * sn(x * .13f + 1) + 4 * sn(x * .31f + 2));
        for (int y = h; y < 224; y++) FG(x, y, (y - h < 2) ? C0 : D1);
        int h2 = 146 + (int)(6 * sn(x * .07f + 2) + 4 * sn(x * .19f));
        for (int y = h2; y < 224; y++) FG(x, y, (y - h2 < 1) ? C0 : INK);
    }
    for (int y = 132; y < 160; y++) for (int x = 0; x < 320; x++) {
        int b = BAYER[(y & 3) * 4 + (x & 3)], dens = 8 - iabs2(y - 146) / 2;
        if (tfg[y][x] && b < dens && ((x + y) & 1)) tfg[y][x] = (y < 146) ? C0 : M0;
    }
    /* castelo gótico em silhueta à frente da lua */
    static const int tw[12][3] = {{282,6,34},{268,8,62},{296,8,56},{256,6,84},{307,10,78},{244,10,98},{232,6,112},{276,12,96},{290,10,100},{314,6,70},{222,8,120},{262,14,108}};
    for (int k = 0; k < 12; k++) { int x0 = tw[k][0] - tw[k][1] / 2, w = tw[k][1], top = tw[k][2];
        for (int y = top; y < 150; y++) for (int x = x0; x < x0 + w; x++) FG(x, y, x == x0 ? C0 : INK);
        for (int r = 0; r < w * 2; r++) { int ww = w - r / 2; if (ww <= 0) break; for (int x = 0; x < ww; x++) FG(x0 + (w - ww) / 2 + x, top - r, INK); }
        for (int i = 0; i < w / 3; i++) { int wx = x0 + 1 + rndi(w - 2), wy = top + 4 + rndi(150 - top - 8); FG(wx, wy, E2); FG(wx, wy + 1, E1); }
    }
    for (int x = 200; x < 232; x++) for (int y = 130; y < 150; y++) { int ax = (x - 200) % 10 - 5, ay = y - 140;   /* ponte em arcos */
        if (!(ay > 0 && ax * ax + ay * ay * 2 < 30)) FG(x, y, (y == 130) ? C0 : INK); }
    /* ruínas góticas à esquerda: parede partida com janelas em arco e contrafortes */
    for (int x = 0; x < 112; x++) {
        int top = 26 + (int)(14 * sn(x * .09f) + 9 * sn(x * .23f + 1) + 6 * sn(x * .61f)) + (x > 70 ? (x - 70) * 2 : 0);
        if (top < 8) top = 8;
        for (int y = top; y < 170; y++) { int b = BAYER[(y & 3) * 4 + (x & 3)];
            uint8_t c = ((y % 8) == 0 || ((x + (y / 8) * 6) % 12) == 0) ? D1 : (b < 4 ? C0 : D2);
            if (y - top < 2) c = G1;
            FG(x, y, c); }
    }
    static const int win[6][3] = {{14,44,7},{40,38,8},{66,58,7},{14,98,6},{40,96,7},{66,104,6}};   /* janelas: centro x, topo do arco, meia largura */
    for (int k = 0; k < 6; k++) { int cx = win[k][0], ty = win[k][1], r = win[k][2], bot = ty + r + 26;
        for (int y = ty - r - 2; y < bot + 2; y++) for (int x = cx - r - 2; x <= cx + r + 2; x++) {
            int dy = y - ty, dx = x - cx;
            int in = (dy >= 0 && y < bot && iabs2(dx) < r) || (dy < 0 && dx * dx + dy * dy * 2 < r * r);
            int edge = (dy >= -1 && y < bot + 2 && iabs2(dx) < r + 2) || (dy < 0 && dx * dx + dy * dy * 2 < (r + 2) * (r + 2));
            if (in) { if (x >= 0 && x < 320 && y >= 0 && y < 224 && tfg[y][x]) tfg[y][x] = 0; }
            else if (edge && x >= 0 && x < 320 && y >= 0 && y < 224 && tfg[y][x]) tfg[y][x] = (dx < 0) ? G1 : D1;
        }
        for (int y = ty; y < bot; y++) if (y >= 0 && y < 224 && tfg[y][cx] == 0) tfg[y][cx] = D2;   /* mainel */
    }
    static const int bt[4] = {2, 28, 54, 80};   /* contrafortes */
    for (int k = 0; k < 4; k++) for (int y = 60 + k * 6; y < 170; y++) { int w = 5 + (y - 60) / 30;
        for (int x = bt[k]; x < bt[k] + w; x++) FG(x, y, x == bt[k] ? G1 : (x == bt[k] + w - 1 ? D1 : D2)); }
    for (int i = 0; i < 40; i++) { int x = rndi(120), y = 150 + rndi(20); FG(x, y, rndi(2) ? D2 : D1); FG(x + 1, y, D1); }
    /* lago com reflexo da lua */
    for (int y = 168; y < 224; y++) for (int x = 120; x < 320; x++) {
        int b = BAYER[(y & 3) * 4 + (x & 3)];
        uint8_t c = (b < 6 && ((y >> 1) & 1)) ? C0 : M0;
        int dx = x - mx; int wid = 22 - (y - 168) / 4;
        if (iabs2(dx) < wid && (sn(y * 1.3f + x * .2f) > -.2f)) c = (uint8_t)(CY_LAKE + ((y + (dx & 7)) & 3));
        else if ((y & 3) == 0 && b < 3) c = M1;
        FG(x, y, c);
    }
    /* falésia em primeiro plano, à esquerda */
    for (int x = 0; x < 170; x++) {
        int top = x < 110 ? 168 + (int)(3 * sn(x * .2f)) : 168 + (x - 110) * (x - 110) / 22;
        for (int y = top; y < 224; y++) { int b = BAYER[(y & 3) * 4 + (x & 3)];
            uint8_t c = y - top < 1 ? E0 : y - top < 3 ? (b < 8 ? C0 : D1) : (b < 3 ? D2 : (b < 6 ? S0 : INK));
            if (((x * 7 + y * 3) % 23) == 0) c = D2;
            FG(x, y, c); }
    }
    for (int y = 0; y < 224; y++) for (int x = 0; x < 320; x++) if (y >= TSKY_H && !tfg[y][x]) tfg[y][x] = INK;
#undef FG
}
static int iabs2(int v) { return v < 0 ? -v : v; }
static void draw_cape(Img *im, int f) {   /* herói de costas na falésia, capa vermelha ao vento (32x48) */
    static const int sway[3] = {0, 2, 4};
    int s = sway[f];
    for (int y = 14; y < 44; y++) { int w = 6 + (y - 14) / 3 + s * (y - 14) / 30, x0 = 15 - w - (y - 14) * (6 + s) / 30;
        p(x0, y, w + 4, 1, (y + f) % 5 == 0 ? E0 : E1); p(x0, y, 2, 1, E2); }
    for (int k = 0; k < 3; k++) p(2 - s + k * 3, 42 + (k + f) % 3, 2, 2, E1);
    p(14,28,3,14,D1); p(18,28,3,14,D1); p(13,41,4,3,INK); p(18,41,4,3,INK);
    p(13,14,10,15,D2); p(13,14,1,15,C0); p(21,14,2,15,D1); p(13,24,10,2,S0);
    p(23,18,3,8,D2); p(24,25,2,2,S2);
    for (int i = 0; i < 16; i++) { p(25 + i / 4, 26 + i, 1, 1, G2); p(26 + i / 4, 26 + i, 1, 1, BONE); }
    p(23,25,4,1,S1);
    e(18,9,4.5f,5,INK); p(15,5,6,3,D1); p(20,6,2,4,D1); p(14,13,9,2,E1);
}
/* ===== HUD: molduras de metal roxo com rebites e pedras vermelhas (os valores são desenhados pelo jogo) ===== */
static void hgem(Img *im, int cx, int cy) {   /* pedra vermelha engastada */
    p(cx-1,cy-2,3,5,INK); p(cx-2,cy-1,5,3,INK);
    p(cx,cy-1,1,3,E1); p(cx-1,cy,3,1,E1); p(cx,cy,1,1,E2); p(cx,cy-1,1,1,E3);
}
static void hframe(Img *im, int x, int y, int w, int h) {
    p(x,y,w,h,INK); p(x+1,y+1,w-2,h-2,C0);
    p(x+1,y+1,w-2,1,D2); p(x+1,y+h-2,w-2,1,D1);
    for (int i = x + 5; i < x + w - 4; i += 8) { p(i,y+1,1,1,G1); p(i,y+h-2,1,1,D2); }   /* rebites */
    p(x+2,y+2,w-4,h-4,INK); p(x+3,y+3,w-6,h-6,D1);
}
static void htip(Img *im, int x, int cy, int dir) {   /* ponta metálica em seta */
    for (int k = 0; k < 4; k++) { p(x + dir * k, cy - 3 + k, 1, 7 - 2 * k, k == 0 ? INK : C0); }
    p(x + dir * 4, cy, 1, 1, INK);
}
static void draw_hud_left(Img *im) {   /* 130x36 */
    hframe(im, 31, 1, 90, 16); htip(im, 121, 9, 1);           /* vida */
    hframe(im, 31, 18, 32, 16);                                /* nível */
    hframe(im, 62, 20, 56, 12); htip(im, 118, 26, 1);         /* experiência */
    hframe(im, 0, 0, 32, 36);                                  /* avatar */
    hgem(im, 2, 2); hgem(im, 29, 2); hgem(im, 2, 33); hgem(im, 29, 33);
    hgem(im, 33, 3); hgem(im, 33, 32);
    p(4,4,24,28,M0); for (int y = 4; y < 32; y++) for (int x = 4; x < 28; x++) if (((x + y) & 1) && (y < 8 || y > 28)) ip(im, x, y, 1, 1, INK);
}
static void draw_hud_timer(Img *im) {   /* 52x18 */
    hframe(im, 0, 1, 52, 16);
    hgem(im, 2, 3); hgem(im, 49, 3); hgem(im, 2, 14); hgem(im, 49, 14);
    p(23,0,6,3,C0); p(23,0,6,1,INK); hgem(im, 26, 1);
}
static void draw_hud_kills(Img *im) {   /* 64x18 */
    hframe(im, 0, 1, 64, 16);
    hgem(im, 2, 3); hgem(im, 61, 3); hgem(im, 2, 14); hgem(im, 61, 14);
    e(10,8,4,3.5f,BONE); p(7,10,7,3,G2); p(8,11,5,1,BONE);   /* caveira */
    p(8,7,2,2,INK); p(11,7,2,2,INK); p(10,9,1,1,INK); p(8,12,1,1,INK); p(10,12,1,1,INK); p(12,12,1,1,INK);
}
static void draw_hud_boss(Img *im) {   /* 224x32 */
    hframe(im, 20, 13, 200, 16); htip(im, 220, 21, 1); hgem(im, 216, 21);
    hframe(im, 64, 0, 100, 14); htip(im, 63, 7, -1); htip(im, 164, 7, 1);
    hgem(im, 67, 3); hgem(im, 160, 3);
    for (int y = 3; y < 32; y++) { int w = 12 - (y > 17 ? y - 17 : 17 - y); if (w < 0) continue;   /* losango atrás da caveira */
        p(13 - w, y, 2 * w + 1, 1, C0); p(13 - w, y, 1, 1, INK); p(13 + w, y, 1, 1, INK); }
    for (int i = 0; i < 10; i++) { p(5 - i / 2, 12 - i, 3, 2, D2); p(5 - i / 2, 12 - i, 1, 1, G1);   /* chifres */
                                   p(19 + i / 2, 12 - i, 3, 2, D2); p(21 + i / 2, 12 - i, 1, 1, G1); }
    e(13,16,7,7,BONE); e(12,15,5,5,BONE); p(8,20,10,6,G2); p(9,25,8,2,G2);   /* caveira */
    e(10,16,2,2,INK); e(16,16,2,2,INK); p(10,16,1,1,E2); p(16,16,1,1,E2);
    p(12,19,2,2,INK); for (int x = 9; x < 17; x += 2) p(x,23,1,3,INK);
}
/* ===== Caçador jogável: capuz e capa verdes, couro, aljava, besta ===== */
static void draw_cacador_dir(Img *im, int dir, int pose, int f) {
    int Y = ((pose == IDLE && f == 1) || (pose == RUN && (f == 1 || f == 3))) ? 1 : 0;
    int la = (pose == RUN && f == 1), lb = (pose == RUN && f == 3), atk = (pose == ATK0 || pose == ATK1);
    if (dir == SIDE) {
        static const int rs[4] = {0, 2, 0, -2};
        int s = pose == RUN ? rs[f] : pose == DASH ? 3 : 0, cl = (pose == RUN || pose == DASH) ? 2 : 0;
        p(9 - cl, 13 + Y, 4 + cl, 13, V0); p(9 - cl, 13 + Y, 1, 13, V1);                    /* capa */
        p(10, 9 + Y, 3, 11, S1); p(10, 7 + Y, 1, 3, BONE); p(12, 7 + Y, 1, 3, BONE);          /* aljava */
        p(13 - s, 25, 3, 4, D2); p(12 - s, 28, 5, 3, S0);                                     /* perna de trás */
        p(17 + s, 25, 3, 4, D2); p(17 + s, 28, 5, 3, S0); p(17 + s, 28, 5, 1, S1);           /* perna da frente */
        p(12, 16 + Y, 9, 10, S0); p(12, 16 + Y, 2, 10, S1); p(12, 22 + Y, 9, 2, S1); p(16, 22 + Y, 2, 2, E3);
        p(14, 16 + Y, 5, 3, V0);
        e(16, 9 + Y, 6.5f, 6.5f, V0); e(14, 6 + Y, 3.5f, 2.5f, V1); p(9, 5 + Y, 4, 3, V0); p(8 - cl / 2, 4 + Y, 2, 2, V0);
        e(19, 11 + Y, 3.5f, 3.5f, INK); p(19, 10 + Y, 1, 1, BONE); p(21, 10 + Y, 1, 1, BONE); p(18, 13 + Y, 5, 2, S2); p(18, 14 + Y, 5, 1, S1);
        if (!atk) { p(18, 18 + Y, 3, 5, S0); p(19, 22 + Y, 8, 2, S1); p(19, 22 + Y, 8, 1, E1); p(26, 18 + Y, 1, 10, G1); p(25, 18 + Y, 1, 10, BONE); p(20, 22 + Y, 2, 2, S2); }
        else { p(18, 15, 8, 3, S0); p(19, 14, 9, 2, S1); p(19, 14, 9, 1, E1); p(27, 9, 1, 11, G1); p(26, 9, 1, 11, BONE); p(22, 15, 2, 2, S2);
               if (pose == ATK1) { p(28, 14, 4, 1, E4); p(30, 13, 2, 3, E3); } else p(28, 14, 3, 1, BONE); }
    } else if (dir == DOWN) {
        p(7, 14 + Y, 5, 13, V0); p(20, 14 + Y, 5, 13, V0); p(7, 14 + Y, 1, 13, V1);           /* capa aos lados */
        p(12, 25, 3, 4 - la, D2); p(17, 25, 3, 4 - lb, D2); p(11, 28 - la, 5, 3, S0); p(16, 28 - lb, 5, 3, S0);
        p(11, 16 + Y, 10, 10, S0); p(11, 16 + Y, 2, 10, S1); p(11, 22 + Y, 10, 2, S1); p(15, 22 + Y, 2, 2, E3);
        p(12, 16 + Y, 8, 2, V0);
        e(16, 9 + Y, 7, 7, V0); e(14, 5 + Y, 3.5f, 2, V1); p(15, 1 + Y, 2, 3, V0);           /* capuz */
        e(16, 11 + Y, 4.5f, 3.5f, INK); p(14, 11 + Y, 1, 1, BONE); p(17, 11 + Y, 1, 1, BONE); p(13, 13 + Y, 6, 2, S2);
        if (!atk) { p(9, 20 + Y, 14, 2, S1); p(9, 20 + Y, 14, 1, E1); p(8, 17 + Y, 1, 7, G1); p(23, 17 + Y, 1, 7, G1); p(10, 21 + Y, 2, 2, S2); p(20, 21 + Y, 2, 2, S2); }
        else { p(9, 18, 14, 1, G1); p(9, 19, 14, 1, BONE); p(15, 17, 2, 7, S1); p(15, 24, 2, 2, pose == ATK1 ? E4 : BONE); p(12, 20, 2, 2, S2); p(18, 20, 2, 2, S2); }
    } else {
        p(12, 25, 3, 4 - la, D2); p(17, 25, 3, 4 - lb, D2); p(11, 28 - la, 5, 3, S0); p(16, 28 - lb, 5, 3, S0);
        p(8, 13 + Y, 16, 14, V0); p(8, 13 + Y, 1, 14, V1); p(23, 13 + Y, 1, 14, D1);          /* capa nas costas */
        for (int k = 0; k < 3; k++) p(11 + k * 4, 18 + Y, 1, 8, D1);
        p(18, 9 + Y, 3, 11, S1); p(18, 7 + Y, 1, 3, BONE); p(20, 7 + Y, 1, 3, BONE);         /* aljava */
        e(16, 9 + Y, 7, 7, V0); e(14, 6 + Y, 3.5f, 2.5f, V1); p(15, 1 + Y, 2, 3, V0);
        if (atk) { p(9, 4, 14, 1, G1); p(15, 2, 2, 7, S1); p(15, 0, 2, 2, pose == ATK1 ? E4 : BONE); }
    }
}
/* ===== Padre jogável: túnica clara com estola dourada, cabelo grisalho, cajado com símbolo sagrado ===== */
static void draw_padre_dir(Img *im, int dir, int pose, int f) {
    int Y = ((pose == IDLE && f == 1) || (pose == RUN && (f == 1 || f == 3))) ? 1 : 0;
    int atk = (pose == ATK0 || pose == ATK1), sw = (pose == RUN) ? ((f & 1) ? 1 : -1) * (f == 0 || f == 2 ? 0 : 1) : 0;
    int pe = (pose == RUN) ? (f == 1 ? 2 : f == 3 ? -2 : 0) : 0;
    if (dir == SIDE) {
        int lean = pose == DASH ? 1 : 0;
        p(13 - pe, 28, 4, 3, S0); p(17 + pe, 28, 4, 3, S0);                                   /* pés */
        for (int y = 14; y <= 28; y++) { int w = 5 + (y - 14) / 3, xs = (y > 24) ? sw : 0; p(16 - w + xs + lean, y + (y < 26 ? Y : 0), 2 * w, 1, BONE);
            p(16 - w + xs + lean, y + (y < 26 ? Y : 0), 2, 1, G2); p(16 + w - 2 + xs + lean, y + (y < 26 ? Y : 0), 2, 1, G2); }
        p(10 + sw, 28, 13, 1, E3); p(17 + lean, 15 + Y, 2, 13, E3); p(17 + lean, 15 + Y, 2, 1, E4); p(14 + lean, 21 + Y, 6, 1, S1);
        p(13 + lean, 14 + Y, 8, 3, G2);
        e(17 + lean, 9 + Y, 4.5f, 5, S2); p(13 + lean, 5 + Y, 4, 7, G1); p(13 + lean, 5 + Y, 8, 2, G1);
        p(18 + lean, 9 + Y, 1, 1, INK); p(20 + lean, 9 + Y, 1, 1, INK); p(19 + lean, 12 + Y, 2, 1, S1);
        if (!atk) { p(24, 3 + Y, 1, 27 - Y, S1); p(23, 2 + Y, 3, 1, E3); p(24, 0 + Y, 1, 5, E3); p(19, 16 + Y, 5, 3, BONE); p(22, 17 + Y, 3, 2, S2); }
        else if (pose == ATK0) { p(23, 0, 1, 24, S1); p(22, 0, 3, 1, E3); p(23, 0, 1, 3, E4); p(19, 12, 5, 4, BONE); p(21, 11, 3, 2, S2);
            for (int y = 0; y < 7; y++) for (int x = 19; x < 28; x++) { int dx = x - 23, dy = y - 1; if (dx * dx + dy * dy * 2 < 20 && ((x + y) & 1)) ip(im, x, y, 1, 1, E4); } }
        else { p(20, 15, 11, 1, S1); p(29, 13, 3, 5, E3); p(30, 14, 1, 3, E4); p(19, 15, 4, 3, BONE); p(21, 15, 2, 2, S2); p(31, 15, 1, 1, BONE); }
    } else if (dir == DOWN) {
        p(11 - pe / 2, 28, 4, 3, S0); p(17 + pe / 2, 28, 4, 3, S0);
        for (int y = 14; y <= 28; y++) { int w = 6 + (y - 14) / 3, xs = (y > 24) ? sw : 0; p(16 - w + xs, y + (y < 26 ? Y : 0), 2 * w, 1, BONE);
            p(16 - w + xs, y + (y < 26 ? Y : 0), 2, 1, G2); p(16 + w - 2 + xs, y + (y < 26 ? Y : 0), 2, 1, G2); }
        p(9 + sw, 28, 15, 1, E3); p(13, 15 + Y, 2, 13, E3); p(18, 15 + Y, 2, 13, E3); p(15, 17 + Y, 3, 1, E4); p(16, 16 + Y, 1, 3, E4);   /* estola e cruz */
        p(12, 14 + Y, 9, 2, G2);
        e(16, 9 + Y, 5, 5.5f, S2); p(11, 5 + Y, 3, 7, G1); p(19, 5 + Y, 3, 7, G1); p(12, 4 + Y, 9, 2, G1);
        p(14, 9 + Y, 1, 1, INK); p(18, 9 + Y, 1, 1, INK); p(15, 12 + Y, 3, 1, S1);
        if (!atk) { p(24, 4 + Y, 1, 26 - Y, S1); p(23, 3 + Y, 3, 1, E3); p(24, 1 + Y, 1, 5, E3); p(22, 18 + Y, 3, 2, S2); p(8, 18 + Y, 3, 2, S2); }
        else { p(24, 0, 1, 22, S1); p(23, 0, 3, 1, E3); p(24, 0, 1, 3, E4); p(22, 10, 3, 2, S2); p(8, 10, 3, 2, S2);
            for (int y = 0; y < 7; y++) for (int x = 19; x < 30; x++) { int dx = x - 24, dy = y - 1; if (dx * dx + dy * dy * 2 < 22 && ((x + y) & 1)) ip(im, x, y, 1, 1, pose == ATK1 ? BONE : E4); } }
    } else {
        p(11 - pe / 2, 28, 4, 3, S0); p(17 + pe / 2, 28, 4, 3, S0);
        for (int y = 14; y <= 28; y++) { int w = 6 + (y - 14) / 3, xs = (y > 24) ? sw : 0; p(16 - w + xs, y + (y < 26 ? Y : 0), 2 * w, 1, BONE);
            p(16 - w + xs, y + (y < 26 ? Y : 0), 2, 1, G2); p(16 + w - 2 + xs, y + (y < 26 ? Y : 0), 2, 1, G2); }
        p(9 + sw, 28, 15, 1, E3); p(15, 15 + Y, 2, 12, G2);
        e(16, 9 + Y, 5, 5.5f, G1); e(15, 7 + Y, 3, 2.5f, G2); p(12, 14 + Y, 9, 2, G2);
        if (!atk) { p(7, 4 + Y, 1, 26 - Y, S1); p(6, 3 + Y, 3, 1, E3); p(7, 1 + Y, 1, 5, E3); }
        else { p(7, 0, 1, 22, S1); p(6, 0, 3, 1, E3); p(7, 0, 1, 3, E4); }
    }
}
static void draw_avatar2(Img *im, int padre) {   /* retratos 24x24 do caçador e do padre */
    if (!padre) {
        p(2, 20, 20, 4, S0); p(2, 20, 20, 1, S1); p(3, 14, 4, 10, V0); p(17, 14, 4, 10, V0); p(18, 4, 3, 16, S1); p(18, 2, 1, 3, BONE); p(20, 2, 1, 3, BONE);
        e(12, 10, 9, 9, V0); e(9, 6, 4, 3, V1); p(11, 0, 2, 3, V0);
        e(12, 12, 5.5f, 4.5f, INK); p(9, 11, 2, 1, BONE); p(14, 11, 2, 1, BONE); p(8, 15, 8, 3, S2); p(8, 17, 8, 1, S1);
    } else {
        p(2, 19, 20, 5, BONE); p(2, 19, 20, 1, G2); p(8, 19, 2, 5, E3); p(14, 19, 2, 5, E3); p(11, 21, 2, 3, E4);
        e(12, 11, 6.5f, 7.5f, S2); p(5, 4, 4, 11, G1); p(15, 4, 4, 11, G1); p(6, 3, 12, 3, G1);
        p(9, 10, 2, 1, INK); p(14, 10, 2, 1, INK); p(10, 15, 4, 1, S1); p(8, 9, 3, 1, G1); p(13, 9, 3, 1, G1);
        p(20, 1, 1, 20, S1); p(19, 1, 3, 1, E3); p(20, 0, 1, 3, E4);
    }
}
static void draw_cruz(Img *im, int f) {   /* 12x12 */
    if (!f) { p(5, 1, 2, 10, E3); p(2, 4, 8, 2, E3); p(5, 1, 1, 10, E4); p(2, 4, 8, 1, E4); p(5, 4, 2, 2, BONE); }
    else { for (int i = 0; i < 9; i++) { p(1 + i, 1 + i, 2, 2, E3); p(9 - i, 1 + i, 2, 2, E3); } p(5, 5, 2, 2, BONE); }
}
static void draw_frasco(Img *im) { e(4, 5, 3, 3, M2); e(4, 5, 2, 2, M3); p(3, 0, 2, 3, G2); p(3, 4, 1, 1, BONE); }   /* 8x8 */
static void draw_armadilha(Img *im, int f) {   /* 12x8: dentes de ferro */
    e(6, 5, 5.5f, 2.5f, D2); e(6, 5, 3.5f, 1.5f, D1);
    if (!f) for (int k = 0; k < 5; k++) { p(1 + k * 2, 2, 1, 2, G2); p(2 + k * 2, 6, 1, 1, G2); }
    else { p(1, 3, 10, 2, G1); for (int k = 0; k < 5; k++) p(1 + k * 2, 2, 1, 1, G2); }
}
static void draw_falcao(Img *im, int f) {   /* 16x12, virado para a direita */
    e(8, 7, 4, 2.5f, S1); e(12, 6, 2.5f, 2, S1); p(14, 6, 2, 1, E3); p(12, 5, 1, 1, INK); p(3, 7, 2, 2, S0);
    if (!f) { for (int i = 0; i < 6; i++) p(5 + i, 1 + i / 2, 2, 1, S0); p(5, 1, 3, 1, G2); }
    else { for (int i = 0; i < 6; i++) p(5 + i, 11 - i / 2, 2, 1, S0); p(5, 11, 3, 1, G2); }
}
static void draw_avatar(Img *im, int mago) {   /* 24x24 */
    if (!mago) {
        p(2,21,20,3,D2); p(2,21,20,1,G1); p(19,1,2,6,M2);
        e(12,11,8.5f,8.5f,G1); e(10,7,4,2.5f,G2);
        p(3,8,18,3,E2); p(3,10,18,1,E1); p(11,8,2,2,E4); p(20,10,3,2,E2);
        p(6,12,12,4,S2); p(6,15,12,1,S1); p(8,13,3,1,M3); p(13,13,3,1,M3); p(9,13,1,1,INK); p(13,13,1,1,INK);
        p(5,16,14,4,D2); p(6,16,12,1,G1);
    } else {
        p(2,20,20,4,C0); p(2,20,20,1,C1); p(11,20,2,4,E3);
        p(3,10,4,11,E2); p(17,10,4,11,E2); p(3,10,1,11,E1); p(20,10,1,11,E1);
        p(7,10,10,7,S2); p(7,16,10,1,S1); p(8,12,3,1,M3); p(13,12,3,1,M3); p(9,12,1,1,INK); p(14,12,1,1,INK);
        p(7,10,10,2,E2); p(8,10,3,1,E3);
        for (int y = 0; y < 8; y++) { int w = 2 + y * 3 / 2, cx = 15 - y / 2; p(cx - w / 2, y, w, 1, C0); p(cx - w / 2, y, 1, 1, C1); }
        e(12,9,11,2,C0); p(2,8,20,1,C1); p(6,6,12,2,S1); p(11,6,2,2,M3);
    }
}
/* ===== Cutscene: Aren (capuz e capa vermelhos, armadura escura, olhos a brilhar) e o Carrasco das Cinzas ===== */
static void aren_capuz(Img *im, int cx, int cy) {
    e(cx, cy, 8, 9, E1); e(cx - 2, cy - 3, 5, 5, E2); p(cx - 7, cy + 5, 3, 4, E1);
    e(cx + 2, cy + 2, 4.5f, 5, INK);
    p(cx, cy + 1, 2, 1, M3); p(cx + 3, cy + 1, 2, 1, M3); p(cx + 1, cy + 1, 1, 1, BONE); p(cx + 4, cy + 1, 1, 1, BONE);
}
static void aren_espada(Img *im, int x0, int y0, int x1, int y1, int chamas) {
    int n = (x1 - x0) > (y1 - y0) ? (x1 - x0) : (y1 - y0);
    for (int i = 0; i <= n; i++) { int x = x0 + (x1 - x0) * i / n, y = y0 + (y1 - y0) * i / n;
        if (chamas) { p(x - 1, y - 1, 3, 3, i & 1 ? E2 : E3); p(x, y, 1, 1, E4); }
        else { p(x, y, 2, 1, G2); p(x, y, 1, 1, BONE); } }
    p(x0 - 3, y0 - 1, 7, 2, S1); p(x0 - 1, y0 - 3, 2, 2, S0);
}
static void draw_aren(Img *im, int pose) {
    if (pose == AREN_DEITADO) {   /* 72x28, de bruços, capa espalhada, mão estendida */
        p(4,18,20,6,D1); p(4,18,20,1,G1); p(2,22,6,5,INK);
        e(30,15,22,8,E1); e(26,12,14,4,E2); for (int i = 0; i < 6; i++) p(10 + i * 7, 20 + (i & 1), 3, 2, E0);
        p(44,14,14,8,D2); p(44,14,14,1,G1);
        e(58,14,7,7,E1); e(56,12,4,3,E2); e(61,16,3,3,INK);
        p(60,20,10,3,D2); p(68,20,3,3,S2);
        for (int i = 0; i < 26; i++) { p(40 + i, 25 + (i > 13), 1, 1, G2); }
        p(38,24,4,2,S1); return;
    }
    if (pose == AREN_JOELHOS) {   /* 48x64, de joelhos, a apoiar-se na espada */
        for (int y = 26; y < 60; y++) { int w = 6 + (y - 26) / 3; p(20 - w, y, w + 4, 1, (y % 5) ? E1 : E0); p(20 - w, y, 1, 1, E2); }
        p(18,46,12,6,D1); p(26,50,6,10,D1); p(16,52,6,8,D1); p(15,59,8,3,INK); p(25,59,9,3,INK);
        p(18,30,14,18,D2); p(18,30,2,18,G1); p(18,44,14,3,S0); p(24,44,2,3,E3);
        p(30,32,5,12,D2); p(33,43,4,4,D1);
        aren_espada(im, 35, 44, 38, 62, 0);
        aren_capuz(im, 26, 24); return;
    }
    if (pose == AREN_PE || pose == AREN_CHAMAS || pose == AREN_CHAMAS2) {   /* 48x64, de pé, espada para baixo */
        int ch = pose != AREN_PE;
        for (int y = 16; y < 60; y++) { int w = 6 + (y - 16) / 4; p(19 - w, y, w + 5, 1, (y % 6) ? E1 : E0); p(19 - w, y, 1, 1, E2); }
        p(19,44,6,15,D1); p(27,44,6,15,D1); p(19,44,1,15,G1); p(18,58,8,4,INK); p(27,58,9,4,INK);
        p(18,24,15,20,D2); p(18,24,2,20,G1); p(21,28,8,1,G1); p(18,40,15,3,S0); p(24,40,3,3,E3);
        p(15,26,4,14,D1); p(31,26,5,14,D2); p(32,38,5,4,D1);
        aren_espada(im, 34, 41, 45, 63, ch);
        if (ch) for (int i = 0; i < 8; i++) { int k = (pose == AREN_CHAMAS2) ? i + 3 : i; p(36 + (k * 5) % 11, 40 + (k * 7) % 20, 2, 2, (k & 1) ? E4 : E3); }
        aren_capuz(im, 25, 16); return;
    }
    if (pose == AREN_CORRE) {   /* 48x64, a carregar para a direita, capa ao vento */
        for (int y = 14; y < 44; y++) { int w = 10 + (y - 14) / 2; p(18 - w, y, w + 4, 1, (y % 5) ? E1 : E0); p(18 - w, y, 1, 1, E2); }
        p(16,40,7,14,D1); p(14,52,8,4,D1); p(12,55,8,3,INK); p(26,42,6,12,D1); p(30,52,8,4,D1); p(31,56,9,3,INK);
        p(19,22,15,20,D2); p(19,22,2,20,G1); p(19,38,15,3,S0);
        p(14,26,6,4,D2); aren_espada(im, 14, 28, 2, 40, 1);
        aren_capuz(im, 29, 15); return;
    }
    /* AREN_ATAQUE: 64x64, golpe com a espada estendida em chamas */
    for (int y = 16; y < 50; y++) { int w = 9 + (y - 16) / 3; p(16 - w, y, w + 4, 1, (y % 5) ? E1 : E0); p(16 - w, y, 1, 1, E2); }
    p(14,44,7,14,D1); p(10,56,9,4,INK); p(26,44,7,12,D1); p(30,54,10,4,INK);
    p(17,24,15,20,D2); p(17,24,2,20,G1); p(17,40,15,3,S0);
    p(30,27,8,5,D2); p(37,27,4,4,D1);
    aren_espada(im, 40, 29, 63, 26, 1);
    aren_capuz(im, 25, 16);
}
static void draw_carrasco(Img *im, int pose) {   /* 96x96, virado para a esquerda */
    for (int y = 24; y < 92; y++) { int w = 16 + (y - 24) / 3;   /* capa esfarrapada */
        int x0 = 52 - w / 2 + 6, x1 = 52 + w / 2 + 14;
        if (y > 82 && ((y + 0) % 4) < 2) x0 += 4;
        p(x0, y, x1 - x0, 1, (y % 7) ? E0 : INK); if ((y % 9) == 0) p(x1 - 3, y, 3, 1, E1); }
    p(36,64,12,26,D1); p(58,64,12,26,D1); p(36,64,2,26,D2); p(58,64,2,26,D2);   /* pernas */
    p(32,88,18,6,INK); p(56,88,18,6,INK); p(34,74,10,3,G1); p(60,74,10,3,G1);
    for (int y = 30; y < 66; y++) { int w = 14 + (y < 44 ? (y - 30) / 2 : 7 - (y - 44) / 4);   /* tronco */
        p(52 - w, y, w * 2, 1, D1); p(52 - w, y, 2, 1, G1); }
    for (int r = 0; r < 4; r++) p(40, 36 + r * 7, 24, 1, D2);
    p(38,58,28,4,S0); e(52,60,3.5f,3,BONE); p(51,60,1,1,INK); p(53,60,1,1,INK);   /* cinto com caveira */
    e(36,32,9,7,D2); e(68,32,9,7,D2); e(35,30,6,4,G1); e(67,30,6,4,G1);   /* ombreiras com espigões */
    for (int k = 0; k < 3; k++) { p(30 + k * 5, 22 - k, 2, 6, G1); p(64 + k * 5, 20 + k, 2, 6, G1); }
    e(52,18,10,11,D1); e(50,15,6,5,D2);   /* elmo com chifres */
    for (int i = 0; i < 9; i++) { p(42 - i / 2, 12 - i, 3, 2, G1); p(60 + i / 2, 12 - i, 3, 2, G1); }
    p(44,20,16,3,INK); p(45,20,4,2,RED); p(54,20,4,2,RED); p(46,20,2,1,E4); p(55,20,2,1,E4);
    int hx, hy;   /* mão e cutelo gigante */
    if (pose == 0) { hx = 26; hy = 52; p(26,40,10,14,D1); }
    else if (pose == 1) { hx = 30; hy = 14; p(30,16,8,24,D1); }
    else { hx = 18; hy = 46; p(18,42,22,8,D1); }
    p(hx - 2, hy - 2, 7, 7, D2);
    int bx0 = hx, by0 = hy, bx1, by1;
    if (pose == 0) { bx1 = 4; by1 = 92; } else if (pose == 1) { bx1 = 14; by1 = 0; } else { bx1 = 0; by1 = 60; }
    int n = iabs2(bx1 - bx0) > iabs2(by1 - by0) ? iabs2(bx1 - bx0) : iabs2(by1 - by0);
    for (int i = 0; i <= n; i++) { int x = bx0 + (bx1 - bx0) * i / n, y = by0 + (by1 - by0) * i / n;
        if (i < 6) { p(x, y, 3, 3, S0); continue; }
        p(x - 5, y - 5, 10, 10, G1); p(x - 5, y - 5, 2, 2, G2); if ((i % 9) == 4) p(x + 3, y - 4, 2, 3, INK); if ((i % 13) == 2) p(x - 2, y, 2, 2, E1); }
}
/* ===== Versões grandes das personagens do jogo para a cutscene =====
   Partem dos mesmos desenhos do jogo (ninja 32x32, Gorgomusgo 64x64): ampliados ao dobro com Scale2x
   (arredonda as diagonais sem inventar cores), com sombreado automático (luz em cima, sombra em baixo e à
   direita) e contorno fino de 1 px. */
static void rot90(const uint8_t *src, uint8_t *dst, int n);
static uint8_t bigA[128 * 128], bigB[128 * 128];
static void scale2x(const uint8_t *s, int w, int h, uint8_t *d) {
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        uint8_t P = s[y * w + x];
        uint8_t A = y > 0 ? s[(y - 1) * w + x] : P, B = x < w - 1 ? s[y * w + x + 1] : P;
        uint8_t C = x > 0 ? s[y * w + x - 1] : P, D = y < h - 1 ? s[(y + 1) * w + x] : P;
        uint8_t e0 = P, e1 = P, e2 = P, e3 = P;
        if (C == A && C != D && A != B) e0 = A;
        if (A == B && A != C && B != D) e1 = B;
        if (D == C && D != B && C != A) e2 = C;
        if (B == D && B != A && D != C) e3 = D;
        d[(2 * y) * 2 * w + 2 * x] = e0; d[(2 * y) * 2 * w + 2 * x + 1] = e1;
        d[(2 * y + 1) * 2 * w + 2 * x] = e2; d[(2 * y + 1) * 2 * w + 2 * x + 1] = e3;
    }
}
static void auto_shade(uint8_t *px, int w, int h) {
    for (int i = 0; i < w * h; i++) bigB[i] = px[i];
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) {
        uint8_t c = bigB[y * w + x]; if (!c || c == INK || c >= NCOL) continue;
        uint8_t up = y > 0 ? bigB[(y - 1) * w + x] : 0, dn = y < h - 1 ? bigB[(y + 1) * w + x] : 0, rt = x < w - 1 ? bigB[y * w + x + 1] : 0;
        if (!up || up == INK) px[y * w + x] = LIGHTER[c];
        else if (!dn || dn == INK || !rt || rt == INK) px[y * w + x] = DARKER[c];
    }
}
static void blade_fire(uint8_t *px, int w, int x0, int y0, int x1, int y1, int seed0) {   /* lâmina em chamas */
    seed = (uint32_t)seed0;
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) {
        uint8_t c = px[y * w + x];
        if (c == G2) px[y * w + x] = E3; else if (c == BONE) px[y * w + x] = E4;
    }
    for (int k = 0; k < 10; k++) { int x = x0 - 2 + rndi(x1 - x0 + 5), y = y0 - 2 + rndi(y1 - y0 + 5);
        if (x >= 0 && y >= 0 && x < w && !px[y * w + x]) px[y * w + x] = (k & 1) ? E3 : E2; }
}
static void add_big(int id, uint8_t *px, int w, int h) {   /* âncora: centro dos pés (linha mais baixa ocupada) */
    int bot = 0, x0 = w, x1 = 0;
    for (int y = 0; y < h; y++) for (int x = 0; x < w; x++) if (px[y * w + x]) { bot = y; if (x < x0) x0 = x; if (x > x1) x1 = x; }
    for (int i = 0; i < w * h; i++) work[i] = px[i];
    add_sprite(id, work, w, h, (x0 + x1) / 2, bot);
}
/* Detalhes desenhados à mão por cima do ninja ampliado (vista de lado, coordenadas a 64x64).
   Só pinta sobre píxeis do próprio desenho, para nunca sair da silhueta. */
static void dpx(uint8_t *px, int x, int y, uint8_t c) { if (x >= 0 && y >= 0 && x < 64 && y < 64 && px[y * 64 + x] && px[y * 64 + x] != INK) px[y * 64 + x] = c; }
static void ninja_detail(uint8_t *px, int pose, int Y) {
    int o = 2 * Y;
    for (int a = 0; a < 40; a++) { float t = 3.4f + a * .045f;   /* dobras do capuz */
        dpx(px, 32 + (int)(__builtin_cosf(t) * 12), 18 + o + (int)(__builtin_sinf(t) * 11), D2);
        if (a % 2 == 0) dpx(px, 32 + (int)(__builtin_cosf(t + .3f) * 7), 15 + o + (int)(__builtin_sinf(t + .3f) * 7), D2); }
    for (int x = 26; x < 33; x++) dpx(px, x, 7 + o, G2);   /* reflexo do capuz */
    dpx(px, 25, 8 + o, G2); dpx(px, 24, 9 + o, BONE);
    for (int x = 18; x < 48; x++) { dpx(px, x, 16 + o, E1); if (x % 4 == 0) dpx(px, x, 14 + o, E3); }   /* faixa */
    for (int y = 13; y < 21; y++) { dpx(px, 17, y + o, E1); dpx(px, 18, y + o, (y & 1) ? E2 : E1); }   /* nó */
    dpx(px, 44, 22 + o, BONE); dpx(px, 45, 23 + o, M2);   /* brilho no olho */
    for (int x = 37; x < 47; x += 3) dpx(px, x, 29 + o, G1);   /* costuras da máscara */
    for (int y = 34; y < 44; y++) dpx(px, 27 + (y - 34) / 2, y + o, D1);   /* pregas da roupa */
    for (int y = 36; y < 43; y++) dpx(px, 36 + (y - 36) / 3, y + o, D1);
    for (int x = 24; x < 42; x++) { if (x % 3 == 0) dpx(px, x, 45 + o, E3); dpx(px, x, 46 + o, E1); }   /* cinto */
    if (pose != ATK0 && pose != ATK1) for (int y = 18; y < 26; y++) dpx(px, 22 + (y & 1), y, (y % 3) ? M3 : M1);   /* cabo da katana */
    for (int y = 50; y < 58; y += 3) for (int x = 26; x < 42; x++) dpx(px, x, y, D1);   /* ligaduras das pernas */
}
/* Caçador (32x32, de lado): capuz e capa verdes, roupa de couro, aljava, besta. pose 0 parado, 1 a apontar */
static void draw_cacador(Img *im, int pose) {
    p(9,13,4,13,V0); p(9,13,1,13,V1);                                       /* capa atrás */
    p(10,9,3,11,S1); p(10,7,1,3,BONE); p(12,7,1,3,BONE); p(11,8,1,2,G2);    /* aljava com setas */
    p(13,25,3,4,D2); p(17,25,3,4,D2); p(12,28,5,3,S0); p(17,28,5,3,S0); p(17,28,5,1,S1);   /* pernas e botas */
    p(12,16,9,10,S0); p(12,16,2,10,S1); p(12,22,9,2,S1); p(16,22,2,2,E3);   /* túnica de couro e cinto */
    p(14,16,5,3,V0);                                                        /* gola da capa */
    e(16,9,6.5f,6.5f,V0); e(14,6,3.5f,2.5f,V1); p(9,5,4,3,V0); p(8,4,2,2,V0);   /* capuz pontiagudo */
    e(19,11,3.5f,3.5f,INK); p(19,10,1,1,BONE); p(21,10,1,1,BONE); p(18,13,5,2,S2); p(18,14,5,1,S1);   /* rosto na sombra */
    if (pose == 0) { p(18,18,3,5,S0); p(19,22,8,2,S1); p(19,22,8,1,E1); p(26,18,1,10,G1); p(25,18,1,10,BONE); p(20,22,2,2,S2); }
    else { p(18,15,8,3,S0); p(19,14,9,2,S1); p(19,14,9,1,E1); p(27,9,1,11,G1); p(26,9,1,11,BONE); p(28,14,3,1,BONE); p(31,14,1,1,G2); p(22,15,2,2,S2); }
}
/* Padre (32x32, de lado): túnica clara com estola dourada, cabelo grisalho, cajado com símbolo sagrado.
   pose 0 parado, 1 a orar com o cajado erguido e luz */
static void draw_padre(Img *im, int pose) {
    for (int y = 14; y <= 29; y++) { int w = 5 + (y - 14) / 3; p(16 - w, y, 2 * w, 1, BONE); p(16 - w, y, 2, 1, G2); p(16 + w - 2, y, 2, 1, G2); }
    p(10,29,13,1,E3); p(17,15,2,14,E3); p(17,15,2,1,E4); p(14,21,6,1,S1);   /* bainha e estola dourada, cordão */
    p(13,14,8,3,G2);                                                        /* gola */
    e(17,9,4.5f,5,S2); p(13,5,4,7,G1); p(13,5,8,2,G1); p(18,9,1,1,INK); p(20,9,1,1,INK); p(19,12,2,1,S1);   /* cabeça */
    if (pose == 0) {
        p(24,3,1,27,S1); p(23,2,3,1,E3); p(24,0,1,5,E3); p(24,0,1,1,E4);    /* cajado */
        p(19,16,5,3,BONE); p(22,17,3,2,S2);
    } else {
        p(23,0,1,24,S1); p(22,0,3,1,E3); p(23,0,1,3,E4);
        p(19,12,5,4,BONE); p(21,11,3,2,S2);
        for (int y = 0; y < 9; y++) for (int x = 18; x < 30; x++) { int dx = x - 23, dy = y - 1; if (dx * dx + dy * dy * 2 < 30 && ((x + y) & 1)) ip(im, x, y, 1, 1, E4); }
    }
}
/* Rei das Cinzas, o boss final (64x64, de frente): coroa dourada com gema, chifres curvos em faixas,
   rosto escuro com olhos roxos, barba branca, capa escura de pelo, gemas no peito, tabardo com debrum azul,
   braçadeiras douradas e garras. f: 0 e 1 (olhos e gema a pulsar, capa a mexer) */
static void rei_chifre(Img *im, int lado, int f) {
    for (int i = 0; i < 20; i++) {   /* sai para o lado e depois sobe a curvar, cada vez mais fino */
        float a = i / 19.f;
        int off = 6 + (int)(11 * __builtin_sinf(a * 1.45f)), y = 12 - (int)(12 * a * a);
        int x = 32 + lado * off, w = 4 - i / 6; uint8_t c = ((i + f) % 4 < 2) ? E3 : E2; if (i > 16) c = E4;
        int x0 = lado < 0 ? x - w + 1 : x;
        p(x0, y, w, 2, c); p(x0, y + 1, w, 1, (c == E3) ? E2 : E1);
    }
}
static void draw_rei(Img *im, int f) {
    for (int y = 18; y < 62; y++) {   /* capa de pelo escura, aberta em baixo */
        int w = 11 + (y - 18) * 11 / 20; if (w > 29) w = 29;
        int bico = ((y + f) % 5 == 0) ? 1 : 0;
        p(32 - w - bico, y, 2 * w + 2 * bico, 1, M0); p(32 - w - bico, y, 2, 1, C0); p(32 + w + bico - 2, y, 2, 1, C0);
    }
    for (int k = 0; k < 8; k++) { int x = 6 + k * 7 + f; p(x, 59 + (k & 1), 3, 3, M0); }   /* franjas da capa */
    for (int k = 0; k < 7; k++) { p(19 + k * 4, 17 + (k & 1), 3, 3, M0); p(20 + k * 4, 16, 1, 1, C0); }   /* gola de pelo */
    p(22, 46, 8, 12, D1); p(34, 46, 8, 12, D1); p(22, 46, 1, 12, M1); p(41, 46, 1, 12, M1);   /* pernas */
    p(20, 57, 11, 4, M0); p(33, 57, 11, 4, M0);
    for (int k = 0; k < 3; k++) { p(20 + k * 4, 61, 2, 2, BONE); p(34 + k * 4, 61, 2, 2, BONE); }   /* garras dos pés */
    p(21, 40, 22, 8, M0); p(21, 46, 22, 1, M3); p(21, 40, 22, 1, M1);   /* saia escura com debrum */
    p(29, 40, 7, 18, D1); p(29, 40, 1, 18, M3); p(35, 40, 1, 18, M3); p(30, 57, 5, 1, M3); p(31, 58, 3, 1, M3); p(32, 59, 1, 1, M3);   /* tabardo */
    p(31, 46, 3, 1, E3); p(30, 47, 1, 2, E3); p(34, 47, 1, 2, E3); p(32, 49, 1, 3, E3); p(31, 52, 3, 1, E3);   /* símbolo dourado */
    p(23, 20, 18, 20, S0); p(23, 20, 2, 20, S1); p(39, 20, 2, 20, E0);   /* tronco */
    for (int k = 0; k < 3; k++) p(26, 31 + k * 3, 12, 1, E0);
    p(22, 38, 20, 3, E3); p(22, 38, 20, 1, E4); p(22, 40, 20, 1, E1);   /* cinto dourado */
    p(16, 22, 6, 14, S0); p(42, 22, 6, 14, S0); p(16, 22, 1, 14, S1); p(47, 22, 1, 14, E0);   /* braços */
    p(15, 34, 8, 5, E3); p(41, 34, 8, 5, E3); p(15, 34, 8, 1, E4); p(41, 34, 8, 1, E4);   /* braçadeiras */
    p(16, 39, 6, 6, S0); p(42, 39, 6, 6, S0);   /* mãos com garras */
    for (int k = 0; k < 3; k++) { p(16 + k * 2, 45, 1, 2, BONE); p(43 + k * 2, 45, 1, 2, BONE); }
    for (int k = 0; k < 2; k++) {   /* ombreiras douradas arredondadas com espigões */
        int cx = k ? 45 : 19, d = k ? 1 : -1;
        e(cx, 20, 5.5f, 3.5f, E3); e(cx - d, 19, 3, 1.5f, E4); p(cx - 4, 22, 9, 1, E1);
        p(cx + d * 3, 14, 2, 5, E3); p(cx + d * 3, 13, 1, 1, E4); p(cx, 15, 2, 3, E2); }
    #define GEMA(cx, cy) do { p((cx) - 1, (cy) - 2, 3, 5, E3); p((cx) - 2, (cy) - 1, 5, 3, E3); p((cx), (cy) - 1, 1, 3, E2); p((cx) - 1, (cy), 3, 1, E2); p((cx), (cy), 1, 1, f ? E4 : E3); } while (0)
    GEMA(32, 28); GEMA(26, 24); GEMA(38, 24); GEMA(32, 39); GEMA(19, 36); GEMA(45, 36);
    rei_chifre(im, -1, f); rei_chifre(im, 1, f);
    e(32, 11, 6, 5.5f, S0); p(26, 8, 13, 2, E0);   /* rosto */
    p(27, 10, 4, 2, C1); p(34, 10, 4, 2, C1); p(28, 10, 2, 1, f ? BONE : C1); p(35, 10, 2, 1, f ? BONE : C1);   /* olhos roxos a brilhar */
    p(29, 14, 7, 2, INK); for (int x = 29; x < 36; x += 2) p(x, 14, 1, 1, BONE);   /* sorriso com dentes */
    for (int y = 16; y < 27; y++) { int w = 7 - (y - 16) / 3 + ((y & 1) ? 1 : 0); p(32 - w, y, 2 * w, 1, y < 23 ? BONE : G2); }   /* barba */
    p(28, 25, 1, 2, G2); p(35, 25, 1, 2, G2); p(32, 27, 1, 1, G2);
    p(25, 4, 15, 3, E3); p(25, 4, 15, 1, E4); p(25, 6, 15, 1, E1);   /* coroa */
    for (int k = 0; k < 3; k++) { int x = 26 + k * 6; p(x, 1, 3, 3, E3); p(x + 1, 0, 1, 1, E4); }
    p(31, 0, 3, 4, E3); p(32, 0, 1, 1, E4);
    GEMA(32, 5);
    #undef GEMA
}
static void big_de(int id, void (*fn)(Img *, int), int pose) {   /* versão grande (64x64) a partir de um desenho de 32x32 */
    static uint8_t s32[32 * 32];
    Img im = {s32, 32, 32}; for (int i = 0; i < 32 * 32; i++) s32[i] = 0;
    fn(&im, pose);
    scale2x(s32, 32, 32, bigA); auto_shade(bigA, 64, 64);
    Img big = {bigA, 64, 64}; outline(&big);
    add_big(id, bigA, 64, 64);
}
static void mago_lado(Img *im, int pose) { draw_mago(im, SIDE, pose ? ATK1 : IDLE, 0); }
static void build_cine_sprites(void) {
    static const int pose[7] = {IDLE, DASH, IDLE, ATK0, ATK0, RUN, ATK1};
    static const int fr[7]   = {0,    0,    0,    0,    0,    1,   0};
    for (int k = 0; k < 7; k++) {
        static uint8_t s32[32 * 32];
        Img im = {s32, 32, 32}; for (int i = 0; i < 32 * 32; i++) s32[i] = 0;
        draw_hero(&im, SIDE, pose[k], fr[k]);
        if (k == AREN_CHAMAS || k == AREN_CHAMAS2) blade_fire(s32, 32, 6, 0, 11, 13, 11 + k * 7);
        if (k == AREN_ATAQUE) blade_fire(s32, 32, 22, 15, 31, 20, 99);
        scale2x(s32, 32, 32, bigA);
        auto_shade(bigA, 64, 64);
        ninja_detail(bigA, pose[k], (pose[k] == RUN && fr[k] == 1) ? 1 : 0);
        Img big = {bigA, 64, 64}; outline(&big);
        if (k == AREN_DEITADO) { rot90(bigA, bigB, 64); for (int i = 0; i < 64 * 64; i++) bigA[i] = bigB[i]; }
        add_big(SPR_AREN + k, bigA, 64, 64);
    }
    static const int bf[3] = {0, 1, 2};   /* Gorgomusgo: andar 0, andar 1, aviso */
    for (int k = 0; k < 3; k++) {
        static uint8_t s64[64 * 64];
        Img im = {s64, 64, 64}; for (int i = 0; i < 64 * 64; i++) s64[i] = 0;
        draw_boss(&im, bf[k]);
        scale2x(s64, 64, 64, bigA);
        auto_shade(bigA, 128, 128);
        Img big = {bigA, 128, 128}; outline(&big);
        add_big(SPR_CARRASCO + k, bigA, 128, 128);
    }
    big_de(SPR_HIST + 0, mago_lado, 0); big_de(SPR_HIST + 1, mago_lado, 1);
    big_de(SPR_HIST + 2, draw_cacador, 0); big_de(SPR_HIST + 3, draw_cacador, 1);
    big_de(SPR_HIST + 4, draw_padre, 0); big_de(SPR_HIST + 5, draw_padre, 1);
    for (int k = 0; k < 2; k++) {   /* Rei das Cinzas ampliado (128x128) */
        static uint8_t s64r[64 * 64];
        Img im = {s64r, 64, 64}; for (int i = 0; i < 64 * 64; i++) s64r[i] = 0;
        draw_rei(&im, k); outline(&im);
        for (int i = 0; i < 64 * 64; i++) if (s64r[i] == INK) s64r[i] = 0;   /* o contorno fino é refeito depois de ampliar */
        scale2x(s64r, 64, 64, bigA); auto_shade(bigA, 128, 128);
        Img big = {bigA, 128, 128}; outline(&big);
        add_big(SPR_REI + k, bigA, 128, 128);
    }
}
static uint8_t tcrypt[224][320];
static void build_crypt(void) {   /* fundo da cripta: parede de pedra, portal em arco, escadas, pilares com braseiros */
    seed = 5150;
    for (int y = 0; y < 224; y++) for (int x = 0; x < 320; x++) {
        int row = y / 10, off = (row & 1) * 10, b = BAYER[(y & 3) * 4 + (x & 3)];
        uint8_t c = ((y % 10) == 0 || ((x + off) % 20) == 0) ? INK : (b < 3 ? C0 : D1);
        if ((y % 10) == 1 && ((x + off) % 20) != 0) c = D2;
        tcrypt[y][x] = c;
    }
    for (int y = 0; y < 224; y++) for (int x = 0; x < 320; x++) {   /* luz dos braseiros na parede */
        for (int k = 0; k < 2; k++) { int bx = k ? 275 : 45, by = 92; int dx = x - bx, dy = y - by, d2 = dx * dx + dy * dy * 2;
            if (d2 < 2600) { int lvl = (2600 - d2) * 3 / 2600; int b = BAYER[(y & 3) * 4 + (x & 3)];
                if (lvl * 4 + 1 > b && tcrypt[y][x] != INK && tcrypt[y][x] != D2) tcrypt[y][x] = (uint8_t)((k ? CY_FIRE_B : CY_FIRE_A) + lvl); } } }
    int cx = 160, top = 34, hw = 58, bot = 172;   /* portal em arco */
    for (int y = top - hw / 2 - 10; y < bot; y++) for (int x = cx - hw - 12; x <= cx + hw + 12; x++) {
        int dx = x - cx, dy = y - (top + hw / 2);
        int in = (dy >= 0 && iabs2(dx) < hw) || (dy < 0 && dx * dx + dy * dy * 4 < hw * hw);
        int ring = (dy >= 0 && iabs2(dx) < hw + 10) || (dy < 0 && dx * dx + dy * dy * 4 < (hw + 10) * (hw + 10));
        if (x < 0 || x >= 320 || y < 0) continue;
        if (in) { int b = BAYER[(y & 3) * 4 + (x & 3)]; tcrypt[y][x] = (y > 120 && b < (y - 120) / 8) ? M0 : INK; }
        else if (ring) tcrypt[y][x] = (((x >> 3) + (y >> 3)) & 1) ? D2 : G1;
    }
    for (int y = 172; y < 224; y++) { int step = (y - 172) / 9, w = hw + 8 + step * 14;   /* escadas */
        for (int x = cx - w; x <= cx + w; x++) if (x >= 0 && x < 320) tcrypt[y][x] = ((y - 172) % 9 == 0) ? G1 : ((y - 172) % 9 == 1 ? D2 : D1); }
    for (int k = 0; k < 2; k++) { int x0 = k ? 262 : 32;   /* pilares e taças dos braseiros */
        for (int y = 104; y < 224; y++) for (int x = x0; x < x0 + 26; x++) tcrypt[y][x] = (x == x0) ? G1 : (x > x0 + 22 ? INK : D2);
        for (int y = 98; y < 106; y++) for (int x = x0 - 4; x < x0 + 30; x++) tcrypt[y][x] = (y == 98) ? G1 : D1; }
    for (int k = 0; k < 6; k++) { int sx = 14 + rndi(292), sy = 20 + rndi(60); if (sx > 90 && sx < 230) continue;   /* caveiras nos nichos */
        for (int y = 0; y < 10; y++) for (int x = 0; x < 12; x++) tcrypt[sy + y][sx + x] = INK;
        for (int y = 2; y < 8; y++) for (int x = 3; x < 9; x++) tcrypt[sy + y][sx + x] = BONE;
        tcrypt[sy + 4][sx + 4] = INK; tcrypt[sy + 4][sx + 7] = INK; tcrypt[sy + 6][sx + 5] = INK; }
}
/* ===== Pântano ===== */
static void draw_salgueiro(Img *im) {   /* 48x64: tronco retorcido e ramos caídos com musgo */
    p(21,30,7,32,S0); p(21,30,2,32,S1); p(26,30,2,32,D1); p(18,58,13,4,S0); p(16,61,5,2,S0); p(29,60,5,2,S0);
    e(24,24,18,12,D1); e(22,20,14,8,D2);
    for (int k = 0; k < 11; k++) { int x = 6 + k * 3 + rndi(2), y = 18 + rndi(8), len = 14 + rndi(18);
        for (int j = 0; j < len; j++) p(x + (j > len / 2 ? (k & 1) : 0), y + j, 1, 1, j % 5 == 4 ? V1 : V0); }
    for (int i = 0; i < 12; i++) p(8 + rndi(32), 14 + rndi(14), 2, 1, V1);
    p(12,28,9,2,S0); p(28,26,10,2,S0);
}
static void draw_juncos(Img *im) {   /* 16x24 */
    for (int k = 0; k < 6; k++) { int x = 2 + k * 2, h = 12 + rndi(10);
        p(x, 24 - h, 1, h, (k & 1) ? V1 : V0); if (k % 2 == 0) { p(x - 1, 24 - h - 1, 2, 4, S0); p(x - 1, 24 - h - 1, 1, 1, S1); } }
}
static void draw_cogumelos(Img *im) {   /* 16x16, chapéus roxos que brilham */
    p(4,10,2,5,BONE); p(10,8,2,7,BONE); p(7,12,1,3,G2);
    e(5,9,3.5f,2,C1); e(11,7,4,2.5f,C1); e(7.5f,11,2,1.5f,C0);
    p(4,8,1,1,BONE); p(11,6,1,1,BONE); p(13,7,1,1,BONE); p(10,5,2,1,E4);
}
/* Olho de Jade (64x64): feiticeiro que flutua, capuz com coroa, olhos verdes, manto azul esfarrapado que
   acaba em névoa, cajado com um olho de jade num aro dourado. f: 0 e 1 a flutuar, 2 aviso (cajado erguido, olhos vermelhos) */
static void draw_boss2(Img *im, int f) {
    int Y = f == 1 ? 1 : 0, warn = f == 2;
    for (int y = 24; y < 58; y++) {   /* manto: alarga e desfaz-se em tiras */
        int w = 10 + (y - 24) / 2; if (w > 22) w = 22;
        for (int x = 32 - w; x < 32 + w; x++) {
            int tira = ((x + (f & 1)) % 5 == 0) && y > 46;
            if (y > 50 && ((x * 7 + y * 3 + f) % 4) == 0) continue;   /* névoa em baixo */
            if (y > 54 && ((x + y) & 1)) continue;
            uint8_t c = M1; if (x < 32 - w + 3) c = M2; if (x > 32 + w - 3) c = M0; if (tira) c = M0;
            ip(im, x, y + Y, 1, 1, c);
        }
    }
    p(30, 26 + Y, 4, 26, E3); p(31, 26 + Y, 2, 26, E4);   /* faixa dourada ao centro */
    for (int k = 0; k < 3; k++) { p(29, 32 + k * 7 + Y, 6, 1, E3); p(31, 31 + k * 7 + Y, 2, 3, V2); }   /* fechos de jade */
    e(32, 22 + Y, 14, 6, M2); p(18, 20 + Y, 28, 2, M3);   /* ombros */
    p(16, 24 + Y, 6, 12, M1); p(42, 24 + Y, 6, 12, M1); p(16, 24 + Y, 1, 12, M3);   /* mangas */
    p(16, 35 + Y, 6, 2, E3); p(42, 35 + Y, 6, 2, E3);
    p(17, 37 + Y, 4, 4, S2); for (int k = 0; k < 3; k++) p(17 + k * 2, 41 + Y, 1, 2, S1);   /* mão esquerda com chama verde */
    if (!warn) { e(19, 43 + Y, 3, 3, V1); p(19, 42 + Y, 1, 2, V2); }
    e(32, 13 + Y, 9, 10, M1); e(30, 9 + Y, 5, 5, M2);   /* capuz */
    e(32, 15 + Y, 6, 6, INK);
    uint8_t olho = warn ? RED : V2;
    p(28, 14 + Y, 3, 2, olho); p(34, 14 + Y, 3, 2, olho); p(29, 14 + Y, 1, 1, BONE); p(35, 14 + Y, 1, 1, BONE);
    p(23, 4 + Y, 18, 3, E3); p(23, 4 + Y, 18, 1, E4);   /* coroa sobre o capuz */
    for (int k = 0; k < 4; k++) { p(24 + k * 5, 0 + Y, 2, 4, E3); p(24 + k * 5, 0 + Y, 1, 1, E4); }
    p(31, 5 + Y, 2, 2, V2);
    int sy = warn ? 0 : 6;   /* cajado com o olho de jade */
    p(46, 20 + sy + Y - 6, 2, 40 - sy, S1); p(46, 20 + sy + Y - 6, 1, 40 - sy, S2);
    p(44, 36 + Y, 6, 4, M1); p(44, 36 + Y, 6, 1, M3);   /* mão direita */
    int cy = (warn ? 6 : 12) + Y;
    e(47, (float)cy, 6, 7, E3); e(47, (float)cy, 4.5f, 5.5f, V0); e(47, (float)cy, 3.5f, 4.5f, V1); e(47, (float)cy - 1, 2, 2.5f, V2);
    p(47, cy - 2, 1, 4, INK); p(46, cy - 3, 1, 1, BONE);
    p(41, cy + 5, 3, 2, E3); p(50, cy + 5, 3, 2, E3); p(46, cy - 8, 2, 2, E4);
    if (warn) for (int i = 0; i < 10; i++) p(40 + rndi(15), cy - 9 + rndi(18), 1, 1, (i & 1) ? V2 : BONE);
}
/* ===== Economia: ouro, comida, vasos, baús e ícones dos itens ===== */
static void draw_moeda(Img *im, int f) {   /* 10x10 */
    if (f == 2) { e(5,6,4.5f,3.5f,S1); e(5,6,3.5f,2.5f,S2); p(3,1,5,2,S1); p(4,0,3,1,E1); p(4,5,3,2,E3); p(5,5,1,1,E4); return; }   /* saco */
    if (f == 0) { e(5,5,4,4,E3); e(5,5,2.5f,2.5f,E4); p(4,3,1,4,E3); p(3,3,1,1,BONE); }
    else { e(5,5,1.8f,4,E3); p(5,2,1,6,E4); }
}
static void draw_frango(Img *im) { e(7,6,5,3.5f,S1); e(6,5,3.5f,2.5f,E3); p(11,3,2,5,BONE); p(12,2,2,2,BONE); p(12,7,2,2,BONE); }   /* 14x10 */
static void draw_vaso(Img *im) {   /* 16x16 */
    e(8,10,6,5.5f,E1); e(7,9,4,3.5f,E2); p(5,2,6,3,E1); p(4,1,8,2,S0); p(5,1,6,1,S1);
    for (int x = 3; x < 13; x += 3) p(x, 10, 2, 1, E3); p(6,6,1,1,E3);
}
static void draw_bau(Img *im, int f) {   /* 24x20: 0 fechado, 1 aberto, 2 dourado */
    uint8_t c0 = f == 2 ? E3 : S1, c1 = f == 2 ? E4 : S2, aro = f == 2 ? BONE : G1;
    p(2,8,20,11,c0); p(2,8,20,1,c1); p(2,8,1,11,c1); p(21,8,1,11,S0);
    for (int x = 2; x < 22; x += 6) p(x, 8, 2, 11, aro);
    p(2,17,20,2,S0);
    if (f == 1) { p(2,3,20,5,c0); p(2,3,20,1,c1); p(4,7,16,2,INK); for (int i = 0; i < 6; i++) p(5 + i * 3, 4 + (i & 1), 1, 1, E4); }
    else { e(12,7,10,4,c0); e(12,6,9,3,c1); p(2,7,20,2,aro); p(10,9,4,5,aro); p(11,11,2,2,INK); }
    if (f == 2) for (int i = 0; i < 6; i++) p(1 + rndi(22), rndi(6), 1, 1, BONE);
}
static void draw_item(Img *im, int k) {   /* ícones 16x16 */
    switch (k) {
    case 0: e(8,9,6,6,E3); e(8,9,3.5f,3.5f,0); p(7,1,3,3,M3); p(8,1,1,1,BONE); p(4,8,1,2,E4); break;   /* anel de ouro */
    case 1: for (int a = 0; a < 4; a++) { int cx = a == 0 ? 8 : a == 1 ? 12 : a == 2 ? 8 : 4, cy = a == 0 ? 4 : a == 1 ? 8 : a == 2 ? 12 : 8; e((float)cx, (float)cy, 3.5f, 3.5f, V1); }
            p(7,7,3,3,V2); p(8,10,1,5,V0); break;   /* trevo */
    case 2: e(8,10,6,4,G1); e(7,9,4,2.5f,G2); p(3,2,1,6,BONE); p(4,3,1,1,E4); p(12,2,1,3,E4); break;   /* pedra de amolar */
    case 3: p(3,1,10,2,S1); p(3,13,10,2,S1); for (int y = 3; y < 13; y++) { int w = 1 + iabs2(y - 8) * 3 / 5; p(8 - w, y, 2 * w, 1, M2); } p(7,4,2,2,E3); p(6,10,4,3,E3); break;   /* ampulheta */
    case 4: for (int i = 0; i < 12; i++) { p(3 + i, 13 - i, 1, 1, G1); p(4 + i, 13 - i - (i > 2), 2, 2, BONE); } break;   /* pena */
    case 5: e(8,8,6,7,BONE); e(8,7,5,5,G2); e(8,7,2.5f,2.5f,BONE); p(7,7,1,1,INK); p(9,7,1,1,INK); p(8,9,1,1,INK); break;   /* escudo de osso */
    case 6: e(8,10,5.5f,5,E3); e(7,9,3,3,E4); p(8,2,1,4,S0); p(9,2,3,2,V1); break;   /* maçã dourada */
    case 7: for (int y = 2; y < 14; y++) { p(3, y, 3, 1, E2); p(10, y, 3, 1, E2); } e(8,13,5,2.5f,E2); e(8,13,2,1,0); p(3,2,3,2,G2); p(10,2,3,2,G2); break;   /* íman */
    case 8: p(5,4,6,9,S1); p(6,5,4,7,E4); p(7,7,2,3,BONE); p(6,2,4,2,S0); p(7,0,2,2,G1); p(4,13,8,2,S0); break;   /* lanterna */
    case 9: e(8,8,6,6,V0); e(8,8,3.5f,3.5f,0); for (int a = 0; a < 8; a++) { int x = 8 + (int)(__builtin_cosf(a * .785f) * 7), y = 8 + (int)(__builtin_sinf(a * .785f) * 7); p(x, y, 1, 1, BONE); } break;   /* espinhos */
    case 10: p(4,2,8,3,E3); e(8,5,4,2.5f,E3); p(7,7,2,5,E3); p(5,12,6,2,E3); p(7,3,2,2,E2); p(5,2,1,1,E4); break;   /* relíquia */
    default: e(5,5,3.5f,3.5f,E3); e(5,5,1.5f,1.5f,0); p(8,5,7,2,E3); p(12,7,1,3,E3); p(14,7,1,2,E3); p(4,3,1,1,E4); break;   /* chave */
    }
}
static void draw_morcego(Img *im, int f) {   /* 18x12 */
    e(9,6,3,3,C0); e(9,5,2,2,D1); p(7,2,1,2,C0); p(11,2,1,2,C0); p(8,5,1,1,RED); p(10,5,1,1,RED); p(8,8,1,1,BONE); p(10,8,1,1,BONE);
    for (int i = 0; i < 6; i++) { int y = f ? 2 + i / 2 : 8 - i / 2;
        p(6 - i, y, 1, 3, C0); p(12 + i, y, 1, 3, C0); if (i & 1) { p(6 - i, y + 3, 1, 1, D1); p(12 + i, y + 3, 1, 1, D1); } }
}
static void draw_orbe(Img *im, int f) { e(4,4,3.5f,3.5f,V1); e(4,4,2.2f,2.2f,V2); p(3,3,2,2,f ? BONE : V2); }
static void rot90(const uint8_t *src, uint8_t *dst, int n) {   /* rotação de 90° no sentido dos ponteiros */
    for (int y = 0; y < n; y++) for (int x = 0; x < n; x++) dst[x * n + (n - 1 - y)] = src[y * n + x];
}
static void build_sprites(void) {
    static const int poses[9] = {IDLE, IDLE, RUN, RUN, RUN, RUN, ATK0, ATK1, DASH};
    static const int frames[9] = {0, 1, 0, 1, 2, 3, 0, 0, 0};
    for (int d = 0; d < 3; d++) for (int k = 0; k < 9; k++) {
        Img im = blank(32, 32); int pose = poses[k], f = frames[k];
        if (pose == DASH && d != SIDE) { pose = RUN; f = 1; }
        draw_hero(&im, d, pose, f); outline(&im); add_sprite_full(SPR_HERO + d * 9 + k, work, 32, 32, 16, 31);
    }
    for (int f = 0; f < 2; f++) { Img im = blank(16, 16); draw_crawler(&im, f); outline(&im); add_sprite_full(SPR_CRAWL + f, work, 16, 16, 8, 15); }
    for (int f = 0; f < 2; f++) { Img im = blank(32, 32); draw_husk(&im, f); outline(&im); add_sprite_full(SPR_HUSK + f, work, 32, 32, 16, 31); }
    for (int f = 0; f < 2; f++) { Img im = blank(32, 32); draw_husk(&im, 2 + f); outline(&im); add_sprite_full(SPR_ESQ_ATK + f, work, 32, 32, 16, 31); }
    for (int f = 0; f < 4; f++) { Img im = blank(16, 24); draw_flame(&im, f); add_sprite(SPR_FLAME + f, work, 16, 24, 8, 23); }
    for (int f = 0; f < 3; f++) {
        Img im = blank(48, 48); draw_smear(&im, f, 0);
        add_sprite_full(SPR_SMEAR + f, work, 48, 48, 24, 24);
        for (int r = 1; r <= 2; r++) {
            for (int j = 0; j < 48; j++) for (int i = 0; i < 48; i++) {
                int u = r == 1 ? 47 - j : j, v = r == 1 ? i : 47 - i;
                work2[j * 48 + i] = work[v * 48 + u];
            }
            add_sprite_full(SPR_SMEAR + r * 3 + f, work2, 48, 48, 24, 24);
        }
    }
    for (int f = 0; f < 3; f++) { Img im = blank(16, 16); draw_spark(&im, f); add_sprite(SPR_SPARK + f, work, 16, 16, 8, 8); }
    for (int f = 0; f < 4; f++) { Img im = blank(32, 32); draw_puff(&im, f); add_sprite(SPR_PUFF + f, work, 32, 32, 16, 28); }
    for (int t = 0; t < 3; t++) { Img im = blank(8, 8); draw_gem(&im, t); outline(&im); add_sprite(SPR_GEM + t, work, 8, 8, 4, 4); }
    for (int f = 0; f < 2; f++) { Img im = blank(8, 8); draw_orb(&im, f); add_sprite(SPR_ORB + f, work, 8, 8, 4, 4); }
    for (int f = 0; f < 2; f++) { Img im = blank(8, 8); draw_bolt(&im, f); add_sprite_full(SPR_BOLT + f, work, 8, 8, 4, 4); }
    { Img im = blank(16, 48); draw_pillar(&im); outline(&im); add_sprite(SPR_PILLAR, work, 16, 48, 8, 47); }
    { Img im = blank(48, 64); draw_tree(&im); outline(&im); add_sprite(SPR_TREE, work, 48, 64, 24, 62); }
    { Img im = blank(32, 48); draw_statue(&im); outline(&im); add_sprite(SPR_STATUE, work, 32, 48, 16, 46); }
    { Img im = blank(16, 24); draw_grave(&im); outline(&im); add_sprite(SPR_GRAVE, work, 16, 24, 8, 22); }
    { Img im = blank(16, 24); draw_brazier(&im); outline(&im); add_sprite(SPR_BRAZIER, work, 16, 24, 8, 23); }
    { Img im = blank(16, 40); draw_banner(&im); outline(&im); add_sprite(SPR_BANNER, work, 16, 40, 8, 39); }
    { Img im = blank(16, 16); draw_rubble(&im); outline(&im); add_sprite(SPR_RUBBLE, work, 16, 16, 8, 15); }
    { Img im = blank(32, 24); draw_bush(&im); outline(&im); add_sprite(SPR_BUSH, work, 32, 24, 16, 22); }
    { Img im = blank(16, 8);  draw_shadow(&im, 16, 8);  add_sprite(SPR_SH_S, work, 16, 8, 8, 4); }
    { Img im = blank(24, 10); draw_shadow(&im, 24, 10); add_sprite(SPR_SH_M, work, 24, 10, 12, 5); }
    { Img im = blank(40, 14); draw_shadow(&im, 40, 14); add_sprite(SPR_SH_L, work, 40, 14, 20, 7); }
    for (int f = 0; f < 3; f++) { Img im = blank(64, 64); draw_boss(&im, f); outline(&im); add_sprite_full(SPR_BOSS + f, work, 64, 64, 32, 62); }
    for (int f = 0; f < 3; f++) { Img im = blank(64, 64); draw_boss2(&im, f); outline(&im); add_sprite_full(SPR_BOSS2 + f, work, 64, 64, 32, 60); }
    for (int f = 0; f < 2; f++) { Img im = blank(8, 8); draw_orbe(&im, f); outline(&im); add_sprite(SPR_ORBE + f, work, 8, 8, 4, 4); }
    for (int f = 0; f < 3; f++) { Img im = blank(10, 10); draw_moeda(&im, f); outline(&im); add_sprite(SPR_MOEDA + f, work, 10, 10, 5, 8); }
    { Img im = blank(14, 10); draw_frango(&im); outline(&im); add_sprite(SPR_FRANGO, work, 14, 10, 7, 8); }
    { Img im = blank(16, 16); draw_vaso(&im); outline(&im); add_sprite(SPR_VASO, work, 16, 16, 8, 15); }
    for (int f = 0; f < 3; f++) { Img im = blank(24, 20); draw_bau(&im, f); outline(&im); add_sprite(SPR_BAU + f, work, 24, 20, 12, 19); }
    for (int k = 0; k < 12; k++) { Img im = blank(16, 16); draw_item(&im, k); outline(&im); add_sprite(SPR_ITEM + k, work, 16, 16, 0, 0); }
    for (int f = 0; f < 2; f++) { Img im = blank(18, 12); draw_morcego(&im, f); outline(&im); add_sprite(SPR_MORCEGO + f, work, 18, 12, 9, 6); }
    { Img im = blank(48, 64); draw_salgueiro(&im); outline(&im); add_sprite(SPR_SALGUEIRO, work, 48, 64, 24, 62); }
    { Img im = blank(16, 24); draw_juncos(&im); outline(&im); add_sprite(SPR_JUNCOS, work, 16, 24, 8, 23); }
    { Img im = blank(16, 16); draw_cogumelos(&im); outline(&im); add_sprite(SPR_COGUMELOS, work, 16, 16, 8, 15); }
    for (int f = 0; f < 4; f++) { Img im = blank(48, 64); draw_portal(&im, f); add_sprite(SPR_PORTAL + f, work, 48, 64, 24, 62); }
    for (int d = 0; d < 2; d++) {   /* setas: base direita e diagonal, rodadas em passos de 90° */
        Img im = blank(16, 16); draw_arrow(&im, d); outline(&im);
        for (int i = 0; i < 256; i++) work2[i] = work[i];
        for (int r = 0; r < 4; r++) {
            add_sprite(SPR_ARROW + d + r * 2, work2, 16, 16, 8, 8);
            uint8_t t[256]; rot90(work2, t, 16); for (int i = 0; i < 256; i++) work2[i] = t[i];
        }
    }
    for (int r = 0; r < 2; r++) { Img im = blank(64, 64); draw_house(&im, r); outline(&im); add_sprite(SPR_HOUSE + r, work, 64, 64, 32, 62); }
    { Img im = blank(64, 64); draw_torii(&im); outline(&im); add_sprite(SPR_TORII, work, 64, 64, 32, 62); }
    { Img im = blank(16, 32); draw_lantern(&im); outline(&im); add_sprite(SPR_LANTERN, work, 16, 32, 8, 31); }
    { Img im = blank(48, 64); draw_sakura(&im); outline(&im); add_sprite(SPR_SAKURA, work, 48, 64, 24, 62); }
    { Img im = blank(32, 16); draw_beams(&im); outline(&im); add_sprite(SPR_BEAMS, work, 32, 16, 16, 14); }
    {   /* feiticeira: mesmas poses e frames do ninja */
        static const int poses[9] = {IDLE, IDLE, RUN, RUN, RUN, RUN, ATK0, ATK1, DASH};
        static const int frames[9] = {0, 1, 0, 1, 2, 3, 0, 0, 0};
        for (int d = 0; d < 3; d++) for (int k = 0; k < 9; k++) {
            Img im = blank(32, 32); int pose = poses[k], f = frames[k];
            if (pose == DASH && d != SIDE) { pose = RUN; f = 1; }
            draw_mago(&im, d, pose, f); outline(&im); add_sprite_full(SPR_MAGO + d * 9 + k, work, 32, 32, 16, 31);
        }
    }
    {   /* caçador e padre: mesmas poses e frames do ninja */
        static const int poses[9] = {IDLE, IDLE, RUN, RUN, RUN, RUN, ATK0, ATK1, DASH};
        static const int frames[9] = {0, 1, 0, 1, 2, 3, 0, 0, 0};
        for (int quem = 0; quem < 2; quem++) for (int d = 0; d < 3; d++) for (int k = 0; k < 9; k++) {
            Img im = blank(32, 32); int pose = poses[k], f = frames[k];
            if (pose == DASH && d != SIDE) { pose = RUN; f = 1; }
            if (quem) draw_padre_dir(&im, d, pose, f); else draw_cacador_dir(&im, d, pose, f);
            outline(&im); add_sprite_full((quem ? SPR_PADRE : SPR_CACADOR) + d * 9 + k, work, 32, 32, 16, 31);
        }
    }
    for (int f = 0; f < 2; f++) { Img im = blank(12, 12); draw_fireball(&im, f); outline(&im); add_sprite(SPR_FIREBALL + f, work, 12, 12, 6, 6); }
    for (int f = 0; f < 4; f++) { Img im = blank(40, 40); draw_explosion(&im, f); add_sprite(SPR_EXPLO + f, work, 40, 40, 20, 22); }
    for (int f = 0; f < 2; f++) { Img im = blank(16, 24); draw_meteor(&im, f); outline(&im); add_sprite(SPR_METEOR + f, work, 16, 24, 8, 22); }
    { Img im = blank(32, 12); draw_target(&im); add_sprite(SPR_TARGET, work, 32, 12, 16, 6); }
    for (int f = 0; f < 2; f++) { Img im = blank(8, 8); draw_missile(&im, f); add_sprite(SPR_MISSILE + f, work, 8, 8, 4, 4); }
    for (int f = 0; f < 4; f++) { Img im = blank(48, 48); draw_hole(&im, f); add_sprite(SPR_HOLE + f, work, 48, 48, 24, 26); }
    for (int f = 0; f < 3; f++) { Img im = blank(32, 48); draw_cape(&im, f); outline(&im); add_sprite(SPR_CAPE + f, work, 32, 48, 16, 45); }
    { Img im = blank(130, 36); draw_hud_left(&im); add_sprite(SPR_HUD_LEFT, work, 130, 36, 0, 0); }
    { Img im = blank(52, 18); draw_hud_timer(&im); add_sprite(SPR_HUD_TIMER, work, 52, 18, 0, 0); }
    { Img im = blank(64, 18); draw_hud_kills(&im); add_sprite(SPR_HUD_KILLS, work, 64, 18, 0, 0); }
    { Img im = blank(224, 32); draw_hud_boss(&im); add_sprite(SPR_HUD_BOSS, work, 224, 32, 0, 0); }
    for (int k = 0; k < 2; k++) { Img im = blank(24, 24); draw_avatar(&im, k); add_sprite(SPR_AVATAR + k, work, 24, 24, 0, 0); }
    for (int k = 0; k < 2; k++) { Img im = blank(24, 24); draw_avatar2(&im, k); add_sprite(SPR_AVATAR2 + k, work, 24, 24, 0, 0); }
    for (int f = 0; f < 2; f++) { Img im = blank(12, 12); draw_cruz(&im, f); outline(&im); add_sprite(SPR_CRUZ + f, work, 12, 12, 6, 6); }
    { Img im = blank(8, 8); draw_frasco(&im); outline(&im); add_sprite(SPR_FRASCO, work, 8, 8, 4, 4); }
    for (int f = 0; f < 2; f++) { Img im = blank(12, 8); draw_armadilha(&im, f); outline(&im); add_sprite(SPR_ARMADILHA + f, work, 12, 8, 6, 5); }
    for (int f = 0; f < 2; f++) { Img im = blank(16, 12); draw_falcao(&im, f); outline(&im); add_sprite_full(SPR_FALCAO + f, work, 16, 12, 8, 6); }
    build_cine_sprites();
}

/* ===== Tiles 16x16 ===== */
enum { T_GRASS = 0, T_DIRT = 4, T_STONE = 7, T_MOSS = 11, T_WATER = 13, T_WATER_N = 14, T_BRIDGE = 15,
       T_WTOP = 16, T_WFACE = 18, T_CORRUPT = 20, T_RUNE = 22, T_RUBBLEF = 23, T_LIT = 24,
       T_ASH = 122, T_SPATH = 125, T_PADDY = 127, T_PADDY_N = 128, T_LIT2 = 129,
       T_MUD = 227, T_SWAMP = 230, T_SWAMP_N = 231, NTILES = 232 };
static uint8_t tiles[NTILES][256];
static Img timg(int t) { Img im = {tiles[t], 16, 16}; return im; }

static void tile_stone(int t, int v) {
    Img I = timg(t), *im = &I;
    p(0,0,16,16,D1); p(0,0,16,1,D2); p(0,0,1,16,D2); p(15,0,1,16,INK); p(0,15,16,1,INK);
    if (v == 3) { p(7,1,1,14,INK); p(8,1,1,14,D2); }
    for (int i = 0; i < 6; i++) p(1 + rndi(14), 1 + rndi(14), 1, 1, chance(50) ? D2 : INK);
    if (v == 1) { for (int i = 0; i < 7; i++) p(3 + i, 4 + i + (i > 3), 1, 1, INK); }
    if (v == 2) for (int i = 0; i < 8; i++) p(1 + rndi(13), 10 + rndi(5), 1 + rndi(2), 1, chance(50) ? V0 : V1);
}
static void build_tiles(void) {
    for (int v = 0; v < 4; v++) { Img I = timg(T_GRASS + v), *im = &I;
        p(0,0,16,16,V0);
        for (int i = 0; i < 24; i++) p(rndi(16), rndi(15), 1, 2, V1);
        for (int i = 0; i < 6; i++) p(rndi(16), rndi(16), 1, 1, V2);
        if (v == 2) for (int i = 0; i < 4; i++) p(rndi(15), rndi(15), 2, 1, S0);
        if (v == 3) for (int i = 0; i < 3; i++) { int x = 1 + rndi(13), y = 1 + rndi(13); p(x,y,1,1,i ? BONE : M3); p(x,y+1,1,1,V1); }
    }
    for (int v = 0; v < 3; v++) { Img I = timg(T_DIRT + v), *im = &I;
        p(0,0,16,16,S0);
        for (int i = 0; i < 18; i++) p(rndi(16), rndi(16), 1, 1, S1);
        for (int i = 0; i < 3; i++) p(rndi(15), rndi(16), 2, 1, D2);
        if (v == 1) for (int i = 0; i < 5; i++) p(2 + i * 2, 6 + (i & 1), 2, 1, INK);
        if (v == 2) for (int i = 0; i < 4; i++) p(rndi(14), rndi(14), 2, 2, S1);
    }
    for (int v = 0; v < 4; v++) tile_stone(T_STONE + v, v);
    for (int v = 0; v < 2; v++) { tile_stone(T_MOSS + v, 2); Img I = timg(T_MOSS + v), *im = &I;
        for (int i = 0; i < 22; i++) p(rndi(15), rndi(15), 1 + rndi(2), 1, chance(60) ? V0 : V1);
        if (v) for (int i = 0; i < 6; i++) p(rndi(15), rndi(15), 1, 1, V2);
    }
    for (int v = 0; v < 2; v++) { Img I = timg(T_WATER + v), *im = &I;   /* água: bandas diagonais em ciclo */
        for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
            int band = ((x + 2 * y + ((x * 7 + y * 3) % 5 == 0)) >> 2) & 3;
            ip(im, x, y, 1, 1, (uint8_t)(CY_WATER + band));
        }
        if (v) { p(0,0,16,2,S0); p(0,2,16,1,INK); for (int i = 0; i < 5; i++) p(rndi(15), 0, 2, 1, S1); }
    }
    { Img I = timg(T_BRIDGE), *im = &I;
        p(0,0,16,16,S1);
        for (int x = 0; x < 16; x += 4) { p(x,0,1,16,INK); p(x+1,0,1,16,S2); }
        p(0,0,16,2,S0); p(0,14,16,2,S0); p(0,15,16,1,INK); p(2,4,1,1,G2); p(10,11,1,1,G2);
    }
    for (int v = 0; v < 2; v++) { Img I = timg(T_WTOP + v), *im = &I;
        p(0,0,16,16,G1); p(0,0,16,1,G2); p(0,15,16,1,D2);
        for (int i = 0; i < 6; i++) p(rndi(15), 1 + rndi(13), 2, 1, D2);
        if (v) for (int i = 0; i < 8; i++) p(rndi(15), rndi(15), 1, 1, V1);
    }
    for (int v = 0; v < 2; v++) { Img I = timg(T_WFACE + v), *im = &I;   /* face do muro: tijolos */
        p(0,0,16,16,D2);
        for (int r = 0; r < 4; r++) { int y = r * 4, off = (r & 1) * 4;
            p(0,y,16,1,INK); p(0,y+1,16,1,G1);
            for (int x = off; x < 16; x += 8) p(x,y,1,4,INK);
        }
        p(0,15,16,1,D1);
        if (v) for (int k = 0; k < 3; k++) { int x = 2 + k * 5; for (int y = 0; y < 10 + k * 2; y++) { p(x + ((y >> 2) & 1), y, 1, 1, V0); if (y % 4 == 1) p(x + 1, y, 2, 1, V1); } }
    }
    for (int v = 0; v < 2; v++) { Img I = timg(T_CORRUPT + v), *im = &I;
        p(0,0,16,16,S0);
        for (int i = 0; i < 10 + v * 10; i++) p(rndi(16), rndi(16), 1, 1, C0);
        for (int i = 0; i < 3; i++) { int x = rndi(12), y = rndi(14); p(x,y,3,1,INK); p(x+2,y+1,2,1,INK); }
        if (v) for (int i = 0; i < 4; i++) p(rndi(16), rndi(16), 1, 1, CY_RUNE);
    }
    { Img I = timg(T_RUNE), *im = &I;   /* runa que pulsa */
        p(0,0,16,16,S0); for (int i = 0; i < 12; i++) p(rndi(16), rndi(16), 1, 1, C0);
        p(3,3,10,1,CY_RUNE); p(3,12,10,1,CY_RUNE); p(3,3,1,10,CY_RUNE); p(12,3,1,10,CY_RUNE);
        p(7,5,2,6,CY_RUNE + 1); p(5,7,6,2,CY_RUNE + 1);
    }
    { tile_stone(T_RUBBLEF, 0); Img I = timg(T_RUBBLEF), *im = &I;
        for (int i = 0; i < 6; i++) { int x = rndi(14), y = rndi(14); p(x,y,2,2,G1); p(x,y,1,1,G2); p(x+1,y+2,2,1,INK); } }
    /* Aldeia: chão de cinza, caminho de pedras, arrozal alagado */
    for (int v = 0; v < 3; v++) { Img I = timg(T_ASH + v), *im = &I;
        p(0,0,16,16,S0);
        for (int i = 0; i < 10; i++) p(rndi(15), rndi(16), 2, 1, D1);
        for (int i = 0; i < 8; i++) p(rndi(16), rndi(16), 1, 1, G1);
        if (v == 1) for (int i = 0; i < 4; i++) { int x = rndi(12), y = rndi(13); p(x,y,4,2,INK); p(x+1,y+2,2,1,D1); }
        if (v == 2) for (int i = 0; i < 5; i++) p(rndi(15), rndi(15), 1, 2, V0);
    }
    for (int v = 0; v < 2; v++) { Img I = timg(T_SPATH + v), *im = &I;
        p(0,0,16,16,S0); for (int i = 0; i < 6; i++) p(rndi(16), rndi(16), 1, 1, G1);
        int o = v * 4; e(4 + o,4,3.5f,2.5f,D2); e(4 + o,3.5f,2.5f,1.5f,G1); e(12 - o,11,3.5f,2.5f,D2); e(12 - o,10.5f,2.5f,1.5f,G1);
        p(1 + o,6,6,1,INK); p(9 - o,13,6,1,INK);
    }
    for (int v = 0; v < 2; v++) { Img I = timg(T_PADDY + v), *im = &I;
        for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) ip(im, x, y, 1, 1, (uint8_t)(CY_WATER + (((x + 2 * y) >> 2) & 3)));
        for (int y = 2; y < 16; y += 5) for (int x = 1; x < 16; x += 4) { p(x, y, 1, 3, V1); p(x, y, 1, 1, V2); }
        if (v) { p(0,0,16,2,S0); p(0,2,16,1,INK); }
    }
    /* Pântano: lama e água estagnada com algas e nenúfares */
    for (int v = 0; v < 3; v++) { Img I = timg(T_MUD + v), *im = &I;
        p(0,0,16,16,S0);
        for (int i = 0; i < 6; i++) { int x = rndi(13), y = rndi(14); p(x,y,3,2,D1); p(x+1,y,1,1,D2); }
        for (int i = 0; i < 10; i++) p(rndi(16), rndi(16), 1, 1, v == 2 ? V1 : V0);
        if (v == 1) for (int i = 0; i < 3; i++) { int x = 1 + rndi(13), y = 2 + rndi(11); p(x,y,1,3,V1); p(x,y,1,1,V2); }
    }
    for (int v = 0; v < 2; v++) { Img I = timg(T_SWAMP + v), *im = &I;
        for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
            int b = BAYER[(y & 3) * 4 + (x & 3)];
            uint8_t c = b < 6 ? V0 : D1;
            if (((x + 2 * y) & 7) == 0) c = (uint8_t)(CY_WATER + (((x + y) >> 2) & 3));
            ip(im, x, y, 1, 1, c);
        }
        for (int i = 0; i < 4; i++) p(rndi(15), rndi(15), 2, 1, V1);
        if (!v && chance(50)) { int x = 3 + rndi(8), y = 4 + rndi(7); e((float)x + 2, (float)y + 1, 2.5f, 1.5f, V1); p(x + 1, y, 1, 1, V2); }
        if (v) { p(0,0,16,2,S0); p(0,2,16,1,INK); for (int i = 0; i < 4; i++) p(rndi(15), 0, 2, 1, V1); }
    }
    /* Poças de luz: 7x7 tiles por grupo, em pontilhado; sobre pedra (fortaleza) e sobre cinza (aldeia) */
    for (int base2 = 0; base2 < 2; base2++)
    for (int g = 0; g < 2; g++) for (int dy = -3; dy <= 3; dy++) for (int dx = -3; dx <= 3; dx++) {
        int t = (base2 ? T_LIT2 : T_LIT) + g * 49 + (dy + 3) * 7 + (dx + 3);
        for (int i = 0; i < 256; i++) tiles[t][i] = tiles[base2 ? T_ASH : T_STONE][i];
        int base = g ? CY_FIRE_B : CY_FIRE_A;
        for (int y = 0; y < 16; y++) for (int x = 0; x < 16; x++) {
            float wx = dx * 16 + x - 8 + .5f, wy = dy * 16 + y - 8 + .5f;
            float d = 0; { float d2 = wx*wx + wy*wy; d = d2 > 0 ? (float)__builtin_sqrtf(d2) : 0; }
            float L = 1.f - d / (base2 ? 46.f : 54.f); if (L <= 0) continue;
            int lvl = (int)(L * (base2 ? 2.6f : 4.f) + BAYER[(y & 3) * 4 + (x & 3)] / 16.f);
            if (lvl <= 0) continue;
            uint8_t c = tiles[t][y * 16 + x];
            if (c == INK && lvl < 3) continue;
            int fi = lvl - 1 + ((c == D2 || c == G1) ? 1 : 0); if (fi > 3) fi = 3;
            tiles[t][y * 16 + x] = (uint8_t)(base + fi);
        }
    }
}

/* ===== Mapa ===== */
static uint8_t mapT[MAP_H][MAP_W], mapA[MAP_H][MAP_W], zone[MAP_H][MAP_W];
static Prop props[1600]; static int nProps;
static int brx[40], bry[40], nBraz;
static int pathX[4000], pathY[4000], nPath;
static int litBase = T_LIT;   /* tiles de luz usados por place_brazier/place_light no mapa em construção */
static int inb(int x, int y) { return x >= 0 && y >= 0 && x < MAP_W && y < MAP_H; }
static void put(int x, int y, int t, int a) { if (inb(x, y)) { mapT[y][x] = (uint8_t)t; mapA[y][x] = (uint8_t)a; } }
static void add_prop(int x, int y, int spr, int flags) { if (nProps < 1600) { props[nProps].x = (uint16_t)x; props[nProps].y = (uint16_t)y; props[nProps].spr = (uint8_t)spr; props[nProps].flags = (uint8_t)flags; nProps++; } }
static void paint_path(int x, int y) {
    for (int j = 0; j < 2; j++) for (int i = 0; i < 2; i++) { int xx = x + i, yy = y + j;
        if (!inb(xx, yy) || zone[yy][xx]) continue;
        if (mapA[yy][xx] & A_WATER) { put(xx, yy, T_BRIDGE, A_NOPROP); zone[yy][xx] = 3; }
        else put(xx, yy, T_DIRT + (int)(hash2(xx, yy) % 3), A_NOPROP);
        if (nPath < 4000) { pathX[nPath] = xx; pathY[nPath] = yy; nPath++; }
    }
}
static void carve(int x0, int y0, int x1, int y1) {
    int x = x0, y = y0, guard = 0;
    while ((x != x1 || y != y1) && guard++ < 1000) {
        paint_path(x, y);
        if (x != x1 && (y == y1 || rndi(100) < 55)) x += x1 > x ? 1 : -1; else if (y != y1) y += y1 > y ? 1 : -1;
    }
    paint_path(x, y);
}
static int far_from_braziers(int x, int y, int d) {
    for (int i = 0; i < nBraz; i++) { int dx = brx[i] - x, dy = bry[i] - y; if (dx*dx + dy*dy < d*d) return 0; }
    return 1;
}
static void place_brazier(int x, int y) {
    if (nBraz >= 40 || !inb(x, y) || !far_from_braziers(x, y, 8)) return;
    if (mapA[y][x] & (A_WATER | A_SOLID)) return;
    int g = nBraz & 1;
    for (int dy = -3; dy <= 3; dy++) for (int dx = -3; dx <= 3; dx++) {
        int xx = x + dx, yy = y + dy; if (!inb(xx, yy) || dx*dx + dy*dy > 10) continue;
        if (mapA[yy][xx] & (A_WATER | A_SOLID) || zone[yy][xx] == 2 || zone[yy][xx] == 3) continue;
        put(xx, yy, litBase + g * 49 + (dy + 3) * 7 + (dx + 3), A_NOPROP);
    }
    mapA[y][x] |= A_SOLID;
    brx[nBraz] = x; bry[nBraz] = y; nBraz++;
    add_prop(x * 16 + 8, y * 16 + 12, SPR_BRAZIER, PF_BRAZIER);
}
static void place_light(int x, int y) {   /* poça de luz sem braseiro (fogo das casas) */
    if (nBraz >= 40 || !inb(x, y) || !far_from_braziers(x, y, 6)) return;
    int g = nBraz & 1;
    for (int dy = -3; dy <= 3; dy++) for (int dx = -3; dx <= 3; dx++) {
        int xx = x + dx, yy = y + dy; if (!inb(xx, yy) || dx*dx + dy*dy > 10) continue;
        if (mapA[yy][xx] & (A_WATER | A_SOLID)) continue;
        put(xx, yy, litBase + g * 49 + (dy + 3) * 7 + (dx + 3), mapA[yy][xx] & A_NOPROP);
    }
    brx[nBraz] = x; bry[nBraz] = y; nBraz++;
}
static void wall_h(int x0, int x1, int y, int gate0, int gate1) {   /* muro horizontal: topo + face */
    for (int x = x0; x <= x1; x++) { if (x >= gate0 && x <= gate1) continue;
        put(x, y, T_WTOP + (int)(hash2(x, y) % 2), A_SOLID | A_NOPROP); zone[y][x] = 2;
        put(x, y + 1, T_WFACE + (hash2(x, y) % 5 == 0), A_SOLID | A_NOPROP); zone[y + 1][x] = 2; }
}
static void wall_v(int x, int y0, int y1, int gate0, int gate1) {
    for (int y = y0; y <= y1; y++) { if (y >= gate0 && y <= gate1) continue;
        put(x, y, T_WTOP + (int)(hash2(x, y) % 2), A_SOLID | A_NOPROP); zone[y][x] = 2; }
    if (!(y1 >= gate0 && y1 <= gate1)) { put(x, y1 + 1, T_WFACE, A_SOLID | A_NOPROP); zone[y1 + 1][x] = 2; }
    if (gate0 > y0) { put(x, gate0, T_WFACE, A_SOLID | A_NOPROP); zone[gate0][x] = 2; }
}
static void map_reset(void) { nProps = 0; nBraz = 0; nPath = 0; }
static void build_fortaleza(void) {
    map_reset(); litBase = T_LIT;
    seed = 4242;
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        uint32_t h = hash2(x, y); int v = (h % 10 < 6) ? 0 : (h % 10 < 8) ? 1 : (h % 10 < 9) ? 2 : 3;
        put(x, y, T_GRASS + v, A_GRASS); zone[y][x] = 0;
    }
    /* rio a serpentear de norte a sul */
    for (int y = 0; y < MAP_H; y++) { int cx = 30 + (int)(5.f * __builtin_sinf(y / 8.f)) + (int)(2.f * __builtin_sinf(y / 3.1f));
        for (int x = cx - 1; x <= cx + 1; x++) put(x, y, T_WATER, A_WATER | A_SOLID | A_NOPROP); }
    /* lago a sudoeste */
    for (int y = 70; y < 92; y++) for (int x = 6; x < 26; x++) { float dx = (x - 15.5f) / 8.f, dy = (y - 80.5f) / 5.5f;
        if (dx*dx + dy*dy <= 1.f) put(x, y, T_WATER, A_WATER | A_SOLID | A_NOPROP); }
    for (int y = 1; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++)
        if ((mapA[y][x] & A_WATER) && !(mapA[y-1][x] & A_WATER)) mapT[y][x] = T_WATER_N;
    /* pátio da fortaleza */
    for (int y = 36; y <= 60; y++) for (int x = 36; x <= 60; x++) { put(x, y, T_STONE + (int)(hash2(x, y) % 4), A_NOPROP); zone[y][x] = 1; }
    wall_h(36, 60, 36, 46, 50); wall_h(36, 60, 59, 46, 50);
    wall_v(36, 36, 58, 46, 50); wall_v(60, 36, 58, 46, 50);
    /* caminhos dos portões para os pontos de interesse */
    carve(47, 34, 16, 16); carve(48, 34, 78, 18); carve(62, 47, 80, 80); carve(47, 62, 20, 66); carve(34, 47, 4, 47);
    /* cemitério a noroeste */
    for (int y = 9; y < 25; y++) for (int x = 9; x < 25; x++) if (!(mapA[y][x] & A_NOPROP) && hash2(x, y) % 3 == 0) put(x, y, T_MOSS + (int)(hash2(x, y) % 2), A_NOPROP);
    for (int y = 10; y < 25; y += 3) for (int x = 10; x < 25; x += 3) {
        int gx = x + (int)(hash2(x, y) % 2), gy = y;
        if (mapA[gy][gx] & (A_NOPROP & 0) ) continue;
        if (zone[gy][gx] || (hash2(gx, gy) % 10) < 3 || (mapT[gy][gx] >= T_DIRT && mapT[gy][gx] < T_STONE)) continue;
        mapA[gy][gx] = A_SOLID | A_NOPROP; add_prop(gx * 16 + 8, gy * 16 + 14, SPR_GRAVE, PF_SH_S);
    }
    /* santuário corrompido a nordeste */
    for (int y = 10; y < 28; y++) for (int x = 70; x < 88; x++) { int dx = x - 78, dy = y - 18, d2 = dx*dx + dy*dy;
        if (d2 > 64) continue;
        if (d2 >= 9 && d2 <= 13) put(x, y, T_RUNE, A_NOPROP);
        else if (mapT[y][x] < T_DIRT || mapT[y][x] >= T_STONE) put(x, y, T_CORRUPT + (d2 < 36), A_NOPROP);
        else mapA[y][x] |= A_NOPROP; }
    mapA[18][78] |= A_SOLID; add_prop(78 * 16 + 8, 18 * 16 + 14, SPR_STATUE, PF_SH_M);
    /* ruínas a sudeste: muros partidos, entulho e pilares tombados */
    for (int k = 0; k < 14; k++) { int x = 68 + rndi(22), y = 66 + rndi(24), len = 3 + rndi(4);
        if (k & 1) { int ok = 1; for (int i = 0; i < len; i++) if (zone[y][x+i] || (mapA[y][x+i] & A_NOPROP) || (mapA[y+1][x+i] & A_NOPROP)) ok = 0;
            if (ok) wall_h(x, x + len - 1, y, -1, -1); }
        else { int ok = 1; for (int i = 0; i <= len + 1; i++) if (zone[y+i][x] || (mapA[y+i][x] & A_NOPROP)) ok = 0;
            if (ok) wall_v(x, y, y + len - 1, -1, -1); } }
    for (int k = 0; k < 30; k++) { int x = 66 + rndi(26), y = 64 + rndi(28);
        if (mapA[y][x] & (A_NOPROP | A_SOLID)) continue;
        if (chance(30)) { mapA[y][x] |= A_SOLID | A_NOPROP; add_prop(x * 16 + 8, y * 16 + 14, SPR_PILLAR, PF_SH_S); }
        else { put(x, y, T_RUBBLEF, A_NOPROP); add_prop(x * 16 + 8, y * 16 + 13, SPR_RUBBLE, 0); } }
    /* pilares e estandartes no pátio */
    { static const int px[4][2] = {{40,44},{56,44},{40,52},{56,52}};
      for (int i = 0; i < 4; i++) { mapA[px[i][1]][px[i][0]] |= A_SOLID; add_prop(px[i][0] * 16 + 8, px[i][1] * 16 + 14, SPR_PILLAR, PF_SH_S); } }
    add_prop(43 * 16 + 8, 37 * 16 + 15, SPR_BANNER, 0); add_prop(53 * 16 + 8, 37 * 16 + 15, SPR_BANNER, 0);
    /* braseiros: pátio, pontos de interesse e ao longo dos caminhos */
    place_brazier(42, 41); place_brazier(54, 41); place_brazier(42, 55); place_brazier(54, 55);
    place_brazier(14, 13); place_brazier(22, 22); place_brazier(74, 74); place_brazier(84, 84); place_brazier(70, 18); place_brazier(22, 66);
    for (int i = 40; i < nPath; i += 23) { int x = pathX[i] + 2, y = pathY[i]; if (far_from_braziers(x, y, 14)) place_brazier(x, y); }
    /* bordo do mapa: floresta densa e sólida */
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++)
        if (x < 3 || y < 3 || x >= MAP_W - 3 || y >= MAP_H - 3) { mapA[y][x] |= A_SOLID | A_NOPROP;
            if (mapA[y][x] & A_WATER) continue;
            if (((x + y) % 3 == 0) && (x % 2 == 0)) add_prop(x * 16 + 8, y * 16 + 15, SPR_TREE, 0); }
    /* árvores e arbustos na relva, com espaço entre si */
    for (int y = 4; y < MAP_H - 4; y++) for (int x = 4; x < MAP_W - 4; x++) {
        if (mapA[y][x] != A_GRASS) continue;
        uint32_t h = hash2(x * 3 + 1, y * 5 + 2);
        int ok = 1; for (int j = -2; j <= 2; j++) for (int i = -2; i <= 2; i++) if (mapA[y+j][x+i] & (A_SOLID | A_NOPROP | A_WATER)) ok = 0;
        if (ok && h % 100 < 5) { mapA[y][x] |= A_SOLID; add_prop(x * 16 + 8, y * 16 + 15, SPR_TREE, PF_SH_L);
            for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) mapA[y+j][x+i] |= A_NOPROP; }
        else if (h % 100 >= 95 && h % 100 < 98 && !(mapA[y][x] & A_NOPROP)) { add_prop(x * 16 + 8, y * 16 + 14, SPR_BUSH, 0); mapA[y][x] |= A_NOPROP; }
    }
}
static void sort_props(void) {   /* ordenar adereços por y (pés) */
    for (int i = 1; i < nProps; i++) { Prop t = props[i]; int j = i - 1; while (j >= 0 && props[j].y > t.y) { props[j+1] = props[j]; j--; } props[j+1] = t; }
}
static int free_area(int x0, int y0, int x1, int y1) {
    for (int y = y0; y <= y1; y++) for (int x = x0; x <= x1; x++) if (!inb(x, y) || (mapA[y][x] & (A_SOLID | A_NOPROP | A_WATER))) return 0;
    return 1;
}
static void house(int cx, int by, int ruined) {   /* casa com 4x2 tiles de base; cx = coluna central, by = linha de baixo */
    if (!free_area(cx - 2, by - 1, cx + 1, by)) return;
    for (int y = by - 1; y <= by; y++) for (int x = cx - 2; x <= cx + 1; x++) mapA[y][x] |= A_SOLID | A_NOPROP;
    for (int y = by - 3; y <= by + 1; y++) for (int x = cx - 3; x <= cx + 2; x++) if (inb(x, y)) mapA[y][x] |= A_NOPROP;
    add_prop(cx * 16, by * 16 + 15, SPR_HOUSE + ruined, ruined ? PF_RUINED : PF_FIRE);
    place_light(cx, by + 1);
}
static void build_aldeia(void) {
    map_reset(); litBase = T_LIT2;
    seed = 777;
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {
        uint32_t h = hash2(x + 300, y); put(x, y, T_ASH + (int)(h % 7 < 4 ? 0 : h % 7 < 6 ? 1 : 2), 0); zone[y][x] = 0; }
    /* arrozais alagados */
    static const int pd[3][4] = {{8,58,24,78},{70,12,88,28},{68,64,86,80}};
    for (int k = 0; k < 3; k++) for (int y = pd[k][1]; y <= pd[k][3]; y++) for (int x = pd[k][0]; x <= pd[k][2]; x++)
        put(x, y, T_PADDY, A_WATER | A_SOLID | A_NOPROP);
    for (int y = 1; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++)
        if ((mapA[y][x] & A_WATER) && !(mapA[y-1][x] & A_WATER)) mapT[y][x] = T_PADDY_N;
    /* ruas de pedra: norte-sul e este-oeste */
    for (int y = 4; y < MAP_H - 4; y++) for (int x = 47; x <= 48; x++) put(x, y, T_SPATH + (int)(hash2(x, y) % 2), A_NOPROP);
    for (int x = 4; x < MAP_W - 4; x++) for (int y = 47; y <= 48; y++) if (!(mapA[y][x] & A_WATER)) put(x, y, T_SPATH + (int)(hash2(x, y) % 2), A_NOPROP);
    /* torii à entrada norte */
    add_prop(48 * 16, 9 * 16 + 15, SPR_TORII, 0); mapA[9][46] |= A_SOLID; mapA[9][49] |= A_SOLID;
    for (int x = 44; x <= 51; x++) for (int y = 6; y <= 10; y++) mapA[y][x] |= A_NOPROP;
    /* casas dos dois lados das ruas: a maioria em chamas, algumas desabadas */
    static const int ys[7] = {16, 24, 32, 40, 58, 66, 76};
    for (int i = 0; i < 7; i++) { house(44, ys[i], hash2(i, 1) % 10 < 3); house(53, ys[i], hash2(i, 2) % 10 < 3); }
    static const int xs[7] = {12, 20, 28, 36, 60, 68, 78};
    for (int i = 0; i < 7; i++) { house(xs[i], 45, hash2(i, 3) % 10 < 3); house(xs[i], 52, hash2(i, 4) % 10 < 3); }
    /* lanternas de pedra ao longo das ruas */
    for (int y = 12; y < 86; y += 7) { if (y > 43 && y < 53) continue;
        if (!(mapA[y][45] & (A_SOLID | A_WATER))) { mapA[y][45] |= A_SOLID; add_prop(45 * 16 + 8, y * 16 + 14, SPR_LANTERN, PF_SH_S); }
        if (!(mapA[y][50] & (A_SOLID | A_WATER))) { mapA[y][50] |= A_SOLID; add_prop(50 * 16 + 8, y * 16 + 14, SPR_LANTERN, PF_SH_S); } }
    /* bordo: floresta densa */
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++)
        if (x < 3 || y < 3 || x >= MAP_W - 3 || y >= MAP_H - 3) { mapA[y][x] |= A_SOLID | A_NOPROP;
            if (((x + y) % 3 == 0) && (x % 2 == 0)) add_prop(x * 16 + 8, y * 16 + 15, SPR_TREE, 0); }
    /* cerejeiras queimadas, vigas caídas e entulho */
    for (int y = 5; y < MAP_H - 5; y++) for (int x = 5; x < MAP_W - 5; x++) {
        if (mapA[y][x]) continue;
        uint32_t h = hash2(x * 7 + 3, y * 11 + 5);
        int ok = 1; for (int j = -2; j <= 2; j++) for (int i = -2; i <= 2; i++) if (mapA[y+j][x+i] & (A_SOLID | A_NOPROP | A_WATER)) ok = 0;
        if (ok && h % 100 < 4) { mapA[y][x] |= A_SOLID; add_prop(x * 16 + 8, y * 16 + 15, SPR_SAKURA, PF_SH_L);
            for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) mapA[y+j][x+i] |= A_NOPROP; }
        else if (h % 100 >= 96) { add_prop(x * 16 + 8, y * 16 + 14, SPR_BEAMS, 0); mapA[y][x] |= A_NOPROP; }
        else if (h % 100 == 95) { add_prop(x * 16 + 8, y * 16 + 13, SPR_RUBBLE, 0); mapA[y][x] |= A_NOPROP; }
    }
}
static void build_pantano(void) {
    map_reset(); seed = 4242;
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) {   /* lama e erva escura */
        uint32_t h = hash2(x + 900, y * 3); int lama = ((x / 6 + y / 5 + (int)(h % 3)) % 4) != 0;
        put(x, y, lama ? T_MUD + (int)(h % 7 < 4 ? 0 : h % 7 < 6 ? 1 : 2) : T_GRASS + (int)(h % 3), lama ? 0 : A_GRASS); zone[y][x] = 0; }
    for (int k = 0; k < 22; k++) {   /* charcos de água estagnada (não se atravessam) */
        int cx = 6 + rndi(84), cy = 6 + rndi(84), rx = 3 + rndi(6), ry = 2 + rndi(5);
        if (iabs2(cx - 48) < 10 && cy > 40) continue;   /* deixa livre o caminho do início e o centro */
        for (int y = cy - ry; y <= cy + ry; y++) for (int x = cx - rx; x <= cx + rx; x++) {
            float dx = (x - cx) / (float)rx, dy = (y - cy) / (float)ry;
            if (dx * dx + dy * dy <= 1.f && inb(x, y)) put(x, y, T_SWAMP, A_WATER | A_SOLID | A_NOPROP); } }
    for (int y = 1; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++)
        if ((mapA[y][x] & A_WATER) && !(mapA[y-1][x] & A_WATER)) mapT[y][x] = T_SWAMP_N;
    /* passadiços: do início ao centro e do centro aos cantos (sobre a água viram ponte) */
    carve(48, 86, 48, 48); carve(48, 48, 14, 14); carve(48, 48, 82, 14); carve(48, 48, 14, 82); carve(48, 48, 82, 82); carve(20, 48, 76, 48);
    for (int y = 44; y <= 52; y++) for (int x = 44; x <= 52; x++) if (mapA[y][x] & A_WATER) put(x, y, T_MUD, A_NOPROP);
    /* bordo: salgueiros densos */
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++)
        if (x < 3 || y < 3 || x >= MAP_W - 3 || y >= MAP_H - 3) { mapA[y][x] |= A_SOLID | A_NOPROP;
            if (((x + y) % 4 == 0) && (x % 3 == 0)) add_prop(x * 16 + 8, y * 16 + 15, SPR_SALGUEIRO, 0); }
    /* salgueiros, juncos à beira da água e cogumelos */
    for (int y = 5; y < MAP_H - 5; y++) for (int x = 5; x < MAP_W - 5; x++) {
        if (mapA[y][x] & (A_SOLID | A_NOPROP | A_WATER)) continue;
        uint32_t h = hash2(x * 5 + 11, y * 13 + 7);
        int perto = 0; for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) if (mapA[y+j][x+i] & A_WATER) perto = 1;
        int ok = 1; for (int j = -2; j <= 2; j++) for (int i = -2; i <= 2; i++) if (mapA[y+j][x+i] & (A_SOLID | A_NOPROP | A_WATER)) ok = 0;
        if (ok && h % 100 < 3) { mapA[y][x] |= A_SOLID; add_prop(x * 16 + 8, y * 16 + 15, SPR_SALGUEIRO, PF_SH_L);
            for (int j = -1; j <= 1; j++) for (int i = -1; i <= 1; i++) mapA[y+j][x+i] |= A_NOPROP; }
        else if (perto && h % 100 < 35) { add_prop(x * 16 + 8, y * 16 + 15, SPR_JUNCOS, 0); mapA[y][x] |= A_NOPROP; }
        else if (h % 100 >= 98) { add_prop(x * 16 + 8, y * 16 + 14, SPR_COGUMELOS, 0); mapA[y][x] |= A_NOPROP; }
    }
}
static uint8_t mapT0[MAP_H][MAP_W], mapA0[MAP_H][MAP_W], mapT1[MAP_H][MAP_W], mapA1[MAP_H][MAP_W], mapT2[MAP_H][MAP_W], mapA2[MAP_H][MAP_W];
static Prop props0[1600], props1[1600], props2[1600]; static int nProps0, nProps1, nProps2;
static void build_maps(void) {
    build_fortaleza(); sort_props();
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) { mapT0[y][x] = mapT[y][x]; mapA0[y][x] = mapA[y][x]; }
    for (int i = 0; i < nProps; i++) props0[i] = props[i];
    nProps0 = nProps;
    fprintf(stderr, "fortaleza: %d adereços, %d luzes\n", nProps, nBraz);
    build_aldeia(); sort_props();
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) { mapT1[y][x] = mapT[y][x]; mapA1[y][x] = mapA[y][x]; }
    for (int i = 0; i < nProps; i++) props1[i] = props[i];
    nProps1 = nProps;
    fprintf(stderr, "aldeia: %d adereços, %d luzes\n", nProps, nBraz);
    build_pantano(); sort_props();
    for (int y = 0; y < MAP_H; y++) for (int x = 0; x < MAP_W; x++) { mapT2[y][x] = mapT[y][x]; mapA2[y][x] = mapA[y][x]; }
    for (int i = 0; i < nProps; i++) props2[i] = props[i];
    nProps2 = nProps;
    fprintf(stderr, "pântano: %d adereços\n", nProps);
}

/* ===== Fonte 5x7 ===== */
typedef struct { int code; const char *r[7]; } Glyph;
static const Glyph GL[] = {
 {'A',{".###.","#...#","#...#","#####","#...#","#...#","#...#"}},{'B',{"####.","#...#","#...#","####.","#...#","#...#","####."}},
 {'C',{".####","#....","#....","#....","#....","#....",".####"}},{'D',{"####.","#...#","#...#","#...#","#...#","#...#","####."}},
 {'E',{"#####","#....","#....","####.","#....","#....","#####"}},{'F',{"#####","#....","#....","####.","#....","#....","#...."}},
 {'G',{".####","#....","#....","#.###","#...#","#...#",".###."}},{'H',{"#...#","#...#","#...#","#####","#...#","#...#","#...#"}},
 {'I',{"#####","..#..","..#..","..#..","..#..","..#..","#####"}},{'J',{"..###","...#.","...#.","...#.","#..#.","#..#.",".##.."}},
 {'K',{"#...#","#..#.","#.#..","##...","#.#..","#..#.","#...#"}},{'L',{"#....","#....","#....","#....","#....","#....","#####"}},
 {'M',{"#...#","##.##","#.#.#","#.#.#","#...#","#...#","#...#"}},{'N',{"#...#","##..#","#.#.#","#..##","#...#","#...#","#...#"}},
 {'O',{".###.","#...#","#...#","#...#","#...#","#...#",".###."}},{'P',{"####.","#...#","#...#","####.","#....","#....","#...."}},
 {'Q',{".###.","#...#","#...#","#...#","#.#.#","#..#.",".##.#"}},{'R',{"####.","#...#","#...#","####.","#.#..","#..#.","#...#"}},
 {'S',{".####","#....","#....",".###.","....#","....#","####."}},{'T',{"#####","..#..","..#..","..#..","..#..","..#..","..#.."}},
 {'U',{"#...#","#...#","#...#","#...#","#...#","#...#",".###."}},{'V',{"#...#","#...#","#...#","#...#","#...#",".#.#.","..#.."}},
 {'W',{"#...#","#...#","#...#","#.#.#","#.#.#","##.##","#...#"}},{'X',{"#...#","#...#",".#.#.","..#..",".#.#.","#...#","#...#"}},
 {'Y',{"#...#","#...#",".#.#.","..#..","..#..","..#..","..#.."}},{'Z',{"#####","....#","...#.","..#..",".#...","#....","#####"}},
 {'0',{".###.","#...#","#..##","#.#.#","##..#","#...#",".###."}},{'1',{"..#..",".##..","..#..","..#..","..#..","..#..",".###."}},
 {'2',{".###.","#...#","....#","...#.","..#..",".#...","#####"}},{'3',{"####.","....#","....#",".###.","....#","....#","####."}},
 {'4',{"...#.","..##.",".#.#.","#..#.","#####","...#.","...#."}},{'5',{"#####","#....","####.","....#","....#","#...#",".###."}},
 {'6',{".###.","#....","#....","####.","#...#","#...#",".###."}},{'7',{"#####","....#","...#.","..#..",".#...",".#...",".#..."}},
 {'8',{".###.","#...#","#...#",".###.","#...#","#...#",".###."}},{'9',{".###.","#...#","#...#",".####","....#","....#",".###."}},
 {':',{".....","..#..","..#..",".....","..#..","..#..","....."}},{'+',{".....","..#..","..#..","#####","..#..","..#..","....."}},
 {'-',{".....",".....",".....","#####",".....",".....","....."}},{'.',{".....",".....",".....",".....",".....","..#..","..#.."}},
 {'%',{"##..#","##..#","...#.","..#..",".#...","#..##","#..##"}},{'!',{"..#..","..#..","..#..","..#..","..#..",".....","..#.."}},
 {'/',{"....#","....#","...#.","..#..",".#...","#....","#...."}},
 {'(',{"...#.","..#..",".#...",".#...",".#...","..#..","...#."}},{')',{".#...","..#..","...#.","...#.","...#.","..#..",".#..."}},
 {',',{".....",".....",".....",".....","..#..","..#..",".#..."}},{'?',{".###.","#...#","....#","...#.","..#..",".....","..#.."}},
};
static uint8_t font[NGLYPH][GLYPH_H];
static void glyph_from(int idx, int code, int accent) {   /* accent: 0 nada, 1 agudo, 2 circunflexo, 3 til, 4 grave, 5 cedilha */
    for (unsigned g = 0; g < sizeof(GL) / sizeof(GL[0]); g++) if (GL[g].code == code)
        for (int r = 0; r < 7; r++) { uint8_t m = 0; for (int c = 0; c < 5; c++) if (GL[g].r[r][c] == '#') m |= (uint8_t)(16 >> c); font[idx][r + 2] = m; }
    static const uint8_t acc[6][2] = {{0,0},{0x04,0x08},{0x04,0x0a},{0x0a,0x14},{0x08,0x04},{0,0}};
    font[idx][0] = acc[accent][0]; font[idx][1] = acc[accent][1];
    if (accent == 5) font[idx][9] = 0x0c;
}
static void build_font(void) {
    for (int c = 32; c < 96; c++) glyph_from(c - 32, c, 0);
    static const int base[12] = {'A','A','A','A','E','E','I','O','O','O','U','C'};
    static const int acc[12]  = { 1,  2,  3,  4,  1,  2,  1,  1,  2,  3,  1,  5 };
    for (int i = 0; i < 12; i++) glyph_from(64 + i, base[i], acc[i]);   /* Á Â Ã À É Ê Í Ó Ô Õ Ú Ç */
}

/* ===== Saída ===== */
static void dump8(const char *name, const uint8_t *d, uint32_t n) {
    printf("static const uint8_t %s[%u] __attribute__((aligned(4))) = {\n", name, n);
    for (uint32_t i = 0; i < n; i++) printf("%d%s", d[i], (i + 1) % 40 ? "," : ",\n");
    printf("};\n");
}
int main(void) {
    init_ramps();
    build_sprites();
    build_tiles();
    build_maps();
    build_font();
    build_title();
    build_crypt();
    printf("/* Gerado por tools/gerar_assets.c — não editar à mão. */\n#ifndef ASSETS_H\n#define ASSETS_H\n#include <stdint.h>\n#include \"mundo.h\"\n");
    printf("static const uint16_t PAL_CRAM[%d] = {", NCOL);
    for (int i = 0; i < NCOL; i++) { uint32_t c = PAL_HEX[i]; int r = (c>>16)&255, g = (c>>8)&255, b = c&255;
        printf("0x%04x%s", ((b>>3)<<10)|((g>>3)<<5)|(r>>3), i + 1 < NCOL ? "," : ""); }
    printf("};\nstatic const uint8_t PAL_LIGHTER[%d] = {", NCOL);
    for (int i = 0; i < NCOL; i++) printf("%d%s", LIGHTER[i] == i ? (i == M0 ? M1 : i == M1 ? M2 : i == E0 ? E1 : i) : LIGHTER[i], i + 1 < NCOL ? "," : "");
    printf("};\n#define NTILES %d\n", NTILES);
    dump8("TILES", &tiles[0][0], NTILES * 256);
    dump8("MAP0_T", &mapT0[0][0], MAP_W * MAP_H);
    dump8("MAP0_A", &mapA0[0][0], MAP_W * MAP_H);
    dump8("MAP1_T", &mapT1[0][0], MAP_W * MAP_H);
    dump8("MAP1_A", &mapA1[0][0], MAP_W * MAP_H);
    dump8("MAP2_T", &mapT2[0][0], MAP_W * MAP_H);
    dump8("MAP2_A", &mapA2[0][0], MAP_W * MAP_H);
    printf("static const SprDesc SPR[%d] = {\n", SPR_COUNT);
    for (int i = 0; i < SPR_COUNT; i++) printf("{%u,%u,%u,%u,%u},\n", desc[i].w, desc[i].h, desc[i].ax, desc[i].ay, desc[i].off);
    printf("};\n");
    dump8("SPR_DATA", sprData, sprLen);
    for (int m = 0; m < 3; m++) { const Prop *pp = m == 2 ? props2 : m ? props1 : props0; int n = m == 2 ? nProps2 : m ? nProps1 : nProps0;
        printf("#define NPROPS%d %d\nstatic const Prop PROPS%d[%d] = {\n", m, n, m, n);
        for (int i = 0; i < n; i++) printf("{%u,%u,%u,%u},%s", pp[i].x, pp[i].y, pp[i].spr, pp[i].flags, (i + 1) % 8 ? "" : "\n");
        printf("};\n"); }
    dump8("FONT", &font[0][0], NGLYPH * GLYPH_H);
    dump8("TSKY", &tsky[0][0], TSKY_H * 320);
    dump8("TFG", &tfg[0][0], 224 * 320);
    dump8("TCRYPT", &tcrypt[0][0], 224 * 320);
    printf("static const int16_t SINT[256] = {");
    for (int i = 0; i < 256; i++) printf("%d%s", (int)(__builtin_sinf(i * 6.2831853f / 256.f) * 256.f), i < 255 ? "," : "");
    printf("};\n#endif\n");
    fprintf(stderr, "sprites: %u bytes, tiles: %d\n", sprLen, NTILES);
    return 0;
}
