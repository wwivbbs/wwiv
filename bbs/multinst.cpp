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



//
// Local funciton prototypes
//

void GetInstanceActivityString( instancerec &ir, char *pszOutActivity );



/*
 * Builds a string (in pszOutInstanceString) like:
 *
 * Instance   1: Offline
 *     LastUser: Sysop #1
 *
 * or
 *
 * Instance  22: Network transmission
 *     CurrUser: Sysop #1
 */
void make_inst_str( int nInstanceNum, char *pszOutInstanceString, int nInstanceFormat )
{
    char s[255];
    sprintf( s, "|#1Instance %-3d: |#2", nInstanceNum );

    instancerec ir;
    get_inst_info(nInstanceNum, &ir);

    char szNodeActivity[81];
    GetInstanceActivityString( ir, szNodeActivity );

    switch ( nInstanceFormat )
    {
        case INST_FORMAT_WFC:
            strcpy( pszOutInstanceString, szNodeActivity);                // WFC Addition
            break;
        case INST_FORMAT_OLD:
            strncat(s, szNodeActivity, sizeof(s));
            strncat(s, "\r\n|#1", sizeof(s));

            if (ir.flags & INST_FLAGS_ONLINE)
            {
                strncat(s, "   CurrUser ", sizeof(s));
            }
            else
            {
                strncat(s, "   LastUser ", sizeof(s));
            }

            strncat(s, ": |#2", sizeof(s));

            if ( ir.user < syscfg.maxusers && ir.user > 0 )
            {
                WUser userRecord;
                app->userManager->ReadUser( &userRecord, ir.user );
                sprintf(szNodeActivity, "%-30.30s", userRecord.GetUserNameAndNumber( ir.user ) );
                strncat( s, szNodeActivity, sizeof( s ) );
            }
            else
            {
                sprintf( szNodeActivity, "%-30.30s", "Nobody" );
                strncat( s, szNodeActivity, sizeof( s ) );
            }

            strcpy( pszOutInstanceString, s );
            break;
        case INST_FORMAT_LIST:
            {
                std::string userName;
                if ( ir.user < syscfg.maxusers && ir.user > 0 )
                {
                    WUser user;
                    app->userManager->ReadUser( &user, ir.user );
                    if (ir.flags & INST_FLAGS_ONLINE)
                    {
                        userName = user.GetUserNameAndNumber( ir.user );
                    }
                    else
                    {
                        userName = "Last: ";
                        userName += user.GetUserNameAndNumber( ir.user );
                    }
                }
                else
                {
                    userName = "(Nobody)";
                }
                char szBuffer[ 255 ];
                sprintf( szBuffer, "|#5%-4d |#2%-35.35s |#1%-37.37s", nInstanceNum, userName.c_str(), szNodeActivity );
                strcpy( pszOutInstanceString, szBuffer );
            }
            break;
        default:
            sprintf( pszOutInstanceString, "** INVALID INSTANCE FORMAT PASSED [%d] **", nInstanceFormat );
            break;
    }
}


void multi_instance()
{

    nl();
    int nNumInstances = num_instances();
    if (nNumInstances < 1)
    {
        sess->bout << "|#6Couldn't find instance data file.\r\n";
        return;
    }

    bprintf( "|#5Node |#1%-35.35s |#2%-37.37s\r\n", "User Name", "Activity" );
    char s1[81], s2[81], s3[81];
    strcpy( s1, charstr(4, '=') );
    strcpy( s2, charstr(35, '=') );
    strcpy( s3, charstr( 37, '=' ) );
    bprintf( "|#7%-4.4s %-35.35s %-37.37s\r\n", s1, s2, s3 );

    for (int nInstance = 1; nInstance <= nNumInstances; nInstance++)
    {
        char szBuffer[255];
        make_inst_str(nInstance, szBuffer, INST_FORMAT_LIST );
        sess->bout << szBuffer;
		nl();
    }
}


int inst_ok( int loc, int subloc )
{
    instancerec instance_temp;

    if (loc == INST_LOC_FSED)
    {
        return 0;
    }

    int nInstNum = 0;
    WFile instFile(  syscfg.datadir, INSTANCE_DAT );
    if ( !instFile.Open( WFile::modeReadOnly | WFile::modeBinary ) )
    {
        return 0;
    }
    int nNumInstances = static_cast<int> ( instFile.GetLength() / sizeof( instancerec ) );
    instFile.Close();
    for (int nInstance = 1; nInstance < nNumInstances; nInstance++)
    {
        if ( instFile.Open( WFile::modeReadOnly | WFile::modeBinary ) )
        {
            instFile.Seek( nInstance * sizeof( instancerec ), WFile::seekBegin );
            instFile.Read( &instance_temp, sizeof( instancerec ) );
            instFile.Close();
            if ( instance_temp.loc == loc &&
                 instance_temp.subloc == subloc &&
                 instance_temp.number != app->GetInstanceNumber() )
            {
                nInstNum = instance_temp.number;
            }
        }
    }
    return nInstNum;
}


void GetInstanceActivityString( instancerec &ir, char *pszOutActivity )
{
    char szNodeActivity[81];

    if ( ir.loc >= INST_LOC_CH1 && ir.loc <= INST_LOC_CH10 )
    {
        sprintf(szNodeActivity,"%s","WWIV Chatroom");
    }
    else switch (ir.loc)
    {
    case INST_LOC_DOWN:
        sprintf(szNodeActivity, "%s", "Offline");
        break;
    case INST_LOC_INIT:
        sprintf(szNodeActivity, "%s", "Initializing BBS");
        break;
    case INST_LOC_EMAIL:
        sprintf(szNodeActivity, "%s", "Sending Email");
        break;
    case INST_LOC_MAIN:
        sprintf(szNodeActivity, "%s", "Main Menu");
        break;
    case INST_LOC_XFER:
        sprintf(szNodeActivity, "%s", "Transfer Area");
        if ( so() && ir.subloc < sess->num_dirs )
        {
            char szTemp2[ 100 ];
            sprintf(szTemp2, "%s: %s", "Dir ", stripcolors(directories[ir.subloc].name));
            strncat(szNodeActivity, szTemp2, sizeof(szNodeActivity));
        }
        break;
    case INST_LOC_CHAINS:
        sprintf( szNodeActivity, "%s", "Chains" );
        if ( ir.subloc > 0 && ir.subloc <= sess->GetNumberOfChains() )
        {
            char szTemp2[ 100 ];
            sprintf(szTemp2, "Door: %s", stripcolors(chains[ir.subloc - 1].description));
            strncat(szNodeActivity, szTemp2, sizeof(szNodeActivity));
        }
        break;
    case INST_LOC_NET:
        sprintf(szNodeActivity, "%s", "Network Transmission");
        break;
    case INST_LOC_GFILES:
        sprintf(szNodeActivity, "%s", "GFiles");
        break;
    case INST_LOC_BEGINDAY:
        sprintf(szNodeActivity, "%s", "Running BeginDay");
        break;
    case INST_LOC_EVENT:
        sprintf(szNodeActivity, "%s", "Executing Event");
        break;
    case INST_LOC_CHAT:
        sprintf(szNodeActivity, "%s", "Normal Chat");
        break;
    case INST_LOC_CHAT2:
        sprintf(szNodeActivity, "%s", "SplitScreen Chat");
        break;
    case INST_LOC_CHATROOM:
        sprintf(szNodeActivity, "%s", "ChatRoom");
        break;
    case INST_LOC_LOGON:
        sprintf(szNodeActivity, "%s", "Logging On");
        break;
    case INST_LOC_LOGOFF:
        sprintf(szNodeActivity, "%s", "Logging off");
        break;
    case INST_LOC_FSED:
        sprintf(szNodeActivity, "%s", "FullScreen Editor");
        break;
    case INST_LOC_UEDIT:
        sprintf(szNodeActivity, "%s", "In UEDIT");
        break;
    case INST_LOC_CHAINEDIT:
        sprintf(szNodeActivity, "%s", "In CHAINEDIT");
        break;
    case INST_LOC_BOARDEDIT:
        sprintf(szNodeActivity, "%s", "In BOARDEDIT");
        break;
    case INST_LOC_DIREDIT:
        sprintf(szNodeActivity, "%s", "In DIREDIT");
        break;
    case INST_LOC_GFILEEDIT:
        sprintf(szNodeActivity, "%s", "In GFILEEDIT");
        break;
    case INST_LOC_CONFEDIT:
        sprintf(szNodeActivity, "%s", "In CONFEDIT");
        break;
    case INST_LOC_DOS:
        sprintf(szNodeActivity, "%s", "In DOS");
        break;
    case INST_LOC_DEFAULTS:
        sprintf(szNodeActivity, "%s", "In Defaults");
        break;
    case INST_LOC_REBOOT:
        sprintf(szNodeActivity, "%s", "Rebooting");
        break;
    case INST_LOC_RELOAD:
        sprintf(szNodeActivity, "%s", "Reloading BBS data");
        break;
    case INST_LOC_VOTE:
        sprintf(szNodeActivity, "%s", "Voting");
        break;
    case INST_LOC_BANK:
        sprintf(szNodeActivity, "%s", "In TimeBank");
        break;
    case INST_LOC_AMSG:
        sprintf(szNodeActivity, "%s", "AutoMessage");
        break;
    case INST_LOC_SUBS:
        sprintf(szNodeActivity, "%s", "Reading Messages");
        if ( so() && ir.subloc < sess->num_subs )
        {
            char szTemp2[ 100 ];
            sprintf(szTemp2, "(Sub: %s)", stripcolors(subboards[ir.subloc].name));
            strncat(szNodeActivity, szTemp2, sizeof(szNodeActivity));
        }
        break;
    case INST_LOC_CHUSER:
        sprintf(szNodeActivity, "%s", "Changing User");
        break;
    case INST_LOC_TEDIT:
        sprintf(szNodeActivity, "%s", "In TEDIT");
        break;
    case INST_LOC_MAILR:
        sprintf(szNodeActivity, "%s", "Reading All Mail");
        break;
    case INST_LOC_RESETQSCAN:
        sprintf(szNodeActivity, "%s", "Resetting QSCAN pointers");
        break;
    case INST_LOC_VOTEEDIT:
        sprintf(szNodeActivity, "%s", "In VOTEEDIT");
        break;
    case INST_LOC_VOTEPRINT:
        sprintf(szNodeActivity, "%s", "Printing Voting Data");
        break;
    case INST_LOC_RESETF:
        sprintf(szNodeActivity, "%s", "Resetting NAMES.LST");
        break;
    case INST_LOC_FEEDBACK:
        sprintf(szNodeActivity, "%s", "Leaving Feedback");
        break;
    case INST_LOC_KILLEMAIL:
        sprintf(szNodeActivity, "%s", "Viewing Old Email");
        break;
    case INST_LOC_POST:
        sprintf(szNodeActivity, "%s", "Posting a Message");
        if ( so() && ir.subloc < sess->num_subs )
        {
            char szTemp2[ 100 ];
            sprintf(szTemp2, "(Sub: %s)", stripcolors( subboards[ir.subloc].name ) );
            strncat(szNodeActivity, szTemp2, sizeof(szNodeActivity));
        }
        break;
    case INST_LOC_NEWUSER:
        sprintf(szNodeActivity, "%s", "Registering a Newuser");
        break;
    case INST_LOC_RMAIL:
        sprintf(szNodeActivity, "%s", "Reading Email");
        break;
    case INST_LOC_DOWNLOAD:
        sprintf(szNodeActivity, "%s", "Downloading");
        break;
    case INST_LOC_UPLOAD:
        sprintf(szNodeActivity, "%s", "Uploading");
        break;
    case INST_LOC_BIXFER:
        sprintf(szNodeActivity, "%s", "Bi-directional Transfer");
        break;
    case INST_LOC_NETLIST:
        sprintf(szNodeActivity, "%s", "Listing Net Info");
        break;
    case INST_LOC_TERM:
        sprintf(szNodeActivity, "%s", "In a terminal program");
        break;
    case INST_LOC_GETUSER:
        sprintf(szNodeActivity, "%s", "Getting User ID");
        break;
    case INST_LOC_WFC:
        sprintf(szNodeActivity, "%s", "Waiting for Call");
        break;
    default:
        sprintf(szNodeActivity, "%s", "Unknown BBS Location!");
        break;
    }
    strcpy( pszOutActivity, szNodeActivity );
}
