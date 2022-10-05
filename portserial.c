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

#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbport.h"

/* ----------------------- static functions ---------------------------------*/
static void prvvUARTTxReadyISR( void );
static void prvvUARTRxISR( void );

/* ----------------------- Start implementation开始实施 -----------------------------*/
/**
 * @description: 串口使能
 * 需要用户自己编写
 * @param {BOOL} xRxEnable
 * @param {BOOL} xTxEnable
 * @return {*}
 */
void vMBPortSerialEnable( BOOL xRxEnable, BOOL xTxEnable )
{
    /* If xRXEnable enable serial receive interrupts. If xTxENable enable
     * transmitter empty interrupts.
     */
}
/**
 * @description: 串口初始化  
 * 需要用户自己编写
 * @param {UCHAR} ucPORT 端口号
 * @param {ULONG} ulBaudRate 波特率
 * @param {UCHAR} ucDataBits 数据长度  RTU模式=8
 * @param {eMBParity} eParity 奇偶校验
 * @return {*}
 */
BOOL xMBPortSerialInit( UCHAR ucPORT, ULONG ulBaudRate, UCHAR ucDataBits, eMBParity eParity )
{
    return FALSE;
}
/**
 * @description: 
 * @param {CHAR} ucByte
 * @return {*}
 */
BOOL xMBPortSerialPutByte( CHAR ucByte )
{
    /* Put a byte in the UARTs transmit buffer. This function is called
     * by the protocol stack if pxMBFrameCBTransmitterEmpty( ) has been
     * called. */
    return TRUE;
}
/**从串口数据寄存器读取一个字节数据
 * 用户自己编写
 * @description: 
 * @param {CHAR *} pucByte
 * @return {*}
 */
BOOL xMBPortSerialGetByte( CHAR * pucByte )
{
    /* 返回UART接收缓冲区中的字节。

        *在调用pxMBFrameCBByteReceived（）后，协议堆栈将调用此函数。
     */
    return TRUE;
}

/* 为目标处理器的传输缓冲区空中断（或等效中断）创建中断处理程序。
*然后，这个函数应该调用pxMBFrameCBTransmitterEmpty（），它告诉协议栈可以发送新字符。
*然后，协议栈将调用xMBPortSerialPutByte（）来发送字符。
 */
/**为目标处理器的传输缓冲区空中断（或等效中断）创建中断处理程序。
 * @description: 
 * @return {*}
 */
static void prvvUARTTxReadyISR( void )
{
    pxMBFrameCBTransmitterEmpty(  );
}

/* 为目标处理器的接收中断创建一个中断处理程序。
*此函数应调用pxMBFrameCBByteReceived（）。
*协议栈将调用xMBPortSerialGetByte（）来检索字符。
 */
static void prvvUARTRxISR( void )
{
    pxMBFrameCBByteReceived(  );
}
