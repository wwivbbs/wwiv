/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2020, WWIV Software Services             */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/
#ifndef __INCLUDED_LOCAL_IO_KEYCODES_H__
#define __INCLUDED_LOCAL_IO_KEYCODES_H__

constexpr int SPACE = 32;
constexpr int RETURN = 13;
constexpr int SOFTRETURN = 10;
constexpr int ENTER = RETURN;

constexpr int BACKSPACE = 8;
constexpr int CBACKSPACE = 127;

constexpr int ESC = 27;
constexpr int TAB = 9;

/* Stab is an extended 15 (0 ascii, 15 ext) */
constexpr int STAB = 15;

constexpr int PAGEUP = 73;
constexpr int PAGEDN = 81;
constexpr int PGUP = PAGEUP;
constexpr int PGDN = PAGEDN;
constexpr int PAGEDOWN = PAGEDN;
constexpr int CPAGEUP = 132;
constexpr int CPAGEDN = 118;
constexpr int CPGUP = CPAGEUP;
constexpr int CPGDN = CPAGEDN;
constexpr int CPGDOWN = CPAGEDN;

constexpr int APAGEUP = 153;
constexpr int APAGEDN = 161;
constexpr int APGDN = APAGEDN;
constexpr int APGUP = APAGEUP;

constexpr int LARROW = 75;
constexpr int RARROW = 77;
constexpr int UPARROW = 72;
constexpr int UARROW = 72;
constexpr int DNARROW = 80;
constexpr int DARROW = 80;

constexpr int CRARROW = 116;
constexpr int CLARROW = 115;
constexpr int CUARROW = 141;
constexpr int CDARROW = 145;

constexpr int ALARROW = 155;
constexpr int ARARROW = 157;
constexpr int AUARROW = 152;
constexpr int ADARROW = 160;

constexpr int HOME = 71;
constexpr int CHOME = 119;
constexpr int END = 79;
constexpr int CEND = 117;
constexpr int INSERT = 82;
constexpr int KEY_DELETE = 83;

constexpr int F1 = 59;
constexpr int F2 = 60;
constexpr int F3 = 61;
constexpr int F4 = 62;
constexpr int F5 = 63;
constexpr int F6 = 64;
constexpr int F7 = 65;
constexpr int F8 = 66;
constexpr int F9 = 67;
constexpr int F10 = 68;

constexpr int AF1 = 104;
constexpr int AF2 = 105;
constexpr int AF3 = 106;
constexpr int AF4 = 107;
constexpr int AF5 = 108;
constexpr int AF6 = 109;
constexpr int AF7 = 110;
constexpr int AF8 = 111;
constexpr int AF9 = 112;
constexpr int AF10 = 113;

constexpr int SF1 = 84;
constexpr int SF2 = 85;
constexpr int SF3 = 86;
constexpr int SF4 = 87;
constexpr int SF5 = 88;
constexpr int SF6 = 89;
constexpr int SF7 = 90;
constexpr int SF8 = 91;
constexpr int SF9 = 92;
constexpr int SF10 = 93;

constexpr int CF1 = 94;
constexpr int CF2 = 95;
constexpr int CF3 = 96;
constexpr int CF4 = 97;
constexpr int CF5 = 98;
constexpr int CF6 = 99;
constexpr int CF7 = 100;
constexpr int CF8 = 101;
constexpr int CF9 = 102;
constexpr int CF10 = 103;

static constexpr int COMMAND_F1 = (F1 + 256);
constexpr int COMMAND_SF1 = (SF1 + 256);
constexpr int COMMAND_CF1 = (CF1 + 256);

constexpr int COMMAND_F2 = (F2 + 256);
constexpr int COMMAND_SF2 = (SF2 + 256);
constexpr int COMMAND_CF2 = (CF2 + 256);

constexpr int COMMAND_F3 = (F3 + 256);
constexpr int COMMAND_SF3 = (SF3 + 256);
constexpr int COMMAND_CF3 = (CF3 + 256);

constexpr int COMMAND_F4 = (F4 + 256);
constexpr int COMMAND_SF4 = (SF4 + 256);
constexpr int COMMAND_CF4 = (CF4 + 256);

constexpr int COMMAND_F5 = (F5 + 256);
constexpr int COMMAND_SF5 = (SF5 + 256);
constexpr int COMMAND_CF5 = (CF5 + 256);

constexpr int COMMAND_F6 = (F6 + 256);
constexpr int COMMAND_SF6 = (SF6 + 256);
constexpr int COMMAND_CF6 = (CF6 + 256);

constexpr int COMMAND_F7 = (F7 + 256);
constexpr int COMMAND_SF7 = (SF7 + 256);
constexpr int COMMAND_CF7 = (CF7 + 256);

constexpr int COMMAND_F8 = (F8 + 256);
constexpr int COMMAND_SF8 = (SF8 + 256);
constexpr int COMMAND_CF8 = (CF8 + 256);

constexpr int COMMAND_F9 = (F9 + 256);
constexpr int COMMAND_SF9 = (SF9 + 256);
constexpr int COMMAND_CF9 = (CF9 + 256);

constexpr int COMMAND_F10 = (F10 + 256);
constexpr int COMMAND_SF10 = (SF10 + 256);
constexpr int COMMAND_CF10 = (CF10 + 256);

constexpr int CA = 1;
constexpr int CB = 2;
constexpr int CC = 3;
constexpr int CD = 4;
constexpr int CE = 5;
constexpr int CF = 6;
constexpr int CG = 7;
constexpr int CH = 8;
constexpr int CI = 9;
constexpr int CJ = 10;
constexpr int CK = 11;
constexpr int CL = 12;
constexpr int CM = 13;
constexpr int CN = 14;
constexpr int CO = 15;
constexpr int CP = 16;
constexpr int CQ = 17;
constexpr int CR = 18;
constexpr int CS = 19;
constexpr int CT = 20;
constexpr int CU = 21;
constexpr int CV = 22;
constexpr int CW = 23;
constexpr int CX = 24;
constexpr int CY = 25;
constexpr int CZ = 26;

constexpr int AA = 30;
constexpr int AB = 48;
constexpr int AC = 46;
constexpr int AD = 32;
constexpr int AE = 18;
constexpr int AF = 33;
constexpr int AG = 34;
constexpr int AH = 35;
constexpr int AI = 23;
constexpr int AJ = 36;
constexpr int AK = 37;
constexpr int AL = 38;
constexpr int AM = 50;
constexpr int AN = 49;
constexpr int AO = 24;
constexpr int AP = 25;
constexpr int AQ = 16;
constexpr int AR = 19;
constexpr int AS = 31;
constexpr int AT = 20;
constexpr int AU = 22;
constexpr int AV = 47;
constexpr int AW = 17;
constexpr int AX = 45;
constexpr int AY = 21;
constexpr int AZ = 44;

constexpr int GET_OUT = (ESC);
constexpr int EXECUTE = (RETURN);

constexpr int COMMAND_LEFT = (LARROW + 256);
constexpr int COMMAND_RIGHT = (RARROW + 256);
constexpr int COMMAND_UP = (UPARROW + 256);
constexpr int COMMAND_DOWN = (DARROW + 256);
constexpr int COMMAND_INSERT = (INSERT + 256);
constexpr int COMMAND_PAGEUP = (PAGEUP + 256);
constexpr int COMMAND_PAGEDN = (PAGEDN + 256);
constexpr int COMMAND_HOME = (HOME + 256);
constexpr int COMMAND_END = (END + 256);
constexpr int COMMAND_STAB = (STAB + 256);
constexpr int COMMAND_DELETE = (KEY_DELETE + 256);

constexpr int COMMAND_AA = (AA + 256);
constexpr int COMMAND_AB = (AB + 256);
constexpr int COMMAND_AC = (AC + 256);
constexpr int COMMAND_AD = (AD + 256);
constexpr int COMMAND_AE = (AE + 256);
constexpr int COMMAND_AF = (AF + 256);
constexpr int COMMAND_AG = (AG + 256);
constexpr int COMMAND_AH = (AH + 256);
constexpr int COMMAND_AI = (AI + 256);
constexpr int COMMAND_AJ = (AJ + 256);
constexpr int COMMAND_AK = (AK + 256);
constexpr int COMMAND_AL = (AL + 256);
constexpr int COMMAND_AM = (AM + 256);
constexpr int COMMAND_AN = (AN + 256);
constexpr int COMMAND_AO = (AO + 256);
constexpr int COMMAND_AP = (AP + 256);
constexpr int COMMAND_AQ = (AQ + 256);
constexpr int COMMAND_AR = (AR + 256);
constexpr int COMMAND_AS = (AS + 256);
constexpr int COMMAND_AT = (AT + 256);
constexpr int COMMAND_AU = (AU + 256);
constexpr int COMMAND_AV = (AV + 256);
constexpr int COMMAND_AW = (AW + 256);
constexpr int COMMAND_AX = (AX + 256);
constexpr int COMMAND_AY = (AY + 256);
constexpr int COMMAND_AZ = (AZ + 256);

#endif // __INCLUDED_LOCAL_IO_KEYCODES_H__
