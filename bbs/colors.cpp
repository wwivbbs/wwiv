/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2004, WWIV Software Services             */
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

#include "wwiv.h"


void get_colordata()
{
    WFile file( syscfg.datadir, COLOR_DAT );
    if ( !file.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile ) )
    {
        return;
    }
    file.Read( &rescolor, sizeof( colorrec ) );
}


void save_colordata()
{
    WFile file( syscfg.datadir, COLOR_DAT );
    if ( !file.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile ) )
    {
        return;
    }
    file.Write( &rescolor, sizeof( colorrec ) );
}

#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4125 )
#endif

void list_ext_colors()
{
    goxy(40, 7);
    GetSession()->bout << "|#7ÚÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ¿";
    goxy(40, 8);
    GetSession()->bout << "|#7³        |#2COLOR LIST|#7       ³";
    goxy(40, 9);
    GetSession()->bout << "|#7ÃÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ´";
    goxy(40, 10);
    GetSession()->bout << "|#7³ .  \003 \003!!\0030 \003\"\"\0030 \003##\0030 \003$$\0030 \003%%\0030 \003&&\0030" <<
        " \003''\0030 \003((\0030 \003))\0030 \003**\0030 \003++\0037 ³";
    goxy(40, 11);
    GetSession()->bout << "|#7³ \003,,\0030 \003--\0030 \003..\0030 \003//\0030 0 \00311\0030 \00322\0030" <<
        " \00333\0030 \00344\0030 \00355\0030 \00366\0030 \00377 ³";
    goxy(40, 12);
    GetSession()->bout << "|#7³ \00388\0030 \00399\0030 \003::\0030 \003;;\0030 \003<<\0030 \003==\0030 \003>>\0030" <<
        " \003??\0030 \003@@\0030 \003AA\0030 \003BB\0030 \003CC\0037 ³";
    goxy(40, 13);
    GetSession()->bout << "|#7³ \003DD\0030 \003EE\0030 \003FF\0030 \003GG\0030 \003HH\0030 \003II\0030 \003JJ\0030" <<
        " \003KK\0030 \003LL\0030 \003MM\0030 \003NN\0030 \003OO\0037 ³";
    goxy(40, 14);
    GetSession()->bout << "|#7³ \003PP\0030 \003QQ\0030 \003RR\0030 \003SS\0030 \003TT\0030 \003UU\0030 \003VV\0030" <<
        " \003WW\0030 \003XX\0030 \003YY\0030 \003ZZ\0030 \003[[\0037 ³";
    goxy(40, 15);
    GetSession()->bout << "|#7³ \003\\\\\0030 \003]]\0030 \003^^\0030 \003__\0030 \003``\0030 \003aa\0030 \003bb\0030" <<
        " \003cc\0030 \003dd\0030 \003ee\0030 \003ff\0030 \003gg\0037 ³";
    goxy(40, 16);
    GetSession()->bout << "|#7³ \003hh\0030 \003ii\0030 \003jj\0030 \003kk\0030 \003ll\0030 \003mm\0030 \003nn\0030" <<
        " \003oo\0030 \003pp\0030 \003qq\0030 \003rr\0030 \003ss\0037 ³";
    goxy(40, 17);
    GetSession()->bout << "|#7³ \003tt\0030 \003uu\0030 \003vv\0030 \003ww\0030 \003xx\0030 \003yy\0030 \003zz\0030" <<
        " \003{{\0030 \003||\0030 \003}}\0030 \003~~\0030 \003[[\0037 ³";
    goxy(40, 18);
    GetSession()->bout << "|#7ÃÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ´";
    goxy(40, 19);
    GetSession()->bout << "|#7³                         ³";
    goxy(40, 20);
    GetSession()->bout << "|#7ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÙ";
}

#if defined( _MSC_VER )
#pragma warning( pop )
#endif // _MSC_VER


void color_config()
{
    char s[81], ch, ch1;
    unsigned char c;
    int done = 0, n;

    strcpy(s, "      SAMPLE COLOR      ");
    get_colordata();
    do
    {
        bputch( CL );
		GetSession()->bout << "Extended Color Configuration - Enter Choice, ^Z to Quit, ^R to Relist\r\n:";
        list_ext_colors();
        goxy(2, 2);
        ch = getkey();
        if (ch == CZ)
        {
            done = 1;
        }
        if (ch == CR)
        {
            list_ext_colors();
            goxy(2, 2);
        }
        if (ch >= 32)
        {
            bputch(ch);
            n = ch - 48;
            nl( 2 );
            color_list();
            ansic( 0 );
            nl();
            GetSession()->bout << "|#2Foreground? ";
            ch1 = onek("01234567");
            c = static_cast< unsigned char > ( ch1 - '0' );
            goxy(41, 19);
            setc( c );
            GetSession()->bout << s;
            ansic( 0 );
            goxy(1, 16);
            GetSession()->bout << "|#2Background? ";
            ch1 = onek("01234567");
            c = static_cast< unsigned char > ( c | ((ch1 - '0') << 4) );
            goxy(41, 19);
            setc( c );
            GetSession()->bout << s;
            ansic( 0 );
            goxy(1, 17);
            nl();
            GetSession()->bout << "|#5Intensified? ";
            if (yesno())
            {
                c |= 0x08;
            }
            goxy( 41, 19 );
            setc( c );
            GetSession()->bout << s;
            ansic( 0 );
            goxy( 1, 18 );
            nl();
            GetSession()->bout << "|#5Blinking? ";
            if ( yesno() )
            {
                c |= 0x80;
            }
            nl();
            goxy(41, 19);
            setc( c );
            GetSession()->bout << s;
            ansic( 0 );
            goxy(1, 21);
            setc( c );
            GetSession()->bout << DescribeColorCode( c );
            ansic( 0 );
            nl( 2 );
            GetSession()->bout << "|#5Is this OK? ";
            if (yesno())
            {
                GetSession()->bout << "\r\nColor saved.\r\n\n";
                if ((n <= -1) && (n >= -16))
                {
                    rescolor.resx[207 + abs(n)] = static_cast< unsigned char > ( c );
                }
                else if ((n >= 0) && (n <= 9))
                {
                    GetSession()->thisuser.SetColor( n, c );
                }
                else
                {
                    rescolor.resx[n - 10] = static_cast< unsigned char > ( c );
                }
            }
            else
            {
                GetSession()->bout << "\r\nNot saved, then.\r\n\n";
            }
        }
    } while ( !done && !hangup );
    nl( 2 );
    GetSession()->bout << "|#5Save changes? ";
    if ( yesno() )
    {
        save_colordata();
    }
    else
    {
        get_colordata();
    }
    bputch( CL );
    nl( 3 );
}


void buildcolorfile()
{
    for (int i = 0; i < 240; i++)
    {
        rescolor.resx[i] = static_cast< unsigned char > ( i + 1 );
    }

    nl();
    save_colordata();
}



