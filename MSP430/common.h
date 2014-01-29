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

#define BCD_SIGN 0
#define BCD_LEN  1
#define BCD_DEC  2

#define STACK_SIZE 10

#define MATH_CELL_SIZE 260
#define MATH_ENTRY_SIZE 37
#define MATH_LOG_TABLE 114
#define MATH_TRIG_TABLE 113

#define K "0.60725293500888125616944675250493"
#define log10_factor  "2.30258509299404568401799145468437"
#define pi            "3.1415926535897932384626433832795"
#define pi2           "1.57079632679489661923132169163975"
#define rad_factor    "0.01745329251994329576923690768489"
#define deg_factor    "57.29577951308232087679815481410522"

#define COMP_GT 0
#define COMP_LT 1
#define COMP_EQ 2

enum SlaveCommands {SlaveRAM_Read,SlaveRAM_Write,SlaveSync,SlaveAnswer,SlaveMakeTables,
                    SlaveAdd,SlaveSubtract,SlaveBuffer,SlaveMultiply,SlaveDivide,SlaveShrink,
                    SlaveFullShrink,SlavePad,SlaveIsZero,SlaveCopy,SlaveLn,SlaveExp,SlaveRol,
                    SlaveRor,SlavePow,SlaveTan,SlaveAcos,SlaveAsin,SlaveAtan,SlaveCalcTan,
                    SlaveCompVar,SlaveTrigPrep,SlaveSettings,SlaveSetDecPlaces};

struct SettingsType
{
  bool ColorStack;
  int DecPlaces;
  bool DegRad;
  int LogTableSize;
  int TrigTableSize;
  bool SciNot;
};

static void delay_ms(int ms)
{
  volatile int i;
  for (i=0;i<ms;i++) __delay_cycles(16000);
}

static void UART_Ready()
{
  while (!(UC0IFG&UCA0TXIFG));
  UCA0TXBUF=0;
}

static unsigned char UART_Receive(bool wait)
{
  while (!(UC0IFG&UCA0RXIFG));
  if (!wait) UART_Ready();
  return UCA0RXBUF;
}

static void UART_Send(unsigned char data, bool wait)
{
  while (!(UC0IFG&UCA0TXIFG));
  UCA0TXBUF=data;
  if (wait) UART_Receive(true);
}

static unsigned int UART_ReceiveWord(bool wait)
{
  unsigned int data;
  data=UART_Receive(false)<<8;
  data+=UART_Receive(wait);
  return data;
}

static void UART_SendWord(unsigned int data, bool wait)
{
  UART_Send(data>>8,true);
  UART_Send(data&0xFF,wait);
}
