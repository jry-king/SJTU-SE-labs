/* 
 * CS:APP Data Lab 
 * 
 * bits.c - Source file with your solutions to the Lab.
 *          This is the file you will hand in to your instructor.
 *
 * WARNING: Do not include the <stdio.h> header; it confuses the dlc
 * compiler. You can still use printf for debugging without including
 * <stdio.h>, although you might get a compiler warning. In general,
 * it's not good practice to ignore compiler warnings, but in this
 * case it's OK.  
 */

#include "btest.h"
#include <limits.h>

/*
 * Instructions to Students:
 *
 * STEP 1: Fill in the following struct with your identifying info.
 */
team_struct team =
{
   /* Team name: Replace with either:
      Your login ID if working as a one person team
      or, ID1+ID2 where ID1 is the login ID of the first team member
      and ID2 is the login ID of the second team member */
   "516030910408", 
   /* Student name 1: Replace with the full name of first team member */
   "JIN Ruiyang",
   /* Login ID 1: Replace with the login ID of first team member */
   "516030910408",

   /* The following should only be changed if there are two team members */
   /* Student name 2: Full name of the second team member */
   "",
   /* Login ID 2: Login ID of the second team member */
   ""
};

#if 0
/*
 * STEP 2: Read the following instructions carefully.
 */

You will provide your solution to the Data Lab by
editing the collection of functions in this source file.

CODING RULES:
 
  Replace the "return" statement in each function with one
  or more lines of C code that implements the function. Your code 
  must conform to the following style:
 
  int Funct(arg1, arg2, ...) {
      /* brief description of how your implementation works */
      int var1 = Expr1;
      ...
      int varM = ExprM;

      varJ = ExprJ;
      ...
      varN = ExprN;
      return ExprR;
  }

  Each "Expr" is an expression using ONLY the following:
  1. Integer constants 0 through 255 (0xFF), inclusive. You are
      not allowed to use big constants such as 0xffffffff.
  2. Function arguments and local variables (no global variables).
  3. Unary integer operations ! ~
  4. Binary integer operations & ^ | + << >>
    
  Some of the problems restrict the set of allowed operators even further.
  Each "Expr" may consist of multiple operators. You are not restricted to
  one operator per line.

  You are expressly forbidden to:
  1. Use any control constructs such as if, do, while, for, switch, etc.
  2. Define or use any macros.
  3. Define any additional functions in this file.
  4. Call any functions.
  5. Use any other operations, such as &&, ||, -, or ?:
  6. Use any form of casting.
 
  You may assume that your machine:
  1. Uses 2s complement, 32-bit representations of integers.
  2. Performs right shifts arithmetically.
  3. Has unpredictable behavior when shifting an integer by more
     than the word size.

EXAMPLES OF ACCEPTABLE CODING STYLE:
  /*
   * pow2plus1 - returns 2^x + 1, where 0 <= x <= 31
   */
  int pow2plus1(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     return (1 << x) + 1;
  }

  /*
   * pow2plus4 - returns 2^x + 4, where 0 <= x <= 31
   */
  int pow2plus4(int x) {
     /* exploit ability of shifts to compute powers of 2 */
     int result = (1 << x);
     result += 4;
     return result;
  }


NOTES:
  1. Use the dlc (data lab checker) compiler (described in the handout) to 
     check the legality of your solutions.
  2. Each function has a maximum number of operators (! ~ & ^ | + << >>)
     that you are allowed to use for your implementation of the function. 
     The max operator count is checked by dlc. Note that '=' is not 
     counted; you may use as many of these as you want without penalty.
  3. Use the btest test harness to check your functions for correctness.
  4. The maximum number of ops for each function is given in the
     header comment for each function. If there are any inconsistencies 
     between the maximum ops in the writeup and in this file, consider
     this file the authoritative source.
#endif

/*
 * STEP 3: Modify the following functions according the coding rules.
 * 
 *   IMPORTANT. TO AVOID GRADING SURPRISES:
 *   1. Use the dlc compiler to check that your solutions conform
 *      to the coding rules.
 *   2. Use the btest test harness to check that your solutions produce 
 *      the correct answers. Watch out for corner cases around Tmin and Tmax.
 */
/* Copyright (C) 1991-2016 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 2.1 of the License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the GNU C Library; if not, see
   <http://www.gnu.org/licenses/>.  */
/* This header is separate from features.h so that the compiler can
   include it implicitly at the start of every compilation.  It must
   not itself include <features.h> or any other header that includes
   <features.h> because the implicit include comes before any feature
   test macros that may be defined in a source file before it first
   explicitly includes a system header.  GCC knows the name of this
   header in order to preinclude it.  */
/* glibc's intent is to support the IEC 559 math functionality, real
   and complex.  If the GCC (4.9 and later) predefined macros
   specifying compiler intent are available, use them to determine
   whether the overall intent is to support these features; otherwise,
   presume an older compiler has intent to support these features and
   define these macros by default.  */
/* wchar_t uses Unicode 9.0.0.  Version 9.0 of the Unicode Standard is
   synchronized with ISO/IEC 10646:2014, fourth edition, plus
   Amd. 1  and Amd. 2 and 273 characters from forthcoming  10646, fifth edition.
   (Amd. 2 was published 2016-05-01,
   see https://www.iso.org/obp/ui/#iso:std:iso-iec:10646:ed-4:v1:amd:2:v1:en) */
/* We do not support C11 <threads.h>.  */
/* 
 * bang - Compute !x without using !
 *   Examples: bang(3) = 0, bang(0) = 1
 *   Legal ops: ~ & ^ | + << >>
 *   Max ops: 12
 *   Rating: 4 
 */
int bang(int x) {
  /*implement logical not by bit-level operations
   *use the first bit(sign)to judge
   */
  int sign = (x|(~x+1))>>31;
  return sign+1;
}
/*
 * bitCount - returns count of number of 1's in word
 *   Examples: bitCount(5) = 2, bitCount(7) = 3
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 40
 *   Rating: 4
 */
int bitCount(int x) {
  /*count how many 1 are there in the binary form of x
   *implemented by 5 masking operations, and the k th operation calculates the number of 1 in each 2^k bits
   */
  //masks
  int temp = 0x55|(0x55<<8);
  int mask1 = temp|(temp<<16);
  int temp1 = 0x33|(0x33<<8);
  int mask2 = temp1|(temp1<<16);
  int temp2 = 0x0f|(0x0f<<8);
  int mask3 = temp2|(temp2<<16);
  int mask4 = 0xff|(0xff<<16);
  int mask5 = 0xff|(0xff<<8);

  //masking operations
  int answer = (x&mask1) + ((x>>1) & mask1);
  answer = (answer&mask2) + ((answer>>2) & mask2);
  answer = (answer&mask3) + ((answer>>4) & mask3);
  answer = (answer&mask4) + ((answer>>8) & mask4);
  answer = (answer&mask5) + ((answer>>16) & mask5);
  return answer;
}
/* 
 * copyLSB - set all bits of result to least significant bit of x
 *   Example: copyLSB(5) = 0xFFFFFFFF, copyLSB(6) = 0x00000000
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 5
 *   Rating: 2
 */
int copyLSB(int x) {
  /*set all bits of result to least significant bit of x by bit-level operation
   */
  int LSB = x&1;
  return ~LSB+1;
}
/* 
 * divpwr2 - Compute x/(2^n), for 0 <= n <= 30
 *  Round toward zero
 *   Examples: divpwr2(15,1) = 7, divpwr2(-33,4) = -2
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 15
 *   Rating: 2
 */
int divpwr2(int x, int n) {
  /*use bit-level operations to calculate x/(2^n)
   *negative number has an integer called sign of 1 while positive number has it as 0
   *use sign to implement this: add 2^n-1 to negative number since all numbers round down by default
   */
  int sign = ~(x >> 31)+1;
  return (x + ((sign<<n)+(~sign+1)))>>n;
}
/* 
 * evenBits - return word with all even-numbered bits set to 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 8
 *   Rating: 2
 */
int evenBits(void) {
  /*set all even-numbered bits to 1
   *it is just a mask we used in bitcount
   */
  int temp = 0x55|(0x55<<8);
  int mask = temp|(temp<<16);
  return mask;
}
/* 
 * fitsBits - return 1 if x can be represented as an 
 *  n-bit, two's complement integer.
 *   1 <= n <= 32
 *   Examples: fitsBits(5,3) = 0, fitsBits(-4,3) = 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 15
 *   Rating: 2
 */
int fitsBits(int x, int n) {
  /*check whether x can be represented as an n-bit 2's complement integer
   *x fulfills the requirement when the first 33-n bits of x are identical
   */
  int mask = x >> 31;//a mask used to check the first 33-n bits of x
  return !((x >> (n + (~0))) ^ mask);
}
/* 
 * getByte - Extract byte n from word x
 *   Bytes numbered from 0 (LSB) to 3 (MSB)
 *   Examples: getByte(0x12345678,1) = 0x56
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 6
 *   Rating: 2
 */
int getByte(int x, int n) {
  /*extract byte n from x
   *use masking operation to implement
   */
  int mask = 0xff;
  x = x >> (n << 3);//move the byte needed to LSB
  return x & mask;
}
/* 
 * isGreater - if x > y  then return 1, else return 0 
 *   Example: isGreater(4,5) = 0, isGreater(5,4) = 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 24
 *   Rating: 3
 */
int isGreater(int x, int y) {
  /*check whetner x is bigger than y
   *check by comfirming whether y-x is nonnegative since we don't have to consider 0
   *when overflow appears, use the sign of x, y and y-x to ensure the correctness of answer*/
  int difference = y + (~x+1);

  //use an integer to identify the polarity of x, y and y-x
  //0 is non-negative while 1 is negative
  int signx = (x >> 31) & 1;
  int signy = (y >> 31) & 1;
  int signdif = ((difference)>>31) & 1;

  //when overflow, x and y are of different polarity. what we need is y-x < 0 in normal case and y-x > 0 when overflow.
  int condition1 = signdif;
  int condition2 = (signx ^ signy) & (signy ^ signdif);
  
  return condition1 ^ condition2;
}
/* 
 * isNonNegative - return 1 if x >= 0, return 0 otherwise 
 *   Example: isNonNegative(-1) = 0.  isNonNegative(0) = 1.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 6
 *   Rating: 3
 */
int isNonNegative(int x) {
  /*examine whether x is nonnegative
   *judge by the sign bit
   */
  int sign = x >> 31;
  return sign + 1;
}
/* 
 * isNotEqual - return 0 if x == y, and 1 otherwise 
 *   Examples: isNotEqual(5,5) = 0, isNotEqual(4,5) = 1
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 6
 *   Rating: 2
 */
int isNotEqual(int x, int y) {
  /*check whether x and y are equal
   *implemented by xor
   */
  int isnotequal = x ^ y;
  return !!isnotequal;
}
/*
 * isPower2 - returns 1 if x is a power of 2, and 0 otherwise
 *   Examples: isPower2(5) = 0, isPower2(8) = 1, isPower2(0) = 0
 *   Note that no negative number is a power of 2.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 60
 *   Rating: 4
 */
int isPower2(int x) {
  /*check whether x is a power of 2
   *x is a power of 2 when there is only one 1 in x's binary form and it cannot be the first bit
   */

  //check the polarity of x to get rid of negative numbers
  int sign = (x >> 31) & 1;

  //bitcount
  int temp = 0x55|(0x55<<8);
  int mask1 = temp|(temp<<16);
  int temp1 = 0x33|(0x33<<8);
  int mask2 = temp1|(temp1<<16);
  int temp2 = 0x0f|(0x0f<<8);
  int mask3 = temp2|(temp2<<16);
  int mask4 = 0xff|(0xff<<16);
  int mask5 = 0xff|(0xff<<8);
  
  int answer = (x&mask1) + ((x>>1) & mask1);
  int answer1 = (answer&mask2) + ((answer>>2) & mask2);
  int answer2 = (answer1&mask3) + ((answer1>>4) & mask3);
  int answer3 = (answer2&mask4) + ((answer2>>8) & mask4);
  int answer4 = (answer3&mask5) + ((answer3>>16) & mask5);
  //now answer4 is the number of 1 in x

  int ispower = answer4 + (~0);
  return !ispower & !sign;
}
/* 
 * leastBitPos - return a mask that marks the position of the
 *               least significant 1 bit. If x == 0, return 0
 *   Example: leastBitPos(96) = 0x20
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 6
 *   Rating: 4 
 */
int leastBitPos(int x) {
  /*use a mask to represent the position of the least significant 1 bit
   *use xor to eliminate bits on the left of it
   *use and to eliminate bits on the right of it
   */
  return (x ^ (x + (~0))) & x;
}
/* 
 * logicalShift - shift x to the right by n, using a logical shift
 *   Can assume that 1 <= n <= 31
 *   Examples: logicalShift(0x87654321,4) = 0x08765432
 *   Legal ops: ~ & ^ | + << >>
 *   Max ops: 16
 *   Rating: 3 
 */
int logicalShift(int x, int n) {
  /*implement logical right shift by bit-level ops
   *use arithmetic shift and a mask
   */
  int mask = (1 << (32 + (~n+1))) + (~0);
  return (x >> n) & mask;
}
/*
 * satAdd - adds two numbers but when positive overflow occurs, returns
 *          maximum possible value, and when negative overflow occurs,
 *          it returns minimum positive value.
 *   Examples: satAdd(0x40000000,0x40000000) = 0x7fffffff
 *             satAdd(0x80000000,0xffffffff) = 0x80000000
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 30
 *   Rating: 4
 */
int satAdd(int x, int y) {
  /*add two number and limit the sum within the range of integer*/
  int sum = x + y;

  //use three integer to represent the polarity of x, y and x+y
  //1 is negative, 0 is positive
  int signx = (x >> 31) & 1;
  int signy = (y >> 31) & 1;
  int signsum = (sum >> 31) & 1;
  
  //overflow occurs when x and y are of the same polarity and the sign of sum is different from those of x and y
  int overflow = (!(signx ^ signy)) & (signx ^ signsum);

  //when overflow, right shift sum for 31 bits and turn it to corresponding possible value
  int shift = (overflow << 5) + (~overflow+1);
  return (sum >> shift) ^ (overflow << 31);
}
/* 
 * tc2sm - Convert from two's complement to sign-magnitude 
 *   where the MSB is the sign bit
 *   You can assume that x > TMin
 *   Example: tc2sm(-5) = 0x80000005.
 *   Legal ops: ! ~ & ^ | + << >>
 *   Max ops: 15
 *   Rating: 4
 */
int tc2sm(int x) {
  /*convert from two's complement to sign-magnitude*/
  
  //get the sign and the absolute value
  int sign = x >> 31;
  int absolute = (x ^ sign) + (~sign + 1);

  return ((sign & 1) << 31) + absolute;
}
