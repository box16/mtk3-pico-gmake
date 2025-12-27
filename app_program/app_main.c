/*
 *----------------------------------------------------------------------
 *    micro T-Kernel 3.0 BSP
 *
 *    Copyright (C) 2022-2023 by Ken Sakamura.
 *    This software is distributed under the T-License 2.2.
 *----------------------------------------------------------------------
 *
 *    Released by TRON Forum(http://www.tron.org) at 2023/05.
 *
 *----------------------------------------------------------------------
 */

/*
 *	app_main.c
 *	Application main program for RaspberryPi Pico
 */

#include <tk/tkernel.h>
#include <tk/syslib.h>
#include <tm/tmonitor.h>
#include <sys/sysdef.h>
#include <bsp/libbsp.h>

#define SERVO_PWM_GPIO		18
#define SERVO_PWM_FREQ_HZ	50U
#define SERVO_PWM_CLK_HZ	1000000U
#define SERVO_PWM_DIVIDER	(CLK_PERI_FREQ / SERVO_PWM_CLK_HZ)
#define SERVO_PWM_WRAP		((SERVO_PWM_CLK_HZ / SERVO_PWM_FREQ_HZ) - 1U)

#define SERVO_PULSE_MIN_US	500
#define SERVO_PULSE_MID_US	1450
#define SERVO_PULSE_MAX_US	2400
#define SERVO_PULSE_SPAN_US	(SERVO_PULSE_MAX_US - SERVO_PULSE_MID_US)
#define SERVO_ANGLE_MAX		90
#define SERVO_STEP_DEG		10
#define SERVO_STEP_WAIT_MS	100

LOCAL void servo_task(INT stacd, void *exinf);
LOCAL ID	tskid_servo;
LOCAL T_CTSK	ctsk_servo = {
	.itskpri	= 10,
	.stksz		= 1024,
	.task		= servo_task,
	.tskatr		= TA_HLNG | TA_RNG3,
};

LOCAL UW servo_angle_to_pulse_us(INT angle_deg)
{
	INT clamped = angle_deg;
	INT pulse;

	if (clamped > SERVO_ANGLE_MAX) {
		clamped = SERVO_ANGLE_MAX;
	} else if (clamped < -SERVO_ANGLE_MAX) {
		clamped = -SERVO_ANGLE_MAX;
	}

	pulse = SERVO_PULSE_MID_US + (clamped * SERVO_PULSE_SPAN_US) / SERVO_ANGLE_MAX;
	if (pulse < SERVO_PULSE_MIN_US) {
		pulse = SERVO_PULSE_MIN_US;
	} else if (pulse > SERVO_PULSE_MAX_US) {
		pulse = SERVO_PULSE_MAX_US;
	}

	return (UW)pulse;
}

LOCAL ER servo_pwm_init(void)
{
	ER er;

	er = pwm_set_pin(SERVO_PWM_GPIO);
	if (er != E_OK) {
		return er;
	}

	er = pwm_set_div(SERVO_PWM_GPIO, SERVO_PWM_DIVIDER, 0);
	if (er != E_OK) {
		return er;
	}

	er = pwm_set_wrap(SERVO_PWM_GPIO, SERVO_PWM_WRAP);
	if (er != E_OK) {
		return er;
	}

	er = pwm_set_cc(SERVO_PWM_GPIO, servo_angle_to_pulse_us(0));
	if (er != E_OK) {
		return er;
	}

	return pwm_set_enabled(SERVO_PWM_GPIO, TRUE);
}

LOCAL ER servo_set_angle(INT angle_deg)
{
	return pwm_set_cc(SERVO_PWM_GPIO, servo_angle_to_pulse_us(angle_deg));
}

LOCAL void servo_task(INT stacd, void *exinf)
{
	ER er;

	er = servo_pwm_init();
	if (er != E_OK) {
		tm_printf((UB*)"PWM init failed: %d\n", er);
		tk_slp_tsk(TMO_FEVR);
	}

	tm_printf((UB*)"Servo PWM started on GPIO %d\n", SERVO_PWM_GPIO);

	while (1) {
		INT angle;

		for (angle = -SERVO_ANGLE_MAX; angle <= SERVO_ANGLE_MAX; angle += SERVO_STEP_DEG) {
			er = servo_set_angle(angle);
			if (er != E_OK) {
				tm_printf((UB*)"PWM set failed: %d\n", er);
				tk_slp_tsk(TMO_FEVR);
			}
			tk_dly_tsk(SERVO_STEP_WAIT_MS);
		}

		for (angle = SERVO_ANGLE_MAX; angle >= -SERVO_ANGLE_MAX; angle -= SERVO_STEP_DEG) {
			er = servo_set_angle(angle);
			if (er != E_OK) {
				tm_printf((UB*)"PWM set failed: %d\n", er);
				tk_slp_tsk(TMO_FEVR);
			}
			tk_dly_tsk(SERVO_STEP_WAIT_MS);
		}
	}
}

EXPORT INT usermain(void)
{
	tm_printf((UB*)"User program started\n");

	tskid_servo = tk_cre_tsk(&ctsk_servo);
	tk_sta_tsk(tskid_servo, 0);

	tk_slp_tsk(TMO_FEVR);
	return 0;
}
