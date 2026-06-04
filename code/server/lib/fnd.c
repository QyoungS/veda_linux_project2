#include <stdio.h>
#include <wiringPi.h>
#include "fnd.h"
#include "device_log.h"

#define FND_A 23
#define FND_B 18
#define FND_C 15
#define FND_D 14

static int initialized = 0;

static void fnd_init(void)
{
    if (initialized) {
        return;
    }

    DEVICE_LOG("[device] FND init: pins=%d,%d,%d,%d\n",
           FND_A, FND_B, FND_C, FND_D);

    wiringPiSetupGpio();

    pinMode(FND_A, OUTPUT);
    pinMode(FND_B, OUTPUT);
    pinMode(FND_C, OUTPUT);
    pinMode(FND_D, OUTPUT);

    initialized = 1;
}

int fnd_display(int num)
{
    int pins[4] = {FND_A, FND_B, FND_C, FND_D};

    int number[10][4] = {
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

    DEVICE_LOG("[device] FND command: display=%d\n", num);

    fnd_init();

    if (num < 0 || num > 9) {
        DEVICE_LOG("[device] invalid FND number: %d\n", num);
        return -1;
    }

    for (int i = 0; i < 4; i++) {
        digitalWrite(pins[i], number[num][i] ? HIGH : LOW);
    }

    DEVICE_LOG("[device] FND display: %d\n", num);

    return 0;
}
