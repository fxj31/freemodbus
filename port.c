#include "mb.h"
#include "mbport.h"
#define setbit(x, y) x |= (1 << y)   //单独位置1
#define restbit(x, y) x &= ~(1 << y) //单独位清零
#define notbit(x, y) x ^= (1 << y)   //单独位取反
#define getbit(x, y) ((x) >> (y)&1)  //获取单独位的状态

#define get_byte0(x) ((x >> 0) & 0x000000ff)  /* 获取第0个字节 */
#define get_byte1(x) ((x >> 8) & 0x000000ff)  /* 获取第1个字节 */
#define get_byte2(x) ((x >> 16) & 0x000000ff) /* 获取第2个字节 */
#define get_byte3(x) ((x >> 24) & 0x000000ff) /* 获取第3个字节 */

#define clear32_byte0(x) (x &= 0xffffff00)  /* 清零32位第0个字节 */
#define clear32_byte1(x) (x &= 0xffff00ff)  /* 清零32位第1个字节 */
#define clear32_byte3(x) (x &= 0xff00ffff)  /* 清零32位第2个字节 */
#define clear32_byte4(x) (x &= 0x00ffffff)  /* 清零32位第3个字节 */

#define clear16_byte0(x) (x &= 0xfff00)  /* 清零16位第0个字节 */
#define clear16_byte1(x) (x &= 0x00ff)  /* 清零16位第1个字节 */

#define swap16(x) (x>>8|x<<8)//16位高低字节交换
#define swap8(x) (x>>4|x<<4)//8位高低4位交换

 // 十路输入寄存器
#define REG_INPUT_SIZE 10
uint16_t REG_INPUT_BUF[REG_INPUT_SIZE];

// 十路保持寄存器
#define REG_HOLD_SIZE 10
uint16_t REG_HOLD_BUF[REG_HOLD_SIZE];

// 十路线圈
#define REG_COILS_SIZE 10
uint8_t REG_COILS_BUF[REG_COILS_SIZE] = {1, 1, 1, 1, 0, 0, 0, 0, 1, 1};

// 十路离散量
#define REG_DISC_SIZE 10
uint8_t REG_DISC_BUF[REG_DISC_SIZE] = {1, 1, 1, 1, 0, 0, 0, 0, 1, 1};

/// 04命令处理回调函数
/**
 * @description: 
 * @param {UCHAR} *pucRegBuffer
 * @param {USHORT} usAddress
 * @param {USHORT} usNRegs
 * @return {*}
 */
eMBErrorCode eMBRegInputCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNRegs)
{
    USHORT usRegIndex = usAddress - 1;

    // 非法检测
    if ((usRegIndex + usNRegs) > REG_INPUT_SIZE)
    {
        return MB_ENOREG;
    }

    // 循环读取
    while (usNRegs > 0)
    {
        *pucRegBuffer++ = (unsigned char)(REG_INPUT_BUF[usRegIndex] >> 8);
        *pucRegBuffer++ = (unsigned char)(REG_INPUT_BUF[usRegIndex] & 0xFF);
        usRegIndex++;
        usNRegs--;
    }

    // 模拟输入寄存器被改变
    for (usRegIndex = 0; usRegIndex < REG_INPUT_SIZE; usRegIndex++)
    {
        REG_INPUT_BUF[usRegIndex]++;
    }

    return MB_ENOERR;
}

/// CMD6、3、16命令处理回调函数
eMBErrorCode eMBRegHoldingCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNRegs, eMBRegisterMode eMode)
{
    USHORT usRegIndex = usAddress - 1;

    // 非法检测
    if ((usRegIndex + usNRegs) > REG_HOLD_SIZE)
    {
        return MB_ENOREG;
    }

    // 写寄存器
    if (eMode == MB_REG_WRITE)
    {
        while (usNRegs > 0)
        {
            REG_HOLD_BUF[usRegIndex] = (pucRegBuffer[0] << 8) | pucRegBuffer[1];
            pucRegBuffer += 2;
            usRegIndex++;
            usNRegs--;
        }
    }

    // 读寄存器
    else
    {
        while (usNRegs > 0)
        {
            *pucRegBuffer++ = (unsigned char)(REG_HOLD_BUF[usRegIndex] >> 8);
            *pucRegBuffer++ = (unsigned char)(REG_HOLD_BUF[usRegIndex] & 0xFF);
            usRegIndex++;
            usNRegs--;
        }
    }

    return MB_ENOERR;
}

///
/**线圈类回调处理函数
 * @description: CMD1、5、15命令处理回调函数
 * @param {UCHAR *} pucRegBuffer 解析中间层函数解析完成解析数组
 * @param {USHORT} usAddress 处理线圈的首地址
 * @param {USHORT} usNCoils 处理的线圈数量
 * @param {eMBRegisterMode} eMode 处理状态 读：MB_REG_READ 写：MB_REG_WRITE
 * @return {*}
 */
eMBErrorCode eMBRegCoilsCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNCoils, eMBRegisterMode eMode)
{
    USHORT usRegIndex = usAddress - 1; //定义处理的起始线圈寄存器地址
    UCHAR ucBits = 0;
    UCHAR ucState = 0;
    UCHAR ucLoops = 0;

    // 非法检测
    if ((usRegIndex + usNCoils) > REG_COILS_SIZE)
    {
        return MB_ENOREG;
    }
    // assert((usRegIndex + usNCoils) < REG_COILS_SIZE);

    if (eMode == MB_REG_WRITE) //线圈写处理
    {
        ucLoops = (usNCoils - 1) / 8 + 1;
        while (ucLoops != 0)
        {
            ucState = *pucRegBuffer++;
            ucBits = 0;
            while (usNCoils != 0 && ucBits < 8)
            {
                REG_COILS_BUF[usRegIndex++] = (ucState >> ucBits) & 0X01;
                usNCoils--;
                ucBits++;
            }
            ucLoops--;
        }
    }
    else //线圈读处理
    {
        ucLoops = (usNCoils - 1) / 8 + 1;
        while (ucLoops != 0)
        {
            ucState = 0;
            ucBits = 0;
            while (usNCoils != 0 && ucBits < 8)
            {
                if (REG_COILS_BUF[usRegIndex])
                {
                    ucState |= (1 << ucBits);
                }
                usNCoils--;
                usRegIndex++;
                ucBits++;
            }
            *pucRegBuffer++ = ucState;
            ucLoops--;
        }
    }

    return MB_ENOERR;
}

/// CMD2命令处理回调函数
eMBErrorCode eMBRegDiscreteCB(UCHAR *pucRegBuffer, USHORT usAddress, USHORT usNDiscrete)
{
    USHORT usRegIndex = usAddress - 1;
    UCHAR ucBits = 0;
    UCHAR ucState = 0;
    UCHAR ucLoops = 0;

    // 非法检测
    if ((usRegIndex + usNDiscrete) > REG_DISC_SIZE)
    {
        return MB_ENOREG;
    }

    ucLoops = (usNDiscrete - 1) / 8 + 1;
    while (ucLoops != 0)
    {
        ucState = 0;
        ucBits = 0;
        while (usNDiscrete != 0 && ucBits < 8)
        {
            if (REG_DISC_BUF[usRegIndex])
            {
                ucState |= (1 << ucBits);
            }
            usNDiscrete--;
            usRegIndex++;
            ucBits++;
        }
        *pucRegBuffer++ = ucState;
        ucLoops--;
    }

    // 模拟离散量输入被改变
    for (usRegIndex = 0; usRegIndex < REG_DISC_SIZE; usRegIndex++)
    {
        REG_DISC_BUF[usRegIndex] = !REG_DISC_BUF[usRegIndex];
    }

    return MB_ENOERR;
}
