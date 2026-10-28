/*
 * Cinzas da Eternidade — lado do 68000 (Mega Drive).
 * Substitui o src_md/main.c do exemplo 32x-skeleton do Marsdev.
 *
 * O único trabalho do 68000 neste jogo é ler os comandos e passá-los aos SH-2 pelos registos de
 * comunicação COMM8 (porta 1) e COMM10 (porta 2). O SH-2 principal lê-os diretamente.
 *
 * Porquê uma versão própria: depois de mudar a linha TH do comando é preciso esperar alguns ciclos
 * antes de ler, senão o hardware real (e o MiSTer, que o reproduz com rigor) devolve o valor antigo.
 * Os emuladores respondem logo, por isso o problema só aparecia no MiSTer. As leituras também ficam
 * espaçadas (mais de 1,5 ms) para os comandos de 6 botões não entrarem no modo de botões extra.
 *
 * Formato entregue (botões a 1 quando carregados):
 *   bit 0 cima, 1 baixo, 2 esquerda, 3 direita, 4 B, 5 C, 6 A, 7 START; 0xF000 = sem comando.
 */
#include <stdint.h>

#define PAD1_DATA (*(volatile uint8_t *)0xA10003)
#define PAD2_DATA (*(volatile uint8_t *)0xA10005)
#define PAD1_CTRL (*(volatile uint8_t *)0xA10009)
#define PAD2_CTRL (*(volatile uint8_t *)0xA1000B)
#define COMM8     (*(volatile uint16_t *)0xA15128)
#define COMM10    (*(volatile uint16_t *)0xA1512A)

static void espera(int n) { for (volatile int i = 0; i < n; i++) {} }

static uint16_t ler_porta(volatile uint8_t *data) {
    *data = 0x40; espera(4);
    uint8_t alto = *data;                 /* TH alto: C B direita esquerda baixo cima */
    *data = 0x00; espera(4);
    uint8_t baixo = *data;                /* TH baixo: START A 0 0 baixo cima */
    *data = 0x40;
    if (baixo & 0x0C) return 0xF000;      /* com TH baixo, um comando ligado devolve estes dois bits a 0 */
    return (uint16_t)((~alto & 0x3F) | ((~baixo & 0x30) << 2));
}

int main(void) {
    PAD1_CTRL = 0x40; PAD2_CTRL = 0x40;   /* TH como saída */
    PAD1_DATA = 0x40; PAD2_DATA = 0x40;
    for (;;) {
        COMM8 = ler_porta(&PAD1_DATA);
        COMM10 = ler_porta(&PAD2_DATA);
        espera(900);                      /* ~3 ms entre leituras */
    }
    return 0;
}
