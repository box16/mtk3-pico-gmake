/*
 * Dual DC motor (PWM) module
 */

#include <tk/tkernel.h>
#include <sys/sysdef.h>
#include <bsp/libbsp.h>

#include "motor.h"

#define MOTOR_PWM_FREQ_HZ		1000U
#define MOTOR_PWM_TOP			1000U
#define MOTOR_PWM_PHASE_CORRECT		0U

typedef struct {
	MotorPins	pins;
	UW		top;
	UB		div_int;
	UB		div_frac;
	BOOL		ready;
} MotorState;

LOCAL MotorState motor_state;

LOCAL ER calc_pwm_div(UW sys_hz, UW pwm_hz, UW top, UB ph_correct,
		      UB *div_int, UB *div_frac)
{
	UD denom;
	UD numer;
	UD div_q4;
	UD i;
	UD f;

	if (sys_hz == 0U || pwm_hz == 0U || div_int == NULL || div_frac == NULL) {
		return E_PAR;
	}

	denom = (UD)pwm_hz * (UD)(top + 1U) * (UD)(ph_correct + 1U);
	if (denom == 0U) {
		return E_PAR;
	}

	numer = ((UD)sys_hz) << 4;
	div_q4 = (numer + (denom / 2U)) / denom;

	i = (div_q4 >> 4) & 0xFFU;
	f = div_q4 & 0xFU;

	if (f >= 16U) {
		i += 1U;
		f = 0U;
	}

	if (i < 1U) {
		i = 1U;
	} else if (i > 255U) {
		i = 255U;
	}

	*div_int = (UB)i;
	*div_frac = (UB)f;
	return E_OK;
}

LOCAL UW duty_to_cc(UW top, UB duty_percent)
{
	UW cc;

	if (duty_percent > 100U) {
		duty_percent = 100U;
	}

	cc = ((UW)(top + 1U) * (UW)duty_percent) / 100U;
	if (cc > top) {
		cc = top;
	}
	return cc;
}

LOCAL ER motor_configure_pin(UINT pin)
{
	ER er;

	er = pwm_set_pin(pin);
	if (er != E_OK) {
		return er;
	}

	er = pwm_set_wrap(pin, motor_state.top);
	if (er != E_OK) {
		return er;
	}

	er = pwm_set_div(pin, motor_state.div_int, motor_state.div_frac);
	if (er != E_OK) {
		return er;
	}

	er = pwm_set_cc(pin, 0);
	if (er != E_OK) {
		return er;
	}

	return pwm_set_enabled(pin, TRUE);
}

LOCAL ER motor_apply_cmd(MotorCmd cmd, UW cc)
{
	ER er;

	switch (cmd) {
	case MOTOR_CMD_STOP:
		er = pwm_set_cc(motor_state.pins.a_in1, 0);
		if (er != E_OK) {
			return er;
		}
		er = pwm_set_cc(motor_state.pins.a_in2, 0);
		if (er != E_OK) {
			return er;
		}
		er = pwm_set_cc(motor_state.pins.b_in1, 0);
		if (er != E_OK) {
			return er;
		}
		return pwm_set_cc(motor_state.pins.b_in2, 0);

	case MOTOR_CMD_FORWARD:
		er = pwm_set_cc(motor_state.pins.a_in1, cc);
		if (er != E_OK) {
			return er;
		}
		er = pwm_set_cc(motor_state.pins.a_in2, 0);
		if (er != E_OK) {
			return er;
		}
		er = pwm_set_cc(motor_state.pins.b_in1, cc);
		if (er != E_OK) {
			return er;
		}
		return pwm_set_cc(motor_state.pins.b_in2, 0);

	case MOTOR_CMD_BACK:
		er = pwm_set_cc(motor_state.pins.a_in1, 0);
		if (er != E_OK) {
			return er;
		}
		er = pwm_set_cc(motor_state.pins.a_in2, cc);
		if (er != E_OK) {
			return er;
		}
		er = pwm_set_cc(motor_state.pins.b_in1, 0);
		if (er != E_OK) {
			return er;
		}
		return pwm_set_cc(motor_state.pins.b_in2, cc);

	case MOTOR_CMD_LEFT:
		er = pwm_set_cc(motor_state.pins.a_in1, 0);
		if (er != E_OK) {
			return er;
		}
		er = pwm_set_cc(motor_state.pins.a_in2, cc);
		if (er != E_OK) {
			return er;
		}
		er = pwm_set_cc(motor_state.pins.b_in1, cc);
		if (er != E_OK) {
			return er;
		}
		return pwm_set_cc(motor_state.pins.b_in2, 0);

	case MOTOR_CMD_RIGHT:
		er = pwm_set_cc(motor_state.pins.a_in1, cc);
		if (er != E_OK) {
			return er;
		}
		er = pwm_set_cc(motor_state.pins.a_in2, 0);
		if (er != E_OK) {
			return er;
		}
		er = pwm_set_cc(motor_state.pins.b_in1, 0);
		if (er != E_OK) {
			return er;
		}
		return pwm_set_cc(motor_state.pins.b_in2, cc);

	default:
		return E_PAR;
	}
}

ER motor_init(const MotorPins *pins)
{
	UB div_int = 0;
	UB div_frac = 0;
	ER er;

	if (pins == NULL) {
		return E_PAR;
	}

	motor_state.ready = FALSE;

	er = calc_pwm_div(CLK_PERI_FREQ, MOTOR_PWM_FREQ_HZ, MOTOR_PWM_TOP,
			  MOTOR_PWM_PHASE_CORRECT, &div_int, &div_frac);
	if (er != E_OK) {
		return er;
	}

	motor_state.pins = *pins;
	motor_state.top = MOTOR_PWM_TOP;
	motor_state.div_int = div_int;
	motor_state.div_frac = div_frac;

	er = motor_configure_pin(motor_state.pins.a_in1);
	if (er != E_OK) {
		return er;
	}
	er = motor_configure_pin(motor_state.pins.a_in2);
	if (er != E_OK) {
		return er;
	}
	er = motor_configure_pin(motor_state.pins.b_in1);
	if (er != E_OK) {
		return er;
	}
	er = motor_configure_pin(motor_state.pins.b_in2);
	if (er != E_OK) {
		return er;
	}

	motor_state.ready = TRUE;
	return E_OK;
}

ER motor_set(MotorCmd cmd, UB power_percent)
{
	UW cc;

	if (!motor_state.ready) {
		return E_OBJ;
	}

	cc = duty_to_cc(motor_state.top, power_percent);
	return motor_apply_cmd(cmd, cc);
}
