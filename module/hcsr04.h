/*
 * HC-SR04 ultrasonic sensor module
 * https://cdn.sparkfun.com/datasheets/Sensors/Proximity/HCSR04.pdf
 */

#ifndef MODULE_HCSR04_H
#define MODULE_HCSR04_H

#include "timer.h"

typedef struct {
	UINT	trig_gpio;
	UINT	echo_gpio;
	UW	max_distance_mm;
	Timer	*timer;
} Hcsr04Device;

ER hcsr04_init(Hcsr04Device *device);
ER hcsr04_measure(const Hcsr04Device *device, UW *distance_mm);

#endif /* MODULE_HCSR04_H */
