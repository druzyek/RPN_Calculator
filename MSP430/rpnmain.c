/**   RPN Scientific Calculator for MSP430
 *    Copyright (C) 2013 Joey Shepard
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

#define RAM_BANK        BIT0  //P1.0 SRAM bank
#define UART_RXD        BIT1  //P1.1 To TXD of slave
#define UART_TXD        BIT2  //P1.2 To RXD of slave
#define SR_CLOCK        BIT3  //P1.3 165 clock
#define SR_LATCH        BIT4  //P1.4 165 latch
#define SR_DATA         BIT5  //P1.5 165 data
#define BUFFER_EN       BIT6  //P1.6 Enable LCD bus buffer
#define LCD_EN          BIT7  //P1.7 LCD clock "Enable"

#define LCD_BUS               //P2.0 - P2.3
#define LCD_DATA        BIT4  //P2.4 LCD data bit
#define KEY_BUS               //P2.0 - P2.5
#define LED_2ND         BIT7  //P2.7 LED for 2nd button

#define LCD_CYCLES 1000

#define KEY_ENTER     13
#define KEY_BACKSPACE 8
#define KEY_DELETE    83
#define KEY_ESCAPE    27
#define KEY_LEFT      75
#define KEY_RIGHT     77
#define KEY_DOWN      80
#define KEY_UP        72

#define SCREEN_WIDTH 20

static const char KeyMatrix[]={0 ,'z','0','.','m','+','p','r',
                                  'd','1','2','3','-','x','q',
                                  'w','4','5','6','*','n','u',
                                   13,'7','8','9','/', 83, 8 ,
                                  'b','s','c','t', 75, 80, 77,
                                  ' ','e','l', 0 ,'o', 72, 27};

#pragma MM_READ RAM_Read
#pragma MM_WRITE RAM_Write
#pragma MM_ON

static void Init();

static void LCD_Nibble(unsigned char nibble, unsigned char RS);
static void LCD_Byte(unsigned char byte, unsigned char RS);
static void LCD_Init();
static void LCD_Text(const char *msg);
static void LCD_Num(unsigned int num);
static void LCD_Hex(unsigned int num);

static void SetBlink(bool status);
static void gotoxy(short x, short y);
static unsigned char GetKey();

static unsigned char RAM_Read(const unsigned char *a1);
static void RAM_Write(const unsigned char *a1, const unsigned char byte);

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
static void RolBCD(unsigned char *result, unsigned char *arg, unsigned int amount);
static void RorBCD(unsigned char *result, unsigned char *arg, unsigned int amount);
static void PowBCD(unsigned char *result, unsigned char *base, unsigned char *exp);
static void TanBCD(unsigned char *sine_result,unsigned char *cos_result,unsigned char *arg);
static void AcosBCD(unsigned char *result,unsigned char *arg);
static void AsinBCD(unsigned char *result,unsigned char *arg);
static void AtanBCD(unsigned char *result,unsigned char *arg);
static void CalcTanBCD(unsigned char *result1,unsigned char *result2,unsigned char *result3,unsigned char *arg,unsigned char flag);
static unsigned char CompBCD(const char *num, unsigned char *var);
static unsigned char CompVarBCD(unsigned char *var1, unsigned char *var2);
static unsigned char TrigPrep(unsigned int stack_ptr_copy,int *cosine);

static void DrawStack(bool menu, bool input, int stack_pointer);
static void DrawInput(unsigned char *line, int input_ptr, int offset, bool menu);
static void ErrorMsg(const char *msg);
static void Number2(int num);
static void SetDecPlaces();

static void TestRAM();

#define ClrLCD() LCD_Byte(1,0)
#define putchar(x) LCD_Byte(x,1)
#define DrawBusy() gotoxy(19,0);LCD_Byte(7,1);

#pragma MM_OFFSET 2000
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
  unsigned char BCD_stack[2600];
  unsigned char stack_buffer[260];
#pragma MM_END

struct SettingsType Settings;
unsigned int stack_ptr;

int main(void)
{
  WDTCTL=WDTPW + WDTHOLD;

  BCSCTL1=CALBC1_16MHZ;
  DCOCTL=CALDCO_16MHZ;

  //delay_ms(1);

  BCSCTL3 |= LFXT1S_2;

  UCA0CTL1=UCSWRST|UCSSEL_2;
  UCA0CTL0 = 0;
  UCA0MCTL = UCBRS_5;
  UCA0BR0 = 131;
  UCA0BR1 = 0;
  UCA0CTL1&=~UCSWRST;
  //UC0IE |= UCA0RXIE | UCA0TXIE;

  P1SEL=(UART_TXD|UART_RXD);
  P1SEL2=(UART_TXD|UART_RXD);

  P2SEL=0;
  P2SEL2=0;

  P1OUT=BUFFER_EN;
  P2OUT=0;

  P1DIR=0xFF & ~(UART_TXD|UART_RXD|SR_DATA);
  P2DIR=0xFF;

  TACCR0 = 1200;
  TACCTL0 = CCIE;
  TACTL = MC_1|ID_0|TASSEL_1|TACLR;
  //__enable_interrupt();

  //LCD_Init();
  //delay_ms(100);

  #pragma MM_ASSIGN_GLOBALS

  /*gotoxy(0,1);
  LCD_Text("Testing");

  for (;;)
  {
    //LCD_Byte(0x80+0x0,0);
    key=GetKey();
    gotoxy(0,3);
    LCD_Num(key);
    delay_ms(500);
  }*/



  /*gotoxy(0,0);
  for (;;)
  {
    LCD_Hex(UART_Receive());
    UART_Ready();
    putchar(' ');
  }*/

  /*for (;;)
  {
    UART_RecFast();
    gotoxy(0,0);
    LCD_Text("A: ");
    LCD_Hex(UART_Receive());
    UART_Ready();
    LCD_Hex(UART_Receive());
    UART_Ready();
    putchar('=');
    LCD_Hex(UART_Receive());
    UART_Ready();
    gotoxy(0,1);
    LCD_Text("V=");
    LCD_Hex(UART_Receive());
    UART_Ready();
    gotoxy(0,2);
    LCD_Text("Read 2:");
    LCD_Hex(UART_Receive());
    UART_Ready();
    gotoxy(0,3);
    LCD_Text("Count=");
    LCD_Hex(UART_Receive());
    UART_Ready();
  }*/

  /*
  #pragma MM_DECLARE
  unsigned char n1[260];
  unsigned char n2[260];
  unsigned char n3[260];
  unsigned char n4[260];
  #pragma MM_END
  */

  /*unsigned char rv=0;
  unsigned int rv2,rv3;
  for (rv=5;;rv++)
  {
    rv3=(rv<<8)|(rv+1);
    UART_SendWord(rv3,true);
    rv2=UART_ReceiveWord(false);
    gotoxy(0,0);
    LCD_Hex(rv3>>8);
    LCD_Hex(rv3&0xFF);
    putchar(' ');
    LCD_Hex(rv2>>8);
    LCD_Hex(rv2&0xFF);

    if (rv3!=rv2)
    {
      gotoxy(0,1);
      LCD_Text("Mismatch");
      for(;;);
    }
    delay_ms(500);
  }

  unsigned char c0=8,c1=0;
  LCD_Text("Testing...");

  for (;;)
  {
    if (c0++==10)
    {
      gotoxy(15,0);
      LCD_Hex(c1++);
      c0=0;
    }
    ImmedBCD("123456",n1);
    ImmedBCD("876544",n2);
    AddBCD(n3,n1,n2);

    gotoxy(0,0);
    PrintBCD(n1,-1);
    putchar('+');
    gotoxy(0,1);
    PrintBCD(n2,-1);
    putchar('=');
    gotoxy(0,2);
    PrintBCD(n3,-1);

    gotoxy(0,3);
    if (CompBCD("1000000",n3)==COMP_EQ) LCD_Text("Match!");
    else
    {
      P1OUT&=~BUFFER_EN;
      LCD_Text("No match!");
      for (;;);
    }
  }*/

  int key,i=0,j=0,k=0,x=0,y=0;
  bool shift=false, clear_shift=false, redraw=true, input=false;
  bool menu=false, redraw_input=false, do_input=false;
  int input_ptr=0, input_offset=0;
  int process_output=0;
  static const char StartInput[]="0123456789.";

  Init();

  ErrorMsg("Finished");

  do
  {
    if (redraw)
    {
      ClrLCD();
      DrawStack(menu,input,stack_ptr);
      redraw=false;
    }
    if (redraw_input)
    {
      DrawInput(p0,input_ptr,input_offset,menu);
      redraw_input=false;
    }

    do
    {
      key=KeyMatrix[GetKey()];
    } while (key==0);
    delay_ms(500);

    j=0;
    do
    {
      if (key==StartInput[j])
      {
        j=0;
        break;
      }
    }while(StartInput[j++]);

    if (j==0)//numbers
    {
      if (input==false)
      {
        if (stack_ptr==STACK_SIZE)
        {
          ErrorMsg("Stack full");
          redraw=true;
        }
        else
        {
          input=true;
          ClrLCD();
          DrawStack(menu,input,stack_ptr);
          input_offset=0;
          input_ptr=1;
          p0[0]=key;
          p0[1]=0;
          redraw_input=true;
          SetBlink(true);
        }
      }
      else
      {
        if (input_ptr<255)
        {
          k=0;
          for (j=input_ptr;p0[j];j++) k++;
          for (j=k;j>=0;j--) p0[input_ptr+j+1]=p0[input_ptr+j];
          p0[input_ptr++]=key;

          if (input_ptr-input_offset==(SCREEN_WIDTH-1))
          {
            if (p0[input_ptr]==0) input_offset+=0;
            else if ((p0[input_ptr]!=0)&&(p0[input_ptr+1]==0)) input_offset+=0;
            else input_offset++;
          }
          else if (input_ptr-input_offset==(SCREEN_WIDTH))
          {
            if (p0[input_ptr]==0) input_offset++;
            else input_offset++;
          }
          redraw_input=true;
        }
      }
    }
    else//not numbers
    {
      if (input)
      {
        switch(key)
        {
          case KEY_BACKSPACE:
            if (input_ptr)
            {
              input_ptr--;
              if (input_ptr-input_offset==0) input_offset-=2;
              else if (input_ptr-input_offset<2) input_offset--;
              if (input_offset<0) input_offset=0;
              j=input_ptr;
              while (p0[j])
              {
                p0[j]=p0[j+1];
                j++;
              }
              redraw_input=true;
            }
            key=0;
            break;
          case KEY_DELETE:
            j=input_ptr;
            while (p0[j])
            {
              p0[j]=p0[j+1];
              j++;
            }
            redraw_input=true;
            key=0;
            break;
          case KEY_LEFT://left
            if (input_ptr)
            {
              input_ptr--;
              if (input_ptr-input_offset==0) input_offset--;
              if (input_offset<0) input_offset=0;
              redraw_input=true;
            }
            key=0;
            break;
          case KEY_RIGHT://right
            if (p0[input_ptr])
            {
              input_ptr++;
              if (input_ptr-input_offset==(SCREEN_WIDTH-1))
              {
                if (p0[input_ptr]==0) input_offset+=0;
                else if ((p0[input_ptr]!=0)&&(p0[input_ptr+1]==0)) input_offset+=0;
                else input_offset++;
              }
              else if (input_ptr-input_offset==(SCREEN_WIDTH))
              {
                if (p0[input_ptr]==0) input_offset++;
              }
              redraw_input=true;
            }
            key=0;
            break;
          case KEY_ESCAPE:
            SetBlink(false);
            input=false;
            redraw=true;
            key=0;
            break;
          case KEY_ENTER:
            do_input=true;
            key=0;
            break;
          case 'z':
            input_ptr=0;
            input_offset=0;
            p0[0]=0;
            redraw_input=true;
            key=0;
            break;
          case KEY_DOWN:
          case KEY_UP:
            key=0;
            break;
          case ' ':
            break;
          default: //other key pressed during input
            do_input=true;
        }
      }

      //else redraw=true; //not inputting and not number

      if (do_input)
      {
        x=0;
        for (j=0;p0[j];j++)
        {
          if (p0[j]=='.') x++;
          if (x==2) break;
        }
        if (x==2)
        {
          ErrorMsg("Invalid input");
          redraw_input=true;
        }
        else if (p0[0]==0)
        {
          SetBlink(false);
          input=false;
          redraw=true;
          key=0;
        }
        else
        {
          SetBlink(false);
          BufferBCD(p0,BCD_stack+stack_ptr*MATH_CELL_SIZE);
          if (IsZero(BCD_stack+stack_ptr*MATH_CELL_SIZE)&&(BCD_stack[stack_ptr*MATH_CELL_SIZE+BCD_SIGN])) BCD_stack[stack_ptr*MATH_CELL_SIZE+BCD_SIGN]=0;
          FullShrinkBCD(BCD_stack+stack_ptr*MATH_CELL_SIZE);
          stack_ptr++;
          input=false;
        }
        redraw=true;
        do_input=false;
      }

      clear_shift=true;
      process_output=0;
      switch (key)
      {
        case '+':
          if (stack_ptr>=2)
          {
            AddBCD(stack_buffer,BCD_stack+(stack_ptr-2)*MATH_CELL_SIZE,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
            process_output=2;
            redraw=true;
          }
          break;
        case '-':
          if (stack_ptr>=2)
          {
            SubBCD(stack_buffer,BCD_stack+(stack_ptr-2)*MATH_CELL_SIZE,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
            process_output=2;
            redraw=true;
          }
          break;
        case '/':
          if (!shift)
          {
            if (stack_ptr>=2)
            {
              if (IsZero(BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE))
              {
                ErrorMsg("Divide by zero");
              }
              else
              {
                DivBCD(stack_buffer,BCD_stack+(stack_ptr-2)*MATH_CELL_SIZE,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
                process_output=2;
              }
              redraw=true;
            }
          }
          else//mod
          {
            if (stack_ptr>=2)
            {
              if (IsZero(BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE))
              {
                ErrorMsg("Invalid Input");
              }
              else
              {
                CopyBCD(p3,BCD_stack+(stack_ptr-2)*MATH_CELL_SIZE);
                CopyBCD(p2,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);

                if (p3[BCD_SIGN]==1) j=1;
                else j=0;
                p3[BCD_SIGN]=0;
                p2[BCD_SIGN]=0;

                while(1)
                {
                  SubBCD(p0,p3,p2);
                  if (p0[BCD_SIGN])
                  {
                    CopyBCD(stack_buffer,p3);
                    break;
                  }
                  else CopyBCD(p3,p0);
                }
                stack_buffer[BCD_SIGN]=j;
                process_output=2;
              }
              redraw=true;
            }
          }
          break;
        case '*':
          if (stack_ptr>=2)
          {
            MultBCD(stack_buffer,BCD_stack+(stack_ptr-2)*MATH_CELL_SIZE,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
            process_output=2;
            redraw=true;
          }
          break;
        case KEY_BACKSPACE:
        case KEY_DELETE:
          if (stack_ptr>=1)
          {
            stack_ptr--;
            redraw=true;
          }
          break;
        case KEY_ENTER:
        case 'd'://dupe
          if (stack_ptr>=1)
          {
            if (stack_ptr==STACK_SIZE)
            {
              ErrorMsg("Stack full");
            }
            else
            {
              CopyBCD(BCD_stack+(stack_ptr)*MATH_CELL_SIZE,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
              stack_ptr++;
            }
            redraw=true;
          }
          break;
        case KEY_LEFT:
          if (stack_ptr>=1)
          {
            RolBCD(stack_buffer,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE,1);
            process_output=1;
            redraw=true;
          }
          break;
        case KEY_RIGHT:
          if (stack_ptr>=1)
          {
            RorBCD(stack_buffer,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE,1);
            process_output=1;
            redraw=true;
          }
          break;
        case KEY_UP:
          if (stack_ptr>=2)
          {
            CopyBCD(stack_buffer,BCD_stack);
            for (i=0;i<(stack_ptr-1);i++)
            {
              CopyBCD(BCD_stack+i*MATH_CELL_SIZE,BCD_stack+(i+1)*MATH_CELL_SIZE);
            }
            CopyBCD(BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE,stack_buffer);
            redraw=true;
          }
          break;
        case KEY_DOWN:
          if (stack_ptr>=2)
          {
            CopyBCD(stack_buffer,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
            for (i=(stack_ptr-1);i>0;i--)
            {
              CopyBCD(BCD_stack+(i)*MATH_CELL_SIZE,BCD_stack+(i-1)*MATH_CELL_SIZE);
            }
            CopyBCD(BCD_stack,stack_buffer);
            redraw=true;
          }
          break;
        case ' '://shift
          shift=!shift;
          if (shift) P2OUT|=LED_2ND;
          else P2OUT&=~LED_2ND;
          clear_shift=false;
          break;
        case 'b'://program
          break;
        case 'c'://cosine
          if (!shift)
          {
            if (stack_ptr>=1)
            {
              BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+BCD_SIGN]=0;
              TrigPrep(stack_ptr,&j);
              if (IsZero(p3)) ImmedBCD("1",stack_buffer);
              else TanBCD(p4,stack_buffer,p3);
              if (j==1) stack_buffer[BCD_SIGN]=1;
              process_output=1;
              redraw=true;
            }
          }
          else//acos
          {
            if (stack_ptr>=1)
            {
              process_output=1;
              i=CompBCD("0",BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
              j=CompBCD("1",BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
              k=CompBCD("-1",BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
              if (i==COMP_EQ) ImmedBCD("90",stack_buffer);
              else if (j==COMP_EQ) ImmedBCD("0",stack_buffer);
              else if (k==COMP_EQ) ImmedBCD("180",stack_buffer);
              else if ((j==COMP_LT)||(k==COMP_GT))
              {
                ErrorMsg("Invalid input");
                process_output=0;
              }
              else AcosBCD(stack_buffer,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
              redraw=true;
            }
          }
          break;
        case 'e'://e^x
          if (!shift)
          {
            if (stack_ptr>=1)
            {
              if (CompBCD("177",BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE)==COMP_LT)
              {
                ErrorMsg("Argument\ntoo large");
              }
              else
              {
                ExpBCD(stack_buffer,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
                process_output=1;
              }
              redraw=true;
            }
          }
          else//10^x
          {
            if (stack_ptr>=1)
            {
              x=0;
              j=CompVarBCD(perm_zero,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);

              if (j==COMP_EQ) ImmedBCD("1",stack_buffer);
              else
              {
                if (j==COMP_GT) j=1;
                else j=0;
                BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+BCD_SIGN]=0;

                CopyBCD(p2,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
                p2[BCD_LEN]=p2[BCD_DEC];
                if (CompVarBCD(p2,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE)==COMP_EQ)//x is an integer
                {
                  ImmedBCD("254",p2);
                  if (CompVarBCD(BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE,p2)==COMP_GT)
                  {
                    i=255;
                  }
                  else
                  {
                    i=BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+3]*100;
                    i+=BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+4]*10;
                    i+=BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+5];
                    if (BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+BCD_DEC]==2) i/=10;
                    else if (BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+BCD_DEC]==1) i/=100;
                  }

                  if (i>254)
                  {
                    ErrorMsg("Invalid input");
                    BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+BCD_SIGN]=j;
                    x=1;
                  }
                  else
                  {
                    stack_buffer[BCD_SIGN]=0;
                    stack_buffer[BCD_DEC]=i+1;
                    stack_buffer[BCD_LEN]=i+1;
                    stack_buffer[3]=1;
                    for (k=0;k<i;k++)
                    {
                      stack_buffer[k+4]=0;
                    }
                  }
                }
                else
                {
                  ImmedBCD("10",p5);
                  PowBCD(stack_buffer,p5,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
                  if (stack_buffer[BCD_DEC]>(Settings.DecPlaces)) stack_buffer[BCD_LEN]=stack_buffer[BCD_DEC];
                  else if (stack_buffer[BCD_LEN]>(Settings.DecPlaces))
                  {
                    stack_buffer[BCD_LEN]=Settings.DecPlaces;
                  }
                }

                if ((j)&&(x==0))
                {
                  ImmedBCD("1",p2);
                  DivBCD(p3,p2,stack_buffer);
                  CopyBCD(stack_buffer,p3);
                }
              }
              if (x==0) process_output=1;
              redraw=true;
            }
          }
          break;
        case 'i'://pi
          if (stack_ptr==STACK_SIZE)
          {
            ErrorMsg("Stack full");
          }
          else
          {
            stack_ptr++;
            ImmedBCD(pi,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
            BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+BCD_LEN]=1+Settings.DecPlaces;
          }
          redraw=true;
          break;
        case 'l'://ln
          if (!shift)
          {
            if (stack_ptr>=1)
            {
              if (CompVarBCD(perm_zero,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE)!=COMP_LT)
              {
                ErrorMsg("Invalid input");
              }
              else
              {
                if (LnBCD(stack_buffer,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE)) process_output=1;
                else ErrorMsg("Argument\ntoo large");
              }
              redraw=true;
            }
          }
          else
          {
            if (stack_ptr>=1)
            {
              if (CompVarBCD(perm_zero,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE)!=COMP_LT)
              {
                ErrorMsg("Invalid input");
              }
              else
              {
                x=0;
                if (BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+3]==1)
                {
                  CopyBCD(p0,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);

                  j=p0[BCD_DEC];
                  k=p0[BCD_LEN];
                  for (i=k;i>j;i--)
                  {
                    if (p0[i+2]==0) k--;
                    else break;
                  }

                  p0[BCD_LEN]=k;

                  if (p0[BCD_LEN]==p0[BCD_DEC])
                  {
                    p0[3]=0;
                    if (IsZero(p0))
                    {
                      stack_buffer[BCD_LEN]=3;
                      stack_buffer[BCD_DEC]=3;
                      stack_buffer[BCD_SIGN]=0;
                      i=p0[BCD_LEN]-1;
                      stack_buffer[3]=i/100;
                      stack_buffer[4]=(i%100)/10;
                      stack_buffer[5]=(i%10);
                      FullShrinkBCD(stack_buffer);
                      process_output=1;
                      x=1;
                    }
                  }
                }

                if (!x)
                {
                  if (LnBCD(p3,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE))
                  {
                    DivBCD(stack_buffer,p3,perm_log10);
                    process_output=1;
                  }
                  else ErrorMsg("Argument\ntoo large");
                }
              }
              redraw=true;
            }
          }
          break;
        case 'm':// +/-
          if (stack_ptr>=1)
          {
            if (CompVarBCD(perm_zero,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE)!=COMP_EQ)
            {
              if (BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+BCD_SIGN]==0)
              {
                BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+BCD_SIGN]=1;
              }
              else BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+BCD_SIGN]=0;
            }
            redraw=true;
          }
          break;
        case 'n':// 1/x
          if (stack_ptr>=1)
          {
            if (IsZero(BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE))
            {
              ErrorMsg("Divide by zero");
            }
            else
            {
              ImmedBCD("1",p0);
              DivBCD(stack_buffer,p0,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
              process_output=1;
            }
            redraw=true;
          }
          break;
        case 'o'://round
          if (stack_ptr>=1)
          {
            if (BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+BCD_LEN]>BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+BCD_DEC])
            {
              BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+BCD_LEN]=BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+BCD_DEC];
              //printf(">%d<",BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+BCD_LEN]+3]);
              //GetKey();
              if (BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+BCD_LEN]+3]>4)
              {
                ImmedBCD("1",p0);
                AddBCD(stack_buffer,p0,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
              }
              else CopyBCD(stack_buffer,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
              process_output=1;
            }
            redraw=true;
          }
          break;
        case 'p'://y^x
        case 'r'://x root y
          if (stack_ptr>=2)
          {
            x=0;
            if (key=='r')
            {
              if (IsZero(BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE))
              {
                ErrorMsg("Invalid Input");
                x=1;
              }
              else
              {
                ImmedBCD("1",p3);
                DivBCD(p5,p3,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
              }
            }
            else CopyBCD(p5,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);

            if (x==0)
            {
              j=CompVarBCD(perm_zero,p5);
              k=CompVarBCD(perm_zero,BCD_stack+(stack_ptr-2)*MATH_CELL_SIZE);

              if (k==COMP_GT)
              {
                y=1;
                BCD_stack[(stack_ptr-2)*MATH_CELL_SIZE+BCD_SIGN]=0;
              }
              else y=0;

              if (j==COMP_GT)
              {
                y+=2;
                p5[BCD_SIGN]=0;
              }

              if (k==COMP_EQ) ImmedBCD("0",stack_buffer);
              else if (j==COMP_EQ) ImmedBCD("1",stack_buffer);
              else
              {
                CopyBCD(p2,BCD_stack+(stack_ptr-2)*MATH_CELL_SIZE);
                p2[BCD_LEN]=p2[BCD_DEC];
                if (CompVarBCD(p2,BCD_stack+(stack_ptr-2)*MATH_CELL_SIZE)==COMP_EQ) j=1;
                else j=0;
                CopyBCD(p2,p5);
                p2[BCD_LEN]=p2[BCD_DEC];
                if (CompVarBCD(p2,p5)==COMP_EQ) j+=2;

                if (y&1)//y is negative
                {
                  if (j&2)//x is an integer
                  {
                    ///BCD_stack[(stack_ptr-2)*MATH_CELL_SIZE+BCD_SIGN]=0;
                  }
                  else
                  {
                    ErrorMsg("Invalid input");
                    BCD_stack[(stack_ptr-2)*MATH_CELL_SIZE+BCD_SIGN]=(y&1);
                    x=1;
                  }
                }

                if (x==0)
                {
                  PowBCD(stack_buffer,BCD_stack+(stack_ptr-2)*MATH_CELL_SIZE,p5);
                  if (stack_buffer[BCD_DEC]>(Settings.DecPlaces))
                  {
                    stack_buffer[BCD_LEN]=stack_buffer[BCD_DEC];
                  }
                  else if (stack_buffer[BCD_LEN]>(Settings.DecPlaces))
                  {
                    stack_buffer[BCD_LEN]=Settings.DecPlaces;
                  }

                  if ((j==3)&&(true)) //both are integers
                  {
                    //this works only if slightly above
                    //stack_buffer[BCD_LEN]=stack_buffer[BCD_DEC];
                    //could also fail if exp is less than 1
                  }

                  if (y&2)
                  {
                    ImmedBCD("1",p2);
                    DivBCD(p3,p2,stack_buffer);
                    CopyBCD(stack_buffer,p3);
                  }

                  if (y&1)
                  {
                    if (p5[p5[BCD_DEC]+2]%2==1)
                    {
                      stack_buffer[BCD_SIGN]=(y&1);
                    }
                  }
                }
                if (x==0) process_output=2;
              }
            }
            redraw=true;
          }
          break;
        case 'q'://sqrt
          if (stack_ptr>=1)
          {
            if (IsZero(BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE))
            {
              ImmedBCD("0",stack_buffer);
              process_output=1;
            }
            else if (BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+BCD_SIGN]==1)
            {
              ErrorMsg("Invalid input");
            }
            else
            {
              ImmedBCD("0.5",p5);
              PowBCD(stack_buffer,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE,p5);
              if (stack_buffer[BCD_DEC]>(Settings.DecPlaces)) stack_buffer[BCD_LEN]=stack_buffer[BCD_DEC];
              else if (stack_buffer[BCD_LEN]>(Settings.DecPlaces))
              {
                stack_buffer[BCD_LEN]=Settings.DecPlaces;
              }
              process_output=1;
            }
            redraw=true;
          }
          break;
        case 's'://sin
        case 't'://tan
          if (!shift)
          {
            if (stack_ptr>=1)
            {
              if (BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+BCD_SIGN]==1)
              {
                BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+BCD_SIGN]=0;
                j=1;
              }
              else j=0;
              j+=TrigPrep(stack_ptr,&k);

              if ((key=='t')&&(CompBCD("90",p3)==COMP_EQ))
              {
                ErrorMsg("Invalid input");
              }
              else
              {
                TanBCD(stack_buffer,p4,p3);
                if (CompBCD("90",p3)==COMP_EQ) ImmedBCD("1",stack_buffer);
                if (j==1) stack_buffer[BCD_SIGN]=1;
                if (k==1) p4[BCD_SIGN]=1;

                if (key=='t')
                {
                  DivBCD(p3,stack_buffer,p4);
                  CopyBCD(stack_buffer,p3);
                }
                process_output=1;
              }
              redraw=true;
            }
          }
          else
          {
            if (key=='s')//asin
            {
              if (stack_ptr>=1)
              {
                process_output=1;
                i=CompBCD("0",BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
                j=CompBCD("1",BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
                k=CompBCD("-1",BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
                if (i==COMP_EQ) ImmedBCD("0",stack_buffer);
                else if (j==COMP_EQ) ImmedBCD("90",stack_buffer);
                else if (k==COMP_EQ) ImmedBCD("-90",stack_buffer);
                else if ((j==COMP_LT)||(k==COMP_GT))
                {
                  ErrorMsg("Invalid input");
                  process_output=0;
                }
                else AsinBCD(stack_buffer,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
                redraw=true;
              }
            }
            else//atan
            {
              if (stack_ptr>=1)
              {
                AtanBCD(stack_buffer,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
                process_output=1;
                redraw=true;
              }
            }
          }
          break;
        case 'u'://settings
          ClrLCD();
          gotoxy(0,0);
          LCD_Text(">Accuracy:");
          gotoxy(1,1);
          LCD_Text("Deg/rad:");
          gotoxy(1,2);
          LCD_Text("Sci. Not.");
          gotoxy(1,3);
          LCD_Text("Color stack:");

          i=Settings.DecPlaces;
          x=5;
          y=0;
          do
          {
            if (x!=5) key=KeyMatrix[GetKey()];

            if ((key==KEY_DOWN)||(key==KEY_UP))
            {
              gotoxy(0,y);
              putchar(' ');

              if ((key==KEY_DOWN)&&(y<3)) y++;
              else if ((key==KEY_UP)&&(y>0)) y--;

              gotoxy(0,y);
              putchar('>');
            }
            else if (key==KEY_RIGHT)
            {
              if (y==0)
              {
                if (Settings.DecPlaces<32)
                {
                  Settings.DecPlaces++;
                  x=1;
                }
              }
            }
            else if (key==KEY_LEFT)
            {
              if (y==0)
              {
                if (Settings.DecPlaces>6)
                {
                  Settings.DecPlaces--;
                  x=1;
                }
              }
            }

            if ((key==KEY_LEFT)||(key==KEY_RIGHT))
            {
              if (y==1)
              {
                Settings.DegRad=!Settings.DegRad;
                x=2;
              }
              else if (y==2)
              {
                Settings.SciNot=!Settings.SciNot;
                x=3;
              }
              else if (y==3)
              {
                Settings.ColorStack=!Settings.ColorStack;
                x=4;
              }
            }

            if ((x==1)||(x==5))
            {
              gotoxy(16,0);
              Number2(Settings.DecPlaces);
            }
            if ((x==2)||(x==5))
            {
              gotoxy(15,1);
              if (Settings.DegRad) LCD_Text("Deg");
              else LCD_Text("Rad");
            }
            if ((x==3)||(x==5))
            {
              gotoxy(15,2);
              if (Settings.SciNot) LCD_Text(" On");
              else LCD_Text("Off");
            }
            if ((x==4)||(x==5))
            {
              gotoxy(15,3);
              if (Settings.ColorStack) LCD_Text(" On");
              else LCD_Text("Off");
            }
            x=0;
          } while ((key!=KEY_ESCAPE)&&(key!=KEY_ENTER));

          if (i!=Settings.DecPlaces)
          {
            j=2;

            for (i=0;i<MATH_TRIG_TABLE;i++)
            {
              trig[i*MATH_ENTRY_SIZE+BCD_LEN]=j+Settings.DecPlaces;
              if (IsZero(trig+i*MATH_ENTRY_SIZE)) break;
            }
            Settings.TrigTableSize=i+1;
            for (i=0;i<MATH_LOG_TABLE;i++)
            {
              logs[i*MATH_ENTRY_SIZE+BCD_LEN]=j+Settings.DecPlaces;
              if (IsZero(logs+i*MATH_ENTRY_SIZE)) break;
            }
            Settings.LogTableSize=i+1;

            perm_K[BCD_LEN]=1+Settings.DecPlaces;
            perm_log10[BCD_LEN]=1+Settings.DecPlaces;
          }

          UART_Send(SlaveSettings,true);
          UART_Send((unsigned char)Settings.DecPlaces,true);
          UART_Send((unsigned char)Settings.DegRad,true);
          UART_Send((unsigned char)Settings.LogTableSize,true);
          UART_Send((unsigned char)Settings.TrigTableSize,true);

          key=0;
          redraw=true;
          break;
        case 'w'://swap
          if (stack_ptr>=2)
          {
            CopyBCD(stack_buffer,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
            CopyBCD(BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE,BCD_stack+(stack_ptr-2)*MATH_CELL_SIZE);
            CopyBCD(BCD_stack+(stack_ptr-2)*MATH_CELL_SIZE,stack_buffer);
            redraw=true;
          }
          break;
        case 'x'://x^2
          if (stack_ptr>=1)
          {
            CopyBCD(p0,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
            MultBCD(stack_buffer,p0,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
            process_output=1;
            redraw=true;
          }
          break;
        case 'y':
          key=KEY_ESCAPE;
          break;
        case 'z'://clear
          if (stack_ptr>=1)
          {
            stack_ptr=0;
            redraw=true;
          }
          break;
        default:
          process_output=0;
      }

      if (Settings.DegRad==false)
      {
        if ((key=='a')||(key=='g')||(key=='h'))
        {
          if (process_output>0)
          {
            CopyBCD(p0,stack_buffer);
            ImmedBCD(deg_factor,p1);
            DivBCD(stack_buffer,p0,p1);
          }
        }
      }

      if (process_output==2) stack_ptr--;
      if (process_output>0)
      {
        FullShrinkBCD(stack_buffer);
        if (IsZero(stack_buffer)&&(stack_buffer[BCD_SIGN])) stack_buffer[BCD_SIGN]=0;
        CopyBCD(BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE,stack_buffer);
      }
    }
    if (clear_shift)
    {
      shift=false;
      P2OUT&=~LED_2ND;
    }
    clear_shift=false;
  } while (key!=KEY_ESCAPE);
  for (;;) {_BIS_SR(LPM3_bits);}
  return 0;
}

/*
#pragma vector=USCIAB0RX_VECTOR
__interrupt void USCI0RX_ISR(void)
{
  UC0IFG &= ~UCA0RXIFG;
  //RXD_Buff = UCA0RXBUF;
}

#pragma vector=USCIAB0TX_VECTOR
__interrupt void USCI0TX_ISR(void)
{
  UC0IFG &= ~UCA0TXIFG;
}
*/

__attribute__((interrupt(TIMER0_A0_VECTOR))) static void TA0_ISR(void)
{
  //P2OUT&=~LCD_NEG;
  //__delay_cycles(CHARGE_CYCLES);
  //P2OUT|=LCD_NEG;
}

/*static void delay_ms(int ms)
{
  volatile int i;
  for (i=0;i<ms;i++) __delay_cycles(16000);
}*/

static void Init()
{
  unsigned int i=0;

  LCD_Init();

  //delay_ms(100);

  Settings.ColorStack=true;
  Settings.DecPlaces=12;
  Settings.DegRad=true;
  Settings.LogTableSize=MATH_LOG_TABLE;
  Settings.SciNot=false;
  Settings.TrigTableSize=MATH_TRIG_TABLE;
  stack_ptr=0;

  UC0IFG&=~UCA0RXIFG;
  LCD_Text("Syncing");
  while(1)
  {
    UART_Send(SlaveSync,false);
    delay_ms(1);
    if ((UC0IFG&UCA0RXIFG))
    {
      gotoxy(7,0);
      if (UCA0RXBUF==SlaveAnswer)
      {
        LCD_Text("...Done");
        UART_Ready();
        break;
      }
      else
      {
        LCD_Text("...Failed!");

        for(;;);
      }
    }
    switch (i++)
    {
      case 1:
        gotoxy(7,0);
        LCD_Text("   ");
        gotoxy(7,0);
        break;
      case 500:
      case 1000:
      case 1500:
        putchar('.');
        break;
      case 2000:
        i=0;
    }
  }

  gotoxy(0,1);
  LCD_Text("Writing...");

  UART_Send(SlaveMakeTables,true);
  UART_Receive(true);

  UART_Send(SlaveSettings,true);
  UART_Send((unsigned char)Settings.DecPlaces,true);
  UART_Send((unsigned char)Settings.DegRad,true);
  UART_Send((unsigned char)Settings.LogTableSize,true);
  UART_Send((unsigned char)Settings.TrigTableSize,true);

  UART_Send(SlaveSetDecPlaces,true);
  UART_Receive(true);

  static unsigned const char CustomChars[]={0,0,0,0,0,0,7,4,         //CUST_NW
                                            0,0,0,0,0,0,28,4,        //CUST_NE
                                            4,4,4,4,4,4,4,4,         //CUST_WE
                                            4,7,0,0,0,0,0,0,         //CUST_SW
                                            0,31,0,0,0,0,0,0,        //CUST_S
                                            31,16,23,21,23,16,31,0,  //CUST_OK_O
                                            31,1,21,25,21,1,31,0,    //CUST_OK_K
                                            31,0,17,27,27,17,0,31};  //Busy hour glass

  LCD_Byte(0x40,0);
  for (i=0;i<64;i++) LCD_Byte(CustomChars[i],1);

  ImmedBCD("0",perm_zero);

  gotoxy(10,1);
  LCD_Text("Done");
}

static void LCD_Nibble(unsigned char nibble, unsigned char RS)
{
  P1OUT&=~BUFFER_EN;

  P2OUT&=0xF0;
  P2OUT|=(nibble&0xF);

  if (RS!=0) P2OUT|=LCD_DATA;
  else P2OUT&=~LCD_DATA;

  P1OUT|=LCD_EN;
  __delay_cycles(LCD_CYCLES);
  P1OUT&=~LCD_EN;

  P1OUT|=BUFFER_EN;

  //really?
  delay_ms(1);
}

static void LCD_Byte(unsigned char byte, unsigned char RS)
{
  LCD_Nibble(byte>>4,RS);
  LCD_Nibble(byte,RS);
  if ((RS==0)&&(!(0xFC&byte))&&(byte&0x03)) delay_ms(2);
}

#define DELAY_SMALL 5
#define DELAY_MEDIUM 30
#define DELAY_LARGE 500

static void LCD_Init()
{
  const unsigned char commands[]={0x3,0x3,0x2,0x28,0xE,0x1,0x6,0xC,0x1};

  int i;

  delay_ms(DELAY_LARGE);
  LCD_Nibble(0x3,0);
  delay_ms(DELAY_MEDIUM);

  for (i=0;i<9;i++)
  {
    if (i<3) LCD_Nibble(commands[i],0);
    else LCD_Byte(commands[i],0);
    delay_ms(DELAY_SMALL);
  }
}

static void LCD_Text(const char *msg)
{
  int ptr=0;

  while (msg[ptr]!=0)
  {
    LCD_Byte(msg[ptr],1);
    ptr++;
  }
}

static void LCD_Num(unsigned int num)
{
  if (num>999) num=num % 1000;
  LCD_Byte(num/100+'0',1);
  num-=(num/100*100);
  LCD_Byte(num/10+'0',1);
  num-=(num/10*10);
  LCD_Byte(num+'0',1);
}

static void LCD_Hex(unsigned int num)
{
  if (((num&0xF0)>>4)<10) LCD_Byte(((num&0xF0)>>4)+'0',1);
  else LCD_Byte(((num&0xF0)>>4)-10+'A',1);
  if ((num&0xF)<10) LCD_Byte((num&0xF)+'0',1);
  else LCD_Byte((num&0xF)-10+'A',1);
}

static void SetBlink(bool status)
{
  if (status) LCD_Byte(0xD,0);
  else LCD_Byte(0xC,0);
}

static void gotoxy(short x, short y)
{
  if (y==1) x+=0x40;
  else if (y==2) x+=0x14;
  else if (y==3) x+=0x54;
  LCD_Byte(0x80+x,0);
}

static unsigned char GetKey()
{
  volatile unsigned char i,j,k=0,l=0;

  //gotoxy(0,0);
  for (j=0;j<6;j++)
  {
    P2OUT&=0xC0;
    P2OUT|=(1<<j);

    P1OUT&=~SR_LATCH;
    delay_ms(1);
    P1OUT|=SR_LATCH;

    k=0;
    for (i=0;i<8;i++)
    {
      k<<=1;
      if (P1IN & SR_DATA)
      {
        k++;
        /*l++;
        if (l==2)
        {
          LCD_Hex(k);
          for(;;)
          {
            P1OUT|=BUFFER_EN;
            delay_ms(250);
            P1OUT&=~BUFFER_EN;
            delay_ms(250);
          }
        }*/
        return j*7+i;
      }

      P1OUT&=~SR_CLOCK;
      delay_ms(1);
      P1OUT|=SR_CLOCK;

    }
    //LCD_Hex(k);
    //putchar(' ');
  }
  return 0;
}

static unsigned char RAM_Read(const unsigned char *a1)
{
  unsigned char data;
  UART_Send(SlaveRAM_Read,true);
  UART_SendWord((unsigned int)a1,true);
  return UART_Receive(false);
}

//make while into nop
static void RAM_Write(const unsigned char *a1, const unsigned char byte)
{
  UART_Send(SlaveRAM_Write,true);
  UART_SendWord((unsigned int)a1,true);
  UART_Send(byte,true);
}

static void AddBCD(unsigned char *result, const unsigned char *n1, const unsigned char *n2)
{
  UART_Send(SlaveAdd,true);
  UART_SendWord((unsigned int)result,true);
  UART_SendWord((unsigned int)n1,true);
  UART_SendWord((unsigned int)n2,true);
}

static void SubBCD(unsigned char *result, const unsigned char *n1, unsigned char *n2)
{
  UART_Send(SlaveSubtract,true);
  UART_SendWord((unsigned int)result,true);
  UART_SendWord((unsigned int)n1,true);
  UART_SendWord((unsigned int)n2,true);
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
  UART_Send(SlaveBuffer,true);
  UART_SendWord((unsigned int)text,true);
  UART_SendWord((unsigned int)BCD,true);
}

static void PrintBCD(const unsigned char *BCD, int dec_point)
{
  #pragma MM_VAR BCD
  int BCD_ptr,BCD_end;
  bool zero=true;

  BCD_end=BCD[BCD_LEN]+3;
  if (dec_point>=0)
  {
    if ((BCD[BCD_LEN]-BCD[BCD_DEC])>dec_point)
    {
      BCD_end=BCD[BCD_DEC]+dec_point+3;
    }
  }

  if (BCD[BCD_SIGN]==1) putchar('-');
  for (BCD_ptr=3;BCD_ptr<BCD_end;BCD_ptr++)
  {
    if (BCD_ptr==BCD[BCD_DEC]+3) putchar('.');
    if (BCD[BCD_ptr]>9) putchar('x');
    else putchar('0'+BCD[BCD_ptr]);
  }
}

static void MultBCD(unsigned char *result, const unsigned char *n1, const unsigned char *n2)
{
  UART_Send(SlaveMultiply,true);
  UART_SendWord((unsigned int)result,true);
  UART_SendWord((unsigned int)n1,true);
  UART_SendWord((unsigned int)n2,true);
}

static void DivBCD(unsigned char *result, const unsigned char *n1, const unsigned char *n2)
{
  UART_Send(SlaveDivide,true);
  UART_SendWord((unsigned int)result,true);
  UART_SendWord((unsigned int)n1,true);
  UART_SendWord((unsigned int)n2,true);
}

static void ShrinkBCD(unsigned char *dest,unsigned char *src)
{
  UART_Send(SlaveShrink,true);
  UART_SendWord((unsigned int)dest,true);
  UART_SendWord((unsigned int)src,true);
}

static void FullShrinkBCD(unsigned char *n1)
{
  UART_Send(SlaveFullShrink,true);
  UART_SendWord((unsigned int)n1,true);
}

static void PadBCD(unsigned char *n1, int amount)
{
  UART_Send(SlavePad,true);
  UART_SendWord((unsigned int)n1,true);
  UART_SendWord((unsigned int)amount,true);
}

static bool IsZero(unsigned char *n1)
{
  UART_Send(SlaveIsZero,true);
  UART_SendWord((unsigned int)n1,true);
  if (UART_Receive(false)) return true;
  return false;
}

//see if using this in other places makes things smaller
static void CopyBCD(unsigned char *dest, unsigned char *src)
{
  UART_Send(SlaveCopy,true);
  UART_SendWord((unsigned int)dest,true);
  UART_SendWord((unsigned int)src,true);
}

static bool LnBCD(unsigned char *result, unsigned char *arg)
{
  UART_Send(SlaveLn,true);
  UART_SendWord((unsigned int)result,true);
  UART_SendWord((unsigned int)arg,true);
  if (UART_Receive(false)) return true;
  return false;
}

static void ExpBCD(unsigned char *result, unsigned char *arg)
{
  UART_Send(SlaveExp,true);
  UART_SendWord((unsigned int)result,true);
  UART_SendWord((unsigned int)arg,true);
}

static void RolBCD(unsigned char *result, unsigned char *arg, unsigned int amount)
{
  UART_Send(SlaveRol,true);
  UART_SendWord((unsigned int)result,true);
  UART_SendWord((unsigned int)arg,true);
  UART_SendWord(amount,true);
}

//Does shifting one bit add an extra 0?
static void RorBCD(unsigned char *result, unsigned char *arg, unsigned int amount)
{
  UART_Send(SlaveRor,true);
  UART_SendWord((unsigned int)result,true);
  UART_SendWord((unsigned int)arg,true);
  UART_SendWord(amount,true);
}

static void PowBCD(unsigned char *result, unsigned char *base, unsigned char *exp)
{
  UART_Send(SlavePow,true);
  UART_SendWord((unsigned int)result,true);
  UART_SendWord((unsigned int)base,true);
  UART_SendWord((unsigned int)exp,true);
}

static void TanBCD(unsigned char *sine_result,unsigned char *cos_result,unsigned char *arg)
{
  unsigned int a0;

  UART_Send(SlaveTan,true);
  UART_SendWord((unsigned int)sine_result,true);
  UART_SendWord((unsigned int)cos_result,true);
  UART_SendWord((unsigned int)arg,true);
}

static void AcosBCD(unsigned char *result,unsigned char *arg)
{
  UART_Send(SlaveAcos,true);
  UART_SendWord((unsigned int)result,true);
  UART_SendWord((unsigned int)arg,true);
}

static void AsinBCD(unsigned char *result,unsigned char *arg)
{
  UART_Send(SlaveAsin,true);
  UART_SendWord((unsigned int)result,true);
  UART_SendWord((unsigned int)arg,true);
}

static void AtanBCD(unsigned char *result,unsigned char *arg)
{
  UART_Send(SlaveAtan,true);
  UART_SendWord((unsigned int)result,true);
  UART_SendWord((unsigned int)arg,true);
}

static void CalcTanBCD(unsigned char *result1,unsigned char *result2,unsigned char *result3,unsigned char *arg,unsigned char flag)
{
  UART_Send(SlaveCalcTan,true);
  UART_SendWord((unsigned int)result1,true);
  UART_SendWord((unsigned int)result2,true);
  UART_SendWord((unsigned int)result3,true);
  UART_SendWord((unsigned int)arg,true);
  UART_Send(flag,true);
}

static unsigned char CompBCD(const char *num, unsigned char *var)
{
  ImmedBCD(num,p0);
  return CompVarBCD(p0,var);
}

static unsigned char CompVarBCD(unsigned char *var1, unsigned char *var2)
{
  UART_Send(SlaveCompVar,true);
  UART_SendWord((unsigned int)var1,true);
  UART_SendWord((unsigned int)var2,true);
  return UART_Receive(false);
}

//convert angle to 0-90 format and put in p3
static unsigned char TrigPrep(unsigned int stack_ptr_copy, int *cosine)
{
  UART_Send(SlaveTrigPrep,true);
  UART_SendWord(stack_ptr_copy,true);
  *cosine=(int)UART_Receive(false);
  return UART_Receive(false);
}

void DrawStack(bool menu, bool input, int stack_ptr)
{
  int i,j=4,k,k_end,l,m;
  if (menu) j--;
  if (input) j--;
  for (i=0;i<j;i++)
  {
    gotoxy(0,i);
    putchar('0'+j-i);
    putchar(':');
    if ((stack_ptr-j+i)>=0)
    {
      //This should be done at the end of all calculations
      //FullShrinkBCD(BCD_stack+(stack_ptr-j+i)*MATH_CELL_SIZE);
      if (BCD_stack[(stack_ptr-j+i)*MATH_CELL_SIZE+BCD_DEC]==0)
      {
        PadBCD(BCD_stack+(stack_ptr-j+i)*MATH_CELL_SIZE,1);
      }
      CopyBCD(p1,BCD_stack+(stack_ptr-j+i)*MATH_CELL_SIZE);

      if (Settings.SciNot)
      {
        if (IsZero(p1)) LCD_Text("0.e0");
        else
        {
          //printf("*");
          //PrintBCD(p1,-1);
          k=0;
          for (l=0;l<p1[BCD_LEN];l++) if (p1[l+3]) k=l;
          p1[BCD_LEN]=k+1;
          //printf("!");
          //PrintBCD(p1,-1);
          //printf("*");
          //getch();

          for (l=0;l<p1[BCD_LEN];l++) if (p1[l+3]!=0) break;

          m=0;
          k=(p1[BCD_DEC]-l-1);//length of e
          if (k<0) k=-k;
          if (p1[BCD_SIGN]) m++;
          if (k>9) m++;
          if (k>99) m++;
          if ((p1[BCD_DEC]-l-1)<0) m++;

          if ((16-m)>(p1[BCD_LEN]-l))
          {
            k_end=p1[BCD_LEN]-l;
            m=17-k_end-m;
          }
          else
          {
            k_end=15-m;
            m=2;
          }

          gotoxy(m,i);
          if (p1[BCD_SIGN]) putchar('-');
          for (k=0;k<k_end;k++)
          {
            putchar(p1[k+l+3]+'0');
            if (k==0) putchar('.');
          }

          putchar('e');
          k=p1[BCD_DEC]-l-1;
          if (k<0)
          {
            putchar('-');
            k=-k;
          }
          m=0;
          if (k/100) {putchar('0'+(k/100));m=1;}
          if (((k%100)/10)||m) {putchar('0'+(k%100)/10);m=1;}
          putchar('0'+k%10);

          //printf("Len: %d\n",x1[BCD_LEN]-i);
          //printf("E: %d\n",x1[BCD_DEC]-i-1);
        }
      }
      else
      {
        k=p1[BCD_LEN];

        while ((p1[k+2]==0)&&(k!=p1[BCD_DEC]))
        {
          p1[BCD_LEN]-=1;
          k--;
        }
        k_end=p1[BCD_LEN];
        if (k_end>=18)
        {
          k=0;
          k_end=18;
          if (p1[BCD_SIGN]) k_end--;
          if (p1[BCD_DEC]<k_end) k_end--;
        }
        else if (k_end==17)
        {
          k=1;
          if (p1[BCD_SIGN]) k=0;
          if (p1[BCD_DEC]<k_end)
          {
            if (k==0) k_end--;
            else k=0;
          }
        }
        else
        {
          k=SCREEN_WIDTH-k_end-2;
          if (p1[BCD_SIGN]) k--;
          if (p1[BCD_DEC]<p1[BCD_LEN]) k--;
        }

        gotoxy(k+2,i);
        if (p1[BCD_SIGN])
        {
          putchar('-');
          k++;
        }
        for (l=3;l<k_end+3;l++)
        {
          //if (p1[BCD_DEC]==k-3) putchar('.');
          //putchar(p1[k]+'0');
          putchar(p1[l]+'0');
          if (p1[BCD_DEC]==l-2)
          {
            if (l+k<20) putchar('.');
          }
        }
        if (p1[BCD_DEC]>k_end)
        {
          gotoxy(19,i);
          putchar('>');
        }
      }
    }
  }
}

void DrawInput(unsigned char *line, int input_ptr, int offset, bool menu)
{
  #pragma MM_VAR line

  int i,j=4;
  bool done=false;

  if (menu) j--;
  gotoxy(0,j-1);

  for (i=0;i<SCREEN_WIDTH;i++)
  {
    if ((i==0)&&(offset>0)) putchar('<');
    else if ((i==SCREEN_WIDTH-1)&&(line[i+offset+1])&&(!done))
    {
      if (line[i+offset])
      {
        putchar('>');
        //gotoxy(25,6);
        //printf("%c %d     ",line[i+offset+1],i+offset+1);
      }
    }
    else
    {
      if (!done)
      {
        if (line[i+offset]) putchar(line[i+offset]);
        else
        {
          done=true;
          //putchar(' ');
        }
      }
      if (done) putchar(' ');
    }
  }
  gotoxy(input_ptr-offset,j-1);
}

static void ErrorMsg(const char *msg)
{
  int i,j=0,tx,char_max=5,height=1;

  enum {CUST_NW,CUST_NE,CUST_WE,CUST_SW,CUST_S,CUST_OK_O,CUST_OK_K,CUST_E};

  for (i=0;msg[i];i++)
  {
    if (msg[i]=='\n')
    {
      j=0;
      height++;
    }
    else
    {
      j++;
      if (j>char_max) char_max=j;
    }
  }
  tx=SCREEN_WIDTH/2-(char_max+1)/2;
  if (height==4) height=0;
  else height=1;
  gotoxy(tx-1,height-1);

  putchar(CUST_NW);
  for (j=0;j<char_max;j++) putchar('_');
  putchar(CUST_NE);
  gotoxy(tx-1,height);
  putchar(CUST_WE);

  j=0;
  for (i=0;msg[i];i++)
  {
    if (msg[i]=='\n')
    {
      for (;j<char_max;j++) putchar(' ');
      putchar(CUST_WE);
      j=0;
      height++;
      gotoxy(tx-1,height);
      putchar(CUST_WE);
    }
    else
    {
      j++;
      putchar(msg[i]);
    }
  }
  for (;j<char_max;j++) putchar(' ');
  putchar(CUST_WE);
  height++;
  gotoxy(tx-1,height);
  putchar(CUST_SW);
  for (j=1;j<char_max;j++) putchar(CUST_S);
  putchar(CUST_OK_O);
  putchar(CUST_OK_K);

  while (KeyMatrix[GetKey()]!=13);
  delay_ms(1000);
}

static void Number2(int num)
{
  if (num<10)
  {
    putchar(' ');
    putchar(num+'0');
  }
  else
  {
    putchar(num/10+'0');
    putchar(num%10+'0');
  }
}

static void SetDecPlaces()
{

}

static void TestRAM()
{
  /*unsigned int ia;
  unsigned int address,oldaddress;
  LCD_Text("Testing RAM...");

  #define ADDRESS_RANGE 65535

  RXD_Buff=0;

  address=0;
  oldaddress=1;
  gotoxy(0,2);
  LCD_Text("Writing...");
  while (address!=ADDRESS_RANGE)
  {
    for (ia=0;ia<138;ia++)
    {
      if ((address>>8)!=oldaddress)
      {
        gotoxy(0,3);
        LCD_Num(address>>8);
        oldaddress=address>>8;
      }

      UART_Send(0);
      UART_Send(address>>8);
      UART_Send(address&0xFF);
      UART_Send(ia);
      UART_Receive();
      address++;
      if (address==ADDRESS_RANGE) break;
    }
  }
  address=0;
  oldaddress=1;
  gotoxy(0,2);
  LCD_Text("Reading...");
  while (address!=ADDRESS_RANGE)
  {
    for (ia=0;ia<138;ia++)
    {
      if ((address>>8)!=oldaddress)
      {
        gotoxy(0,3);
        LCD_Num(address>>8);
        oldaddress=address>>8;
      }
      UART_Send(1);
      UART_Send(address>>8);
      UART_Send(address&0xFF);
      RXD_Buff=UART_Receive();
      if (RXD_Buff!=ia)
      {
        gotoxy(0,1);
        LCD_Text("Mismatch ");
        LCD_Num(RXD_Buff);
        putchar(':');
        LCD_Num(ia);
        for(;;);
      }
      address++;
      if (address==ADDRESS_RANGE) break;
    }
  }*/
}
