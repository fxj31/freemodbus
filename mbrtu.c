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
#include "mbrtu.h"
#include "mbframe.h"

#include "mbcrc.h"
#include "mbport.h"

/* ----------------------- Defines RTU模式PDU设置------------------------------------------*/
#define MB_SER_PDU_SIZE_MIN 4   /*!< Minimum size of a Modbus RTU frame. */
#define MB_SER_PDU_SIZE_MAX 256 /*!< Maximum size of a Modbus RTU frame. */
#define MB_SER_PDU_SIZE_CRC 2   /*!< Size of CRC field in PDU. */
#define MB_SER_PDU_ADDR_OFF 0   /*!< Offset of slave address in Ser-PDU. */
#define MB_SER_PDU_PDU_OFF 1    /*!< Offset of Modbus-PDU in Ser-PDU. */

/* ----------------------- Type definitions 定义接收发送状态机 ---------------------------------*/
typedef enum
{
    STATE_RX_INIT, /*!< 接收初始化状态. */
    STATE_RX_IDLE, /*!< 接收空闲状态. */
    STATE_RX_RCV,  /*!< 正在接收帧. */
    STATE_RX_ERROR /*!< 无效帧. */
} eMBRcvState;     //接收状态机

typedef enum
{
    STATE_TX_IDLE, /*!< 发送空闲状态. */
    STATE_TX_XMIT  /*!< 发送传输状态. */
} eMBSndState;     //发送状态机

/* ----------------------- Static variables 全局变量---------------------------------*/
static volatile eMBSndState eSndState; //发送状态机
static volatile eMBRcvState eRcvState; //接收状态机
static volatile UCHAR SlaveAddress;

volatile UCHAR ucRTUBuf[MB_SER_PDU_SIZE_MAX]; //全局接收数组

static volatile UCHAR *pucSndBufferCur;//发送指针
static volatile USHORT usSndBufferCount;//发送字节数

static volatile USHORT usRcvBufferPos;

/* ----------------------- Start implementation 实现部分-----------------------------*/
/**
 * @description: MODBUS RTU初始化
 * @param {UCHAR} ucSlaveAddress 从站地址
 * @param {UCHAR} ucPort 端口
 * @param {ULONG} ulBaudRate 波特率
 * @param {eMBParity} eParity 串行通讯的奇偶校验
 * @return {*}
 */
eMBErrorCode eMBRTUInit(UCHAR ucSlaveAddress, UCHAR ucPort, ULONG ulBaudRate, eMBParity eParity)
{
    eMBErrorCode eStatus = MB_ENOERR;
    ULONG usTimerT35_50us;

    (void)ucSlaveAddress;
    SlaveAddress=ucSlaveAddress;
    ENTER_CRITICAL_SECTION();

    /* Modbus RTU uses 8 Databits. */
    if (xMBPortSerialInit(ucPort, ulBaudRate, 8, eParity) != TRUE) //串口初始化
    {
        eStatus = MB_EPORTERR;
    }
    else
    {
        /* If baudrate > 19200 then we should use the fixed timer values
         * t35 = 1750us. Otherwise t35 must be 3.5 times the character time.
         */
        if (ulBaudRate > 19200) //判断波特率是否大于19200 然后计算空闲间隔
        {
            usTimerT35_50us = 35; /* 1800us. */
        }
        else
        {
            /* The timer reload value for a character is given by:
             *
             * ChTimeValue = Ticks_per_1s / ( Baudrate / 11 )
             *             = 11 * Ticks_per_1s / Baudrate
             *             = 220000 / Baudrate
             * The reload for t3.5 is 1.5 times this value and similary
             * for t3.5.
             */
            usTimerT35_50us = (7UL * 220000UL) / (2UL * ulBaudRate);
        }
        if (xMBPortTimersInit((USHORT)usTimerT35_50us) != TRUE) //初始化3.5T空闲时钟
        {
            eStatus = MB_EPORTERR;
        }
    }
    EXIT_CRITICAL_SECTION();

    return eStatus;
}

/*函数功能 使能modbus协议栈
 *1:设置接收状态机eRcvState为STATE_RX_INIT；
 *2:使能串口接收,禁止串口发送,作为从机,等待主机传送的数据;
 *3:开启定时器，3.5T时间后定时器发生第一次中断,此时eRcvState为STATE_RX_INIT,上报初始化完成事件,然后设置eRcvState为空闲STATE_RX_IDLE;
 *4:每次进入3.5T定时器中断,定时器被禁止，等待串口有字节接收后，才使能定时器;
 */
void eMBRTUStart(void)
{
    ENTER_CRITICAL_SECTION();

    eRcvState = STATE_RX_INIT;        //设置接收状态机eRcvState为STATE_RX_INIT（接收初始化状态）
    vMBPortSerialEnable(TRUE, FALSE); //使能串口接收中断,禁止串口发送中断,作为从机,等待主机传送的数据
    vMBPortTimersEnable();            //开启3.5T定时器

    EXIT_CRITICAL_SECTION();
}
/**禁用modbus协议栈
 * @description:
 * @return {*}
 */
void eMBRTUStop(void)
{
    ENTER_CRITICAL_SECTION();
    vMBPortSerialEnable(FALSE, FALSE); //关闭接收发送中断
    vMBPortTimersDisable();            //失能3.5T计时器
    EXIT_CRITICAL_SECTION();
}
/**接收到一帧后处理的回调函数
 * @description: modbus RTU报文接收函数回调函数
 * @param {UCHAR *} pucRcvAddress 接收到数据帧的站号
 * @param {UCHAR **} pucFrame PDU部分的数据
 * @param {USHORT *} pusLength PDU部分的长度（扣除站号和CRC)
 * @return {*}
 */
eMBErrorCode eMBRTUReceive(UCHAR *pucRcvAddress, UCHAR **pucFrame, USHORT *pusLength)
{
    BOOL xFrameReceived = FALSE;
    eMBErrorCode eStatus = MB_ENOERR;

    ENTER_CRITICAL_SECTION();
    assert(usRcvBufferPos < MB_SER_PDU_SIZE_MAX); //断言 接收是否溢出

    /* PDU长度是否大于4及CRC是否正确 */
    if ((usRcvBufferPos >= MB_SER_PDU_SIZE_MIN) && (usMBCRC16((UCHAR *)ucRTUBuf, usRcvBufferPos) == 0))
    {
        /* Save the address field. All frames are passed to the upper layed
         * and the decision if a frame is used is done there.
         */
        *pucRcvAddress = ucRTUBuf[MB_SER_PDU_ADDR_OFF]; //保存接收到的MODBUS数据中的站号

        /* Total length of Modbus-PDU is Modbus-Serial-Line-PDU minus
         * size of address field and CRC checksum.
         */
        *pusLength = (USHORT)(usRcvBufferPos - MB_SER_PDU_PDU_OFF - MB_SER_PDU_SIZE_CRC); //计算PDU部分长度

        /* Return the start of the Modbus PDU to the caller. */
        *pucFrame = (UCHAR *)&ucRTUBuf[MB_SER_PDU_PDU_OFF]; //保存PDU部分首地址指针
        xFrameReceived = TRUE;
    }
    else
    {
        eStatus = MB_EIO; //否则就是返回I/O错误
    }

    EXIT_CRITICAL_SECTION();
    return eStatus;
}
/**
 * @description: RTU报文发送回调函数
 * @param {UCHAR} ucSlaveAddress
 * @param {UCHAR *} pucFrame
 * @param {USHORT} usLength
 * @return {*}
 */
eMBErrorCode eMBRTUSend(UCHAR ucSlaveAddress, const UCHAR *pucFrame, USHORT usLength)
{
    eMBErrorCode eStatus = MB_ENOERR;
    USHORT usCRC16;

    ENTER_CRITICAL_SECTION();

    /* Check if the receiver is still in idle state. If not we where to
     * slow with processing the received frame and the master sent another
     * frame on the network. We have to abort sending the frame.
     */
    if (eRcvState == STATE_RX_IDLE)//发送帧拼接
    {
        /* First byte before the Modbus-PDU is the slave address. */
                    
//pucFrame - 1相等于ucMBFrame【0】
        pucSndBufferCur = (UCHAR *)pucFrame - 1;
        
        usSndBufferCount = 1;

        /* Now copy the Modbus-PDU into the Modbus-Serial-Line-PDU. */
        pucSndBufferCur[MB_SER_PDU_ADDR_OFF] = ucSlaveAddress;
       
        usSndBufferCount += usLength;

        /* Calculate CRC16 checksum for Modbus-Serial-Line-PDU. */
        usCRC16 = usMBCRC16((UCHAR *)pucSndBufferCur, usSndBufferCount);
        *(pucSndBufferCur+usSndBufferCount++)=(UCHAR)(usCRC16 & 0xFF);
        *(pucSndBufferCur+usSndBufferCount++)=(UCHAR)(usCRC16 >> 8);
        // ucRTUBuf[usSndBufferCount++] = (UCHAR)(usCRC16 & 0xFF);
        // ucRTUBuf[usSndBufferCount++] = (UCHAR)(usCRC16 >> 8);

        /* Activate the transmitter. */
        eSndState = STATE_TX_XMIT;
        vMBPortSerialEnable(FALSE, TRUE);
    }
    else
    {
        eStatus = MB_EIO;
    }
    EXIT_CRITICAL_SECTION();
    return eStatus;
}

/*函数功能
 *将接收到的数据存入ucRTUBuf[]中;
 *usRcvBufferPos为全局变量，表示接收数据的个数;
 *每接收到一个字节的数据，3.5T定时器清0
 */
/**串口接收中断最终调用此函数接收数据
 * @description:
 * @return {*} 接收锁
 */
BOOL xMBRTUReceiveFSM(void)
{
    BOOL xTaskNeedSwitch = FALSE;
    UCHAR ucByte;

    assert(eSndState == STATE_TX_IDLE); /*确保没有数据在发送*/

    /* Always read the character. */
    (void)xMBPortSerialGetByte((CHAR *)&ucByte); /*从串口数据寄存器读取一个字节数据*/

    switch (eRcvState) //判断接收状态
    {
        /* If we have received a character in the init state we have to
         * wait until the frame is finished.
         */
    case STATE_RX_INIT:        //在初始化状态
        vMBPortTimersEnable(); /*开启3.5T定时器*/
        break;

        /* In the error state we wait until all characters in the
         * damaged frame are transmitted.
         */
    case STATE_RX_ERROR:       //接收错误状态 /*数据帧被损坏，重启定时器，不保存串口接收的数据*/
        vMBPortTimersEnable(); /*开启3.5T定时器*/
        break;

        /* In the idle state we wait for a new character. If a character
         * is received the t1.5 and t3.5 timers are started and the
         * receiver is in the state STATE_RX_RECEIVCE.
         */
    case STATE_RX_IDLE: /*接收器空闲状态，开始接收，进入STATE_RX_RCV状态*/
        usRcvBufferPos = 0;
        if (ucRTUBuf[usRcvBufferPos] == SlaveAddress)//判断数据是否是本站数据（修改的部分）
        {
            ucRTUBuf[usRcvBufferPos++] = ucByte; //保存数据
            eRcvState = STATE_RX_RCV;            //状态切换为接收状态
        }

        /* Enable t3.5 timers. */
        vMBPortTimersEnable(); /*每收到一个字节，都重启3.5T定时器*/
        break;

        /* We are currently receiving a frame. Reset the timer after
         * every character received. If more than the maximum possible
         * number of bytes in a modbus frame is received the frame is
         * ignored.
         */
    case STATE_RX_RCV: //接收状态
        if (usRcvBufferPos < MB_SER_PDU_SIZE_MAX)
        {
            ucRTUBuf[usRcvBufferPos++] = ucByte; //保存数据
        }
        else
        {
            eRcvState = STATE_RX_ERROR; /*一帧报文的字节数大于最大PDU长度，忽略超出的数据*/
        }
        vMBPortTimersEnable(); /*每收到一个字节，都重启3.5T定时器*/
        break;
    }
    return xTaskNeedSwitch;
}

BOOL xMBRTUTransmitFSM(void)
{
    BOOL xNeedPoll = FALSE;

    assert(eRcvState == STATE_RX_IDLE);

    switch (eSndState)
    {
        /* We should not get a transmitter event if the transmitter is in
         * idle state.  */
    case STATE_TX_IDLE:
        /* enable receiver/disable transmitter. */
        vMBPortSerialEnable(TRUE, FALSE);
        break;

    case STATE_TX_XMIT:
        /* check if we are finished. */
        if (usSndBufferCount != 0)
        {
            xMBPortSerialPutByte((CHAR)*pucSndBufferCur);
            pucSndBufferCur++; /* next byte in sendbuffer. */
            usSndBufferCount--;
        }
        else
        {
            xNeedPoll = xMBPortEventPost(EV_FRAME_SENT);
            /* Disable transmitter. This prevents another transmit buffer
             * empty interrupt. */
            vMBPortSerialEnable(TRUE, FALSE);
            eSndState = STATE_TX_IDLE;
        }
        break;
    }

    return xNeedPoll;
}
/*函数功能：
 *1:从机接受完成一帧数据后，接收状态机eRcvState为STATE_RX_RCV；
 *2:上报“接收到报文”事件(EV_FRAME_RECEIVED)
 *3:禁止3.5T定时器，设置接收状态机eRcvState状态为STATE_RX_IDLE空闲;
 */
/**3.5T定时器中断回调函数
 * @description:
 * @return {*}
 */
BOOL xMBRTUTimerT35Expired(void)
{
    BOOL xNeedPoll = FALSE;

    switch (eRcvState) //判断接收状态
    {
        /* Timer t35 expired. Startup phase is finished. */

    case STATE_RX_INIT: /*初始化状态 上报modbus协议栈的事件状态给poll函数,EV_READY:初始化完成事件*/
        xNeedPoll = xMBPortEventPost(EV_READY);
        break;

        /* A frame was received and t35 expired. Notify the listener that
         * a new frame was received. */
    case STATE_RX_RCV: /*接收状态 上报modbus协议栈的事件状态给poll函数,EV_FRAME_RECEIVED:接收到帧*/
        xNeedPoll = xMBPortEventPost(EV_FRAME_RECEIVED);
        break;

        /* An error occured while receiving the frame. */
    case STATE_RX_ERROR:
        break;

        /* Function called in an illegal state. */
    default:
        assert((eRcvState == STATE_RX_INIT) ||
               (eRcvState == STATE_RX_RCV) || (eRcvState == STATE_RX_ERROR));
    }

    vMBPortTimersDisable();    //失能 3.5T中断
    eRcvState = STATE_RX_IDLE; //给出空闲状态

    return xNeedPoll;
}
