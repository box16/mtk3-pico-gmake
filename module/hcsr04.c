/*
 * HC-SR04 ultrasonic sensor module
 */

#include <tk/tkernel.h>
#include <bsp/libbsp.h>

#include "hcsr04.h"

#define HCSR04_SOUND_SPEED_MM_S		343000U
#define HCSR04_PULSE_READY_TIMEOUT_US	3000U

LOCAL UW hcsr04_ticks_to_mm(const Timer *timer, UD ticks)
{
	return (UW)((ticks * HCSR04_SOUND_SPEED_MM_S) /
		    (2ULL * (UD)timer->clk_hz));
}

LOCAL UD hcsr04_distance_mm_to_ticks(const Hcsr04Device *device, UW distance_mm)
{
	return ((UD)distance_mm * 2ULL * (UD)device->timer->clk_hz) /
	       HCSR04_SOUND_SPEED_MM_S;
}

/*指定したレベルになるまで待つ(ただし、timeout_ticks上限)*/
LOCAL ER hcsr04_wait_echo_level(const Hcsr04Device *device, UINT level,
				UD timeout_ticks, UD *timestamp)
{
	UD start;

	start = timer_get_ticks(device->timer);
	while (gpio_get_val(device->echo_gpio) != level) {
		if ((timer_get_ticks(device->timer) - start) > timeout_ticks) {
			return E_TMOUT;
		}
	}

	if (timestamp != NULL) {
		*timestamp = timer_get_ticks(device->timer);
	}
	return E_OK;
}

LOCAL void hcsr04_trigger(const Hcsr04Device *device)
{
	gpio_set_val(device->trig_gpio, 0);
	WaitUsec(2);
	gpio_set_val(device->trig_gpio, 1);
	WaitUsec(10);
	gpio_set_val(device->trig_gpio, 0);
}

ER hcsr04_init(Hcsr04Device *device)
{
	if (device == NULL || device->timer == NULL) {
		return E_PAR;
	}

	gpio_set_pin(device->trig_gpio, GPIO_MODE_OUT);
	gpio_set_pin(device->echo_gpio, GPIO_MODE_IN);
	gpio_set_val(device->trig_gpio, 0);

	return timer_start(device->timer);
}

ER hcsr04_measure(const Hcsr04Device *device, UW *distance_mm)
{
	UD timeout_ticks;
	UD ready_timeout_ticks;
	UD start_ticks;
	UD end_ticks;
	UD pulse_ticks;
	ER er;

	if (device == NULL || device->timer == NULL) {
		return E_PAR;
	}

	ready_timeout_ticks = timer_usec_to_ticks(device->timer,
						  HCSR04_PULSE_READY_TIMEOUT_US);
	timeout_ticks = hcsr04_distance_mm_to_ticks(device,
						    device->max_distance_mm);

	er = hcsr04_wait_echo_level(device, 0, ready_timeout_ticks, NULL);
	if (er != E_OK) {
		return er;
	}

	hcsr04_trigger(device);

	er = hcsr04_wait_echo_level(device, 1, ready_timeout_ticks, &start_ticks);
	if (er != E_OK) {
		return er;
	}
	er = hcsr04_wait_echo_level(device, 0, timeout_ticks, &end_ticks);
	if (er != E_OK) {
		return er;
	}

	pulse_ticks = end_ticks - start_ticks;
	if (distance_mm != NULL) {
		*distance_mm = hcsr04_ticks_to_mm(device->timer, pulse_ticks);
	}
	return E_OK;
}
