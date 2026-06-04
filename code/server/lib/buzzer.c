#include <stdio.h>
#include <pthread.h>
#include <wiringPi.h>
#include <softTone.h>
#include "buzzer.h"

/* Change only these pin numbers */
#define BUZZER_PIN  21
#define BUZZER_FREQ 392

static int initialized = 0;
static int stop_requested = 0;
static int playing = 0;
static pthread_t play_thread;
static pthread_mutex_t buzzer_lock = PTHREAD_MUTEX_INITIALIZER;

static int healing_chime[] = {
    523, 659, 784, 1047, 0,
    784, 1047, 1319, 1568, 0,
    1319, 1568, 1760, 1568, 1319, 1047, 0,
    784, 988, 1175, 1568, 0,
    1047, 1319, 1568, 2093, 0
};

static int buzzer_should_stop(void)
{
    int should_stop;

    pthread_mutex_lock(&buzzer_lock);
    should_stop = stop_requested;
    pthread_mutex_unlock(&buzzer_lock);

    return should_stop;
}

static void buzzer_stop_output(void)
{
    softToneWrite(BUZZER_PIN, 0);
    digitalWrite(BUZZER_PIN, HIGH);
}

static void buzzer_init(void)
{
    if (initialized) {
        return;
    }

    wiringPiSetupGpio();

    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, HIGH);
    softToneCreate(BUZZER_PIN);
    softToneWrite(BUZZER_PIN, 0);

    initialized = 1;
}

static void *buzzer_play_thread(void *arg)
{
    int total = sizeof(healing_chime) / sizeof(healing_chime[0]);

    (void)arg;

    for (int i = 0; i < total; i++) {
        if (buzzer_should_stop()) {
            break;
        }

        softToneWrite(BUZZER_PIN, healing_chime[i]);

        for (int elapsed = 0; elapsed < 170; elapsed += 20) {
            if (buzzer_should_stop()) {
                break;
            }
            delay(20);
        }
    }

    buzzer_stop_output();

    pthread_mutex_lock(&buzzer_lock);
    playing = 0;
    stop_requested = 0;
    pthread_mutex_unlock(&buzzer_lock);

    return NULL;
}

int buzzer_on(void)
{
    buzzer_init();

    pthread_mutex_lock(&buzzer_lock);
    if (playing) {
        stop_requested = 1;
        pthread_mutex_unlock(&buzzer_lock);
        return 0;
    }

    stop_requested = 0;
    playing = 1;
    pthread_mutex_unlock(&buzzer_lock);

    if (pthread_create(&play_thread, NULL, buzzer_play_thread, NULL) != 0) {
        pthread_mutex_lock(&buzzer_lock);
        playing = 0;
        pthread_mutex_unlock(&buzzer_lock);
        return -1;
    }

    pthread_detach(play_thread);

    return 0;
}

int buzzer_off(void)
{
    buzzer_init();

    pthread_mutex_lock(&buzzer_lock);
    stop_requested = 1;
    pthread_mutex_unlock(&buzzer_lock);

    buzzer_stop_output();

    return 0;
}
