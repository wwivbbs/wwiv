/****************************************************************************/
/*                                                                          */
/*                             WWIV Version 5.0x                            */
/*            Copyright (C) 1998-2003 by WWIV Software Services             */
/*                                                                          */
/*      Distribution or publication of this source code, it's individual    */
/*       components, or a compiled version thereof, whether modified or     */
/*        unmodified, without PRIOR, WRITTEN APPROVAL of WWIV Software      */
/*        Services is expressly prohibited.  Distribution of compiled       */
/*            versions of WWIV is restricted to copies compiled by          */
/*           WWIV Software Services.  Violators will be procecuted!         */
/*                                                                          */
/****************************************************************************/
#include "../../wwiv.h"

// %%TODO: Move this code into platform/OS2/WLocalIO.cpp as MakeLocalWindow(...)

/*
 * Sets screen attribute at screen pos x,y to attribute contained in a.
 */

void set_attr_xy(int x, int y, int a)
{
  BYTE Attrib = a;
  VioWrtNAttr(&Attrib, 1, y + topline, x, 0);
}

/*
 * WWIV_MakeLocalWindow makes a "shadowized" window with the upper-left hand corner at
 * (x,y), and the lower-right corner at (x+xlen,y+ylen).
 */

void WWIV_MakeLocalWindow(int x, int y, int xlen, int ylen)
{
    int xx;
    int yy;
    int i;

    // Make sure that we are within the range of {(0,0), (80,screenbottom)}
    if (xlen > 80)
    {
        xlen = 80;
    }
    if (ylen > (screenbottom + 1 - topline))
    {
        ylen = (screenbottom + 1 - topline);
    }
    if ((x + xlen) > 80)
    {
        x = 80 - xlen;
    }
    if ((y + ylen) > screenbottom + 1)
    {
        y = screenbottom + 1 - ylen;
    }
    
    xx = GetApplication()->GetLocalIO()->WhereX();
    yy = GetApplication()->GetLocalIO()->WhereY();

    unsigned char s[81];
    
    for (i = 1; i < xlen - 1; i++)
    {
        s[i] = 196; // Ä
    }
    s[0] = 218;
    s[xlen - 1] = 191;  // ¿
    s[xlen] = 0;
    GetApplication()->GetLocalIO()->LocalGotoXY(x, y);
    GetApplication()->GetLocalIO()->LocalFastPuts((char*) s);
    s[0] = 192; // À
    s[xlen - 1] = 217;  // Ù
    GetApplication()->GetLocalIO()->LocalGotoXY(x, y + ylen - 1);
    GetApplication()->GetLocalIO()->LocalFastPuts((char*) s);
    for (i = 1; i < xlen - 1; i++)
    {
        s[i] = 32;
    }
    s[0] = 179; // ³
    s[xlen - 1] = 179;  // ³
    for (i = 1; i < ylen - 1; i++) 
    {
        GetApplication()->GetLocalIO()->LocalGotoXY(x, i + y);
        GetApplication()->GetLocalIO()->LocalFastPuts((char*) s);
    }

    //
    // Draw shadow around boxed window
    //

    for (i = 0; i < xlen; i++)
    {
        set_attr_xy(x + 1 + i, y + ylen, 0x08);
    }
    
    for (i = 0; i < ylen; i++)
    {
        set_attr_xy(x + xlen, y + 1 + i, 0x08);
    }
    
    GetApplication()->GetLocalIO()->LocalGotoXY(xx, yy);


}


