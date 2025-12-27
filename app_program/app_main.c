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
#include <bsp/libbsp.h>

#define HCSR04_TRIG_GPIO		16
#define HCSR04_ECHO_GPIO		17
#define STATUS_LED_GPIO			25
#define HCSR04_TIMER_NO			1
#define HCSR04_ECHO_TIMEOUT_US		30000
#define HCSR04_MEASURE_INTERVAL_MS	60
#define HCSR04_SOUND_SPEED_MM_S		343000U

typedef struct {
	UINT	timer_no;
	volatile UW	wraps;
	UW	max_count;
	UW	clk_hz;
} PtmrState;

typedef struct {
	UINT	trig_gpio;
	UINT	echo_gpio;
	UW	echo_timeout_us;
	PtmrState timer;
} Hcsr04Device;

LOCAL Hcsr04Device hc_sr04 = {
	.trig_gpio = HCSR04_TRIG_GPIO,
	.echo_gpio = HCSR04_ECHO_GPIO,
	.echo_timeout_us = HCSR04_ECHO_TIMEOUT_US,
	.timer = {
		.timer_no = HCSR04_TIMER_NO,
	},
};

LOCAL void ptmr_overflow_handler(void *exinf)
{
	PtmrState *state = (PtmrState *)exinf;

	state->wraps++;
}

LOCAL UD ptmr_get_ticks(const PtmrState *state)
{
	UW count;
	UW wraps_before;
	UW wraps_after;

	do {
		wraps_before = state->wraps;
		GetPhysicalTimerCount(state->timer_no, &count);
		wraps_after = state->wraps;
	} while (wraps_before != wraps_after);

	return ((UD)wraps_before * ((UD)state->max_count + 1ULL)) + (UD)count;
}

LOCAL UD ptmr_usec_to_ticks(const PtmrState *state, UW usec)
{
	return ((UD)usec * (UD)state->clk_hz) / 1000000ULL;
}

LOCAL UW ptmr_ticks_to_usec(const PtmrState *state, UD ticks)
{
	return (UW)((ticks * 1000000ULL) / (UD)state->clk_hz);
}

LOCAL UW hcsr04_ticks_to_mm(const PtmrState *state, UD ticks)
{
	return (UW)((ticks * HCSR04_SOUND_SPEED_MM_S) /
		    (2ULL * (UD)state->clk_hz));
}

LOCAL ER ptmr_start(PtmrState *state)
{
	T_RPTMR config;
	T_DPTMR handler;
	ER er;

	er = GetPhysicalTimerConfig(state->timer_no, &config);
	if (er != E_OK) {
		return er;
	}

	state->max_count = config.maxcount;
	state->clk_hz = config.ptmrclk;
	state->wraps = 0;

	handler.exinf = state;
	handler.ptmratr = TA_HLNG;
	handler.ptmrhdr = (FP)ptmr_overflow_handler;
	er = DefinePhysicalTimerHandler(state->timer_no, &handler);
	if (er != E_OK) {
		return er;
	}

	return StartPhysicalTimer(state->timer_no, state->max_count, TA_CYC_PTMR);
}

LOCAL ER hcsr04_wait_echo_level(const Hcsr04Device *device, UINT level,
				UD timeout_ticks, UD *timestamp)
{
	UD start;

	start = ptmr_get_ticks(&device->timer);
	while (gpio_get_val(device->echo_gpio) != level) {
		if ((ptmr_get_ticks(&device->timer) - start) > timeout_ticks) {
			return E_TMOUT;
		}
	}

	if (timestamp != NULL) {
		*timestamp = ptmr_get_ticks(&device->timer);
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

LOCAL ER hcsr04_init(Hcsr04Device *device)
{
	gpio_set_pin(device->trig_gpio, GPIO_MODE_OUT);
	gpio_set_pin(device->echo_gpio, GPIO_MODE_IN);
	gpio_set_val(device->trig_gpio, 0);

	return ptmr_start(&device->timer);
}

LOCAL ER hcsr04_measure(const Hcsr04Device *device, UW *pulse_us, UW *distance_mm)
{
	UD timeout_ticks;
	UD start_ticks;
	UD end_ticks;
	UD pulse_ticks;
	ER er;

	timeout_ticks = ptmr_usec_to_ticks(&device->timer,
					   device->echo_timeout_us);
	er = hcsr04_wait_echo_level(device, 0, timeout_ticks, NULL);
	if (er != E_OK) {
		return er;
	}

	hcsr04_trigger(device);

	er = hcsr04_wait_echo_level(device, 1, timeout_ticks, &start_ticks);
	if (er != E_OK) {
		return er;
	}
	er = hcsr04_wait_echo_level(device, 0, timeout_ticks, &end_ticks);
	if (er != E_OK) {
		return er;
	}

	pulse_ticks = end_ticks - start_ticks;
	if (pulse_us != NULL) {
		*pulse_us = ptmr_ticks_to_usec(&device->timer, pulse_ticks);
	}
	if (distance_mm != NULL) {
		*distance_mm = hcsr04_ticks_to_mm(&device->timer, pulse_ticks);
	}
	return E_OK;
}

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
	UW pulse_us;
	UW distance_mm;
	UINT led_state = 0;

	(void)stacd;
	(void)exinf;

	gpio_set_pin(STATUS_LED_GPIO, GPIO_MODE_OUT);

	er = hcsr04_init(&hc_sr04);
	if (er != E_OK) {
		tm_printf((UB*)"PTMR init failed: %d\n", er);
		tk_slp_tsk(TMO_FEVR);
	}

	while(1) {
		er = hcsr04_measure(&hc_sr04, &pulse_us, &distance_mm);
		if (er == E_OK) {
			tm_printf((UB*)"Echo %lu us, Distance %lu mm\n",
				  (UW)pulse_us, (UW)distance_mm);
		} else {
			tm_printf((UB*)"Echo timeout (%d)\n", er);
		}

		led_state ^= 1;
		gpio_set_val(STATUS_LED_GPIO, led_state);
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
