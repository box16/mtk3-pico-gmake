/*
 * Timer module (physical timer wrapper)
 */

#include <tk/tkernel.h>
#include "timer.h"

LOCAL void timer_overflow_handler(void *exinf)
{
	// 物理タイマーがオーバーフローしたときに呼ばれるハンドラ
	// オーバーフローの回数を数えて時間計算に使う
	Timer *state = (Timer *)exinf;

	state->wraps++;
}

ER timer_start(Timer *state)
{
	T_RPTMR config;
	T_DPTMR handler;
	ER er;

	if (state == NULL) {
		return E_PAR;
	}

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

UD timer_get_ticks(const Timer *state)
{
	UW count;
	UW wraps_before;
	UW wraps_after;

	if (state == NULL) {
		return 0;
	}

	do {
		// 読み込み中にオーバーフローした場合に備えて2回読む
		wraps_before = state->wraps;
		GetPhysicalTimerCount(state->timer_no, &count);
		wraps_after = state->wraps;
	} while (wraps_before != wraps_after);

	return ((UD)wraps_before * ((UD)state->max_count + 1ULL)) + (UD)count;
}

UD timer_usec_to_ticks(const Timer *state, UW usec)
{
	if (state == NULL) {
		return 0;
	}

	return ((UD)usec * (UD)state->clk_hz) / 1000000ULL;
}
