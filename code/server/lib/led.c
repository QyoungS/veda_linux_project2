#include <stdio.h>
#include <wiringPi.h>
#include <softPwm.h>
#include "led.h"

/* Hardware pin and PWM settings */
#define LED_PIN      12
#define PWM_RANGE    255
#define LED_ACTIVE_LOW 1

static int initialized = 0;
static int pwm_ready = 0;

static int led_pwm_value(int value) /* Convert brightness for active-low wiring. */
{
    if (LED_ACTIVE_LOW) {
        return PWM_RANGE - value;
    }

    return value;
}

static void led_init(void) /* Initialize GPIO once. */
{
    if (initialized) {
        return;
    }

    wiringPiSetupGpio();

    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LED_ACTIVE_LOW ? HIGH : LOW);

    initialized = 1;
}

int led_on(void) /* Turn the LED on. */
{
    led_init();

    if (pwm_ready) {
        softPwmWrite(LED_PIN, led_pwm_value(PWM_RANGE));
    } else {
        digitalWrite(LED_PIN, LED_ACTIVE_LOW ? LOW : HIGH);
    }
    return 0;
}

int led_off(void) /* Turn the LED off. */
{
    led_init();

    if (pwm_ready) {
        softPwmWrite(LED_PIN, led_pwm_value(0));
    } else {
        digitalWrite(LED_PIN, LED_ACTIVE_LOW ? HIGH : LOW);
    }
    return 0;
}

int led_brightness(int value) /* Set LED brightness from 0 to 255. */
{
    led_init();

    if (value < 0) {
        value = 0;
    }

    if (value > PWM_RANGE) {
        value = PWM_RANGE;
    }

    if (!pwm_ready) {
        softPwmCreate(LED_PIN, led_pwm_value(0), PWM_RANGE);
        pwm_ready = 1;
    }

    softPwmWrite(LED_PIN, led_pwm_value(value));

    return 0;
}
