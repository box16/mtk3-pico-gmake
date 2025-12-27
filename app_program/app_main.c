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
#define HCSR04_TIMER_NO			1
#define HCSR04_MAX_DISTANCE_MM		1500U
#define HCSR04_MEASURE_INTERVAL_MS	250
#define HCSR04_SOUND_SPEED_MM_S		343000U // 常温での空気中の音速の近似値
#define HCSR04_PULSE_READY_TIMEOUT_US	3000U

typedef struct {
	UINT	timer_no;
	volatile UW	wraps;
	UW	max_count;
	UW	clk_hz;
} Timer;

typedef struct {
	UINT	trig_gpio;
	UINT	echo_gpio;
	UW	max_distance_mm;
	Timer timer;
} Hcsr04Device;

LOCAL void timer_overflow_handler(void *exinf)
{
	// 物理タイマーがオーバーフローした際に、exinfで渡して置いたオーバーフローカウンタを進める
	Timer *state = (Timer *)exinf;

	state->wraps++;
}

LOCAL UD timer_get_ticks(const Timer *state)
{
	UW count;
	UW wraps_before;
	UW wraps_after;

	do {
		// 読み込み中にオーバーフロー発生していないか確認するため、2回読み込む
		wraps_before = state->wraps;
		GetPhysicalTimerCount(state->timer_no, &count);
		wraps_after = state->wraps;
	} while (wraps_before != wraps_after);

	return ((UD)wraps_before * ((UD)state->max_count + 1ULL)) + (UD)count;
}

LOCAL UD timer_usec_to_ticks(const Timer *state, UW usec)
{
	return ((UD)usec * (UD)state->clk_hz) / 1000000ULL;
}

LOCAL UW hcsr04_ticks_to_mm(const Timer *state, UD ticks)
{
	return (UW)((ticks * HCSR04_SOUND_SPEED_MM_S) /
		    (2ULL * (UD)state->clk_hz));
}

LOCAL UD hcsr04_distance_mm_to_ticks(const Hcsr04Device *device, UW distance_mm)
{
	return ((UD)distance_mm * 2ULL * (UD)device->timer.clk_hz) /
	       HCSR04_SOUND_SPEED_MM_S;
}

LOCAL ER timer_start(Timer *state)
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
	handler.ptmrhdr = (FP)timer_overflow_handler;
	er = DefinePhysicalTimerHandler(state->timer_no, &handler);
	if (er != E_OK) {
		return er;
	}

	return StartPhysicalTimer(state->timer_no, state->max_count, TA_CYC_PTMR);
}

/* Wait until echo reaches level, with timeout. */
LOCAL ER hcsr04_wait_echo_level(const Hcsr04Device *device, UINT level,
				UD timeout_ticks, UD *timestamp)
{
	UD start;

	start = timer_get_ticks(&device->timer);
	while (gpio_get_val(device->echo_gpio) != level) {
		if ((timer_get_ticks(&device->timer) - start) > timeout_ticks) {
			return E_TMOUT;
		}
	}

	if (timestamp != NULL) {
		*timestamp = timer_get_ticks(&device->timer);
	}
	return E_OK;
}

LOCAL void hcsr04_trigger(const Hcsr04Device *device)
{
	// トリガーパルス生成. 10us以上Highにする必要がある
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

	return timer_start(&device->timer);
}

LOCAL ER hcsr04_measure(const Hcsr04Device *device, UW *distance_mm)
{
	UD timeout_ticks;
	UD ready_timeout_ticks;
	UD start_ticks;
	UD end_ticks;
	UD pulse_ticks;
	ER er;

	ready_timeout_ticks = timer_usec_to_ticks(&device->timer,
						  HCSR04_PULSE_READY_TIMEOUT_US);
	er = hcsr04_wait_echo_level(device, 0, ready_timeout_ticks, NULL);
	if (er != E_OK) {
		return er;
	}

	hcsr04_trigger(device);

	er = hcsr04_wait_echo_level(device, 1, ready_timeout_ticks, &start_ticks);
	if (er != E_OK) {
		return er;
	}

	timeout_ticks = hcsr04_distance_mm_to_ticks(device,
						    device->max_distance_mm);
	er = hcsr04_wait_echo_level(device, 0, timeout_ticks, &end_ticks);
	if (er != E_OK) {
		return er;
	}

	pulse_ticks = end_ticks - start_ticks;
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
	UW distance_mm;

	Hcsr04Device hc_sr04 = {
		.trig_gpio = HCSR04_TRIG_GPIO,
		.echo_gpio = HCSR04_ECHO_GPIO,
		.max_distance_mm = HCSR04_MAX_DISTANCE_MM,
		.timer = {
			.timer_no = HCSR04_TIMER_NO,
		},
	};

	er = hcsr04_init(&hc_sr04);
	if (er != E_OK) {
		tm_printf((UB*)"PTMR init failed: %d\n", er);
		tk_slp_tsk(TMO_FEVR);
	}

	while(1) {
		er = hcsr04_measure(&hc_sr04, &distance_mm);
		if (er == E_OK) {
			tm_printf((UB*)"Distance %lu mm\n", (UW)distance_mm);
		} else {
			// 測定範囲内に物体がない場合など、測定失敗
		}
		tk_dly_tsk(HCSR04_MEASURE_INTERVAL_MS);
	}

	tk_ext_tsk();
}

EXPORT INT usermain(void)
{
	tm_printf((UB*)"User program started\n");

	tskid_1 = tk_cre_tsk(&ctsk_1);
	tk_sta_tsk(tskid_1, 0);

	tk_slp_tsk(TMO_FEVR);
	return 0;
}
