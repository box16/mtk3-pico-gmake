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

LOCAL void task_get_temp(INT stacd, void *exinf);
LOCAL ID	id_get_temp;
LOCAL T_CTSK	ctsk_get_temp = {
	.itskpri	= 10,
	.stksz		= 1024,
	.task		= task_get_temp,
	.tskatr		= TA_HLNG | TA_RNG3,
};

LOCAL void task_get_temp(INT stacd, void *exinf)
{
	ID devid_iic = tk_opn_dev("iica", TD_UPDATE);
	UB data[4];
	SZ size;
	ER error;
	// 仕様書 : https://akizukidenshi.com/goodsaffix/Sensirion_Humidity_Sensors_SHT3x_DIS_Datasheet_V3_J.pdf
	while(1) {
		error = tk_swri_dev(devid_iic, 0x44, (UB[2]){0x24,0x00}, 2, &size);
		tk_dly_tsk(15);// 測定待ち
		error = tk_srea_dev(devid_iic, 0x44, data, 4, &size);
		UH raw_temp = (data[0]<<8) | data[1];
		H temp = (W)(175 * raw_temp) / 65535 - 45;
		UH raw_humi = (data[2]<<8) | data[3];
		H humi = (W)(100 * raw_humi) / 65535;
		tm_printf((UB*)"temp: %d\n",temp);
		tm_printf((UB*)"humi: %d\n",humi);
		tk_dly_tsk(1000);
	}
}

EXPORT INT usermain(void)
{
	tm_printf((UB*)"User program started\n");

	id_get_temp = tk_cre_tsk(&ctsk_get_temp);
	tk_sta_tsk(id_get_temp, 0);

	tk_slp_tsk(TMO_FEVR);
	return 0;
}