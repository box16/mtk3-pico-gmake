/*
 * Servo motor (PWM) module : SG-90
 * https://akizukidenshi.com/goodsaffix/SG90_a.pdf
 */

#ifndef MODULE_SERVO_H
#define MODULE_SERVO_H

#define SERVO_ANGLE_MAX		90

ER sg90_init(UINT pin_no);
ER sg90_set_angle(UINT pin_no, INT angle_deg);

#endif /* MODULE_SERVO_H */
