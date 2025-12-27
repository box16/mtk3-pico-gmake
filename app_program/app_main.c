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

#include "../module/hcsr04.h"

#define HCSR04_TRIG_GPIO		16
#define HCSR04_ECHO_GPIO		17
#define HCSR04_TIMER_NO			1
#define HCSR04_MAX_DISTANCE_MM		1500U
#define HCSR04_MEASURE_INTERVAL_MS	250

LOCAL void task_1(INT stacd, void *exinf);
LOCAL ID	tskid_1;
LOCAL T_CTSK	ctsk_1 = {
	.itskpri	= 10,
	.stksz		= 1024,
	.task		= task_1,
	.tskatr		= TA_HLNG | TA_RNG3,
};

LOCAL void task_1(INT stacd, void *exinf)
{
	ER er;
	UW distance_mm;
	Timer hcsr04_timer = {
		.timer_no = HCSR04_TIMER_NO,
	};
	Hcsr04Device hcsr04 = {
		.trig_gpio = HCSR04_TRIG_GPIO,
		.echo_gpio = HCSR04_ECHO_GPIO,
		.max_distance_mm = HCSR04_MAX_DISTANCE_MM,
		.timer = &hcsr04_timer,
	};

	er = hcsr04_init(&hcsr04);
	if (er != E_OK) {
		tm_printf((UB*)"PTMR init failed: %d\n", er);
		tk_slp_tsk(TMO_FEVR);
	}

	while (1) {
		er = hcsr04_measure(&hcsr04, &distance_mm);
		if (er == E_OK) {
			tm_printf((UB*)"Distance %lu mm\n", (UW)distance_mm);
		} else {
			tm_printf((UB*)"Echo timeout (%d)\n", er);
		}
		tk_dly_tsk(HCSR04_MEASURE_INTERVAL_MS);
	}
}

EXPORT INT usermain(void)
{
	tm_printf((UB*)"User program started\n");

	tskid_1 = tk_cre_tsk(&ctsk_1);
	tk_sta_tsk(tskid_1, 0);

	tk_slp_tsk(TMO_FEVR);
	return 0;
}
