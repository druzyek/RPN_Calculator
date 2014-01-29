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

#include <stdio.h>
#include <stdbool.h>

#define WINDOWS
//#define LINUX

#ifdef WINDOWS
  #include <windows.h>

  //Block character for error message borders
  #define CHAR_BLOCK    219

  #define KEY_ENTER     13
  #define KEY_BACKSPACE 8
  #define KEY_DELETE    83
  #define KEY_ESCAPE    27
  #define KEY_LEFT      75
  #define KEY_RIGHT     77
  #define KEY_DOWN      80
  #define KEY_UP        72
#elif defined LINUX
  #include <ncurses.h>

  #define CHAR_BLOCK    '*'

  //#define KEY_ENTER     KEY_ENTER
  //#define KEY_BACKSPACE KEY_BACKSPACE
  #define KEY_DELETE    KEY_DC
  #define KEY_ESCAPE    'y'
  //#define KEY_LEFT      KEY_LEFT
  //#define KEY_RIGHT     KEY_RIGHT
  //#define KEY_DOWN      KEY_DOWN
  //#define KEY_UP        KEY_UP

  #define printf printw
  #define putchar addch
#endif

#define SCREEN_WIDTH 20

//Offsets for the first three bytes of every BCD number holding information.
//The fourth byte onward is unpacked BCD.
#define BCD_SIGN 0//0 for positive and 1 for negative
#define BCD_LEN  1//The length of the entire number
#define BCD_DEC  2//Decimal place. Always smaller or equal to BCD_LEN.

//Maximum stack size. Can be changed to be much bigger.
#define STACK_SIZE 10

//Maximum number of bytes needed for a BCD number
#define MATH_CELL_SIZE 260
//Size of elements in the trig and log table
//3 bytes for info, 2 for whole number part, 32 decimal places
#define MATH_ENTRY_SIZE 37
//Number of entries in the CORDIC log table
#define MATH_LOG_TABLE 114
//Number of entries in the CORDIC trig table
#define MATH_TRIG_TABLE 113

//Starting point for CORDIC trig calculations
#define K             "0.60725293500888125616944675250493"
//Conversion factor for log10 and natural logs
#define log10_factor  "2.30258509299404568401799145468437"
#define pi            "3.1415926535897932384626433832795"
#define pi2           "1.57079632679489661923132169163975"
//Degree to radians conversion factor
#define rad_factor    "0.01745329251994329576923690768489"
//Radians to degree conversion factor
#define deg_factor    "57.29577951308232087679815481410522"

//Return values for funtions that compare two BCD numbers
#define COMP_GT 0//Greater than
#define COMP_LT 1//Less than
#define COMP_EQ 2//Equal to

//#pragmas beginning with MM_ are interpretted by my preprocessor.
//They should be ignored by the compiler.
//Function to use to access variables stored in external RAM
#pragma MM_READ RAM_Read
//Function to use to access variables stored in external RAM
#pragma MM_WRITE RAM_Write
//Enable preprocessing of code
#pragma MM_ON

//Set cursor blink
static void SetBlink(bool status);
//Set text color
static void SetColor(bool black);
//Position text cursor
static void gotoxy(short x, short y);
//Clear screen
static void clrscr();
//Get input from keyboard
static int GetKey();

//Reads data from external RAM. Simulated on the PC with the array "memory" declared below.
static unsigned char RAM_Read(const unsigned char *a1);
//Writes data from external RAM. Simulated on the PC with the array "memory" declared below.
static void RAM_Write(const unsigned char *a1, const unsigned char byte);

//Add two BCD numbers. result should not be the same as n1 or n2.
static void AddBCD(unsigned char *result, const unsigned char *n1, const unsigned char *n2);
//Subtract two BCD numbers. n2 may be modified during operation.
static void SubBCD(unsigned char *result, const unsigned char *n1, unsigned char *n2);
//Convert a string in internal memory into a BCD number
static void ImmedBCD(const char *text, unsigned char *BCD);
//Convert a string in external memory into a BCD number
static void BufferBCD(const unsigned char *text, unsigned char *BCD);
//Print a BCD number
static void PrintBCD(const unsigned char *BCD, int dec_point);
//Multiply two BCD numbers
static void MultBCD(unsigned char *result, const unsigned char *n1, const unsigned char *n2);
//Divide two BCD numbers
static void DivBCD(unsigned char *result, const unsigned char *n1, const unsigned char *n2);
//Remove one leading zero from a BCD number
static void ShrinkBCD(unsigned char *dest,unsigned char *src);
//Remove all leading zeroes from a BCD number
static void FullShrinkBCD(unsigned char *n1);
//Add a leading zero to a BCD number
static void PadBCD(unsigned char *n1, int amount);
//Check if a BCD number is equal to zero
static bool IsZero(unsigned char *n1);
//Copy a BCD number from one location in external RAM to another location in external RAM
static void CopyBCD(unsigned char *dest, unsigned char *src);
//Unpack trig and log tables and write them to an array in RAM
static void MakeTables();
//Natural logarithm of a BCD number
static bool LnBCD(unsigned char *result, unsigned char *arg);
//Power of e of a BCD number
static void ExpBCD(unsigned char *result, unsigned char *arg);
//Roll a BCD number left by one BCD decimal place
static void RolBCD(unsigned char *result, unsigned char *arg, int amount);
//Roll a BCD number right by one BCD decimal place
static void RorBCD(unsigned char *result, unsigned char *arg, int amount);
//Exponents of a BCD number
static void PowBCD(unsigned char *result, unsigned char *base, unsigned char *exp);
//Calculate sine and cosine of a BCD number
static void TanBCD(unsigned char *sine_result,unsigned char *cos_result,unsigned char *arg);
//Arccosine of a BCD number
static void AcosBCD(unsigned char *result,unsigned char *arg);
//Arcsine of a BCD number
static void AsinBCD(unsigned char *result,unsigned char *arg);
//Arctangent of a BCD number
static void AtanBCD(unsigned char *result,unsigned char *arg);
//CORDIC routine used to calculate above trig functions
static void CalcTanBCD(unsigned char *result1,unsigned char *result2,unsigned char *result3,unsigned char *arg,int flag);
//Compare a BCD number to a string
static int CompBCD(const char *num, unsigned char *var);
//Compare two BCD numbers
static int CompVarBCD(unsigned char *var1, unsigned char *var2);
//Convert the number on the top of the stack to 0-90 degree format. Store result in p3.
static int TrigPrep(int *cosine);

//Draw the stack
static void DrawStack(bool menu, bool input, int stack_pointer);
//Redraw the input line
static void DrawInput(unsigned char *line, int input_ptr, int offset, bool menu);
//Error message
static void ErrorMsg(const char *msg);
//Print a number between 0 and 99.
static void Number2(int num);
//Rewrite decimal places in trig and log tables after decimal place is changed
static void SetDecPlaces();
//For compatibility with the MSP430 version
#define LCD_Text printf

//Simulated memory of the calculator. Can be set much larger.
#define PC_MEM_SIZE 20000
unsigned char memory[PC_MEM_SIZE];

//Offset for addresses of the following variables
#pragma MM_OFFSET 5000
//Global variables stored in external RAM. Use MM_ASSIGN_GLOBALS where you want to insert
//code for initializing them.
#pragma MM_GLOBALS
  //p0-p7 are general register variables. Some (not all) of the functions they are used in
  //are listed here. If one function calls another, they should use separate registers.
  unsigned char p0[260]; //PowBCD, LnBCD, ExpBCD, TanBCD, AtanBCD, typing,    CompBCD
  unsigned char p1[260]; //PowBCD, LnBCD, ExpBCD, TanBCD, AtanBCD, DrawStack, CompBCD, CompVarBCD
  unsigned char p2[260]; //PowBCD, LnBCD, ExpBCD, TanBCD, AtanBCD
  unsigned char p3[260]; //PowBCD
  unsigned char p4[260]; //PowBCD
  unsigned char p5[260];
  unsigned char p6[260];
  unsigned char p7[260];
  //Buffer variable for calculations in AddBCD
  unsigned char buffer[260]; //AddBCD
  //Buffer variables for DivBCD and MultBCD
  unsigned char perm_buff1[260]; //DivBCD, MultBCD
  unsigned char perm_buff2[260]; //DivBCD
  unsigned char perm_buff3[260]; //DivBCD
  //Table of log values for CORDIC routines
  //My preprocessor does not evaluate define values. They have to be calculated manually.
  //unsigned char logs[MATH_LOG_TABLE*MATH_ENTRY_SIZE];
  unsigned char logs[4218];
  //Table of trig values for CORDIC routines
  //unsigned char trig[MATH_TRIG_TABLE*MATH_ENTRY_SIZE];
  unsigned char trig[4218];
  //Stores 0 in BCD format so it doesn't have to be created in memory every time it's used.
  unsigned char perm_zero[4];
  //Stores the value of K for use with trig functions.
  unsigned char perm_K[36];
  //Stores the log10 conversion factor
  unsigned char perm_log10[36];
  //Total size of the stack. Should be equal to STACK_SIZE * MATH_CELL_SIZE
  unsigned char BCD_stack[2600];
  //Return values are placed here before being added to the stack
  unsigned char stack_buffer[260];
//End of global variables that will be stored externally
#pragma MM_END

//Information used in the settings page
struct SettingsType
{
  bool ColorStack;
  int DecPlaces;
  bool DegRad;
  int LogTableSize;
  int TrigTableSize;
  bool SciNot;

} Settings;

//Pointer to the top of the BCD stack
int stack_ptr;

//Debug variables to count how many accesses to external memory an operation takes
unsigned long counter1,counter2;

//Functions for console operations under Windows
#ifdef WINDOWS
void SetBlink(bool status)
{
  CONSOLE_CURSOR_INFO cursorInfo;
  HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
  cursorInfo.dwSize=10;
  cursorInfo.bVisible=status;
  SetConsoleCursorInfo(hStdout,&cursorInfo);
}

void gotoxy(short x, short y)
{
  HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
  //Add 1 to leave room for screen border
  COORD position = { x+1, y+1 };

  SetConsoleCursorPosition(hStdout, position);
}

void SetColor(bool black)
{
  HANDLE hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
  if (black) SetConsoleTextAttribute(hStdout,FOREGROUND_RED|FOREGROUND_BLUE|FOREGROUND_GREEN|FOREGROUND_INTENSITY);
  else SetConsoleTextAttribute(hStdout,BACKGROUND_RED|BACKGROUND_BLUE|BACKGROUND_GREEN|BACKGROUND_INTENSITY);
}

void clrscr()
{
  system("cls");
}

int GetKey()
{
  int i;
  i=getch();
  //Filter for direction keys
  if (i==224) i=getch();
  return i;
}
//Functions for console operations under Linux using ncurses
#elif defined LINUX
void SetBlink(bool status)
{
  if (status) curs_set(1);
  else curs_set(0);
}

void gotoxy(short x, short y)
{
  move(y+1,x+1);
}

void SetColor(bool black)
{
  return;
}

void clrscr()
{
  erase();
}

int GetKey()
{
  int i;
  i=getch();
  return i;
}
#endif

static unsigned char RAM_Read(const unsigned char *a1)
{
  counter1++;
  if (a1>(unsigned char *)PC_MEM_SIZE)
  {
    printf("\nRead error:%p\n",a1);
    GetKey();
  }
  return *(memory+(ptrdiff_t)a1);
}

static void RAM_Write(const unsigned char *a1, const unsigned char byte)
{
  counter2++;
  if (a1>(unsigned char *)PC_MEM_SIZE)
  {
    printf("\nWrite error:%p\n",a1);
    GetKey();
  }
  memory[(ptrdiff_t)a1]=byte;
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
  #ifdef LINUX
  refresh();
  #endif
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
  if ((result[BCD_LEN]-result[BCD_DEC])>Settings.DecPlaces) result[BCD_LEN]=result[BCD_DEC]+Settings.DecPlaces;

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

static void CopyBCD(unsigned char *dest, unsigned char *src)
{
  #pragma MM_VAR dest
  #pragma MM_VAR src
  int i,i_end;
  i_end=src[BCD_LEN]+3;
  for (i=0;i<i_end;i++) dest[i]=src[i];
}

static void MakeTables()
{
  //The first number of every line is the number of BCD bytes that follow it.
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

static void RolBCD(unsigned char *result, unsigned char *arg, int amount)
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

static void RorBCD(unsigned char *result, unsigned char *arg, int amount)
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

static void CalcTanBCD(unsigned char *result1,unsigned char *result2,unsigned char *result3,unsigned char *arg,int flag)
{
  #pragma MM_VAR result2

  unsigned int i;
  unsigned int trig_ptr=0;

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

static int CompBCD(const char *num, unsigned char *var)
{
  ImmedBCD(num,p0);
  return CompVarBCD(p0,var);
}

static int CompVarBCD(unsigned char *var1, unsigned char *var2)
{
  SubBCD(p1,var1,var2);
  if (IsZero(p1)) return COMP_EQ;
  else if (p1[BCD_SIGN]==0) return COMP_GT;
  else return COMP_LT;
}

int TrigPrep(int *cosine)
{
  int sine;

  if (Settings.DegRad) CopyBCD(p3,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);
  else
  {
    ImmedBCD(deg_factor,p0);
    MultBCD(p3,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE,p0);
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

void ClrLCD()
{
  gotoxy(-1,-1);
  printf("|--------------------|\n");
  printf("|                    |\n");
  printf("|                    |\n");
  printf("|                    |\n");
  printf("|                    |\n");
  printf("|--------------------|\n");
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
          k=0;
          for (l=0;l<p1[BCD_LEN];l++) if (p1[l+3]) k=l;
          p1[BCD_LEN]=k+1;

          for (l=0;l<p1[BCD_LEN];l++) if (p1[l+3]!=0) break;

          m=0;
          k=(p1[BCD_DEC]-l-1);
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
  #ifdef LINUX
  refresh();
  #endif
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
        }
      }
      if (done) putchar(' ');
    }
  }
  gotoxy(input_ptr-offset,j-1);
  #ifdef LINUX
  refresh();
  #endif
}

void ErrorMsg(const char *msg)
{
  int i,j=0,tx,char_max=5,height=1;
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

  putchar(CHAR_BLOCK);

  for (j=0;j<char_max+1;j++) putchar(CHAR_BLOCK);
  gotoxy(tx-1,height);
  putchar(CHAR_BLOCK);

  j=0;
  for (i=0;msg[i];i++)
  {
    if (msg[i]=='\n')
    {
      for (;j<char_max;j++) putchar(' ');
      putchar(CHAR_BLOCK);
      j=0;
      height++;
      gotoxy(tx-1,height);
      putchar(CHAR_BLOCK);
    }
    else
    {
      j++;
      putchar(msg[i]);
    }
  }
  for (;j<char_max;j++) putchar(' ');
  putchar(CHAR_BLOCK);
  height++;
  gotoxy(tx-1,height);
  for (j=0;j<char_max+2;j++) putchar(CHAR_BLOCK);

  #ifdef LINUX
  refresh();
  #endif

  while (GetKey()!=KEY_ENTER);
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
  int i,j=2;

  for (i=0;i<MATH_TRIG_TABLE;i++)
  {
    trig[i*MATH_ENTRY_SIZE+BCD_LEN]=j+Settings.DecPlaces;
    if (IsZero(trig+i*MATH_ENTRY_SIZE)) break;
  }

  Settings.TrigTableSize=i;

  for (i=0;i<MATH_LOG_TABLE;i++)
  {
    logs[i*MATH_ENTRY_SIZE+BCD_LEN]=j+Settings.DecPlaces;
    if (IsZero(logs+i*MATH_ENTRY_SIZE)) break;
  }

  Settings.LogTableSize=i;

  perm_K[BCD_LEN]=1+Settings.DecPlaces;
  perm_log10[BCD_LEN]=1+Settings.DecPlaces;
}


int main()
{
  int key,i,j,k,x,y;
  #pragma MM_ASSIGN_GLOBALS

  bool shift=false, redraw=true, input=false;
  bool menu=false, redraw_input=false, do_input=false;
  int input_ptr=0, input_offset=0;
  int process_output;
  static const char StartInput[]="0123456789.";

  #ifdef LINUX
  initscr();
  raw();
  noecho();
  keypad(stdscr, TRUE);
  #endif

  stack_ptr=0;
  ImmedBCD("0",perm_zero);
  ImmedBCD(K,perm_K);
  ImmedBCD(log10_factor,perm_log10);

  MakeTables();

  Settings.ColorStack=true;
  Settings.DecPlaces=32;
  Settings.DegRad=true;
  Settings.LogTableSize=MATH_LOG_TABLE;
  Settings.SciNot=false;
  Settings.TrigTableSize=MATH_TRIG_TABLE;
  SetDecPlaces();

  clrscr();

  SetColor(true);
  SetBlink(false);
  ClrLCD();
  gotoxy(-1,6);
  const char legend[36][10]={"atan","","cos","dupe","e^x","","acos","asin","pi","10^x","log","ln","+/-","1/x","round","y^x","sqrt","x root y","sin","tan","settings",
                             "mod","swap","x^2","EXIT","clear","","","","","","","","","",""};
  const char leg2[10]="{},.!@#$%^";
  for (i=0;i<36;i++)
  {
    if (i<13) gotoxy(-1,6+i);
    else if (i<26) gotoxy(15,6+i-13);
    else gotoxy(31,6+i-26);

    if (legend[i][0]=='!')
    {
      j=1;
      SetColor(false);
    }
    else
    {
      j=0;
      SetColor(true);
    }

    if (i<26) printf("[%c]",i+97);
    //else printf("[%c]",leg2[i-26]);
    printf(" %s",legend[i]+j);
  }
  SetColor(true);

  int redraw_count=0;

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

    key=GetKey();

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
        case 'd':
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
          break;
        case 'a'://atan
          if (stack_ptr>=1)
          {
            //counter1=0;
            //counter2=0;
            AtanBCD(stack_buffer,BCD_stack+(stack_ptr-1)*MATH_CELL_SIZE);

            //printf("*%ul %ul*",counter1,counter2);
            //getch();

            process_output=1;
            redraw=true;
          }
          break;
        case 'b':
          break;
        case 'c'://cosine
          if (stack_ptr>=1)
          {
            BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+BCD_SIGN]=0;
            TrigPrep(&j);
            if (IsZero(p3)) ImmedBCD("1",stack_buffer);
            else TanBCD(p4,stack_buffer,p3);
            if (j==1) stack_buffer[BCD_SIGN]=1;
            process_output=1;
            redraw=true;
          }
          break;
        case 'e'://e^x
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
          break;
        case 'g'://acos
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
          break;
        case 'h'://asin
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
        case 'j'://10^x
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
          break;
        case 'k'://log
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
          break;
        case 'l'://ln
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
          if (stack_ptr>=1)
          {
            if (BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+BCD_SIGN]==1)
            {
              BCD_stack[(stack_ptr-1)*MATH_CELL_SIZE+BCD_SIGN]=0;
              j=1;
            }
            else j=0;
            j+=TrigPrep(&k);

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
            if (x!=5) key=GetKey();

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

          if (i!=Settings.DecPlaces) SetDecPlaces();
          key=0;
          redraw=true;
          break;
        case 'v'://mod
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
  } while (key!=KEY_ESCAPE);
  gotoxy(-1,19);
  SetColor(true);
  SetBlink(true);
  #ifdef LINUX
  endwin();
  #endif
  return;
}
