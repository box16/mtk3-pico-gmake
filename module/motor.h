/*
 * Dual DC motor (PWM) module
 */

#ifndef MODULE_MOTOR_H
#define MODULE_MOTOR_H

typedef enum {
	MOTOR_CMD_STOP = 0,
	MOTOR_CMD_FORWARD,
	MOTOR_CMD_BACK,
	MOTOR_CMD_LEFT,
	MOTOR_CMD_RIGHT,
} MotorCmd;

typedef struct {
	UINT	a_in1;
	UINT	a_in2;
	UINT	b_in1;
	UINT	b_in2;
} MotorPins;

ER motor_init(const MotorPins *pins);
ER motor_set(MotorCmd cmd, UB power_percent);

#endif /* MODULE_MOTOR_H */
