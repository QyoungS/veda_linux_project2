#ifndef DEVICE_H
#define DEVICE_H

int led_on(void);
int led_off(void);
int led_brightness(int value);

int buzzer_on(void);
int buzzer_off(void);

int light_read(void);

int fnd_display(int num);

#endif
