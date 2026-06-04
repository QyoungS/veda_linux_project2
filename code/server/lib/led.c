#include <stdio.h>
#include <wiringPi.h>
#include <softPwm.h>
#include "led.h"
#include "device_log.h"

/* Change only these pin numbers */
#define LED_PIN      12
#define PWM_RANGE    255
#define LED_ACTIVE_LOW 1

static int initialized = 0;
static int pwm_ready = 0;

static int led_pwm_value(int value)
{
    if (LED_ACTIVE_LOW) {
        return PWM_RANGE - value;
    }

    return value;
}

static void led_init(void)
{
    if (initialized) {
        return;
    }

    DEVICE_LOG("[device] LED init: pin=%d pwm_range=%d\n", LED_PIN, PWM_RANGE);

    wiringPiSetupGpio();

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LED_ACTIVE_LOW ? HIGH : LOW);

    initialized = 1;
}

int led_on(void)
{
    DEVICE_LOG("[device] LED command: on\n");

    led_init();

    if (pwm_ready) {
        softPwmWrite(LED_PIN, led_pwm_value(PWM_RANGE));
    } else {
        digitalWrite(LED_PIN, LED_ACTIVE_LOW ? LOW : HIGH);
    }
    DEVICE_LOG("[device] LED ON\n");

    return 0;
}

int led_off(void)
{
    DEVICE_LOG("[device] LED command: off\n");

    led_init();

    if (pwm_ready) {
        softPwmWrite(LED_PIN, led_pwm_value(0));
    } else {
        digitalWrite(LED_PIN, LED_ACTIVE_LOW ? HIGH : LOW);
    }
    DEVICE_LOG("[device] LED OFF\n");

    return 0;
}

int led_brightness(int value)
{
    int requested = value;

    DEVICE_LOG("[device] LED command: brightness=%d\n", value);

    led_init();

    if (value < 0) {
        value = 0;
    }

    if (value > PWM_RANGE) {
        value = PWM_RANGE;
    }

    if (value != requested) {
        DEVICE_LOG("[device] LED brightness adjusted: %d -> %d\n", requested, value);
    }

    if (!pwm_ready) {
        DEVICE_LOG("[device] LED PWM init: pin=%d range=%d\n", LED_PIN, PWM_RANGE);
        softPwmCreate(LED_PIN, led_pwm_value(0), PWM_RANGE);
        pwm_ready = 1;
    }

    softPwmWrite(LED_PIN, led_pwm_value(value));
    DEVICE_LOG("[device] LED brightness: %d\n", value);

    return 0;
}
