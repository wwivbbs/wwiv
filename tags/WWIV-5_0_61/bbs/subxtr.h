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

#ifndef __INCLUDED_SUBXTR_H__
#define __INCLUDED_SUBXTR_H__


/*
 * Info for each network the sub is on.
 *  flags - bitmask
 *  sess->m_nNetworkNumber - index into networks.dat
 *  type - numeric sub type = atoi(stype)
 *  host - host system of sub, or 0 if locally hosted
 *  stype - string sub type (up to 7 chars)
 */
struct xtrasubsnetrec
{
    long flags;
    short net_num;
    unsigned short type;
    short host;
    short category;
    char stype[8];
};


#define XTRA_NET_AUTO_ADDDROP 0x00000001    /* can auto add-drop the sub */
#define XTRA_NET_AUTO_INFO    0x00000002    /* sends subs.lst info for sub */


/*
 * Extended info for each sub, relating to network.
 *  flags - bitmask
 *  desc - long description, for auto subs.lst info
 *  num_nets - # records in "nets" field
 *  nets - pointer to network info for sub
 */
struct xtrasubsrec
{
    long flags;
    char desc[61];
    short num_nets;
    short num_nets_max;
    xtrasubsnetrec *nets;
};

#define XTRA_MALLOCED         0x80000000    /* "nets" is malloced */

#define XTRA_MASK             (~(XTRA_MALLOCED))


#ifdef _DEFINE_GLOBALS_
xtrasubsrec *xsubs;
#else
extern xtrasubsrec *xsubs;
#endif


#endif // __INCLUDED_SUBXTR_H__

