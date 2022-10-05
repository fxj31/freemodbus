/* 
 * FreeModbus Libary: A portable Modbus implementation for Modbus ASCII/RTU.
 * Copyright (c) 2006-2018 Christian Walter <cwalter@embedded-solutions.at>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#ifndef _MB_PROTO_H
#define _MB_PROTO_H

#ifdef __cplusplus
PR_BEGIN_EXTERN_C
#endif
/* ----------------------- Defines 协议栈基本定义------------------------------------------*/
#define MB_ADDRESS_BROADCAST    ( 0 )   /*! Modbus 广播地址. */
#define MB_ADDRESS_MIN          ( 1 )   /*! 最小从站地址. */
#define MB_ADDRESS_MAX          ( 247 ) /*! 最大从站地址. */
#define MB_FUNC_NONE                          (  0 )
#define MB_FUNC_READ_COILS                    (  1 )//01功能读线圈
#define MB_FUNC_READ_DISCRETE_INPUTS          (  2 )//02功能读离散输入
#define MB_FUNC_WRITE_SINGLE_COIL             (  5 )//05功能写单个线圈
#define MB_FUNC_WRITE_MULTIPLE_COILS          ( 15 )//15功能写多个线圈
#define MB_FUNC_READ_HOLDING_REGISTER         (  3 )//03功能读保持寄存器
#define MB_FUNC_READ_INPUT_REGISTER           (  4 )//04功能读输入寄存器
#define MB_FUNC_WRITE_REGISTER                (  6 )//06功能写单个保持寄存器
#define MB_FUNC_WRITE_MULTIPLE_REGISTERS      ( 16 )//16功能写多个保持寄存器
#define MB_FUNC_READWRITE_MULTIPLE_REGISTERS  ( 23 )//23功能读写多个保持寄存器
#define MB_FUNC_DIAG_READ_EXCEPTION           (  7 )//07功能读取异常状态
#define MB_FUNC_DIAG_DIAGNOSTIC               (  8 )//08功能回送诊断校验
#define MB_FUNC_DIAG_GET_COM_EVENT_CNT        ( 11 )//11功能 读取事件技术
#define MB_FUNC_DIAG_GET_COM_EVENT_LOG        ( 12 )//12功能 读取通讯事件记录
#define MB_FUNC_OTHER_REPORT_SLAVEID          ( 17 )//17功能  报告从机标识
#define MB_FUNC_ERROR                         ( 128 )
/* ----------------------- Type definitions MODBUS异常错误码响应枚举 ---------------------------------*/
    typedef enum
{
    MB_EX_NONE = 0x00,//正常
    MB_EX_ILLEGAL_FUNCTION = 0x01,//无效功能
    MB_EX_ILLEGAL_DATA_ADDRESS = 0x02,//无效数据地址
    MB_EX_ILLEGAL_DATA_VALUE = 0x03,//无效数据值
    MB_EX_SLAVE_DEVICE_FAILURE = 0x04,//从站设备故障
    MB_EX_ACKNOWLEDGE = 0x05,//确认
    MB_EX_SLAVE_BUSY = 0x06,//从站忙
    MB_EX_MEMORY_PARITY_ERROR = 0x08,//内存校验错误
    MB_EX_GATEWAY_PATH_FAILED = 0x0A,//网关路径错误
    MB_EX_GATEWAY_TGT_FAILED = 0x0B//网关目标设备相应失效
} eMBException;

typedef         eMBException( *pxMBFunctionHandler ) ( UCHAR * pucFrame, USHORT * pusLength );//解析回调函数原型

typedef struct
{
    UCHAR           ucFunctionCode;//功能码
    pxMBFunctionHandler pxHandler;//功能码解析回调函数（钩子函数）
} xMBFunctionHandler;

#ifdef __cplusplus
PR_END_EXTERN_C
#endif
#endif
