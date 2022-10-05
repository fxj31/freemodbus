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
#include "mbframe.h"
#include "mbproto.h"
#include "mbconfig.h"

/* ----------------------- Defines ------------------------------------------*/
#define MB_PDU_FUNC_READ_ADDR_OFF (MB_PDU_DATA_OFF)        //读功能的起始地址
#define MB_PDU_FUNC_READ_COILCNT_OFF (MB_PDU_DATA_OFF + 2) //读线圈数量起始地址
#define MB_PDU_FUNC_READ_SIZE (4)
#define MB_PDU_FUNC_READ_COILCNT_MAX (0x07D0)

#define MB_PDU_FUNC_WRITE_ADDR_OFF (MB_PDU_DATA_OFF)
#define MB_PDU_FUNC_WRITE_VALUE_OFF (MB_PDU_DATA_OFF + 2)
#define MB_PDU_FUNC_WRITE_SIZE (4)

#define MB_PDU_FUNC_WRITE_MUL_ADDR_OFF (MB_PDU_DATA_OFF)
#define MB_PDU_FUNC_WRITE_MUL_COILCNT_OFF (MB_PDU_DATA_OFF + 2)
#define MB_PDU_FUNC_WRITE_MUL_BYTECNT_OFF (MB_PDU_DATA_OFF + 4)
#define MB_PDU_FUNC_WRITE_MUL_VALUES_OFF (MB_PDU_DATA_OFF + 5)
#define MB_PDU_FUNC_WRITE_MUL_SIZE_MIN (5)
#define MB_PDU_FUNC_WRITE_MUL_COILCNT_MAX (0x07B0)

/* ----------------------- Static functions ---------------------------------*/
eMBException prveMBError2Exception(eMBErrorCode eErrorCode);

/* ----------------------- Start implementation -----------------------------*/

#if MB_FUNC_READ_COILS_ENABLED > 0
/**O1功能读线圈
 * @description: O1功能读线圈 帧解析中间层
 * @param {UCHAR *} pucFrame 解析帧=PDU
 * @param {USHORT *} usLen 帧长度 PDU长度
 * @return {*}MODBUS异常码
 * add+01+起始地址（2byte)+数量（2byte)+CRC
 * PDU=01+起始地址（2byte)+数量（2byte) 长度5位
 * 返回格式=ADD+01+字节数（1byte）+线圈状态（int((占线圈数量/8))+1)个字节）
 */
eMBException eMBFuncReadCoils(UCHAR *pucFrame, USHORT *usLen)
{
    USHORT usRegAddress; //用户寄存器地址
    USHORT usCoilCount;  //用户寄存器的数量
    UCHAR ucNBytes;
    UCHAR *pucFrameCur;

    eMBException eStatus = MB_EX_NONE;
    eMBErrorCode eRegStatus;

    if (*usLen == (MB_PDU_FUNC_READ_SIZE + MB_PDU_SIZE_MIN)) //判断长度是否为5位
    {
        /*线圈寄存器的起始地址（2个byte）合并到usRegAddress*/
        usRegAddress = (USHORT)(pucFrame[MB_PDU_FUNC_READ_ADDR_OFF] << 8);
        usRegAddress |= (USHORT)(pucFrame[MB_PDU_FUNC_READ_ADDR_OFF + 1]);
        usRegAddress++;
        /*线圈寄存器个数（2个byte）合并到usCoilCount*/
        usCoilCount = (USHORT)(pucFrame[MB_PDU_FUNC_READ_COILCNT_OFF] << 8);
        usCoilCount |= (USHORT)(pucFrame[MB_PDU_FUNC_READ_COILCNT_OFF + 1]);

        /* Check if the number of registers to read is valid. If not
         * return Modbus illegal data value exception.
         */
        if ((usCoilCount >= 1) &&
            (usCoilCount < MB_PDU_FUNC_READ_COILCNT_MAX)) //判断读线圈的是否在1-2000以内
        {
            /* Set the current PDU data pointer to the beginning. */
            /*为发送缓冲pucFrameCur赋值*/
            pucFrameCur = &pucFrame[MB_PDU_FUNC_OFF]; //给回调的帧第一位附加站号
            *usLen = MB_PDU_FUNC_OFF;

            /* First byte contains the function code. */
            *pucFrameCur++ = MB_FUNC_READ_COILS; //给回调的帧第二位功能码
            *usLen += 1;

            /* Test if the quantity of coils is a multiple of 8. If not last
             * byte is only partially field with unused coils set to zero. */
            if ((usCoilCount & 0x0007) != 0) //判断数量%8有没有余数
            {
                ucNBytes = (UCHAR)(usCoilCount / 8 + 1); //有余数则字节数=数量/8+1
            }
            else
            {
                ucNBytes = (UCHAR)(usCoilCount / 8); //被整除则字节数=数量/8
            }
            *pucFrameCur++ = ucNBytes; //给回调的帧第三位字节数
            *usLen += 1;
            //执行回调eMBRegCoilsCB（）
            eRegStatus =
                eMBRegCoilsCB(pucFrameCur, usRegAddress, usCoilCount,
                              MB_REG_READ);

            /* If an error occured convert it into a Modbus exception. */
            /* 如果回调执行错误切换协议栈状态 */
            if (eRegStatus != MB_ENOERR)
            {
                eStatus = prveMBError2Exception(eRegStatus);
            }
            else
            {
                /* The response contains the function code, the starting address
                 * and the quantity of registers. We reuse the old values in the
                 * buffer because they are still valid. */
                *usLen += ucNBytes;//最终长度加上字节数
                ;
            }
        }
        else
        {
            eStatus = MB_EX_ILLEGAL_DATA_VALUE;
        }
    }
    else
    {
        /* Can't be a valid read coil register request because the length
         * is incorrect. */
        eStatus = MB_EX_ILLEGAL_DATA_VALUE;
    }
    return eStatus;
}

#if MB_FUNC_WRITE_COIL_ENABLED > 0
/**05功能写单个线圈
 * @description: O5功能写单个线圈 帧解析中间层
 * @param {UCHAR *} pucFrame 解析帧=PDU
 * @param {USHORT *} usLen 帧长度 PDU长度
 * @return {*}
 * add+05+起始地址（2byte)+状态（2byte)+CRC
 * PDU=01+起始地址（2byte)+状态（2byte) 长度5位
 * 状态：ON=0XFF00  OFF=0X0000 其他值全为非法
 * 返回格式=ADD+01+地址（2byte）+线圈状态（0x0000或0XFF00）
 */
eMBException eMBFuncWriteCoil(UCHAR *pucFrame, USHORT *usLen)
{
    USHORT usRegAddress;
    UCHAR ucBuf[2];

    eMBException eStatus = MB_EX_NONE;
    eMBErrorCode eRegStatus;

    if (*usLen == (MB_PDU_FUNC_WRITE_SIZE + MB_PDU_SIZE_MIN))
    {
        usRegAddress = (USHORT)(pucFrame[MB_PDU_FUNC_WRITE_ADDR_OFF] << 8);
        usRegAddress |= (USHORT)(pucFrame[MB_PDU_FUNC_WRITE_ADDR_OFF + 1]);
        usRegAddress++;
        //判断是否非法字符 ON=0XFF00  OFF=0X0000 其他值全为非法
        if ((pucFrame[MB_PDU_FUNC_WRITE_VALUE_OFF + 1] == 0x00) &&
            ((pucFrame[MB_PDU_FUNC_WRITE_VALUE_OFF] == 0xFF) ||
             (pucFrame[MB_PDU_FUNC_WRITE_VALUE_OFF] == 0x00)))
        {
            ucBuf[1] = 0;
            if (pucFrame[MB_PDU_FUNC_WRITE_VALUE_OFF] == 0xFF)
            {
                ucBuf[0] = 1;
            }
            else
            {
                ucBuf[0] = 0;
            }
            eRegStatus =
                eMBRegCoilsCB(&ucBuf[0], usRegAddress, 1, MB_REG_WRITE);

            /* If an error occured convert it into a Modbus exception. */
            if (eRegStatus != MB_ENOERR)
            {
                eStatus = prveMBError2Exception(eRegStatus);
            }
        }
        else
        {
            eStatus = MB_EX_ILLEGAL_DATA_VALUE;
        }
    }
    else
    {
        /* Can't be a valid write coil register request because the length
         * is incorrect. */
        eStatus = MB_EX_ILLEGAL_DATA_VALUE;
    }
    return eStatus;
}

#endif

#if MB_FUNC_WRITE_MULTIPLE_COILS_ENABLED > 0
eMBException
eMBFuncWriteMultipleCoils(UCHAR *pucFrame, USHORT *usLen)
{
    USHORT usRegAddress;
    USHORT usCoilCnt;
    UCHAR ucByteCount;
    UCHAR ucByteCountVerify;

    eMBException eStatus = MB_EX_NONE;
    eMBErrorCode eRegStatus;

    if (*usLen > (MB_PDU_FUNC_WRITE_SIZE + MB_PDU_SIZE_MIN))
    {
        usRegAddress = (USHORT)(pucFrame[MB_PDU_FUNC_WRITE_MUL_ADDR_OFF] << 8);
        usRegAddress |= (USHORT)(pucFrame[MB_PDU_FUNC_WRITE_MUL_ADDR_OFF + 1]);
        usRegAddress++;

        usCoilCnt = (USHORT)(pucFrame[MB_PDU_FUNC_WRITE_MUL_COILCNT_OFF] << 8);
        usCoilCnt |= (USHORT)(pucFrame[MB_PDU_FUNC_WRITE_MUL_COILCNT_OFF + 1]);

        ucByteCount = pucFrame[MB_PDU_FUNC_WRITE_MUL_BYTECNT_OFF];

        /* Compute the number of expected bytes in the request. */
        if ((usCoilCnt & 0x0007) != 0)
        {
            ucByteCountVerify = (UCHAR)(usCoilCnt / 8 + 1);
        }
        else
        {
            ucByteCountVerify = (UCHAR)(usCoilCnt / 8);
        }

        if ((usCoilCnt >= 1) &&
            (usCoilCnt <= MB_PDU_FUNC_WRITE_MUL_COILCNT_MAX) &&
            (ucByteCountVerify == ucByteCount))
        {
            eRegStatus =
                eMBRegCoilsCB(&pucFrame[MB_PDU_FUNC_WRITE_MUL_VALUES_OFF],
                              usRegAddress, usCoilCnt, MB_REG_WRITE);

            /* If an error occured convert it into a Modbus exception. */
            if (eRegStatus != MB_ENOERR)
            {
                eStatus = prveMBError2Exception(eRegStatus);
            }
            else
            {
                /* The response contains the function code, the starting address
                 * and the quantity of registers. We reuse the old values in the
                 * buffer because they are still valid. */
                *usLen = MB_PDU_FUNC_WRITE_MUL_BYTECNT_OFF;
            }
        }
        else
        {
            eStatus = MB_EX_ILLEGAL_DATA_VALUE;
        }
    }
    else
    {
        /* Can't be a valid write coil register request because the length
         * is incorrect. */
        eStatus = MB_EX_ILLEGAL_DATA_VALUE;
    }
    return eStatus;
}

#endif

#endif
