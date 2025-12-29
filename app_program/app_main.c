/*
 *----------------------------------------------------------------------
 *    micro T-Kernel 3.0 BSP  (RaspberryPi Pico / RP2040)
 *----------------------------------------------------------------------
 */

#include <tk/tkernel.h>
#include <tm/tmonitor.h>
#include "../module/motor.h"

LOCAL const MotorPins motor_pins = {
    .a_in1 = 16,
    .a_in2 = 17,
    .b_in1 = 18,
    .b_in2 = 19,
};

LOCAL void motor_task(INT stacd, void *exinf);
LOCAL ID tskid_motor;

LOCAL T_CTSK ctsk_motor = {
    .itskpri = 10,
    .stksz   = 1024,
    .task    = motor_task,
    .tskatr  = TA_HLNG | TA_RNG3,
};

LOCAL void motor_task(INT stacd, void *exinf)
{
    ER er;

    er = motor_init(&motor_pins);
    if (er != E_OK) {
        tm_printf((UB*)"motor_init failed: %d\n", er);
        tk_slp_tsk(TMO_FEVR);
        return;
    }

    while (1) {
        motor_set(MOTOR_CMD_FORWARD, 100);
        tk_dly_tsk(2000);

        motor_set(MOTOR_CMD_BACK, 100);
        tk_dly_tsk(2000);

        motor_set(MOTOR_CMD_LEFT, 100);
        tk_dly_tsk(2000);

        motor_set(MOTOR_CMD_RIGHT, 100);
        tk_dly_tsk(2000);

        motor_set(MOTOR_CMD_STOP, 0);
        tk_dly_tsk(2000);
    }
}
EXPORT INT usermain(void)
{
    tm_printf((UB*)"User program started\n");

    tskid_motor = tk_cre_tsk(&ctsk_motor);
    tk_sta_tsk(tskid_motor, 0);

    tk_slp_tsk(TMO_FEVR);
    return 0;
}
