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

#define HC_SR04_TRIG_GPIO	16
#define HC_SR04_ECHO_GPIO	17
#define LED_GPIO		25
#define HC_SR04_PTMR_NO		1
#define HC_SR04_ECHO_TIMEOUT_US	30000
#define HC_SR04_MEASURE_INTERVAL_MS	60

LOCAL volatile UW g_ptmr_overflow;
LOCAL UW g_ptmr_limit;
LOCAL UW g_ptmr_clk;

LOCAL void ptmr_wrap_handler(void *exinf)
{
	(void)exinf;
	g_ptmr_overflow++;
}

LOCAL UD get_ptmr_ticks(void)
{
	UW	count;
	UW	of_before;
	UW	of_after;

	do {
		of_before = g_ptmr_overflow;
		GetPhysicalTimerCount(HC_SR04_PTMR_NO, &count);
		of_after = g_ptmr_overflow;
	} while (of_before != of_after);

	return ((UD)of_before * ((UD)g_ptmr_limit + 1ULL)) + (UD)count;
}

LOCAL UD usec_to_ticks(UW usec)
{
	return ((UD)usec * (UD)g_ptmr_clk) / 1000000ULL;
}

LOCAL UW ticks_to_usec(UD ticks)
{
	return (UW)((ticks * 1000000ULL) / (UD)g_ptmr_clk);
}

LOCAL UW ticks_to_mm(UD ticks)
{
	return (UW)((ticks * 343000ULL) / (2ULL * (UD)g_ptmr_clk));
}

LOCAL ER wait_echo_state(UINT state, UD timeout_ticks, UD *timestamp)
{
	UD start;

	start = get_ptmr_ticks();
	while (gpio_get_val(HC_SR04_ECHO_GPIO) != state) {
		if ((get_ptmr_ticks() - start) > timeout_ticks) {
			return E_TMOUT;
		}
	}

	if (timestamp != NULL) {
		*timestamp = get_ptmr_ticks();
	}
	return E_OK;
}

LOCAL ER init_hcsr04_timer(void)
{
	T_RPTMR rptmr;
	T_DPTMR dptmr;
	ER er;

	er = GetPhysicalTimerConfig(HC_SR04_PTMR_NO, &rptmr);
	if (er != E_OK) {
		return er;
	}

	g_ptmr_limit = rptmr.maxcount;
	g_ptmr_clk = rptmr.ptmrclk;
	g_ptmr_overflow = 0;

	dptmr.exinf = NULL;
	dptmr.ptmratr = TA_HLNG;
	dptmr.ptmrhdr = (FP)ptmr_wrap_handler;
	er = DefinePhysicalTimerHandler(HC_SR04_PTMR_NO, &dptmr);
	if (er != E_OK) {
		return er;
	}

	return StartPhysicalTimer(HC_SR04_PTMR_NO, g_ptmr_limit, TA_CYC_PTMR);
}

LOCAL void hcsr04_trigger(void)
{
	gpio_set_val(HC_SR04_TRIG_GPIO, 0);
	WaitUsec(2);
	gpio_set_val(HC_SR04_TRIG_GPIO, 1);
	WaitUsec(10);
	gpio_set_val(HC_SR04_TRIG_GPIO, 0);
}

LOCAL ER hcsr04_measure(UW *time_us, UW *distance_mm)
{
	UD timeout_ticks;
	UD start_ticks;
	UD end_ticks;
	UD pulse_ticks;
	ER er;

	timeout_ticks = usec_to_ticks(HC_SR04_ECHO_TIMEOUT_US);
	er = wait_echo_state(0, timeout_ticks, NULL);
	if (er != E_OK) {
		return er;
	}

	hcsr04_trigger();

	er = wait_echo_state(1, timeout_ticks, &start_ticks);
	if (er != E_OK) {
		return er;
	}
	er = wait_echo_state(0, timeout_ticks, &end_ticks);
	if (er != E_OK) {
		return er;
	}

	pulse_ticks = end_ticks - start_ticks;
	if (time_us != NULL) {
		*time_us = ticks_to_usec(pulse_ticks);
	}
	if (distance_mm != NULL) {
		*distance_mm = ticks_to_mm(pulse_ticks);
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
	UW time_us;
	UW distance_mm;
	UINT led = 0;

	gpio_set_pin(HC_SR04_TRIG_GPIO, GPIO_MODE_OUT);
	gpio_set_pin(HC_SR04_ECHO_GPIO, GPIO_MODE_IN);
	gpio_set_pin(LED_GPIO, GPIO_MODE_OUT);
	gpio_set_val(HC_SR04_TRIG_GPIO, 0);

	er = init_hcsr04_timer();
	if (er != E_OK) {
		tm_printf((UB*)"PTMR init failed: %d\n", er);
		tk_slp_tsk(TMO_FEVR);
	}

	while(1) {
		er = hcsr04_measure(&time_us, &distance_mm);
		if (er == E_OK) {
			tm_printf((UB*)"Echo %lu us, Distance %lu mm\n",
				  (UW)time_us, (UW)distance_mm);
		} else {
			tm_printf((UB*)"Echo timeout (%d)\n", er);
		}

		led ^= 1;
		gpio_set_val(LED_GPIO, led);
		tk_dly_tsk(HC_SR04_MEASURE_INTERVAL_MS);
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
