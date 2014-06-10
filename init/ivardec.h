/**************************************************************************/
/*                                                                        */
/*                 WWIV Initialization Utility Version 5.0                */
/*                Copyright (C)2014, WWIV Software Services               */
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
#ifndef __INCLUDED_IVARDEC_H__
#define __INCLUDED_IVARDEC_H__

// Data
#pragma pack(push, 1)
typedef struct {
  short int frm_state;
  short int to_state;
  char *type;
  char *send;
  char *recv;
} autosel_data;

struct externalrec {
  char description[81],                     /* protocol description */
   receivefn[81],                           /* receive filename */
   sendfn[81];                              /* send filename */

  unsigned short ok1, ok2,                  /* if sent */
   nok1, nok2;                              /* if not sent */
};

struct resultrec {
  char curspeed[23];                        /* description of speed */

  char return_code[23];                     /* modem result code */

  unsigned short modem_speed,               /* speed modems talk at */
   com_speed;                               /* speed com port runs at */
};

struct initinfo_rec {
    int numexterns;
    int numeditors;
    int num_languages;
    int net_num_max;
    int num_subs;
    int usernum;
    int nNumMsgsInCurrentSub;
};
#pragma pack(pop)


#endif // __INCLUDED_IVARDEC_H__
