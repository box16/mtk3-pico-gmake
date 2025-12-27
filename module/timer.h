/*
 * Timer module (physical timer wrapper)
 */

#ifndef MODULE_TIMER_H
#define MODULE_TIMER_H

typedef struct {
	UINT	timer_no;
	volatile UW	wraps;
	UW	max_count;
	UW	clk_hz;
} Timer;

ER timer_start(Timer *state);
UD timer_get_ticks(const Timer *state);
UD timer_usec_to_ticks(const Timer *state, UW usec);

#endif /* MODULE_TIMER_H */
