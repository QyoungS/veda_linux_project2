#include <stdio.h>
#include <wiringPi.h>
#include "fnd.h"

#define FND_A 23
#define FND_B 18
#define FND_C 15
#define FND_D 14

static int initialized = 0;

static void fnd_init(void) /* Initialize 7-segment GPIO pins once. */
{
    if (initialized) {
        return;
    }

    wiringPiSetupGpio();

    pinMode(FND_A, OUTPUT);
    pinMode(FND_B, OUTPUT);
    pinMode(FND_C, OUTPUT);
    pinMode(FND_D, OUTPUT);

    initialized = 1;
}

int fnd_display(int num) /* Display one digit from 0 to 9. */
{
    int pins[4] = {FND_A, FND_B, FND_C, FND_D};

    int number[10][4] = { /* BCD output table for digits 0-9. */
        {0, 0, 0, 0},
        {0, 0, 0, 1},
        {0, 0, 1, 0},
        {0, 0, 1, 1},
        {0, 1, 0, 0},
        {0, 1, 0, 1},
        {0, 1, 1, 0},
        {0, 1, 1, 1},
        {1, 0, 0, 0},
        {1, 0, 0, 1}
    };

    fnd_init();

    if (num < 0 || num > 9) {
        return -1;
    }

    for (int i = 0; i < 4; i++) {
        digitalWrite(pins[i], number[num][i] ? HIGH : LOW);
    }

    return 0;
}
