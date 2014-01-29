/**   RPN Scientific Calculator for MSP430
 *    Copyright (C) 2014 Joey Shepard
 *
 *    This program is free software: you can redistribute it and/or modify
 *    it under the terms of the GNU General Public License as published by
 *    the Free Software Foundation, either version 3 of the License, or
 *    (at your option) any later version.
 *
 *    This program is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
**/

#include <msp430.h>
#include <stdbool.h>
#include "common.h"

#define ADDRESS_LATCH   BIT0  //P1.0 SRAM 595 latches
#define UART_RXD        BIT1  //P1.1 To TXD of slave
#define UART_TXD        BIT2  //P1.2 To RXD of slave
#define RAM_OE          BIT3  //P1.3 SRAM #OE
#define RAM_WE          BIT4  //P1.4 SRAM #WE
#define ADRESSS_CLOCK   BIT5  //P1.5 SRAM 595 clock
#define LED             BIT6  //P1.6 LED
#define ADDRESS_DATA    BIT7  //P1.7 SRAM 595 data

#pragma MM_READ RAM_Read
#pragma MM_WRITE RAM_Write
#pragma MM_ON

static void Init();

static void RAM_Write(const unsigned char *a1, const unsigned char byte);
static unsigned char RAM_Read(const unsigned char *a1);

static void MakeTables();

static void AddBCD(unsigned char *result, const unsigned char *n1, const unsigned char *n2);
static void SubBCD(unsigned char *result, const unsigned char *n1, unsigned char *n2);
static void ImmedBCD(const char *text, unsigned char *BCD);
static void BufferBCD(const unsigned char *text, unsigned char *BCD);
static void PrintBCD(const unsigned char *BCD, int dec_point);
static void MultBCD(unsigned char *result, const unsigned char *n1, const unsigned char *n2);
static void DivBCD(unsigned char *result, const unsigned char *n1, const unsigned char *n2);
static void ShrinkBCD(unsigned char *dest,unsigned char *src);
static void FullShrinkBCD(unsigned char *n1);
static void PadBCD(unsigned char *n1, int amount);
static bool IsZero(unsigned char *n1);
static void CopyBCD(unsigned char *dest, unsigned char *src);
static bool LnBCD(unsigned char *result, unsigned char *arg);
static void ExpBCD(unsigned char *result, unsigned char *arg);
static void RolBCD(unsigned char *result, unsigned char *arg, unsigned char amount);
static void RorBCD(unsigned char *result, unsigned char *arg, unsigned char amount);
static void PowBCD(unsigned char *result, unsigned char *base, unsigned char *exp);
static void TanBCD(unsigned char *sine_result,unsigned char *cos_result,unsigned char *arg);
static void AcosBCD(unsigned char *result,unsigned char *arg);
static void AsinBCD(unsigned char *result,unsigned char *arg);
static void AtanBCD(unsigned char *result,unsigned char *arg);
static void CalcTanBCD(unsigned char *result1,unsigned char *result2,unsigned char *result3,unsigned char *arg,unsigned char flag);
static unsigned char CompBCD(const char *num, unsigned char *var);
static unsigned char CompVarBCD(unsigned char *var1, unsigned char *var2);
static unsigned char TrigPrep(unsigned int stack_ptr_copy,unsigned char *cosine);

static void SetDecPlaces();

#pragma MM_OFFSET 0
#pragma MM_GLOBALS
  unsigned char p0[260]; //PowBCD, LnBCD, ExpBCD, TanBCD, AtanBCD, typing,    CompBCD
  unsigned char p1[260]; //PowBCD, LnBCD, ExpBCD, TanBCD, AtanBCD, DrawStack, CompBCD, CompVarBCD
  unsigned char p2[260]; //PowBCD, LnBCD, ExpBCD, TanBCD, AtanBCD
  unsigned char p3[260]; //PowBCD
  unsigned char p4[260]; //PowBCD
  unsigned char p5[260];
  unsigned char p6[260];
  unsigned char p7[260];
  unsigned char buffer[260]; //AddBCD
  unsigned char perm_buff1[260]; //DivBCD, MultBCD
  unsigned char perm_buff2[260]; //DivBCD
  unsigned char perm_buff3[260]; //DivBCD
  //unsigned char logs[MATH_LOG_TABLE*MATH_ENTRY_SIZE];
  unsigned char logs[4218];
  //unsigned char trig[MATH_TRIG_TABLE*MATH_ENTRY_SIZE];
  unsigned char trig[4218];
  unsigned char perm_zero[4];
  unsigned char perm_K[36];
  unsigned char perm_log10[36];
  unsigned char BCD_stack[52000];
  unsigned char stack_buffer[260];
#pragma MM_END

struct SettingsType Settings;

int main(void)
{
  WDTCTL=WDTPW + WDTHOLD;

  BCSCTL1=CALBC1_16MHZ;
  DCOCTL=CALDCO_16MHZ;

  volatile unsigned int i;
  for (i=0;i<32000;i++); //Let clock settle. Necessary?

  UCA0CTL1=UCSWRST|UCSSEL_2;
  UCA0CTL0 = 0;
  UCA0MCTL = UCBRS_5;
  UCA0BR0 = 131;
  UCA0BR1 = 0;
  UCA0CTL1&=~UCSWRST;
  __enable_interrupt();

  UCB0CTL1=UCSWRST;
  UCB0CTL0=UCCKPL|UCMST|UCSYNC;
  UCB0CTL1|=UCSSEL_2;
  UCB0BR0=0;
  UCB0BR1=0;
  UCB0CTL1&=~UCSWRST;

  P1SEL=ADRESSS_CLOCK|ADDRESS_DATA|UART_RXD|UART_TXD;
  P1SEL2=ADRESSS_CLOCK|ADDRESS_DATA|UART_RXD|UART_TXD;

  P2SEL=0;
  P2SEL2=0;

  P1OUT=RAM_OE|RAM_WE;
  P1DIR=RAM_OE|RAM_WE|ADDRESS_LATCH|LED|UART_TXD;

  P2DIR=0;

  #pragma MM_ASSIGN_GLOBALS

  unsigned int a0,a1,a2,a3;
  unsigned char b0,b1;
  unsigned char command;

  Init();

  for (;;)
  {
    command=UART_Receive(false);
    P1OUT|=LED;
    switch (command)
    {
      case SlaveRAM_Read:
        a0=UART_ReceiveWord(false);
        UART_Send(RAM_Read((unsigned char *)a0),true);
        break;
      case SlaveRAM_Write:
        a0=UART_ReceiveWord(false);
        RAM_Write((unsigned char *)a0,UART_Receive(true));
        UART_Ready();
        break;
      case SlaveSync:
        UART_Send(SlaveAnswer,true);
        break;
      case SlaveMakeTables:
        MakeTables();
        UART_Ready();
        break;
      case SlaveAdd:
        a0=UART_ReceiveWord(false);
        a1=UART_ReceiveWord(false);
        a2=UART_ReceiveWord(true);
        AddBCD((unsigned char *)a0,(unsigned char *)a1,(unsigned char *)a2);
        UART_Ready();
        break;
      case SlaveSubtract:
        a0=UART_ReceiveWord(false);
        a1=UART_ReceiveWord(false);
        a2=UART_ReceiveWord(true);
        SubBCD((unsigned char *)a0,(unsigned char *)a1,(unsigned char *)a2);
        UART_Ready();
        break;
      case SlaveBuffer:
        a0=UART_ReceiveWord(false);
        a1=UART_ReceiveWord(true);
        BufferBCD((unsigned char *)a0,(unsigned char *)a1);
        UART_Ready();
        break;
      case SlaveMultiply:
        a0=UART_ReceiveWord(false);
        a1=UART_ReceiveWord(false);
        a2=UART_ReceiveWord(true);
        MultBCD((unsigned char *)a0,(unsigned char *)a1,(unsigned char *)a2);
        UART_Ready();
        break;
      case SlaveDivide:
        a0=UART_ReceiveWord(false);
        a1=UART_ReceiveWord(false);
        a2=UART_ReceiveWord(true);
        DivBCD((unsigned char *)a0,(unsigned char *)a1,(unsigned char *)a2);
        UART_Ready();
        break;
      case SlaveShrink:
        a0=UART_ReceiveWord(false);
        a1=UART_ReceiveWord(true);
        ShrinkBCD((unsigned char *)a0,(unsigned char *)a1);
        UART_Ready();
        break;
      case SlaveFullShrink:
        a0=UART_ReceiveWord(true);
        FullShrinkBCD((unsigned char *)a0);
        UART_Ready();
        break;
      case SlavePad:
        a0=UART_ReceiveWord(false);
        a1=UART_ReceiveWord(true);
        PadBCD((unsigned char *)a0,(int)a1);
        UART_Ready();
        break;
      case SlaveIsZero:
        a0=UART_ReceiveWord(false);
        if (IsZero((unsigned char *)a0)) UART_Send(1,true);
        else UART_Send(0,true);
        break;
      case SlaveCopy:
        a0=UART_ReceiveWord(false);
        a1=UART_ReceiveWord(true);
        CopyBCD((unsigned char *)a0,(unsigned char *)a1);
        UART_Ready();
        break;
      case SlaveLn:
        a0=UART_ReceiveWord(false);
        a1=UART_ReceiveWord(false);
        if (LnBCD((unsigned char *)a0,(unsigned char *)a1)) UART_Send(1,true);
        else UART_Send(0,true);
        break;
      case SlaveExp:
        a0=UART_ReceiveWord(false);
        a1=UART_ReceiveWord(true);
        ExpBCD((unsigned char *)a0,(unsigned char *)a1);
        UART_Ready();
        break;
      case SlaveRol:
        a0=UART_ReceiveWord(false);
        a1=UART_ReceiveWord(false);
        a2=UART_ReceiveWord(true);
        RolBCD((unsigned char *)a0,(unsigned char *)a1,a2);
        UART_Ready();
        break;
      case SlaveRor:
        a0=UART_ReceiveWord(false);
        a1=UART_ReceiveWord(false);
        a2=UART_ReceiveWord(true);
        RorBCD((unsigned char *)a0,(unsigned char *)a1,a2);
        UART_Ready();
        break;
      case SlavePow:
        a0=UART_ReceiveWord(false);
        a1=UART_ReceiveWord(false);
        a2=UART_ReceiveWord(true);
        PowBCD((unsigned char *)a0,(unsigned char *)a1,(unsigned char *)a2);
        UART_Ready();
        break;
      case SlaveTan:
        a0=UART_ReceiveWord(false);
        a1=UART_ReceiveWord(false);
        a2=UART_ReceiveWord(true);
        TanBCD((unsigned char *)a0,(unsigned char *)a1,(unsigned char *)a2);
        UART_Ready();
        break;
      case SlaveAcos:
        a0=UART_ReceiveWord(false);
        a1=UART_ReceiveWord(true);
        AcosBCD((unsigned char *)a0,(unsigned char *)a1);
        UART_Ready();
        break;
      case SlaveAsin:
        a0=UART_ReceiveWord(false);
        a1=UART_ReceiveWord(true);
        AsinBCD((unsigned char *)a0,(unsigned char *)a1);
        UART_Ready();
        break;
      case SlaveAtan:
        a0=UART_ReceiveWord(false);
        a1=UART_ReceiveWord(true);
        AtanBCD((unsigned char *)a0,(unsigned char *)a1);
        UART_Ready();
        break;
      case SlaveCalcTan:
        a0=UART_ReceiveWord(false);
        a1=UART_ReceiveWord(false);
        a2=UART_ReceiveWord(false);
        a3=UART_ReceiveWord(false);
        b0=UART_Receive(true);
        CalcTanBCD((unsigned char *)a0,(unsigned char *)a1,(unsigned char *)a2,(unsigned char *)a3,b0);
        UART_Ready();
        break;
      case SlaveCompVar:
        a0=UART_ReceiveWord(false);
        a1=UART_ReceiveWord(false);
        UART_Send(CompVarBCD((unsigned char *)a0,(unsigned char *)a1),true);
        break;
      case SlaveTrigPrep:
        a0=UART_ReceiveWord(false);
        b1=TrigPrep(a0,&b0);
        UART_Send(b0,true);
        UART_Send(b1,true);
        break;
      case SlaveSettings:
        Settings.DecPlaces=UART_Receive(false);
        Settings.DegRad=UART_Receive(false);
        Settings.LogTableSize=UART_Receive(false);
        Settings.TrigTableSize=UART_Receive(false);
        break;
    }
    P1OUT&=~LED;
  }
}

static void Init()
{
  Settings.DecPlaces=12;
  Settings.DegRad=true;
  Settings.LogTableSize=MATH_LOG_TABLE;
  Settings.TrigTableSize=MATH_TRIG_TABLE;
}

static unsigned char RAM_Read(const unsigned char *a1)
{
  volatile unsigned char value,v2;

  UCB0TXBUF=((unsigned int)a1)>>8;

  UCB0TXBUF=((unsigned int)a1)&0xFF;
  __delay_cycles(10);
  P1OUT&=~ADDRESS_LATCH;
  P1OUT|=ADDRESS_LATCH;
  P1OUT&=~RAM_OE;
  value=P2IN;
  P1OUT|=RAM_OE;
  return value;
}

static void RAM_Write(const unsigned char *a1, const unsigned char byte)
{
  UCB0TXBUF=((unsigned int)a1)>>8;
  UCB0TXBUF=((unsigned int)a1)&0xFF;
  __delay_cycles(10);
  P1OUT&=~ADDRESS_LATCH;
  P1OUT|=ADDRESS_LATCH;
  P1OUT|=RAM_OE;//this is redundant
  P2OUT=byte;
  P2DIR=0xFF;
  P1OUT&=~RAM_WE;
  P1OUT|=RAM_WE;
  P2DIR=0;
}

static void MakeTables()
{
  //Log table
  static const unsigned char table[]={
  17 ,0x88,0x72,0x28,0x39,0x11,0x16,0x72,0x99,0x96,0x05,0x40,0x57,0x11,0x54,0x66,0x46,0x60,
  17 ,0x44,0x36,0x14,0x19,0x55,0x58,0x36,0x49,0x98,0x02,0x70,0x28,0x55,0x77,0x33,0x23,0x30,
  17 ,0x22,0x18,0x07,0x09,0x77,0x79,0x18,0x24,0x99,0x01,0x35,0x14,0x27,0x88,0x66,0x61,0x65,
  17 ,0x11,0x09,0x03,0x54,0x88,0x89,0x59,0x12,0x49,0x50,0x67,0x57,0x13,0x94,0x33,0x30,0x83,
  17 ,0x05,0x54,0x51,0x77,0x44,0x44,0x79,0x56,0x24,0x75,0x33,0x78,0x56,0x97,0x16,0x65,0x41,
  17 ,0x02,0x77,0x25,0x88,0x72,0x22,0x39,0x78,0x12,0x37,0x66,0x89,0x28,0x48,0x58,0x32,0x71,
  17 ,0x01,0x38,0x62,0x94,0x36,0x11,0x19,0x89,0x06,0x18,0x83,0x44,0x64,0x24,0x29,0x16,0x35,
  16 ,0x69,0x31,0x47,0x18,0x05,0x59,0x94,0x53,0x09,0x41,0x72,0x32,0x12,0x14,0x58,0x18,
  16 ,0x40,0x54,0x65,0x10,0x81,0x08,0x16,0x43,0x81,0x97,0x80,0x13,0x11,0x54,0x64,0x35,
  16 ,0x22,0x31,0x43,0x55,0x13,0x14,0x20,0x97,0x55,0x76,0x62,0x95,0x09,0x03,0x09,0x83,
  16 ,0x11,0x77,0x83,0x03,0x56,0x56,0x38,0x34,0x54,0x53,0x87,0x94,0x10,0x94,0x70,0x52,
  16 ,0x06,0x06,0x24,0x62,0x18,0x16,0x43,0x48,0x42,0x58,0x06,0x06,0x13,0x20,0x40,0x42,
  16 ,0x03,0x07,0x71,0x65,0x86,0x66,0x75,0x36,0x88,0x37,0x10,0x28,0x20,0x75,0x96,0x77,
  16 ,0x01,0x55,0x04,0x18,0x65,0x35,0x96,0x52,0x54,0x15,0x08,0x54,0x04,0x60,0x42,0x45,
  15 ,0x77,0x82,0x14,0x04,0x42,0x05,0x49,0x48,0x94,0x74,0x62,0x90,0x00,0x61,0x14,
  15 ,0x38,0x98,0x64,0x04,0x15,0x65,0x73,0x23,0x01,0x39,0x37,0x34,0x30,0x95,0x84,
  15 ,0x19,0x51,0x22,0x01,0x31,0x26,0x17,0x49,0x43,0x96,0x74,0x04,0x95,0x31,0x84,
  15 ,0x09,0x76,0x08,0x59,0x73,0x05,0x54,0x58,0x89,0x59,0x60,0x82,0x49,0x08,0x02,
  15 ,0x04,0x88,0x16,0x20,0x79,0x50,0x13,0x51,0x18,0x85,0x37,0x04,0x96,0x92,0x65,
  15 ,0x02,0x44,0x11,0x08,0x27,0x52,0x73,0x62,0x70,0x91,0x60,0x47,0x90,0x85,0x82,
  15 ,0x01,0x22,0x06,0x28,0x62,0x52,0x56,0x77,0x37,0x16,0x23,0x05,0x53,0x67,0x16,
  14 ,0x61,0x03,0x32,0x93,0x68,0x06,0x38,0x52,0x49,0x13,0x15,0x87,0x89,0x65,
  14 ,0x30,0x51,0x71,0x12,0x47,0x31,0x86,0x37,0x85,0x69,0x06,0x95,0x14,0x17,
  14 ,0x15,0x25,0x86,0x72,0x64,0x83,0x62,0x39,0x74,0x05,0x75,0x73,0x25,0x13,
  14 ,0x07,0x62,0x93,0x65,0x42,0x75,0x67,0x57,0x21,0x55,0x88,0x52,0x96,0x85,
  14 ,0x03,0x81,0x46,0x89,0x98,0x96,0x85,0x88,0x94,0x80,0x71,0x17,0x84,0x98,
  14 ,0x01,0x90,0x73,0x46,0x81,0x38,0x25,0x40,0x94,0x15,0x46,0x94,0x42,0x51,
  13 ,0x95,0x36,0x73,0x86,0x16,0x59,0x18,0x82,0x33,0x90,0x84,0x15,0x51,
  13 ,0x47,0x68,0x37,0x04,0x45,0x16,0x32,0x34,0x18,0x44,0x34,0x61,0x75,
  13 ,0x23,0x84,0x18,0x55,0x06,0x79,0x85,0x75,0x87,0x10,0x42,0x36,0x79,
  13 ,0x11,0x92,0x09,0x28,0x24,0x45,0x35,0x44,0x57,0x08,0x75,0x79,0x16,
  13 ,0x05,0x96,0x04,0x64,0x29,0x99,0x03,0x38,0x56,0x18,0x58,0x25,0x32,
  13 ,0x02,0x98,0x02,0x32,0x19,0x43,0x60,0x61,0x11,0x47,0x31,0x97,0x05,
  13 ,0x01,0x49,0x01,0x16,0x10,0x82,0x82,0x53,0x54,0x89,0x03,0x91,0x82,
  12 ,0x74,0x50,0x58,0x05,0x69,0x16,0x82,0x52,0x64,0x72,0x34,0x52,
  12 ,0x37,0x25,0x29,0x02,0x91,0x52,0x30,0x20,0x17,0x58,0x25,0x70,
  12 ,0x18,0x62,0x64,0x51,0x47,0x49,0x62,0x33,0x55,0x74,0x27,0x31,
  12 ,0x09,0x31,0x32,0x25,0x74,0x18,0x17,0x97,0x64,0x69,0x00,0x06,
  12 ,0x04,0x65,0x66,0x12,0x87,0x19,0x93,0x19,0x04,0x05,0x97,0x61,
  12 ,0x02,0x32,0x83,0x06,0x43,0x62,0x67,0x64,0x57,0x45,0x98,0x32,
  12 ,0x01,0x16,0x41,0x53,0x21,0x82,0x01,0x58,0x55,0x08,0x75,0x62,
  11 ,0x58,0x20,0x76,0x60,0x91,0x17,0x73,0x34,0x13,0x32,0x12,
  11 ,0x29,0x10,0x38,0x30,0x45,0x63,0x10,0x18,0x71,0x39,0x66,
  11 ,0x14,0x55,0x19,0x15,0x22,0x82,0x60,0x97,0x26,0x88,0x23,
  11 ,0x07,0x27,0x59,0x57,0x61,0x41,0x56,0x95,0x61,0x23,0x72,
  11 ,0x03,0x63,0x79,0x78,0x80,0x70,0x85,0x09,0x55,0x06,0x76,
  11 ,0x01,0x81,0x89,0x89,0x40,0x35,0x44,0x20,0x21,0x14,0x60,
  10 ,0x90,0x94,0x94,0x70,0x17,0x72,0x51,0x46,0x47,0x61,
  10 ,0x45,0x47,0x47,0x35,0x08,0x86,0x36,0x07,0x21,0x38,
  10 ,0x22,0x73,0x73,0x67,0x54,0x43,0x20,0x62,0x10,0x08,
  10 ,0x11,0x36,0x86,0x83,0x77,0x21,0x60,0x95,0x67,0x39,
  10 ,0x05,0x68,0x43,0x41,0x88,0x60,0x80,0x63,0x99,0x28,
  10 ,0x02,0x84,0x21,0x70,0x94,0x30,0x40,0x36,0x03,0x54,
  10 ,0x01,0x42,0x10,0x85,0x47,0x15,0x20,0x19,0x02,0x74,
  9 ,0x71,0x05,0x42,0x73,0x57,0x60,0x09,0x76,0x61,
  9 ,0x35,0x52,0x71,0x36,0x78,0x80,0x04,0x94,0x62,
  9 ,0x17,0x76,0x35,0x68,0x39,0x40,0x02,0x48,0x89,
  9 ,0x08,0x88,0x17,0x84,0x19,0x70,0x01,0x24,0x84,
  9 ,0x04,0x44,0x08,0x92,0x09,0x85,0x00,0x62,0x52,
  9 ,0x02,0x22,0x04,0x46,0x04,0x92,0x50,0x31,0x28,
  9 ,0x01,0x11,0x02,0x23,0x02,0x46,0x25,0x15,0x65,
  8 ,0x55,0x51,0x11,0x51,0x23,0x12,0x57,0x83,
  8 ,0x27,0x75,0x55,0x75,0x61,0x56,0x28,0x91,
  8 ,0x13,0x87,0x77,0x87,0x80,0x78,0x14,0x46,
  8 ,0x06,0x93,0x88,0x93,0x90,0x39,0x07,0x23,
  8 ,0x03,0x46,0x94,0x46,0x95,0x19,0x53,0x61,
  8 ,0x01,0x73,0x47,0x23,0x47,0x59,0x76,0x81,
  7 ,0x86,0x73,0x61,0x73,0x79,0x88,0x40,
  7 ,0x43,0x36,0x80,0x86,0x89,0x94,0x20,
  7 ,0x21,0x68,0x40,0x43,0x44,0x97,0x10,
  7 ,0x10,0x84,0x20,0x21,0x72,0x48,0x55,
  7 ,0x05,0x42,0x10,0x10,0x86,0x24,0x27,
  7 ,0x02,0x71,0x05,0x05,0x43,0x12,0x14,
  7 ,0x01,0x35,0x52,0x52,0x71,0x56,0x07,
  6 ,0x67,0x76,0x26,0x35,0x78,0x03,
  6 ,0x33,0x88,0x13,0x17,0x89,0x02,
  6 ,0x16,0x94,0x06,0x58,0x94,0x51,
  6 ,0x08,0x47,0x03,0x29,0x47,0x25,
  6 ,0x04,0x23,0x51,0x64,0x73,0x63,
  6 ,0x02,0x11,0x75,0x82,0x36,0x81,
  6 ,0x01,0x05,0x87,0x91,0x18,0x41,
  5 ,0x52,0x93,0x95,0x59,0x20,
  5 ,0x26,0x46,0x97,0x79,0x60,
  5 ,0x13,0x23,0x48,0x89,0x80,
  5 ,0x06,0x61,0x74,0x44,0x90,
  5 ,0x03,0x30,0x87,0x22,0x45,
  5 ,0x01,0x65,0x43,0x61,0x22,
  4 ,0x82,0x71,0x80,0x61,
  4 ,0x41,0x35,0x90,0x31,
  4 ,0x20,0x67,0x95,0x15,
  4 ,0x10,0x33,0x97,0x58,
  4 ,0x05,0x16,0x98,0x79,
  4 ,0x02,0x58,0x49,0x39,
  4 ,0x01,0x29,0x24,0x70,
  3 ,0x64,0x62,0x35,
  3 ,0x32,0x31,0x17,
  3 ,0x16,0x15,0x59,
  3 ,0x08,0x07,0x79,
  3 ,0x04,0x03,0x90,
  3 ,0x02,0x01,0x95,
  3 ,0x01,0x00,0x97,
  2 ,0x50,0x49,
  2 ,0x25,0x24,
  2 ,0x12,0x62,
  2 ,0x06,0x31,
  2 ,0x03,0x15,
  2 ,0x01,0x58,
  1 ,0x79,
  1 ,0x39,
  1 ,0x20,
  1 ,0x10,
  1 ,0x05,
  1 ,0x02,
  1 ,0x01,
  //Trig table
  17 ,0x45,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
  17 ,0x26,0x56,0x50,0x51,0x17,0x70,0x77,0x98,0x93,0x51,0x57,0x21,0x93,0x72,0x04,0x53,0x29,
  17 ,0x14,0x03,0x62,0x43,0x46,0x79,0x26,0x47,0x85,0x82,0x89,0x23,0x20,0x15,0x91,0x63,0x42,
  17 ,0x07,0x12,0x50,0x16,0x34,0x89,0x01,0x79,0x75,0x61,0x95,0x33,0x00,0x84,0x12,0x06,0x84,
  17 ,0x03,0x57,0x63,0x34,0x37,0x49,0x97,0x35,0x10,0x30,0x68,0x47,0x78,0x91,0x44,0x58,0x82,
  17 ,0x01,0x78,0x99,0x10,0x60,0x82,0x46,0x06,0x93,0x07,0x15,0x02,0x49,0x77,0x60,0x79,0x09,
  16 ,0x89,0x51,0x73,0x71,0x02,0x11,0x07,0x43,0x13,0x64,0x12,0x16,0x82,0x30,0x79,0x53,
  16 ,0x44,0x76,0x14,0x17,0x08,0x60,0x55,0x30,0x73,0x09,0x43,0x53,0x82,0x54,0x23,0x82,
  16 ,0x22,0x38,0x10,0x50,0x03,0x68,0x53,0x80,0x75,0x12,0x35,0x33,0x54,0x24,0x30,0x59,
  16 ,0x11,0x19,0x05,0x67,0x70,0x66,0x20,0x68,0x87,0x27,0x54,0x75,0x79,0x70,0x34,0x72,
  16 ,0x05,0x59,0x52,0x89,0x18,0x93,0x80,0x36,0x68,0x17,0x44,0x24,0x13,0x44,0x04,0x23,
  16 ,0x02,0x79,0x76,0x45,0x26,0x17,0x00,0x36,0x74,0x59,0x91,0x79,0x11,0x92,0x36,0x83,
  16 ,0x01,0x39,0x88,0x22,0x71,0x42,0x26,0x50,0x14,0x62,0x86,0x87,0x63,0x57,0x24,0x36,
  15 ,0x69,0x94,0x11,0x36,0x75,0x35,0x29,0x18,0x45,0x75,0x24,0x89,0x32,0x87,0x82,
  15 ,0x34,0x97,0x05,0x68,0x50,0x70,0x40,0x11,0x05,0x84,0x42,0x77,0x35,0x40,0x77,
  15 ,0x17,0x48,0x52,0x84,0x26,0x98,0x04,0x49,0x52,0x15,0x80,0x88,0x73,0x44,0x18,
  15 ,0x08,0x74,0x26,0x42,0x13,0x69,0x37,0x80,0x26,0x02,0x61,0x92,0x68,0x64,0x27,
  15 ,0x04,0x37,0x13,0x21,0x06,0x87,0x23,0x34,0x56,0x75,0x78,0x22,0x83,0x81,0x83,
  15 ,0x02,0x18,0x56,0x60,0x53,0x43,0x93,0x47,0x83,0x84,0x70,0x43,0x88,0x58,0x09,
  15 ,0x01,0x09,0x28,0x30,0x26,0x72,0x00,0x71,0x48,0x85,0x70,0x39,0x80,0x29,0x58,
  14 ,0x54,0x64,0x15,0x13,0x36,0x00,0x85,0x44,0x04,0x52,0x09,0x67,0x46,0x64,
  14 ,0x27,0x32,0x07,0x56,0x68,0x00,0x48,0x93,0x22,0x46,0x91,0x06,0x02,0x51,
  14 ,0x13,0x66,0x03,0x78,0x34,0x00,0x25,0x24,0x26,0x26,0x06,0x30,0x80,0x30,
  14 ,0x06,0x83,0x01,0x89,0x17,0x00,0x12,0x71,0x83,0x75,0x85,0x75,0x12,0x55,
  14 ,0x03,0x41,0x50,0x94,0x58,0x50,0x06,0x37,0x13,0x20,0x78,0x20,0x02,0x82,
  14 ,0x01,0x70,0x75,0x47,0x29,0x25,0x03,0x18,0x71,0x76,0x99,0x76,0x57,0x23,
  13 ,0x85,0x37,0x73,0x64,0x62,0x51,0x59,0x37,0x78,0x07,0x46,0x60,0x59,
  13 ,0x42,0x68,0x86,0x82,0x31,0x25,0x79,0x69,0x12,0x73,0x43,0x09,0x29,
  13 ,0x21,0x34,0x43,0x41,0x15,0x62,0x89,0x84,0x59,0x32,0x92,0x77,0x02,
  13 ,0x10,0x67,0x21,0x70,0x57,0x81,0x44,0x92,0x30,0x03,0x49,0x03,0x81,
  13 ,0x05,0x33,0x60,0x85,0x28,0x90,0x72,0x46,0x15,0x06,0x37,0x35,0x07,
  13 ,0x02,0x66,0x80,0x42,0x64,0x45,0x36,0x23,0x07,0x53,0x76,0x52,0x93,
  13 ,0x01,0x33,0x40,0x21,0x32,0x22,0x68,0x11,0x53,0x76,0x95,0x49,0x64,
  12 ,0x66,0x70,0x10,0x66,0x11,0x34,0x05,0x76,0x88,0x48,0x65,0x22,
  12 ,0x33,0x35,0x05,0x33,0x05,0x67,0x02,0x88,0x44,0x24,0x43,0x91,
  12 ,0x16,0x67,0x52,0x66,0x52,0x83,0x51,0x44,0x22,0x12,0x23,0x37,
  12 ,0x08,0x33,0x76,0x33,0x26,0x41,0x75,0x72,0x11,0x06,0x11,0x86,
  12 ,0x04,0x16,0x88,0x16,0x63,0x20,0x87,0x86,0x05,0x53,0x05,0x95,
  12 ,0x02,0x08,0x44,0x08,0x31,0x60,0x43,0x93,0x02,0x76,0x52,0x98,
  12 ,0x01,0x04,0x22,0x04,0x15,0x80,0x21,0x96,0x51,0x38,0x26,0x49,
  11 ,0x52,0x11,0x02,0x07,0x90,0x10,0x98,0x25,0x69,0x13,0x24,
  11 ,0x26,0x05,0x51,0x03,0x95,0x05,0x49,0x12,0x84,0x56,0x62,
  11 ,0x13,0x02,0x75,0x51,0x97,0x52,0x74,0x56,0x42,0x28,0x31,
  11 ,0x06,0x51,0x37,0x75,0x98,0x76,0x37,0x28,0x21,0x14,0x16,
  11 ,0x03,0x25,0x68,0x87,0x99,0x38,0x18,0x64,0x10,0x57,0x08,
  11 ,0x01,0x62,0x84,0x43,0x99,0x69,0x09,0x32,0x05,0x28,0x54,
  10 ,0x81,0x42,0x21,0x99,0x84,0x54,0x66,0x02,0x64,0x27,
  10 ,0x40,0x71,0x10,0x99,0x92,0x27,0x33,0x01,0x32,0x13,
  10 ,0x20,0x35,0x55,0x49,0x96,0x13,0x66,0x50,0x66,0x07,
  10 ,0x10,0x17,0x77,0x74,0x98,0x06,0x83,0x25,0x33,0x03,
  10 ,0x05,0x08,0x88,0x87,0x49,0x03,0x41,0x62,0x66,0x52,
  10 ,0x02,0x54,0x44,0x43,0x74,0x51,0x70,0x81,0x33,0x26,
  10 ,0x01,0x27,0x22,0x21,0x87,0x25,0x85,0x40,0x66,0x63,
  9 ,0x63,0x61,0x10,0x93,0x62,0x92,0x70,0x33,0x31,
  9 ,0x31,0x80,0x55,0x46,0x81,0x46,0x35,0x16,0x66,
  9 ,0x15,0x90,0x27,0x73,0x40,0x73,0x17,0x58,0x33,
  9 ,0x07,0x95,0x13,0x86,0x70,0x36,0x58,0x79,0x16,
  9 ,0x03,0x97,0x56,0x93,0x35,0x18,0x29,0x39,0x58,
  9 ,0x01,0x98,0x78,0x46,0x67,0x59,0x14,0x69,0x79,
  8 ,0x99,0x39,0x23,0x33,0x79,0x57,0x34,0x90,
  8 ,0x49,0x69,0x61,0x66,0x89,0x78,0x67,0x45,
  8 ,0x24,0x84,0x80,0x83,0x44,0x89,0x33,0x72,
  8 ,0x12,0x42,0x40,0x41,0x72,0x44,0x66,0x86,
  8 ,0x06,0x21,0x20,0x20,0x86,0x22,0x33,0x43,
  8 ,0x03,0x10,0x60,0x10,0x43,0x11,0x16,0x72,
  8 ,0x01,0x55,0x30,0x05,0x21,0x55,0x58,0x36,
  7 ,0x77,0x65,0x02,0x60,0x77,0x79,0x18,
  7 ,0x38,0x82,0x51,0x30,0x38,0x89,0x59,
  7 ,0x19,0x41,0x25,0x65,0x19,0x44,0x79,
  7 ,0x09,0x70,0x62,0x82,0x59,0x72,0x40,
  7 ,0x04,0x85,0x31,0x41,0x29,0x86,0x20,
  7 ,0x02,0x42,0x65,0x70,0x64,0x93,0x10,
  7 ,0x01,0x21,0x32,0x85,0x32,0x46,0x55,
  6 ,0x60,0x66,0x42,0x66,0x23,0x27,
  6 ,0x30,0x33,0x21,0x33,0x11,0x64,
  6 ,0x15,0x16,0x60,0x66,0x55,0x82,
  6 ,0x07,0x58,0x30,0x33,0x27,0x91,
  6 ,0x03,0x79,0x15,0x16,0x63,0x95,
  6 ,0x01,0x89,0x57,0x58,0x31,0x98,
  5 ,0x94,0x78,0x79,0x15,0x99,
  5 ,0x47,0x39,0x39,0x57,0x99,
  5 ,0x23,0x69,0x69,0x79,0x00,
  5 ,0x11,0x84,0x84,0x89,0x50,
  5 ,0x05,0x92,0x42,0x44,0x75,
  5 ,0x02,0x96,0x21,0x22,0x37,
  5 ,0x01,0x48,0x10,0x61,0x19,
  4 ,0x74,0x05,0x30,0x59,
  4 ,0x37,0x02,0x65,0x30,
  4 ,0x18,0x51,0x32,0x65,
  4 ,0x09,0x25,0x66,0x32,
  4 ,0x04,0x62,0x83,0x16,
  4 ,0x02,0x31,0x41,0x58,
  4 ,0x01,0x15,0x70,0x79,
  3 ,0x57,0x85,0x40,
  3 ,0x28,0x92,0x70,
  3 ,0x14,0x46,0x35,
  3 ,0x07,0x23,0x17,
  3 ,0x03,0x61,0x59,
  3 ,0x01,0x80,0x79,
  2 ,0x90,0x40,
  2 ,0x45,0x20,
  2 ,0x22,0x60,
  2 ,0x11,0x30,
  2 ,0x05,0x65,
  2 ,0x02,0x82,
  2 ,0x01,0x41,
  1 ,0x71,
  1 ,0x35,
  1 ,0x18,
  1 ,0x09,
  1 ,0x04,
  1 ,0x02,
  1 ,0x01,
  0};

  int i,i_end;
  int table_ptr=0,log_ptr=0;
  do
  {
    logs[log_ptr+BCD_SIGN]=0;
    logs[log_ptr+BCD_DEC]=2;
    logs[log_ptr+BCD_LEN]=34;
    log_ptr+=3;
    i_end=table[table_ptr];
    table_ptr++;
    for (i=0;i<(17-i_end);i++)
    {
      logs[log_ptr++]=0;
      logs[log_ptr++]=0;
    }
    for (i=0;i<i_end;i++)
    {
      logs[log_ptr]=table[table_ptr]>>4;
      logs[log_ptr+1]=table[table_ptr]&0xF;
      table_ptr++;
      log_ptr+=2;
    }
  } while(table[table_ptr]);
}

static void AddBCD(unsigned char *result, const unsigned char *n1, const unsigned char *n2)
{
  #pragma MM_VAR result
  #pragma MM_VAR n1
  #pragma MM_VAR n2

  unsigned char carry;
  const unsigned char *temp;
  unsigned char sign;
  int BCD_ptr, BCD_end;
  int n1_whole=0, n2_whole=0;
  int n1_dec=0, n2_dec=0;
  unsigned char carry_number=0;
  bool subtracting=false;
  int t1,t2,d1,d2;

  t1=n1[BCD_SIGN];
  t2=n2[BCD_SIGN];


  if ((t1==0)&&(t2==0)) sign=0;
  else if ((t1==1)&&(t2==1)) sign=1;
  else sign=2;

  if ((t1==1)&&(t2==0))
  {
    temp=n1;
    n1=n2;
    n2=temp;
  }

  if ((n1[BCD_SIGN]==0)&&(n2[BCD_SIGN]==1))
  {
    buffer[BCD_DEC]=n2[BCD_DEC];
    buffer[BCD_LEN]=n2[BCD_LEN];
    carry=1;
    for (BCD_ptr=n2[BCD_LEN]+2;BCD_ptr>=3;BCD_ptr--)
    {
      t1=9-n2[BCD_ptr]+carry;
      if (t1==10) buffer[BCD_ptr]=0;
      else
      {
        carry=0;
        buffer[BCD_ptr]=t1;
      }
    }
    carry_number=9;
    if (carry==1)
    {
      //buffer[3]=0;
      carry_number=0;
    }
    n2=buffer;
    subtracting=true;
  }

  t1=n1[BCD_DEC];
  t2=n2[BCD_DEC];
  if (t1>t2)
  {
    n2_whole=t1-t2;
    result[BCD_DEC]=t1;
  }
  else
  {
    n1_whole=t2-t1;
    result[BCD_DEC]=t2;
  }

  d1=n1[BCD_LEN];
  d2=n2[BCD_LEN];

  if ((d1-t1)>(d2-t2))
  {
    n2_dec=(d1-t1)-(d2-t2);
    result[BCD_LEN]=result[BCD_DEC]+d1-t1;
  }
  else
  {
    n1_dec=(d2-t2)-(d1-t1);
    result[BCD_LEN]=result[BCD_DEC]+d2-t2;
  }

  carry=0;
  BCD_end=result[BCD_LEN]+2;
  for (BCD_ptr=BCD_end;BCD_ptr>=3;BCD_ptr--)
  {
    t1=carry;
    if ((BCD_ptr<=BCD_end-n1_dec)&&(BCD_ptr>n1_whole+2)) t1+=n1[BCD_ptr-n1_whole];
    if ((BCD_ptr<=BCD_end-n2_dec)&&(BCD_ptr>n2_whole+2)) t1+=n2[BCD_ptr-n2_whole];
    if (BCD_ptr<=n2_whole+2) t1+=carry_number;

    if (t1>9)
    {
      t1-=10;
      carry=1;
    }
    else carry=0;
    result[BCD_ptr]=t1;
  }

  if ((carry==1)&&(subtracting==false))
  {
    PadBCD(result,1);
    result[3]=1;
  }

  if ((carry==0)&&(carry_number==9)&&(sign==2))
  {
    carry=1;
    for (BCD_ptr=result[BCD_LEN]+2;BCD_ptr>=3;BCD_ptr--)
    {
      t1=9-result[BCD_ptr]+carry;
      if (t1==10) t1=0;
      else carry=0;
      result[BCD_ptr]=t1;
    }
    sign=1;
  }
  else if (sign==2) sign=0;
  result[BCD_SIGN]=sign;
}

static void SubBCD(unsigned char *result, const unsigned char *n1, unsigned char *n2)
{
  #pragma MM_VAR result
  #pragma MM_VAR n1
  #pragma MM_VAR n2
  n2[BCD_SIGN]=!n2[BCD_SIGN];
  AddBCD(result,n1,n2);
  n2[BCD_SIGN]=!n2[BCD_SIGN];
}

static void ImmedBCD(const char *text, unsigned char *BCD)
{
  int text_ptr=0;
  do
  {
    perm_buff2[text_ptr]=(unsigned char)text[text_ptr];
  } while(text[text_ptr++]);
  BufferBCD(perm_buff2,BCD);
}

static void BufferBCD(const unsigned char *text, unsigned char *BCD)
{
  #pragma MM_VAR text
  #pragma MM_VAR BCD

  unsigned char *RAM_ptr;
  int BCD_ptr=3,text_ptr=0;
  char found=0;

  if (text[0]=='-')
  {
    text++;
    BCD[BCD_SIGN]=1;
  }
  else BCD[BCD_SIGN]=0;

  BCD[BCD_LEN]=0;
  BCD[BCD_DEC]=0;

  while (text[text_ptr])
  {
    if (text[text_ptr]=='.')
    {
      BCD[BCD_DEC]=text_ptr;
      found=1;
    }
    else
    {
      BCD[BCD_ptr]=text[text_ptr]-'0';
      BCD_ptr++;
    }
    text_ptr++;
  }
  BCD[BCD_LEN]=text_ptr-found;
  if (found==0) BCD[BCD_DEC]=BCD[BCD_LEN];
}

static void MultBCD(unsigned char *result, const unsigned char *n1, const unsigned char *n2)
{
  #pragma MM_VAR result
  #pragma MM_VAR n1
  #pragma MM_VAR n2
  #pragma MM_VAR temp

  #pragma MM_DECLARE
    unsigned char temp[5];
  #pragma MM_END

  unsigned char i,j,k,flip=0;
  unsigned char i_end, j_end, k_end;
  //maybe b1,b2 is faster
  unsigned char b0,b1;
  CopyBCD(result,perm_zero);
  CopyBCD(perm_buff1,perm_zero);

  temp[BCD_SIGN]=0;
  temp[BCD_LEN]=2;

  i_end=n1[BCD_LEN];
  j_end=n2[BCD_LEN];
  for (i=0;i<i_end;i++)
  {
    for (j=0;j<j_end;j++)
    {
      b0=0;
      b1=0;
      k_end=n1[i+3];
      for (k=0;k<k_end;k++) b0+=n2[j+3];
      while (b0>9)
      {
        b1+=1;
        b0-=10;
      }
      temp[3]=b1;
      temp[4]=b0;

      temp[BCD_DEC]=2+i_end-i+j_end-j-2;
      if (flip==0) AddBCD(result,temp,perm_buff1);
      else AddBCD(perm_buff1,temp,result);
      flip=!flip;
    }
  }

  if (flip==0)
  {
    //Just CopyBCD?
    i=perm_buff1[BCD_LEN];
    for (j=0;j<i+3;j++) result[j]=perm_buff1[j];
  }
  i=(i_end-n1[BCD_DEC])+(j_end-n2[BCD_DEC]);

  if (i>Settings.DecPlaces)
  {
    result[BCD_LEN]-=(i-Settings.DecPlaces-1);
    result[BCD_DEC]=result[BCD_LEN];

    if (result[result[BCD_LEN]+2]>4)
    {
      ImmedBCD("10",temp);
      AddBCD(perm_buff1,result,temp);
      CopyBCD(result,perm_buff1);
    }
    result[BCD_LEN]-=1;
    i=Settings.DecPlaces+1;
  }
  result[BCD_DEC]-=i;
  result[BCD_SIGN]=n1[BCD_SIGN]^n2[BCD_SIGN];

  FullShrinkBCD(result);
}

static void DivBCD(unsigned char *result, const unsigned char *n1, const unsigned char *n2)
{
  #pragma MM_VAR result
  #pragma MM_VAR n1
  #pragma MM_VAR n2

  int i,j;
  int i_end, j_end;
  int result_ptr,n1_ptr;
  int max_offset,dec_offset=0;
  int post_offset=0, pre_offset=0;
  unsigned char remainder=0, res_ptr_off=0;
  bool logic;

  max_offset=n1[BCD_LEN]-n1[BCD_DEC];
  if ((n2[BCD_LEN]-n2[BCD_DEC])>max_offset) max_offset=n2[BCD_LEN]-n2[BCD_DEC];
  if (Settings.DecPlaces>max_offset) max_offset=Settings.DecPlaces;

  result[BCD_LEN]=0;

  result_ptr=3;

  post_offset=n1[BCD_LEN]-n2[BCD_LEN];

  if ((n1[BCD_DEC]-n2[BCD_DEC]+1)<post_offset)
  {
    pre_offset=-1*(n1[BCD_DEC]-n2[BCD_DEC]+1);
    post_offset=0;
    if (pre_offset<0)
    {
      result[BCD_DEC]=-pre_offset;
    }
    else
    {
      result[BCD_DEC]=0;
      result_ptr+=pre_offset;
      res_ptr_off+=pre_offset;
      for (i=3;i<(pre_offset+3);i++) result[i]=0;
    }
  }
  else if (post_offset>0)
  {
    post_offset=0;
    result[BCD_DEC]=n1[BCD_DEC]-n2[BCD_DEC]+1;
  }
  else
  {
    post_offset*=-1;
    result[BCD_DEC]=post_offset+n1[BCD_DEC]-n2[BCD_DEC]+1;
  }

  perm_buff2[BCD_SIGN]=1;
  perm_buff2[BCD_LEN]=n2[BCD_LEN];
  perm_buff2[BCD_DEC]=n2[BCD_LEN];

  perm_buff1[BCD_SIGN]=0;
  perm_buff1[BCD_LEN]=n2[BCD_LEN]+1;
  perm_buff1[BCD_DEC]=perm_buff1[BCD_LEN];
  perm_buff1[3]=0;

  i_end=n2[BCD_LEN]+3;
  for (i=3;i<i_end;i++)
  {
    //<=? was <
    if ((i-3)<post_offset) perm_buff1[i+1]=0;
    else if ((i-post_offset)>(n1[BCD_LEN]+2)) perm_buff1[i+1]=0;
    else perm_buff1[i+1]=n1[i-post_offset];
    perm_buff2[i]=n2[i];
  }

  n1_ptr=n2[BCD_LEN]+3+post_offset;
  do
  {
    result[result_ptr]=0;
    result[BCD_LEN]+=1;

    do
    {
      AddBCD(perm_buff3,perm_buff1,perm_buff2);

      if ((perm_buff3[BCD_SIGN]==0)||(IsZero(perm_buff3)))
      {
        result[result_ptr]+=1;
        if (result[result_ptr]==10)
        {
          result[result_ptr]=0;
          for (i=result_ptr-1;i>=3;i--)
          {
            result[i]+=1;
            if (result[i]<10) break;
            else result[i]=0;
          }
          if (i==2)
          {
            result[3]=1;
            for (i=4;i<result_ptr;i++) result[i]=0;
            result_ptr++;
            result[BCD_LEN]+=1;
            result[BCD_DEC]+=1;
            result[result_ptr]=0;
          }
        }
        i_end=perm_buff1[BCD_LEN]+3;
        for (i=3;i<i_end;i++) perm_buff1[i]=perm_buff3[i];
      }
    } while ((perm_buff3[BCD_SIGN]==0)&&(!IsZero(perm_buff3)));

    i_end=n2[BCD_LEN]+3;
    for (i=3;i<i_end;i++) perm_buff1[i]=perm_buff1[i+1];

    if ((n1_ptr-3)>=n1[BCD_LEN])
    {
      perm_buff1[i]=0;
    }
    else
    {
      perm_buff1[i]=n1[n1_ptr];
      n1_ptr++;
    }
    result_ptr++;

    //< or <=?
    if ((result_ptr-3)<(result[BCD_DEC]))
    {
      logic=true;
    }
    else if (((result[BCD_LEN]-result[BCD_DEC])<(max_offset+1)))
    {
      logic=true;
    }
    else logic=false;
  } while (logic);

  if ((result[BCD_LEN]-result[BCD_DEC])>max_offset)
  {
    if (result[result[BCD_LEN]+2]>4)
    {
      i_end=result[BCD_LEN]+2;
      for (i=3;i<i_end;i++) perm_buff3[i]=result[i];
      i=result[BCD_LEN];
      j=result[BCD_DEC];
      perm_buff3[BCD_SIGN]=0;
      perm_buff3[BCD_LEN]=result[BCD_LEN]-1;
      perm_buff3[BCD_DEC]=perm_buff3[BCD_LEN];
      perm_buff1[BCD_SIGN]=0;
      perm_buff1[BCD_LEN]=1;
      perm_buff1[BCD_DEC]=1;
      perm_buff1[3]=1;
      AddBCD(result,perm_buff3,perm_buff1);
      result[BCD_DEC]=j;
      if (result[BCD_LEN]==i) result[BCD_DEC]+=1;
    }
    else
    {
      result[BCD_LEN]-=1;
    }
  }
  result[BCD_SIGN]=n1[BCD_SIGN]^n2[BCD_SIGN];
  FullShrinkBCD(result);
}

static void ShrinkBCD(unsigned char *dest,unsigned char *src)
{
  #pragma MM_VAR dest
  #pragma MM_VAR src

  int BCD_ptr,off_ptr=0;
  int ptr_end;
  if ((src[3]==0)&&(src[BCD_DEC]!=0)&&(src[BCD_LEN]>1)) off_ptr=1;
  ptr_end=src[BCD_LEN]+3-off_ptr;
  for (BCD_ptr=3;BCD_ptr<ptr_end;BCD_ptr++) dest[BCD_ptr]=src[BCD_ptr+off_ptr];
  dest[BCD_SIGN]=src[BCD_SIGN];
  dest[BCD_LEN]=src[BCD_LEN];
  dest[BCD_DEC]=src[BCD_DEC];
  if (off_ptr==1)
  {
    dest[BCD_LEN]-=1;
    dest[BCD_DEC]-=1;
  }
}

static void FullShrinkBCD(unsigned char *n1)
{
  #pragma MM_VAR n1
  while ((n1[3]==0)&&(n1[BCD_DEC]>1)) ShrinkBCD(n1,n1);
}

static void PadBCD(unsigned char *n1, int amount)
{
  #pragma MM_VAR n1
  int i;
  for (i=n1[BCD_LEN]+3;i>=3;i--) n1[i+amount]=n1[i];
  for (i=3;i<(amount+3);i++) n1[i]=0;
  n1[BCD_LEN]+=amount;
  n1[BCD_DEC]+=amount;
}

static bool IsZero(unsigned char *n1)
{
  #pragma MM_VAR n1
  int i,i_end;
  i_end=n1[BCD_LEN]+3;
  for (i=3;i<i_end;i++) if (n1[i]!=0) return false;
  return true;
}

//see if using this in other places makes things smaller
static void CopyBCD(unsigned char *dest, unsigned char *src)
{
  #pragma MM_VAR dest
  #pragma MM_VAR src
  int i,i_end;
  i_end=src[BCD_LEN]+3;
  for (i=0;i<i_end;i++) dest[i]=src[i];
}

static bool LnBCD(unsigned char *result, unsigned char *arg)
{
  #pragma MM_VAR result
  #pragma MM_VAR arg
  #pragma MM_VAR temp

  #pragma MM_DECLARE
    unsigned char temp[4];
  #pragma MM_END

  bool flip_sign=false;
  unsigned int i,j=1,k=0;

  ImmedBCD("1",temp);

  SubBCD(p1,arg,temp);
  if (IsZero(p1))
  {
    CopyBCD(result,perm_zero);
    return true;
  }
  else if (p1[BCD_SIGN]==1)
  {
    DivBCD(p1,temp,arg);
    flip_sign=true;
  }
  else CopyBCD(p1,arg);

  for (i=0;i<8;i++)
  {
    RorBCD(p0,p1,j);
    CopyBCD(p1,p0);
    SubBCD(p0,p1,temp);
    if (p0[BCD_SIGN]==1) break;
    j=1<<(k++);
  }

  if (i==8) return false;
  if (IsZero(p1)) return false;

  j=1<<i;
  k=7-k;
  CopyBCD(result,logs+k*MATH_ENTRY_SIZE);

  for (i=k;i<Settings.LogTableSize;i++)
  {
    if (j!=0)
    {
      RolBCD(p0,p1,j);
      SubBCD(p2,p0,temp);
      j>>=1;
    }
    else
    {
      RorBCD(p2,p1,i-7);
      AddBCD(p0,p1,p2);
      SubBCD(p2,p0,temp);
    }
    if (p2[BCD_SIGN]==1)
    {
      CopyBCD(p1,p0);
      SubBCD(p2,result,logs+i*MATH_ENTRY_SIZE);
      CopyBCD(result,p2);
    }
  }
  SubBCD(p2,temp,p1);
  SubBCD(p0,result,p2);
  CopyBCD(result,p0);
  if (flip_sign) result[BCD_SIGN]=1;
  return true;
}


//take out multiplies for speed
static void ExpBCD(unsigned char *result, unsigned char *arg)
{
  #pragma MM_VAR result
  #pragma MM_VAR arg
  #pragma MM_VAR temp

  #pragma MM_DECLARE
    unsigned char temp[4];
  #pragma MM_END

  int i,j=128;
  unsigned int log_ptr=0;
  bool invert=false;
  if (arg[BCD_SIGN]==1)
  {
    invert=true;
    arg[BCD_SIGN]=0;
  }

  if (CompVarBCD(perm_zero,arg)==COMP_EQ)
  {
    ImmedBCD("1",result);
    return;
  }

  ImmedBCD("1",temp);
  CopyBCD(p0,arg);
  CopyBCD(result,temp);
  for (i=0;i<Settings.LogTableSize;i++)
  {
    SubBCD(p1,p0,logs+log_ptr);
    if (p1[BCD_SIGN]==0)
    {
      CopyBCD(p0,p1);
      if (i<8)
      {
        RolBCD(p1,result,j);
      }
      else
      {
        RorBCD(p2,result,i-7);
        AddBCD(p1,result,p2);
      }
      CopyBCD(result,p1);
    }
    j>>=1;
    log_ptr+=MATH_ENTRY_SIZE;
  }
  AddBCD(p1,p0,temp);
  MultBCD(p2,p1,result);
  CopyBCD(p2,result);

  if (invert) DivBCD(result,temp,p2);
  else CopyBCD(result,p2);
}

static void RolBCD(unsigned char *result, unsigned char *arg, unsigned char amount)
{
  #pragma MM_VAR result
  #pragma MM_VAR arg

  int i,i_end,j;
  unsigned char b0,b1;

  CopyBCD(result,arg);
  for (j=0;j<amount;j++)
  {
    b1=0;
    i_end=result[BCD_LEN]+2;
    for (i=i_end;i>=3;i--)
    {
      b0=(result[i]<<1)+b1;
      if (b0>9) b0+=6;
      b1=b0>>4;
      result[i]=(b0&0xF);
    }
    if (b1)
    {
      PadBCD(result,1);
      result[3]=1;
    }
  }
}

//Does shifting one bit add an extra 0?
static void RorBCD(unsigned char *result, unsigned char *arg, unsigned char amount)
{
  #pragma MM_VAR result
  #pragma MM_VAR arg

  int i,i_end,j;
  unsigned char b0,b1,b2;
  unsigned char t1;

  static const unsigned char table[]={
  0,0,0,0, // 0/16
  0,6,2,5, // 1/16
  1,2,5,0, // 2/16
  1,8,7,5, // 3/16
  2,5,0,0, // 4/16
  3,1,2,5, // 5/16
  3,7,5,0, // 6/16
  4,3,7,5, // 7/16
  5,0,0,0, // 8/16
  5,6,2,5};// 9/16

  unsigned char accum[6];

  CopyBCD(result,arg);

  while (amount)
  {
    if (amount>3)
    {
      for (i=0;i<6;i++) accum[i]=0;
      amount-=4;

      t1=result[BCD_DEC];
      b0=0;
      i_end=result[BCD_LEN]+3;
      for (i=3;i<i_end;i++)
      {
        b1=0;
        b2=result[i]*4;
        for (j=3;j>=-1;j--)
        {
          if (j>=0) accum[b0+j]+=table[b2+j]+b1;
          else accum[b0+j]+=b1;

          if (accum[b0+j]>9)
          {
            accum[b0+j]-=10;
            b1=1;
          }
          else b1=0;
        }

        if (b0!=2) b0++;
        else
        {
          if (t1) result[i-2]=accum[0];
          else result[i-1]=accum[0];

          for (j=0;j<5;j++) accum[j]=accum[j+1];
          accum[5]=0;
        }
      }

      b1=result[BCD_LEN]+3;
      if (t1)
      {
        t1--;
        result[BCD_LEN]=b1;
        result[BCD_DEC]=t1;
        b2=i-b0;
      }
      else
      {
        b1++;
        result[3]=0;
        result[BCD_LEN]=b1;
        b2=i-b0+1;
      }
      for (j=0;j<5;j++) result[j+b2]=accum[j];

      if ((b1-t1)>Settings.DecPlaces) result[BCD_LEN]=t1+Settings.DecPlaces;
    }
    else
    {
      amount--;
      i_end=result[BCD_LEN]+3;
      b1=0;
      for (i=3;i<i_end;i++)
      {
        b0=result[i];
        b2=b0;
        b0=(b0>>1)+b1;
        b1=0;
        if (b2&1) b1=8;
        if (b0>7) b0-=3;
        result[i]=b0;
      }
      if (b1)
      {
        if((result[BCD_LEN]-result[BCD_DEC])<Settings.DecPlaces)
        {
          b1=result[BCD_LEN]+1;
          result[BCD_LEN]=b1;
          result[b1+2]=5;
        }
      }
    }
  }
}

static void PowBCD(unsigned char *result, unsigned char *base, unsigned char *exp)
{
  LnBCD(p3,base);
  MultBCD(p4,p3,exp);
  ExpBCD(result,p4);
}

static void TanBCD(unsigned char *sine_result,unsigned char *cos_result,unsigned char *arg)
{
  #pragma MM_VAR sine_result
  #pragma MM_VAR cos_result

  CopyBCD(p2,perm_zero);
  CopyBCD(sine_result,perm_zero);
  CopyBCD(cos_result,perm_K);

  CalcTanBCD(sine_result,cos_result,p2,arg,0);
  sine_result[BCD_LEN]=Settings.DecPlaces;
  cos_result[BCD_LEN]=Settings.DecPlaces;
}

static void AcosBCD(unsigned char *result,unsigned char *arg)
{
  CopyBCD(p0,arg);
  MultBCD(p1,p0,arg);
  ImmedBCD("1",p0);
  SubBCD(p5,p0,p1);
  ImmedBCD("0.5",p6);
  PowBCD(p7,p5,p6);
  DivBCD(p6,p7,arg);
  AtanBCD(result,p6);
}

static void AsinBCD(unsigned char *result,unsigned char *arg)
{
  CopyBCD(p0,arg);
  MultBCD(p1,p0,arg);
  ImmedBCD("1",p0);
  SubBCD(p5,p0,p1);
  ImmedBCD("0.5",p6);
  PowBCD(p7,p5,p6);
  DivBCD(p6,arg,p7);
  AtanBCD(result,p6);
}

static void AtanBCD(unsigned char *result,unsigned char *arg)
{
  #pragma MM_VAR result

  CopyBCD(result,perm_zero);
  ImmedBCD("1",p2);
  CopyBCD(p3,arg);
  CalcTanBCD(p2,p3,result,arg,1);



  if ((result[BCD_DEC]<=Settings.DecPlaces)&&(result[BCD_LEN]>Settings.DecPlaces)) result[BCD_LEN]=Settings.DecPlaces;
}

static void CalcTanBCD(unsigned char *result1,unsigned char *result2,unsigned char *result3,unsigned char *arg,unsigned char flag)
{
  #pragma MM_VAR result2

  unsigned int i;
  unsigned int trig_ptr=0;

  //function pointers could reduce flash size
  for (i=0;i<Settings.TrigTableSize;i++)
  {
    if (flag==0) SubBCD(p1,arg,result3);

    if (((flag==0)&&(p1[BCD_SIGN]==0))||((flag==1)&&(result2[BCD_SIGN]==0)))
    {
      RorBCD(p0,result2,i);
      AddBCD(p1,result1,p0);
      RorBCD(p0,result1,i);
      CopyBCD(result1,p1);
      SubBCD(p1,result2,p0);
      CopyBCD(result2,p1);
      AddBCD(p0,result3,trig+trig_ptr);
    }
    else
    {
      RorBCD(p0,result2,i);
      SubBCD(p1,result1,p0);
      RorBCD(p0,result1,i);
      CopyBCD(result1,p1);
      AddBCD(p1,result2,p0);
      CopyBCD(result2,p1);
      SubBCD(p0,result3,trig+trig_ptr);
    }
    CopyBCD(result3,p0);
    trig_ptr+=MATH_ENTRY_SIZE;
  }
}

static unsigned char CompBCD(const char *num, unsigned char *var)
{
  ImmedBCD(num,p0);
  return CompVarBCD(p0,var);
}

static unsigned char CompVarBCD(unsigned char *var1, unsigned char *var2)
{
  SubBCD(p1,var1,var2);
  if (IsZero(p1)) return COMP_EQ;
  else if (p1[BCD_SIGN]==0) return COMP_GT;
  else return COMP_LT;
}

//convert angle to 0-90 format and put in p3
static unsigned char TrigPrep(unsigned int stack_ptr_copy,unsigned char *cosine)
{
  int sine;

  if (Settings.DegRad) CopyBCD(p3,BCD_stack+(stack_ptr_copy-1)*MATH_CELL_SIZE);
  else
  {
    ImmedBCD(deg_factor,p0);
    MultBCD(p3,BCD_stack+(stack_ptr_copy-1)*MATH_CELL_SIZE,p0);
  }

  ImmedBCD("360",p2);
  while(CompVarBCD(p3,p2)==COMP_GT) CopyBCD(p3,p1);

  if (CompBCD("180",p3)==COMP_LT)
  {
    SubBCD(stack_buffer,p2,p3);
    sine=1;
  }
  else
  {
    CopyBCD(stack_buffer,p3);
    sine=0;
  }
  if (CompBCD("90",stack_buffer)==COMP_LT)
  {
    ImmedBCD("180",p2);
    SubBCD(p3,p2,stack_buffer);
    *cosine=1;
  }
  else
  {
    CopyBCD(p3,stack_buffer);
    *cosine=0;
  }
  return sine;
}
