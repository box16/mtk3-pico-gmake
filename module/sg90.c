/*
 * Servo motor (PWM) module
 */

#include <tk/tkernel.h>
#include <sys/sysdef.h>
#include <bsp/libbsp.h>

#include "sg90.h"

#define SERVO_PULSE_MIN_US	500
#define SERVO_PULSE_MID_US	1450
#define SERVO_PULSE_MAX_US	2400

#define SERVO_PWM_FREQ_HZ	50U
#define SERVO_PWM_CLK_HZ	1000000U

LOCAL UW angle_to_pulse_us(INT angle_deg)
{
	INT clamped = angle_deg;
	INT pulse;
	INT span_us;

	if (clamped > SERVO_ANGLE_MAX) {
		clamped = SERVO_ANGLE_MAX;
	} else if (clamped < -SERVO_ANGLE_MAX) {
		clamped = -SERVO_ANGLE_MAX;
	}

	span_us = (INT)SERVO_PULSE_MAX_US - (INT)SERVO_PULSE_MID_US;
	pulse = (INT)SERVO_PULSE_MID_US + (clamped * span_us) / SERVO_ANGLE_MAX;
	if (pulse < (INT)SERVO_PULSE_MIN_US) {
		pulse = (INT)SERVO_PULSE_MIN_US;
	} else if (pulse > (INT)SERVO_PULSE_MAX_US) {
		pulse = (INT)SERVO_PULSE_MAX_US;
	}

	return (UW)pulse;
}

ER sg90_init(UINT pin_no)
{
	UW divider;
	UW period_counts;
	UW wrap;
	ER er;

	if (SERVO_PWM_CLK_HZ == 0 || SERVO_PWM_FREQ_HZ == 0 ||
	    SERVO_ANGLE_MAX <= 0) {
		return E_PAR;
	}

	divider = CLK_PERI_FREQ / SERVO_PWM_CLK_HZ;
	if (divider == 0 || divider > 0xFF) {
		return E_PAR;
	}

	if (SERVO_PWM_CLK_HZ < SERVO_PWM_FREQ_HZ) {
		return E_PAR;
	}

	period_counts = SERVO_PWM_CLK_HZ / SERVO_PWM_FREQ_HZ;
	if (period_counts == 0 || period_counts > 0x10000) {
		return E_PAR;
	}
	wrap = period_counts - 1U;

	er = pwm_set_pin(pin_no);
	if (er != E_OK) {
		return er;
	}

	er = pwm_set_div(pin_no, divider, 0);
	if (er != E_OK) {
		return er;
	}

	er = pwm_set_wrap(pin_no, wrap);
	if (er != E_OK) {
		return er;
	}

	er = pwm_set_cc(pin_no, angle_to_pulse_us(0));
	if (er != E_OK) {
		return er;
	}

	return pwm_set_enabled(pin_no, TRUE);
}

ER sg90_set_angle(UINT pin_no, INT angle_deg)
{
	return pwm_set_cc(pin_no,
			  angle_to_pulse_us(angle_deg));
}
