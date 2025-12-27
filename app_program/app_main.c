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
#include <tm/tmonitor.h>

#include "../module/sg90.h"

#define SERVO_PWM_GPIO		18
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

LOCAL void servo_task(INT stacd, void *exinf)
{
	ER er;
	er = sg90_init(SERVO_PWM_GPIO);
	if (er != E_OK) {
		tm_printf((UB*)"PWM init failed: %d\n", er);
		tk_slp_tsk(TMO_FEVR);
	}

	tm_printf((UB*)"Servo PWM started on GPIO %d\n", SERVO_PWM_GPIO);

	while (1) {
		INT angle;

		for (angle = -SERVO_ANGLE_MAX; angle <= SERVO_ANGLE_MAX; angle += SERVO_STEP_DEG) {
			er = sg90_set_angle(SERVO_PWM_GPIO, angle);
			if (er != E_OK) {
				tm_printf((UB*)"PWM set failed: %d\n", er);
				tk_slp_tsk(TMO_FEVR);
			}
			tk_dly_tsk(SERVO_STEP_WAIT_MS);
		}

		for (angle = SERVO_ANGLE_MAX; angle >= -SERVO_ANGLE_MAX; angle -= SERVO_STEP_DEG) {
			er = sg90_set_angle(SERVO_PWM_GPIO, angle);
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
