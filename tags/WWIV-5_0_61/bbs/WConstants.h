/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2006, WWIV Software Services             */
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

#ifndef __INCLUDED_WCONSTANTS_H__
#define __INCLUDED_WCONSTANTS_H__



#define COMMAND_F1        (F1+256)
#define COMMAND_SF1       (SF1+256)
#define COMMAND_CF1       (CF1+256)

#define COMMAND_F2        (F2+256)
#define COMMAND_SF2       (SF2+256)
#define COMMAND_CF2       (CF2+256)

#define COMMAND_F3        (F3+256)
#define COMMAND_SF3       (SF3+256)
#define COMMAND_CF3       (CF3+256)

#define COMMAND_F4        (F4+256)
#define COMMAND_SF4       (SF4+256)
#define COMMAND_CF4       (CF4+256)

#define COMMAND_F5        (F5+256)
#define COMMAND_SF5       (SF5+256)
#define COMMAND_CF5       (CF5+256)

#define COMMAND_F6        (F6+256)
#define COMMAND_SF6       (SF6+256)
#define COMMAND_CF6       (CF6+256)

#define COMMAND_F7        (F7+256)
#define COMMAND_SF7       (SF7+256)
#define COMMAND_CF7       (CF7+256)

#define COMMAND_F8        (F8+256)
#define COMMAND_SF8       (SF8+256)
#define COMMAND_CF8       (CF8+256)

#define COMMAND_F9        (F9+256)
#define COMMAND_SF9       (SF9+256)
#define COMMAND_CF9       (CF9+256)

#define COMMAND_F10       (F10+256)
#define COMMAND_SF10      (SF10+256)
#define COMMAND_CF10      (CF10+256)

#define GET_OUT     (ESC)
#define EXECUTE     (RETURN)

#define COMMAND_LEFT     (LARROW+256)
#define COMMAND_RIGHT    (RARROW+256)
#define COMMAND_UP       (UPARROW+256)
#define COMMAND_DOWN     (DARROW+256)
#define COMMAND_INSERT   (INSERT+256)
#define COMMAND_PAGEUP   (PAGEUP+256)
#define COMMAND_PAGEDN   (PAGEDN+256)
#define COMMAND_HOME     (HOME+256)
#define COMMAND_END      (END+256)
#define COMMAND_STAB     (STAB+256)
#define COMMAND_DELETE   (KEY_DELETE+256)

#define COMMAND_AA       (AA+256)
#define COMMAND_AB       (AB+256)
#define COMMAND_AC       (AC+256)
#define COMMAND_AD       (AD+256)
#define COMMAND_AE       (AE+256)
#define COMMAND_AF       (AF+256)
#define COMMAND_AG       (AG+256)
#define COMMAND_AH       (AH+256)
#define COMMAND_AI       (AI+256)
#define COMMAND_AJ       (AJ+256)
#define COMMAND_AK       (AK+256)
#define COMMAND_AL       (AL+256)
#define COMMAND_AM       (AM+256)
#define COMMAND_AN       (AN+256)
#define COMMAND_AO       (AO+256)
#define COMMAND_AP       (AP+256)
#define COMMAND_AQ       (AQ+256)
#define COMMAND_AR       (AR+256)
#define COMMAND_AS       (AS+256)
#define COMMAND_AT       (AT+256)
#define COMMAND_AU       (AU+256)
#define COMMAND_AV       (AV+256)
#define COMMAND_AW       (AW+256)
#define COMMAND_AX       (AX+256)
#define COMMAND_AY       (AY+256)
#define COMMAND_AZ       (AZ+256)



// The final character of an ansi sequence
#define OB ('[')
#define O ('O')
#define A_HOME ('H')
#define A_LEFT ('D')
#define A_END ('K')
#define A_UP ('A')
#define A_DOWN ('B')
#define A_RIGHT ('C')
#define A_INSERT ('r')
#define A_DELETE ('s')

#define SPACE        32

#define RETURN      13
#define SOFTRETURN  10
#define HARDRETURN  RETURN
#define CRETURN     SOFTRETURN

#define HRETURN     HARDRETURN
#define SRETURN     SOFTRETURN

#define ENTER       RETURN
#define SOFTENTER   SOFTRETURN
#define HARDENTER   HARDRETURN
#define CENTER      CRETURN

#define BKSPACE     8
#define BACKSPACE   8
#define CBACKSPACE  127
#define CBKSPACE    127

#define ESC         27
#define TAB         9

/* Stab is an extended 15 (0 ascii, 15 ext) */
#define STAB        15

#define PAGEUP      73
#define PAGEDN      81
#define PGUP        PAGEUP
#define PGDN        PAGEDN
#define PAGEDOWN    PAGEDN
#define CPAGEUP     132
#define CPAGEDN     118
#define CPGUP       CPAGEUP
#define CPGDN       CPAGEDN
#define CPGDOWN     DPAGEDN

#define APAGEUP     153
#define APAGEDN     161
#define APGDN       APAGEDN
#define APGUP       APAGEUP

#define LARROW      75
#define RARROW      77
#define UPARROW     72
#define UARROW      72
#define DNARROW     80
#define DARROW      80

#define CRARROW     116
#define CLARROW     115
#define CUARROW     141
#define CDARROW     145

#define ALARROW     155
#define ARARROW     157
#define AUARROW     152
#define ADARROW     160

//#define AUPARROW    AUARROW
//#define ADNARROW    ADARROW

#define HOME        71
#define CHOME       119
#define END         79
#define CEND        117
#define INSERT      82
#define KEY_DELETE  83

#define F1          59
#define F2          60
#define F3          61
#define F4          62
#define F5          63
#define F6          64
#define F7          65
#define F8          66
#define F9          67
#define F10         68

#define AF1         104
#define AF2         105
#define AF3         106
#define AF4         107
#define AF5         108
#define AF6         109
#define AF7         110
#define AF8         111
#define AF9         112
#define AF10        113

#define SF1         84
#define SF2         85
#define SF3         86
#define SF4         87
#define SF5         88
#define SF6         89
#define SF7         90
#define SF8         91
#define SF9         92
#define SF10        93

#define CF1         94
#define CF2         95
#define CF3         96
#define CF4         97
#define CF5         98
#define CF6         99
#define CF7         100
#define CF8         101
#define CF9         102
#define CF10        103

#define CA          1
#define CB          2
#define CC          3
#define CD          4
#define CE          5
#define CF          6
#define CG          7
#define CH          8
#define CI          9
#define CJ          10
#define CK          11
#define CL          12
#define CM          13
#define CN          14
#define CO          15
#define CP          16
#define CQ          17
#define CR          18
#define CS          19
#define CT          20
#define CU          21
#define CV          22
#define CW          23
#define CX          24
#define CY          25
#define CZ          26

#define AA  30
#define AB  48
#define AC  46
#define AD  32
#define AE  18
#define AF  33
#define AG  34
#define AH  35
#define AI  23
#define AJ  36
#define AK  37
#define AL  38
#define AM  50
#define AN  49
#define AO  24
#define AP  25
#define AQ  16
#define AR  19
#define AS  31
#define AT  20
#define AU  22
#define AV  47
#define AW  17
#define AX  45
#define AY  21
#define AZ  44

#define AR_A 1                                     /* Added */
#define AR_B 2                                     /* Added */
#define AR_C 4                                     /* Added */
#define AR_D 8                                     /* Added */
#define AR_E 16                                    /* Added */
#define AR_F 32                                    /* Added */
#define AR_G 64                                    /* Added */
#define AR_H 128                                   /* Added */
#define AR_I 256                                   /* Added */
#define AR_J 512                                   /* Added */
#define AR_K 1024                                  /* Added */
#define AR_L 2048                                  /* Added */
#define AR_M 4096                                  /* Added */
#define AR_N 8192                                  /* Added */
#define AR_O 16384                                 /* Added */
#define AR_P 32768                                 /* Added */

#define JUSTIFY_LEFT   0
#define JUSTIFY_RIGHT  1
#define JUSTIFY_CENTER 2

// For get_kb_event, decides if the number pad turns '8' into an arrow etc.. or not
#define NOTNUMBERS 1
#define NUMBERS    0

// Defines for listplus
#define LIST_DIR    0
#define SEARCH_ALL  1
#define NSCAN_DIR   2
#define NSCAN_NSCAN 3

#define ALL_DIRS    0
#define THIS_DIR    1
#define NSCAN_DIRS  2

// Defines for Q/Nscan Plus
#define QSCAN       0
#define NSCAN       1

#define INST_FORMAT_OLD     0
#define INST_FORMAT_WFC     1
#define INST_FORMAT_LIST    2

// Defines for UEDIT
#define UEDIT_NONE          0
#define UEDIT_FULLINFO      1
#define UEDIT_CLEARSCREEN   2


#define MAX_NUM_ACT 100

#define NUM_DOTS 5
#define FRAME_COLOR 7

//
// inmsg/external_edit flags
//
#define MSGED_FLAG_NONE             0
#define MSGED_FLAG_NO_TAGLINE       1
#define MSGED_FLAG_HAS_REPLY_NAME   2
#define MSGED_FLAG_HAS_REPLY_TITLE  4

#define INMSG_NOFSED            0
#define INMSG_FSED              1
#define INMSG_FSED_WORKSPACE    2

//
// Protocols
//
#define WWIV_INTERNAL_PROT_ASCII        1
#define WWIV_INTERNAL_PROT_XMODEM       2
#define WWIV_INTERNAL_PROT_XMODEMCRC    3
#define WWIV_INTERNAL_PROT_YMODEM       4
#define WWIV_INTERNAL_PROT_BATCH        5
#define WWIV_INTERNAL_PROT_ZMODEM       6
#define WWIV_NUM_INTERNAL_PROTOCOLS     7 // last protocol +1

#define MAX_ARCS 15
#define MAXMAIL 255

#ifdef _DEFINE_GLOBALS_
const char *DELIMS_NORMAL = " ;.!:-?,\t\r\n";
const char *DELIMS_WHITE  = " \t\r\n";
#else 
extern const char *DELIMS_NORMAL;
extern const char *DELIMS_WHITE;
#endif 

#define LIST_USERS_MESSAGE_AREA     0
#define LIST_USERS_FILE_AREA        1

enum COMPRESSIONS
{
    COMPRESSION_UNKNOWN,
    COMPRESSION_ARJ,
    COMPRESSION_ZIP,
    COMPRESSION_PAK,
    COMPRESSION_ZOO,
    COMPRESSION_LHA,
    COMPRESSION_RAR,
};

#endif // __INCLUDED_WCONSTANTS_H__
