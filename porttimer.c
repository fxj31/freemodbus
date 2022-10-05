/*
 * @Author: fxj 11461825+fxj31@user.noreply.gitee.com
 * @Date: 2022-10-01 14:29:03
 * @LastEditors: fxj 11461825+fxj31@user.noreply.gitee.com
 * @LastEditTime: 2022-10-02 16:22:53
 * @FilePath: \port\porttimer.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
/*
 * FreeModbus Libary: BARE Port
 * Copyright (C) 2006 Christian Walter <wolti@sil.at>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * File: $Id$
 */

/* ----------------------- Platform includes --------------------------------*/
#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"

/* ----------------------- static functions ---------------------------------*/
static void prvvTIMERExpiredISR( void );

/* ----------------------- Start implementation -----------------------------*/
/**3.5T计时器初始化
 * 用户实现
 * @description: 
 * @param {USHORT} usTim1Timerout50us
 * @return {*}
 */
BOOL xMBPortTimersInit( USHORT usTim1Timerout50us )
{
    return FALSE;
}

/**3.5T计时器使能
 * 用户实现
 * @description: 
 * @return {*}
 */
inline void vMBPortTimersEnable(  )
{
    /* Enable the timer with the timeout passed to xMBPortTimersInit( ) 
    使用传递给xMBPortTimersInit（）的超时时间启用计时器
    */
}
/**3.5T计时器失能
 * 用户实现
 * @description: 
 * @return {*}
 */
inline void vMBPortTimersDisable(  )
{
    /* Disable any pending timers. */
}

/* Create an ISR which is called whenever the timer has expired. This function
 * must then call pxMBPortCBTimerExpired( ) to notify the protocol stack that
 * the timer has expired.
 */
static void prvvTIMERExpiredISR( void )
{
    ( void )pxMBPortCBTimerExpired(  );
}

