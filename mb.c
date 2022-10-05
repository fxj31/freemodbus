/*
 * @Author: fxj 11461825+fxj31@user.noreply.gitee.com
 * @Date: 2022-10-02 12:34:12
 * @LastEditors: fxj 11461825+fxj31@user.noreply.gitee.com
 * @LastEditTime: 2022-10-04 15:40:04
 * @FilePath: \port\mb.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
/*
 * @Author: fxj 11461825+fxj31@user.noreply.gitee.com
 * @Date: 2022-10-02 12:34:12
 * @LastEditors: fxj 11461825+fxj31@user.noreply.gitee.com
 * @LastEditTime: 2022-10-03 22:26:55
 * @FilePath: \port\mb.c
 * @Description: 这是默认设置,请设置`customMade`, 打开koroFileHeader查看配置 进行设置: https://github.com/OBKoro1/koro1FileHeader/wiki/%E9%85%8D%E7%BD%AE
 */
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

/* ----------------------- System includes ----------------------------------*/
#include "stdlib.h"
#include "string.h"

/* ----------------------- Platform includes --------------------------------*/
#include "port.h"

/* ----------------------- Modbus includes ----------------------------------*/
#include "mb.h"
#include "mbconfig.h"
#include "mbframe.h"
#include "mbproto.h"
#include "mbfunc.h"

#include "mbport.h"
#if MB_RTU_ENABLED == 1
#include "mbrtu.h"
#endif
#if MB_ASCII_ENABLED == 1
#include "mbascii.h"
#endif
#if MB_TCP_ENABLED == 1
#include "mbtcp.h"
#endif

#ifndef MB_PORT_HAS_CLOSE
#define MB_PORT_HAS_CLOSE 0
#endif

/* ----------------------- Static variables ---------------------------------*/

static UCHAR ucMBAddress;      //从站地址
static eMBMode eMBCurrentMode; //当前模式

static enum {
    STATE_ENABLED,        //使能完成
    STATE_DISABLED,       //没有使能
    STATE_NOT_INITIALIZED //没有初始化
} eMBState = STATE_NOT_INITIALIZED;

/* Functions pointer which are initialized in eMBInit( ). Depending on the
 * mode (RTU or ASCII) the are set to the correct implementations.
 */
static peMBFrameSend peMBFrameSendCur;
static pvMBFrameStart pvMBFrameStartCur;
static pvMBFrameStop pvMBFrameStopCur;
static peMBFrameReceive peMBFrameReceiveCur;
static pvMBFrameClose pvMBFrameCloseCur;

/* Callback functions required by the porting layer. They are called when
 * an external event has happend which includes a timeout or the reception
 * or transmission of a character.
 */
BOOL (*pxMBFrameCBByteReceived)
(void);
BOOL (*pxMBFrameCBTransmitterEmpty)
(void);
BOOL (*pxMBPortCBTimerExpired)
(void);

BOOL (*pxMBFrameCBReceiveFSMCur)
(void);
BOOL (*pxMBFrameCBTransmitFSMCur)
(void);

/* An array of Modbus functions handlers which associates Modbus function
 * codes with implementing functions.
 关联MODBUS功能码对应的函数
 */
static xMBFunctionHandler xFuncHandlers[MB_FUNC_HANDLERS_MAX] = {
#if MB_FUNC_OTHER_REP_SLAVEID_ENABLED > 0
    {MB_FUNC_OTHER_REPORT_SLAVEID, eMBFuncReportSlaveID}, // 17功能 报告从机标识 实现函数
#endif
#if MB_FUNC_READ_INPUT_ENABLED > 0
    {MB_FUNC_READ_INPUT_REGISTER, eMBFuncReadInputRegister}, // 04功能读输入寄存器 实现函数
#endif
#if MB_FUNC_READ_HOLDING_ENABLED > 0
    {MB_FUNC_READ_HOLDING_REGISTER, eMBFuncReadHoldingRegister}, // 03功能读保持寄存器 实现函数
#endif
#if MB_FUNC_WRITE_MULTIPLE_HOLDING_ENABLED > 0
    {MB_FUNC_WRITE_MULTIPLE_REGISTERS, eMBFuncWriteMultipleHoldingRegister}, // 16功能写多个保持寄存器 关联函数
#endif
#if MB_FUNC_WRITE_HOLDING_ENABLED > 0
    {MB_FUNC_WRITE_REGISTER, eMBFuncWriteHoldingRegister}, // 06功能写单个保持寄存器 关联函数
#endif
#if MB_FUNC_READWRITE_HOLDING_ENABLED > 0
    {MB_FUNC_READWRITE_MULTIPLE_REGISTERS, eMBFuncReadWriteMultipleHoldingRegister}, // 23功能写多个寄存器 关联函数
#endif
#if MB_FUNC_READ_COILS_ENABLED > 0
    {MB_FUNC_READ_COILS, eMBFuncReadCoils}, // 01功能读线圈 关联函数
#endif
#if MB_FUNC_WRITE_COIL_ENABLED > 0
    {MB_FUNC_WRITE_SINGLE_COIL, eMBFuncWriteCoil}, // 05功能写单个线圈 关联函数
#endif
#if MB_FUNC_WRITE_MULTIPLE_COILS_ENABLED > 0
    {MB_FUNC_WRITE_MULTIPLE_COILS, eMBFuncWriteMultipleCoils}, // 15功能写多个线圈  关联函数
#endif
#if MB_FUNC_READ_DISCRETE_INPUTS_ENABLED > 0
    {MB_FUNC_READ_DISCRETE_INPUTS, eMBFuncReadDiscreteInputs}, // 02功能读离散输入  关联函数
#endif
};

/* ----------------------- Start implementation -----------------------------*/
/*函数功能：
 *1:实现RTU模式和ASCALL模式的协议栈初始化;
 *2:完成协议栈核心函数回调函数赋值，包括Modbus协议栈的使能和禁止、报文的接收和响应、3.5T定时器中断回调函数、串口发送和接收中断回调函数;
 *3:eMBRTUInit完成RTU模式下串口和3.5T定时器的初始化，需用户自己移植;
 *4:设置Modbus协议栈的模式eMBCurrentMode为MB_RTU，设置Modbus协议栈状态eMBState为STATE_DISABLED;
 */
eMBErrorCode
eMBInit(eMBMode eMode, UCHAR ucSlaveAddress, UCHAR ucPort, ULONG ulBaudRate, eMBParity eParity)
{

    //错误状态初始值
    eMBErrorCode eStatus = MB_ENOERR;

    /* 验证从机地址 */
    if ((ucSlaveAddress == MB_ADDRESS_BROADCAST) ||
        (ucSlaveAddress < MB_ADDRESS_MIN) || (ucSlaveAddress > MB_ADDRESS_MAX))
    {
        eStatus = MB_EINVAL; //地址小于等于0或地址大于247 返回无效节点
    }
    else
    {
        ucMBAddress = ucSlaveAddress;

        switch (eMode)
        {
#if MB_RTU_ENABLED > 0
        case MB_RTU:                             //挂钩子
            pvMBFrameStartCur = eMBRTUStart;     /*使能modbus协议栈回调函数*/
            pvMBFrameStopCur = eMBRTUStop;       /*禁用modbus协议栈回调函数*/
            peMBFrameSendCur = eMBRTUSend;       /*modbus从机响应发送函数回调函数*/
            peMBFrameReceiveCur = eMBRTUReceive; /*modbus报文接收函数回调函数*/
            pvMBFrameCloseCur = MB_PORT_HAS_CLOSE ? vMBPortClose : NULL;
            //接收状态机
            pxMBFrameCBByteReceived = xMBRTUReceiveFSM; /*串口接收中断最终调用此函数接收数据*/
            //发送状态机
            pxMBFrameCBTransmitterEmpty = xMBRTUTransmitFSM; /*串口发送中断最终调用此函数发送数据*/
            //报文达到间隔检查
            pxMBPortCBTimerExpired = xMBRTUTimerT35Expired; /*定时器中断函数最终调用次函数完成定时器中断*/
            //初始化RTU
            eStatus = eMBRTUInit(ucMBAddress, ucPort, ulBaudRate, eParity);
            break;
#endif
#if MB_ASCII_ENABLED > 0
        case MB_ASCII:
            pvMBFrameStartCur = eMBASCIIStart;
            pvMBFrameStopCur = eMBASCIIStop;
            peMBFrameSendCur = eMBASCIISend;
            peMBFrameReceiveCur = eMBASCIIReceive;
            pvMBFrameCloseCur = MB_PORT_HAS_CLOSE ? vMBPortClose : NULL;
            pxMBFrameCBByteReceived = xMBASCIIReceiveFSM;
            pxMBFrameCBTransmitterEmpty = xMBASCIITransmitFSM;
            pxMBPortCBTimerExpired = xMBASCIITimerT1SExpired;

            eStatus = eMBASCIIInit(ucMBAddress, ucPort, ulBaudRate, eParity);
            break;
#endif
        default:
            eStatus = MB_EINVAL;
        }

        if (eStatus == MB_ENOERR)
        {
            if (!xMBPortEventInit())
            {
                /* 端口初始化失败. */
                eStatus = MB_EPORTERR;
            }
            else
            {
                //设定当前状态
                eMBCurrentMode = eMode; //设定为RTU模式
                eMBState = STATE_DISABLED;
            }
        }
    }
    return eStatus;
}

#if MB_TCP_ENABLED > 0
eMBErrorCode
eMBTCPInit(USHORT ucTCPPort)
{
    eMBErrorCode eStatus = MB_ENOERR;

    if ((eStatus = eMBTCPDoInit(ucTCPPort)) != MB_ENOERR)
    {
        eMBState = STATE_DISABLED;
    }
    else if (!xMBPortEventInit())
    {
        /* Port dependent event module initalization failed. */
        eStatus = MB_EPORTERR;
    }
    else
    {
        pvMBFrameStartCur = eMBTCPStart;
        pvMBFrameStopCur = eMBTCPStop;
        peMBFrameReceiveCur = eMBTCPReceive;
        peMBFrameSendCur = eMBTCPSend;
        pvMBFrameCloseCur = MB_PORT_HAS_CLOSE ? vMBTCPPortClose : NULL;
        ucMBAddress = MB_TCP_PSEUDO_ADDRESS;
        eMBCurrentMode = MB_TCP;
        eMBState = STATE_DISABLED;
    }
    return eStatus;
}
#endif
/**
 * @description: 功能码处理回调解析。在POLL中已经实现
 * @param {UCHAR} ucFunctionCode
 * @param {pxMBFunctionHandler} pxHandler
 * @return {*}
 */
eMBErrorCode eMBRegisterCB(UCHAR ucFunctionCode, pxMBFunctionHandler pxHandler)
{
    int i;
    eMBErrorCode eStatus;

    if ((0 < ucFunctionCode) && (ucFunctionCode <= 127))
    {
        ENTER_CRITICAL_SECTION();
        if (pxHandler != NULL)
        {
            for (i = 0; i < MB_FUNC_HANDLERS_MAX; i++)
            {
                if ((xFuncHandlers[i].pxHandler == NULL) ||
                    (xFuncHandlers[i].pxHandler == pxHandler))
                {
                    xFuncHandlers[i].ucFunctionCode = ucFunctionCode;
                    xFuncHandlers[i].pxHandler = pxHandler;
                    break;
                }
            }
            eStatus = (i != MB_FUNC_HANDLERS_MAX) ? MB_ENOERR : MB_ENORES;
        }
        else
        {
            for (i = 0; i < MB_FUNC_HANDLERS_MAX; i++)
            {
                if (xFuncHandlers[i].ucFunctionCode == ucFunctionCode)
                {
                    xFuncHandlers[i].ucFunctionCode = 0;
                    xFuncHandlers[i].pxHandler = NULL;
                    break;
                }
            }
            /* Remove can't fail. */
            eStatus = MB_ENOERR;
        }
        EXIT_CRITICAL_SECTION();
    }
    else
    {
        eStatus = MB_EINVAL;
    }
    return eStatus;
}

eMBErrorCode
eMBClose(void)
{
    eMBErrorCode eStatus = MB_ENOERR;

    if (eMBState == STATE_DISABLED)
    {
        if (pvMBFrameCloseCur != NULL)
        {
            pvMBFrameCloseCur();
        }
    }
    else
    {
        eStatus = MB_EILLSTATE;
    }
    return eStatus;
}

/*函数功能
 *1:设置Modbus协议栈工作状态eMBState为STATE_ENABLED;
 *2:调用pvMBFrameStartCur()函数激活协议栈
 */
eMBErrorCode eMBEnable(void)
{
    eMBErrorCode eStatus = MB_ENOERR;

    if (eMBState == STATE_DISABLED)
    {
        /* 激活协议栈. */
        pvMBFrameStartCur(); /*pvMBFrameStartCur = eMBRTUStart;调用eMBRTUStart函数*/
        eMBState = STATE_ENABLED;
    }
    else
    {
        eStatus = MB_EILLSTATE;
    }
    return eStatus;
}

eMBErrorCode
eMBDisable(void)
{
    eMBErrorCode eStatus;

    if (eMBState == STATE_ENABLED)
    {
        pvMBFrameStopCur();
        eMBState = STATE_DISABLED;
        eStatus = MB_ENOERR;
    }
    else if (eMBState == STATE_DISABLED)
    {
        eStatus = MB_ENOERR;
    }
    else
    {
        eStatus = MB_EILLSTATE;
    }
    return eStatus;
}

/*函数功能:协议栈轮询
*1:检查协议栈状态是否使能，eMBState初值为STATE_NOT_INITIALIZED
在eMBInit()函数中被赋值为STATE_DISABLED,在eMBEnable函数中被赋值为STATE_ENABLE;
*2:轮询EV_FRAME_RECEIVED事件发生，若EV_FRAME_RECEIVED事件发生，接收一帧报文数据，
上报EV_EXECUTE事件，解析一帧报文，响应(发送)一帧数据给主机;
*/
/**
 * @description:
 * @return {*}
 */
eMBErrorCode eMBPoll(void)
{
    static UCHAR *ucMBFrame;        //接收和发送报文数据缓存区
    static UCHAR ucRcvAddress;      // modbus从机地址
    static UCHAR ucFunctionCode;    //功能码
    static USHORT usLength;         //报文长度
    static eMBException eException; //错误码响应枚举

    int i;
    eMBErrorCode eStatus = MB_ENOERR; // modbus协议栈错误码
    eMBEventType eEvent;              //事件标志枚举

    /* 检测协议栈是否准备. */
    if (eMBState != STATE_ENABLED)
    {
        return MB_EILLSTATE;
    }

    /* Check if there is a event available. If not return control to caller.
     * Otherwise we will handle the event. */
    //查询事件
    if (xMBPortEventGet(&eEvent) == TRUE) //如果有事件发生 判断事件类型
    {
        switch (eEvent)
        {
        case EV_READY: //启动完成事件
            break;

        case EV_FRAME_RECEIVED: //接收到帧事件
            eStatus = peMBFrameReceiveCur(&ucRcvAddress, &ucMBFrame, &usLength);
            if (eStatus == MB_ENOERR) /*报文长度和CRC校验正确*/
            {
                /* Check if the frame is for us. If not ignore the frame. */
                /*判断接收到的报文数据是否可接受，如果是，处理报文数据*/
                if ((ucRcvAddress == ucMBAddress) || (ucRcvAddress == MB_ADDRESS_BROADCAST))
                {
                    (void)xMBPortEventPost(EV_EXECUTE); //修改事件标志为EV_EXECUTE执行事件
                }
                else
                {
                    (void)xMBPortEventPost(EV_READY); //修改事件标志为EV_READY执行事件
                }
            }
            break;

        case EV_EXECUTE:                                 //执行功能
            ucFunctionCode = ucMBFrame[MB_PDU_FUNC_OFF]; //获取PDU中第一个字节，为功能码
            eException = MB_EX_ILLEGAL_FUNCTION;         //赋错误码初值为无效的功能码
            for (i = 0; i < MB_FUNC_HANDLERS_MAX; i++)
            {
                /* No more function handlers registered. Abort. */
                if (xFuncHandlers[i].ucFunctionCode == 0)
                {
                    break;
                }
                else if (xFuncHandlers[i].ucFunctionCode == ucFunctionCode) /*根据报文中的功能码，处理报文*/
                {
                    eException = xFuncHandlers[i].pxHandler(ucMBFrame, &usLength); /*对接收到的报文进行解析*/
                    break;
                }
            }

            /* If the request was not sent to the broadcast address we
             * return a reply. */
            /*接收到的报文返回处理*/
            if (ucRcvAddress != MB_ADDRESS_BROADCAST) //判断不是广播地址
            {
                if (eException != MB_EX_NONE) /*接收到的报文有错误时返回处理*/
                {
                    /* An exception occured. 编辑错误帧.（0X80+功能码+错误代码） */
                    usLength = 0;
                    ucMBFrame[usLength++] = (UCHAR)(ucFunctionCode | MB_FUNC_ERROR);
                    ucMBFrame[usLength++] = eException;
                }
                // if( ( eMBCurrentMode == MB_ASCII ) && MB_ASCII_TIMEOUT_WAIT_BEFORE_SEND_MS )
                // {
                //     vMBPortTimersDelay( MB_ASCII_TIMEOUT_WAIT_BEFORE_SEND_MS );
                // }
                if (ucRcvAddress == ucMBAddress)//本机应答返回
                {
                    eStatus = peMBFrameSendCur(ucMBAddress, ucMBFrame, usLength); //发送返回数据
                }
            }
            break;

        case EV_FRAME_SENT: //帧已经发送
            break;
        }
    }
    return MB_ENOERR;
}
