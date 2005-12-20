/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2005, WWIV Software Services             */
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


//////////////////////////////////////////////////////////////////////////////
//
// Implementation
//
//
//


void old_sublist()
{
    char s[ 255 ];

    int oc = GetSession()->GetCurrentConferenceMessageArea();
    int os = usub[GetSession()->GetCurrentMessageArea()].subnum;

    bool abort = false;
    int sn = 0;
    int en = subconfnum - 1;
    if ( okconf( GetSession()->GetCurrentUser() ) )
    {
        if (uconfsub[1].confnum != -1)
        {
            GetSession()->bout.NewLine();
            GetSession()->bout << "|#2A)ll conferences, Q)uit, <space> for current conference: ";
            char ch = onek("Q A");
            GetSession()->bout.NewLine();
            switch (ch)
            {
            case ' ':
                sn = GetSession()->GetCurrentConferenceMessageArea();
                en = GetSession()->GetCurrentConferenceMessageArea();
                break;
            case 'Q':
                return;
            }
        }
    }
    else
    {
        oc = -1;
    }

    GetSession()->bout.NewLine();
    pla("|#9Sub-Conferences Available: ", &abort);
    GetSession()->bout.NewLine();
    int i = sn;
    while ( i <= en && uconfsub[i].confnum != -1 && !abort )
    {
        if ( uconfsub[1].confnum != -1 && okconf( GetSession()->GetCurrentUser() ) )
        {
            setuconf(CONF_SUBS, i, -1);
            sprintf( s, "|#1%s %c|#0:|#2 %s", "Conference",
                     subconfs[uconfsub[i].confnum].designator,
                     stripcolors( reinterpret_cast<char*>( subconfs[uconfsub[i].confnum].name ) ) );
            pla(s, &abort);
        }
        int i1 = 0;
        while ((i1 < GetSession()->num_subs) && (usub[i1].subnum != -1) && (!abort))
        {
            sprintf(s, "  |10%4.4s|#2", usub[i1].keys);
            if (qsc_q[usub[i1].subnum / 32] & (1L << (usub[i1].subnum % 32)))
            {
                strcat(s, " - ");
            }
            else
            {
                strcat(s, "  ");
            }
            if ( net_sysnum || GetSession()->GetMaxNetworkNumber() > 1 )
            {
                if ( xsubs[usub[i1].subnum].num_nets )
                {
					char *ss;
                    if ( xsubs[usub[i1].subnum].num_nets > 1 )
                    {
                        ss = "Gated";
                    }
                    else
                    {
                        ss = stripcolors(net_networks[xsubs[usub[i1].subnum].nets[0].net_num].name);
                    }

					char s1[80];
                    if (subboards[usub[i1].subnum].anony & anony_val_net)
                    {
                        sprintf(s1, "|B1|15[%-8.8s]|#9 ", ss);
                    }
                    else
                    {
                        sprintf(s1, "|B1|15<%-8.8s>|#9 ", ss);
                    }
                    strcat(s, s1);
                }
                else
                {
                    strcat(s, charstr( 11, ' ' ) );
                }
                strcat(s, "|#9");
            }
            strcat(s, stripcolors(subboards[usub[i1].subnum].name));
            pla(s, &abort);
            i1++;
        }
        i++;
        GetSession()->bout.NewLine();
        if ( !okconf( GetSession()->GetCurrentUser() ) )
        {
            break;
        }
    }

	if (i == 0)
    {
        pla("|#6None.", &abort);
        GetSession()->bout.NewLine();
    }

	if ( okconf( GetSession()->GetCurrentUser() ) )
    {
        setuconf(CONF_SUBS, oc, os);
    }
}


void SubList()
{
    int i, i1, i2,                        //loop variables
        ns,                               //number of subs
        p,
        firstp,                           //first sub on page
        lastp,                            //last sub on page
        wc,                               //color code
        oldConf,                          //old conference
        oldSub,                           //old sub
        sn,                               //current sub number
        en,
        msgIndex,                         //message Index
        newTally;                         //new message tally
    char ch, s[81], s2[10], s3[81], sdf[130], *ss;
    bool next;

    oldConf = GetSession()->GetCurrentConferenceMessageArea();
    oldSub = usub[GetSession()->GetCurrentMessageArea()].subnum;

    sn = lastp = firstp = 0;
    ns = GetSession()->GetCurrentConferenceMessageArea();
    en = subconfnum - 1;

    if ( okconf( GetSession()->GetCurrentUser() ) )
    {
        if (uconfsub[1].confnum != -1)
        {
            GetSession()->bout.NewLine();
            GetSession()->bout << "|#2A)ll conferences, Q)uit, <space> for current conference: ";
            ch = onek("Q A");
            GetSession()->bout.NewLine();
            switch (ch)
            {
            case ' ':
                sn = GetSession()->GetCurrentConferenceMessageArea();
                en = GetSession()->GetCurrentConferenceMessageArea();
                break;
            case 'Q':
                return;
            }
        }
    }
    else
    {
        oldConf = -1;
    }

    bool abort = false;
    bool done = false;
    do
    {
        p = 1;
        i = sn;
        i1 = 0;
        ns = GetSession()->GetCurrentConferenceMessageArea();
        while ( i <= en && uconfsub[i].confnum != -1 && !abort )
        {
            if ( uconfsub[1].confnum != -1 && okconf( GetSession()->GetCurrentUser() ) )
            {
                setuconf(CONF_SUBS, i, -1);
                i1 = 0;
            }
            while ( i1 < GetSession()->num_subs && usub[i1].subnum != -1 && !abort )
            {
                if (p)
                {
                    p = 0;
                    firstp = i1;
                    GetSession()->bout.ClearScreen();
                    if ( uconfsub[1].confnum != -1 && okconf( GetSession()->GetCurrentUser() ) )
                    {
                        sprintf( s, " [ %s %c ] [ %s ] ", "Conference",
                                 subconfs[uconfsub[i].confnum].designator,
                                 stripcolors( reinterpret_cast<char*>( subconfs[uconfsub[i].confnum].name ) ) );
                    }
                    else
                    {
                        sprintf(s, " [ %s Message Areas ] ", syscfg.systemname);
                    }
                    GetSession()->bout.DisplayLiteBar(s);
                    DisplayHorizontalBar( 78, 7 );
                    GetSession()->bout << "|#2 Sub   Scan   Net/Local   Sub Name                                 Old   New\r\n";
                    DisplayHorizontalBar( 78, 7 );
                }
                ++ns;
                sprintf(s, "    %-3.3s", usub[i1].keys);
                if (qsc_q[usub[i1].subnum / 32] & (1L << (usub[i1].subnum % 32)))
                {
                    strcpy(s2, "|10Yes");
                }
                else
                {
                    strcpy(s2, "|12No ");
                }
                iscan(i1);
                if ( net_sysnum || GetSession()->GetMaxNetworkNumber() > 1 )
                {
                    if (xsubs[usub[i1].subnum].num_nets)
                    {
                        if (xsubs[usub[i1].subnum].num_nets > 1)
                        {
                            wc = 6;
                            ss = "Gated";
                        }
                        else
                        {
                            strcpy(s3, net_networks[xsubs[usub[i1].subnum].nets[0].net_num].name);
                            ss = stripcolors(s3);
                            wc = GetSession()->GetNetworkNumber() % 8;
                        }
                        if (subboards[usub[i1].subnum].anony & anony_val_net)
                        {
                            sprintf(s3, "|#7[|#%i%-8.8s|#7]", wc, ss);
                        }
                        else
                        {
                            sprintf(s3, "|#7<|#%i%-8.8s|#7>", wc, ss);
                        }
                    }
                    else
                    {
                        strcpy(s3, " |#7>|#1LOCAL|#7<  ");
                    }
                }
                else
                {
                    strcpy(s3, "|#7>|#1LOCAL|#7<  ");
                }
                msgIndex = 1;
                while ((msgIndex <= GetSession()->GetNumMessagesInCurrentMessageArea()) && (get_post(msgIndex)->qscan <= qsc_p[usub[i1].subnum]))
                {
                    ++msgIndex;
                }
                newTally = GetSession()->GetNumMessagesInCurrentMessageArea() - msgIndex + 1;
                if (usub[GetSession()->GetCurrentMessageArea()].subnum == usub[i1].subnum)
                {
                    sprintf(sdf, " |#9%-3.3d |#9³ %3s |#9³ %6s |#9³ |B1|15%-36.36s |#9³ |#9%5ld |#9³ |#%c%5d |#9",
                    i1 + 1, s2, s3, subboards[usub[i1].subnum].name, GetSession()->GetNumMessagesInCurrentMessageArea(), newTally ? '6' : '3', newTally);
                }
                else
                {
                    sprintf(sdf, " |#9%-3.3d |#9³ %3s |#9³ %6s |#9³ |#1%-36.36s |#9³ |#9%5ld |#9³ |#%c%5d |#9",
                    i1 + 1, s2, s3, subboards[usub[i1].subnum].name, GetSession()->GetNumMessagesInCurrentMessageArea(), newTally ? '6' : '3', newTally);
                }
                if ( okansi() )
                {
                    osan(sdf, &abort, &next);
                }
                else
                {
                    osan(stripcolors(sdf), &abort, &next);
                }
                lastp = i1++;
                GetSession()->bout.NewLine();
                if (lines_listed >= GetSession()->screenlinest - 2)
                {
                    p = 1;
                    lines_listed = 0;
                    DisplayHorizontalBar( 78, 7 );
                    GetSession()->bout.WriteFormatted("|#1Select |#9[|#2%d-%d, [Enter]=Next Page, Q=Quit|#9]|#0 : ", firstp + 1, lastp + 1);
                    ss = mmkey( 0, true );
                    if ( isdigit( ss[0] ) )
                    {
                        for ( i2 = 0; i2 < GetSession()->num_subs; i2++ )
                        {
                            if ( wwiv::stringUtils::IsEquals( usub[i2].keys, ss ) )
                            {
                                GetSession()->SetCurrentMessageArea( i2 );
                                oldSub = usub[GetSession()->GetCurrentMessageArea()].subnum;
                                done = true;
                                abort = true;
                            }
                        }
                    }
                    else
                    {
                        switch (ss[0])
                        {
                        case 'Q':
							{
								if ( okconf( GetSession()->GetCurrentUser() ) )
                                {
									setuconf( CONF_SUBS, oldConf, oldSub );
                                }
								done = true;
								abort = true;
							}
                            break;
                        default:
                            GetSession()->bout.BackLine();
                            break;
                        }
                    }
                }
            }
            if (ns)
            {
                i++;
            }

            if (!abort)
            {
                p = 1;
                DisplayHorizontalBar( 78, 7 );
                if ( okconf( GetSession()->GetCurrentUser() ) )
                {
                    if (uconfsub[1].confnum != -1)
                    {
                        GetSession()->bout.WriteFormatted("|#1Select |#9[|#21-%d, J=Join Conference, ?=List Again, Q=Quit|#9]|#0 : ", ns);
                    }
                    else
                    {
                        GetSession()->bout.WriteFormatted("|#1Select |#9[|#21-%d, ?=List Again, Q=Quit|#9]|#0 : ", ns);
                    }
                }
                else
                {
                    GetSession()->bout.WriteFormatted("|#1Select |#9[|#21-%d, ?=List Again, Q=Quit|#9]|#0 : ", ns);
                }
                ss = mmkey( 0, true );

                if ( wwiv::stringUtils::IsEquals( ss, "?" ) )
                {
                    p = 1;
                    ns = i = i1 = 0;
                }

                if ( wwiv::stringUtils::IsEquals( ss, " " ) ||
                     wwiv::stringUtils::IsEquals( ss, "Q" ) ||
                     wwiv::stringUtils::IsEquals( ss, "\r" ) )
                {
                    GetSession()->bout.NewLine( 2 );
                    done = true;
                    if ( !okconf( GetSession()->GetCurrentUser() ) )
                    {
                        abort = true;
                    }
                }
                if ( wwiv::stringUtils::IsEquals( ss, "J" ) )
                {
                    if ( okconf( GetSession()->GetCurrentUser() ) )
                    {
                        jump_conf( CONF_SUBS );
                    }
                    sn = en = oldConf = GetSession()->GetCurrentConferenceMessageArea();
                    ns = i = 0;
                }
                if (isdigit(ss[0]))
                {
                    for (i2 = 0; i2 < GetSession()->num_subs; i2++)
                    {
                        if ( wwiv::stringUtils::IsEquals( usub[i2].keys, ss ) )
                        {
                            GetSession()->SetCurrentMessageArea( i2 );
                            oldSub = usub[GetSession()->GetCurrentMessageArea()].subnum;
                            done = true;
                            abort = true;
                        }
                    }
                }
            }
            else
            {
                if ( okconf( GetSession()->GetCurrentUser() ) )
                {
                    setuconf(CONF_SUBS, oldConf, oldSub);
                }
                done = true;
            }
        }
        if (i == 0)
        {
            pla("None.", &abort);
            GetSession()->bout.NewLine();
        }
    } while ( !hangup && !done );

    if ( okconf( GetSession()->GetCurrentUser() ) )
    {
        setuconf(CONF_SUBS, oldConf, oldSub);
    }
}


