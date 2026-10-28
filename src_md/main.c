/*
 * Cinzas da Eternidade — lado do 68000 (Mega Drive).
 * Substitui o src_md/main.c do exemplo 32x-skeleton do Marsdev.
 *
 * O único trabalho do 68000 neste jogo é ler os comandos e passá-los ao 32X.
 *  - Depois de mudar a linha TH do comando espera alguns ciclos antes de ler: o hardware real e o MiSTer
 *    precisam disso (os emuladores respondem logo e escondiam o problema).
 *  - Espaça as leituras (mais de 1,5 ms) para os comandos de 6 botões não entrarem no modo de botões extra.
 *  - Desliga as interrupções do 68000, para nenhum código do exemplo voltar a mexer no comando.
 *  - Usa registos de comunicação que o exemplo não usa:
 *      COMM12 = botões da porta 1 (byte baixo) e da porta 2 (byte alto)
 *      COMM14 = 0x8000 (marca de que este código corre) | 0x1000 sem comando na porta 1
 *               | 0x2000 sem comando na porta 2 | contador de 12 bits que sobe a cada leitura
 *  Botões: bit 0 cima, 1 baixo, 2 esquerda, 3 direita, 4 B, 5 C, 6 A, 7 START (1 = carregado).
 */
#include <stdint.h>

#define PAD1_DATA (*(volatile uint8_t *)0xA10003)
#define PAD2_DATA (*(volatile uint8_t *)0xA10005)
#define PAD1_CTRL (*(volatile uint8_t *)0xA10009)
#define PAD2_CTRL (*(volatile uint8_t *)0xA1000B)
#define COMM12    (*(volatile uint16_t *)0xA1512C)
#define COMM14    (*(volatile uint16_t *)0xA1512E)

static void espera(int n) { for (volatile int i = 0; i < n; i++) {} }

static int ler_porta(volatile uint8_t *data, uint8_t *botoes) {   /* devolve 0 se não houver comando */
    *data = 0x40; espera(4);
    uint8_t alto = *data;                 /* TH alto: C B direita esquerda baixo cima */
    *data = 0x00; espera(4);
    uint8_t baixo = *data;                /* TH baixo: START A 0 0 baixo cima */
    *data = 0x40;
    if (baixo & 0x0C) { *botoes = 0; return 0; }   /* com TH baixo, um comando ligado devolve estes bits a 0 */
    *botoes = (uint8_t)((~alto & 0x3F) | ((~baixo & 0x30) << 2));
    return 1;
}

int main(void) {
    __asm__ volatile ("move.w #0x2700, %sr");   /* sem interrupções */
    PAD1_CTRL = 0x40; PAD2_CTRL = 0x40;          /* TH como saída */
    PAD1_DATA = 0x40; PAD2_DATA = 0x40;
    uint16_t cont = 0;
    for (;;) {
        uint8_t b1, b2;
        int c1 = ler_porta(&PAD1_DATA, &b1), c2 = ler_porta(&PAD2_DATA, &b2);
        COMM12 = (uint16_t)(b1 | (b2 << 8));
        cont = (uint16_t)((cont + 1) & 0x0FFF);
        COMM14 = (uint16_t)(0x8000 | (c1 ? 0 : 0x1000) | (c2 ? 0 : 0x2000) | cont);
        espera(900);                             /* ~3 ms entre leituras */
    }
    return 0;
}
