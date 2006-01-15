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

#include "wwiv.h"


void gfl_hdr(int which);
void list_sec(int *map, int nmap);
void list_gfiles(gfilerec * g, int nf, int sn);
void gfile_sec(int sn);
void gfiles2();
void gfiles3(int n);


char *get_file( const char *pszFileName, long *len )
{
    WFile file( pszFileName );
    if ( !file.Open( WFile::modeBinary | WFile::modeReadOnly ) )
    {
        *len = 0L;
        return NULL;
    }

    long lFileSize = file.GetLength();
    char* pszFileText = static_cast< char *>( BbsAllocA( lFileSize + 50 ) );
    if ( pszFileText == NULL )
    {
        *len = 0L;
        return NULL;
    }
    *len = static_cast< long >( file.Read( pszFileText, lFileSize ) );
    return pszFileText;
}


gfilerec *read_sec(int sn, int *nf)
{
    gfilerec *pRecord;

    int nSectionSize = sizeof( gfilerec ) * gfilesec[sn].maxfiles;
    if ( ( pRecord = static_cast< gfilerec *>( BbsAllocA(  nSectionSize ) ) ) == NULL )
    {
        *nf = 0;
        return NULL;
    }

    char szFileName[ MAX_PATH ];
    sprintf( szFileName, "%s%s.gfl", syscfg.datadir, gfilesec[sn].filename );

    WFile file( szFileName );
    if ( !file.Open( WFile::modeBinary | WFile::modeReadOnly ) )
    {
        *nf = 0;
    }
    else
    {
        *nf = file.Read( pRecord, nSectionSize ) / sizeof( gfilerec );
    }
    return pRecord;
}


void gfl_hdr( int which )
{
    char s[255], s1[81], s2[81], s3[81];

    if ( okansi() )
    {
        strcpy( s2, charstr( 29, 'Ä' ) );
    }
    else
    {
        strcpy( s2, charstr( 29, '-' ) );
    }
    if ( which )
    {
        strcpy( s1, charstr( 12, ' ' ) );
        strcpy( s3, charstr( 11, ' ' ) );
    }
    else
    {
        strcpy( s1, charstr( 12, ' ' ) );
        strcpy( s3, charstr( 17, ' ' ) );
    }
    bool abort = false;
    if ( okansi() )
    {
        if ( which )
        {
            sprintf( s, "|#7ÉÄÄÄË%sËÄÄÄÄËÄÄÄË%sËÄÄÄÄ»", s2, s2 );
        }
        else
        {
            sprintf( s, "|#7ÉÄÄÄË%sÄÄÄÄÄËÄÄÄË%sÄÄÄÄ»", s2, s2 );
        }
    }
    else
    {
        if ( which )
        {
            sprintf( s, "+---+%s+----+---+%s+----+", s2, s2 );
        }
        else
        {
            sprintf(s, "+---+%s-----+---+%s----+", s2, s2);
        }
    }
    pla( s, &abort );
    GetSession()->bout.Color( 0 );
    if ( okansi() )
    {
        if ( which )
        {
            sprintf( s, "|#7³|#2 # |#7³%s|#1 Name %s|#7³|#9Size|#7³|#2 # |#7³%s|#1 Name %s|#7³|#9Size|#7³",
                     s1, s3, s1, s3 );
        }
        else
        {
            sprintf( s, "|#7³|#2 # |#7³%s|#1 Name%s|#7³|#2 # |#7³%s|#1Name%s|#7³", s1, s3, s1, s3 );
        }
    }
    else
    {
        if ( which )
        {
            sprintf( s, "| # |%sName %s|Size| # |%s Name%s|Size|", s1, s1, s1, s1 );
        }
        else
        {
            sprintf( s, "| # |%s Name     %s| # |%s Name    %s|", s1, s1, s1, s1 );
        }
    }
    pla( s, &abort );
    GetSession()->bout.Color( 0 );
    if ( okansi() )
    {
        if ( which )
        {
            sprintf( s, "|#7ÌÄÄÄÎ%sÎÄÄÄÄÎÄÄÄÎ%sÎÄÄÄÄ¹", s2, s2 );
        }
        else
        {
            sprintf( s, "|#7ÌÄÄÄÎ%sÄÄÄÄÄÎÄÄÄÎ%sÄÄÄÄ¹", s2, s2 );
        }
    }
    else
    {
        if ( which )
        {
            sprintf( s, "+---+%s+----+---+%s+----+", s2, s2 );
        }
        else
        {
            sprintf( s, "+---+%s-----+---+%s----+", s2, s2 );
        }
    }
    pla( s, &abort );
    GetSession()->bout.Color( 0 );
}


void list_sec( int *map, int nmap )
{
    char s[255], s1[255], s2[81], s3[81], s4[81], s5[81], s7[81];
    char lnum[5], rnum[5];

    int i2 = 0;
    bool abort = false;
    if ( okansi() )
    {
        strcpy(s2, charstr(29, 'Ä'));
        strcpy(s3, charstr(12, 'Ä'));
        strcpy(s7, charstr(12, 'Ä'));
    }
    else
    {
        strcpy(s2, charstr(29, '-'));
        strcpy(s3, charstr(12, '-'));
        strcpy(s7, charstr(12, '-'));
    }

    GetSession()->bout.DisplayLiteBar( " [ %s G-Files Section ] ", syscfg.systemname);
    gfl_hdr( 0 );
    for (int i = 0; i < nmap && !abort && !hangup; i++)
    {
        sprintf(lnum, "%d", i + 1);
        strncpy(s4, gfilesec[map[i]].name, 34);
        s4[34] = '\0';
        if (i + 1 >= nmap)
        {
            if ( okansi() )
            {
                sprintf(rnum, "%s", charstr(3, '\xFE'));
                sprintf(s5, "%s", charstr(29, '\xFE'));
            }
            else
            {
                sprintf(rnum, "%s", charstr(3, 'o'));
                sprintf(s5, "%s", charstr(29, 'o'));
            }
        }
        else
        {
            sprintf(rnum, "%d", i + 2);
            strncpy(s5, gfilesec[map[i + 1]].name, 29);
            s5[29] = '\0';
        }
        if ( okansi() )
        {
            sprintf(s, "|#7³|#2%3s|#7³|#1%-34s|#7³|#2%3s|#7³|#1%-33s|#7³", lnum, s4, rnum, s5);
        }
        else
        {
            sprintf(s, "|%3s|%-34s|%3s|%-33s|", lnum, s4, rnum, s5);
        }
        pla(s, &abort);
        GetSession()->bout.Color( 0 );
        i++;
        if (i2 > 10)
        {
            i2 = 0;
            if ( okansi() )
            {
                sprintf(s1, "|#7ÈÄÄÄÊ%sÄÄÄÄÄÊÄÄÄÊÄÄÄÄÄÄ%s|#1þ|#7Ä|#2%s|#7Ä|#2þ|#7ÄÄÄ¼",
                    s2, s3, times());
            }
            else
            {
                sprintf(s1, "+---+%s-----+--------+%s-o-%s-o---+",
                    s2, s3, times());
            }
            pla(s1, &abort);
            GetSession()->bout.Color( 0 );
            GetSession()->bout.NewLine();
            pausescr();
            gfl_hdr( 1 );
        }
    }
    if (!abort)
    {
        if (so())
        {
            if ( okansi() )
            {
                sprintf(s1, "|#7ÌÄÄÄÊ%sÄÄÄÄÄÊÄÄÄÊ%sÄÄÄÄ¹", s2, s2);
            }
            else
            {
                sprintf(s1, "+---+%s-----+---+%s----+", s2, s2);
            }
            pla(s1, &abort);
            GetSession()->bout.Color( 0 );

            if ( okansi() )
            {
                sprintf(s1, "|#7³  |#2G|#7)|#1G-File Edit%s|#7³", charstr(61, ' '));
            }
            else
            {
                sprintf(s1, "|  G)G-File Edit%s|", charstr(61, ' '));
            }
            pla(s1, &abort);
            GetSession()->bout.Color( 0 );
            if ( okansi() )
            {
                sprintf(s1, "|#7ÈÄÄÄÄ%sÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ%s|#1þ|#7Ä|#2%s|#7Ä|#1þ|#7ÄÄÄ¼", s2, s7, times());
            }
            else
            {
                sprintf(s1, "+----%s----------------%so-%s-o---+", s2, s7, times());
            }
            pla(s1, &abort);
            GetSession()->bout.Color( 0 );
        }
        else
        {
            if ( okansi() )
            {
                sprintf(s1, "|#7ÈÄÄÄÊ%sÄÄÄÄÄÊÄÄÄÊÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ%s|#1þ|#7Ä|#2%s|#7Ä|#1þ|#7ÄÄÄ¼",
                    s2, s3, times());
            }
            else
            {
                sprintf(s1, "+---+%s-----+---------------------+%so-%s-o---+", s2, s3, times());
            }
            pla(s1, &abort);
            GetSession()->bout.Color( 0 );
        }
    }
    GetSession()->bout.Color( 0 );
    GetSession()->bout.NewLine();
}


void list_gfiles(gfilerec * g, int nf, int sn)
{
    int i, i2;
    char s[255], s1[255], s2[81], s3[81], s4[30], s5[30];
    char lnum[5], rnum[5], lsize[5], rsize[5], path_name[255];

    bool abort = false;
    GetSession()->bout.DisplayLiteBar(" [ %s] ", gfilesec[sn].name);
    i2 = 0;
    if ( okansi() )
    {
        strcpy(s2, charstr(29, 'Ä'));
        strcpy(s3, charstr(12, 'Ä'));
    }
    else
    {
        strcpy(s2, charstr(29, '-'));
        strcpy(s3, charstr(12, '-'));
    }
    gfl_hdr( 1 );
    for (i = 0; i < nf && !abort && !hangup; i++)
    {
        i2++;
        sprintf(lnum, "%d", i + 1);
        strncpy(s4, g[i].description, 29);
        s4[29] = '\0';
        sprintf( path_name, "%s%s%c%s", syscfg.gfilesdir, gfilesec[sn].filename, WWIV_FILE_SEPERATOR_CHAR, g[i].filename );
        if ( WFile::Exists( path_name ) )
        {
			WFile handle( path_name );
            sprintf( lsize, "%ld""k", bytes_to_k( handle.GetLength() ) );
        }
        else
        {
            sprintf( lsize, "OFL" );
        }
        if ( i + 1 >= nf )
        {
            if ( okansi() )
            {
                sprintf(rnum, "%s", charstr(3, '\xFE'));
                sprintf(s5, "%s", charstr(29, '\xFE'));
                sprintf(rsize, "%s", charstr(4, '\xFE'));
            }
            else
            {
                sprintf(rnum, "%s", charstr(3, 'o'));
                sprintf(s5, "%s", charstr(29, 'o'));
                sprintf(rsize, "%s", charstr(4, 'o'));
            }
        }
        else
        {
            sprintf(rnum, "%d", i + 2);
            strncpy(s5, g[i + 1].description, 29);
            s5[29] = '\0';
            sprintf( path_name, "%s%s%c%s", syscfg.gfilesdir, gfilesec[sn].filename,
                     WWIV_FILE_SEPERATOR_CHAR, g[i + 1].filename );
            if ( WFile::Exists( path_name ) )
            {
				WFile handle( path_name );
                sprintf( rsize, "%ld", bytes_to_k( handle.GetLength() ) );
                strcat(rsize, "k");
            }
            else
            {
                sprintf( rsize, "OFL" );
            }
        }
        if ( okansi() )
        {
            sprintf( s, "|#7³|#2%3s|#7³|#1%-29s|#7³|#2%4s|#7³|#2%3s|#7³|#1%-29s|#7³|#2%4s|#7³",
                     lnum, s4, lsize, rnum, s5, rsize);
        }
        else
        {
            sprintf( s, "|%3s|%-29s|%4s|%3s|%-29s|%4s|", lnum, s4, lsize, rnum, s5, rsize );
        }
        pla( s, &abort );
        GetSession()->bout.Color( 0 );
        i++;
        if ( i2 > 10 )
        {
            i2 = 0;
            if ( okansi() )
            {
                sprintf(s1, "|#7ÈÄÄÄÊ%sÊÄÄÄÄÊÄÄÄÊÄ%s|#1þ|#7Ä|#2%s|#7Ä|#1þ|#7ÄÊÄÄÄÄ¼",
                    s2, s3, times());
            }
            else
            {
                sprintf(s1, "+---+%s+----+---+%s-o-%s-o-+----+", s2, s3, times());
            }
            pla( s1, &abort );
            GetSession()->bout.Color( 0 );
            GetSession()->bout.NewLine();
            pausescr();
            gfl_hdr( 1 );
        }
    }
    if (!abort)
    {
        if ( okansi() )
        {
            sprintf( s, "|#7ÌÄÄÄÊ%sÊÄÄÄÄÊÄÄÄÊ%sÊÄÄÄÄ¹", s2, s2 );
        }
        else
        {
            sprintf( s, "+---+%s+----+---+%s+----+", s2, s2 );
        }
        pla( s, &abort );
        GetSession()->bout.Color( 0 );
        if ( so() )
        {
            if ( okansi() )
            {
                sprintf( s1, "|#7³ |#1A|#7)|#2Add a G-File  |#1D|#7)|#2Download a G-file  |#1E|#7)|#2Edit this section  |#1R|#7)|#2Remove a G-" );
            }
            else
            {
                sprintf( s1, "| A)Add a G-File  D)Download a G-file  E)Edit this section  R)Remove a G-File |" );
            }
            pla( s1, &abort );
            GetSession()->bout.Color( 0 );
        }
        else
        {
            if ( okansi() )
            {
                sprintf( s1, "|#7³  |#2D  |#1Download a G-file%s|#7³", charstr( 55, ' ' ) );
            }
            else
            {
                sprintf( s1, "|  D  Download a G-file%s|", charstr( 55, ' ' ) );
            }
            pla( s1, &abort );
            GetSession()->bout.Color( 0 );
        }
    }
    if ( okansi() )
    {
        sprintf( s1, "|#7ÈÄÄÄÄ%sÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ%s|#1þ|#7Ä|#2%s|#7Ä|#1þ|#7ÄÄÄÄ¼", s2, s3, times() );
    }
    else
    {
        sprintf( s1, "+----%s----------------%so-%s-o----+", s2, s3, times() );
    }
    pla( s1, &abort );
    GetSession()->bout.Color( 0 );
    GetSession()->bout.NewLine();
}


void gfile_sec( int sn )
{
    int i, i1, i2, nf;
    char xdc[81], *ss, *ss1, szFileName[ MAX_PATH ];
    bool abort;

    gfilerec *g = read_sec( sn, &nf );
    if ( g == NULL )
    {
        return;
    }
    strcpy( xdc, odc );
    for (i = 0; i < 20; i++)
    {
        odc[i] = 0;
    }
    for ( i = 1; i <= nf / 10; i++ )
    {
        odc[i - 1] = static_cast<char>( i + '0' );
    }
    list_gfiles( g, nf, sn );
    bool done = false;
    while ( !done && !hangup )
    {
        GetSession()->localIO()->tleft( true );
        GetSession()->bout << "|#9Current G|#1-|#9File Section |#1: |#5" << gfilesec[sn].name << "|#0\r\n";
        GetSession()->bout << "|#9Which G|#1-|#9File |#1(|#21|#1-|#2" << nf << "|#1), |#1(|#2Q|#1=|#9Quit|#1, |#2?|#1=|#9Relist|#1) : |#5";
        ss = mmkey( 2 );
        i = atoi(ss);
        if ( wwiv::stringUtils::IsEquals( ss, "Q" ) )
        {
            done = true;
        }
        else if ( wwiv::stringUtils::IsEquals( ss, "E" ) && so() )
        {
            done = true;
            gfiles3( sn );
        }
        if ( wwiv::stringUtils::IsEquals( ss, "A" ) && so() )
        {
            BbsFreeMemory( g );
            fill_sec( sn );
            g = read_sec( sn, &nf );
            if ( g == NULL )
            {
                return;
            }
            for ( i = 0; i < 20; i++ )
            {
                odc[i] = 0;
            }
            for ( i = 1; i <= nf / 10; i++ )
            {
                odc[i - 1] = static_cast<char>( i + '0' );
            }
        }
        else if ( wwiv::stringUtils::IsEquals(ss, "R") && so() )
        {
            GetSession()->bout.NewLine();
            GetSession()->bout << "|#2G-file number to delete? ";
            ss1 = mmkey( 2 );
            i = atoi( ss1 );
            if ( i > 0 && i <= nf )
            {
				GetSession()->bout << "|#9Remove " << g[i - 1].description << "|#1? |#5";
                if ( yesno() )
                {
                    GetSession()->bout << "|#5Erase file too? ";
                    if ( yesno() )
                    {
                        sprintf( szFileName, "%s%s%c%s", syscfg.gfilesdir,
								 gfilesec[sn].filename, WWIV_FILE_SEPERATOR_CHAR, g[i - 1].filename );
                        WFile::Remove( szFileName );
                    }
                    for ( i1 = i; i1 < nf; i1++ )
                    {
                        g[i1 - 1] = g[i1];
                    }
                    --nf;
                    sprintf( szFileName, "%s%s.gfl", syscfg.datadir, gfilesec[sn].filename );
                    WFile file( szFileName );
                    file.Open( WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile | WFile::modeTruncate, WFile::shareUnknown, WFile::permReadWrite );
                    file.Write( g, nf * sizeof( gfilerec ) );
                    file.Close();
                    GetSession()->bout << "\r\nDeleted.\r\n\n";
                }
            }
        }
        else if ( wwiv::stringUtils::IsEquals( ss, "?" ) )
        {
            list_gfiles( g, nf, sn );
        }
        else if ( wwiv::stringUtils::IsEquals( ss, "Q" ) )
        {
            done = true;
        }
        else if ( i > 0 && i <= nf )
        {
            sprintf( szFileName, "%s%c%s", gfilesec[sn].filename, WWIV_FILE_SEPERATOR_CHAR, g[i - 1].filename );
            i1 = printfile( szFileName );
            GetSession()->GetCurrentUser()->SetNumGFilesRead( GetSession()->GetCurrentUser()->GetNumGFilesRead() + 1 );
            if ( i1 == 0 )
            {
                sysoplogf( "Read G-file '%s'", g[i - 1].filename );
            }
        }
        else if ( wwiv::stringUtils::IsEquals( ss, "D" ) )
        {
            bool done1 = false;
            while ( !done1 && !hangup )
            {
                GetSession()->bout << "|#9Download which G|#1-|#9file |#1(|#2Q|#1=|#9Quit|#1, |#2?|#1=|#9Relist) : |#5";
                ss = mmkey( 2 );
                i2 = atoi( ss );
                abort = false;
                if ( wwiv::stringUtils::IsEquals( ss, "?" ) )
                {
                    list_gfiles( g, nf, sn );
					GetSession()->bout << "|#9Current G|#1-|#9File Section |#1: |#5" << gfilesec[sn].name << wwiv::endl;
                }
                else if ( wwiv::stringUtils::IsEquals( ss, "Q" ) )
                {
                    list_gfiles( g, nf, sn );
                    done1 = true;
                }
                else if ( !abort )
                {
                    if ( i2 > 0 && i2 <= nf )
                    {
                        sprintf( szFileName, "%s%s%c%s", syscfg.gfilesdir, gfilesec[sn].filename, WWIV_FILE_SEPERATOR_CHAR, g[i2 - 1].filename );
                        WFile file( szFileName );
                        if ( !file.Open( WFile::modeReadOnly | WFile::modeBinary ) )
                        {
                            GetSession()->bout << "|#6File not found : [" << file.GetFullPathName() << "]";
                        }
                        else
                        {
                            long lFileSize = file.GetLength();
                            file.Close();
                            bool sent = false;
                            abort = false;
                            send_file(szFileName, &sent, &abort, 0, g[i2 - 1].filename, -1, lFileSize);
                            char s1[ 255 ];
                            if ( sent )
                            {
                                sprintf( s1, "|#2%s |#9successfully transferred|#1.|#0\r\n", g[i2 - 1].filename );
                                done1 = true;
                            }
                            else
                            {
                                sprintf( s1, "|#6þ |#9Error transferring |#2%s|#1.|#0", g[i2 - 1].filename );
                                done1 = true;
                            }
							GetSession()->bout.NewLine();
                            GetSession()->bout << s1;
							GetSession()->bout.NewLine();
                            sysoplog( s1 );
                        }
                    }
                    else
                    {
                        done1 = true;
                    }
                }
            }
        }
    }
    BbsFreeMemory( g );
    strcpy( odc, xdc );
}


void gfiles2()
{
    write_inst( INST_LOC_GFILEEDIT, 0, INST_FLAGS_ONLINE );
    sysoplog( "@ Ran Gfile Edit" );
    gfileedit();
    gfiles();
}


void gfiles3(int n)
{
    write_inst( INST_LOC_GFILEEDIT, 0, INST_FLAGS_ONLINE );
    sysoplog( "@ Ran Gfile Edit" );
    modify_sec( n );
    gfile_sec( n );
}


void gfiles()
{
    int * map = static_cast<int *>( BbsAllocA( GetSession()->max_gfilesec * sizeof( int ) ) );
    WWIV_ASSERT( map );
    if ( !map )
    {
        return;
    }

    bool done   = false;
    int nmap    = 0;
    int i       = 0;
    for ( i = 0; i < 20; i++ )
    {
        odc[i] = 0;
    }
    for ( i = 0; i < GetSession()->num_sec; i++ )
    {
        bool ok = true;
        if ( GetSession()->GetCurrentUser()->GetAge() < gfilesec[i].age )
        {
            ok = false;
        }
        if ( GetSession()->GetEffectiveSl() < gfilesec[i].sl )
        {
            ok = false;
        }
        if ( !GetSession()->GetCurrentUser()->HasArFlag( gfilesec[i].ar) && gfilesec[i].ar )
        {
            ok = false;
        }
        if ( ok )
        {
            map[nmap++] = i;
            if ((nmap % 10) == 0)
			{
                odc[nmap / 10 - 1] = static_cast<char>( '0' + ( nmap / 10 ) );
			}
        }
    }
    if ( nmap == 0 )
    {
        GetSession()->bout << "\r\nNo G-file sections available.\r\n\n";
        BbsFreeMemory( map );
        return;
    }
    list_sec( map, nmap );
    while ( !done && !hangup )
    {
        GetSession()->localIO()->tleft( true );
        GetSession()->bout << "|#9G|#1-|#9Files Main Menu|#0\r\n";
        GetSession()->bout << "|#9Which Section |#1(|#21|#1-|#2" << nmap << "|#1), |#1(|#2Q|#1=|#9Quit|#1, |#2?|#1=|#9Relist|#1) : |#5";
        char * ss = mmkey( 2 );
        if ( wwiv::stringUtils::IsEquals( ss, "Q" ) )
        {
            done = true;
        }
        else if ( wwiv::stringUtils::IsEquals( ss, "G" ) && so() )
        {
            done = true;
            gfiles2();
        }
        else if ( wwiv::stringUtils::IsEquals( ss, "A" ) && cs() )
        {
            bool bIsSectionFull = false;
            for ( i = 0; i < nmap && !bIsSectionFull; i++ )
            {
                GetSession()->bout.NewLine();
                GetSession()->bout << "Now loading files for " << gfilesec[map[i]].name << "\r\n\n";
                bIsSectionFull = fill_sec( map[i] );
            }
        }
        else
        {
            i = atoi( ss );
            if ( i > 0 && i <= nmap )
            {
                gfile_sec( map[ i - 1 ] );
            }
        }
        if ( !done )
        {
            list_sec( map, nmap );
        }
    }

    BbsFreeMemory( map );
}

