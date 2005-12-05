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
#include "WStringUtils.h"

// How far to indent extended descriptions
#define INDENTION 24

// the archive type to use
#define ARC_NUMBER 0

int foundany;

int this_date;

static int ed_num, ed_got;
static ext_desc_rec *ed_info;


void zap_ed_info()
{
    if (ed_info)
    {
        BbsFreeMemory(ed_info);
        ed_info = NULL;
    }
    ed_num = 0;
    ed_got = 0;
}


void get_ed_info()
{
    if (ed_got)
    {
        return;
    }

    zap_ed_info();
    ed_got = 1;

    if (!GetSession()->numf)
    {
        return;
    }

    long lCurFilePos = 0;
	WFile fileExtDescr( g_szExtDescrFileName );
	if ( fileExtDescr.Open( WFile::modeReadOnly | WFile::modeBinary ) )
    {
		long lFileSize = fileExtDescr.GetLength();
        if ( lFileSize > 0 )
        {
            ed_info = static_cast<ext_desc_rec *>( BbsAllocA(static_cast<long>( GetSession()->numf ) * sizeof( ext_desc_rec ) ) );
            if (ed_info == NULL)
            {
				fileExtDescr.Close();
                return;
            }
            ed_num = 0;
            while ( lCurFilePos < lFileSize && ed_num < GetSession()->numf )
            {
				fileExtDescr.Seek( lCurFilePos, WFile::seekBegin );
                ext_desc_type ed;
				int nNumRead = fileExtDescr.Read( &ed, sizeof( ext_desc_type ) );
                if ( nNumRead == sizeof( ext_desc_type ) )
                {
                    strcpy( ed_info[ed_num].name, ed.name );
                    ed_info[ed_num].offset = lCurFilePos;
                    lCurFilePos += static_cast<long>( ed.len ) + sizeof( ext_desc_type );
                    ed_num++;
                }
            }
            if ( lCurFilePos < lFileSize )
            {
                ed_got = 2;
            }
        }
		fileExtDescr.Close();
    }
}


unsigned long bytes_to_k( unsigned long lBytes )
{
    return ( lBytes ) ? ( ( unsigned long ) ( ( lBytes + 1023 ) / 1024 ) ) : 0L;
}


int check_batch_queue(const char *pszFileName)
{
	for (int i = 0; i < GetSession()->numbatch; i++)
	{
		if ( wwiv::stringUtils::IsEquals( pszFileName, batch[i].filename ) )
		{
            return (batch[i].sending) ? 1 : -1;
		}
	}
	return 0;
}

/**
 * returns true if everything is ok, false if the file
 */
bool check_ul_event( int nDirectoryNum, uploadsrec * u )
{
    if ( syscfg.upload_c[0] )
    {
        char szComPort[ 10 ];
        sprintf( szComPort, "%d", incom ? syscfgovr.primaryport : 0 );

        char szCmdLine[ MAX_PATH ];
        stuff_in(szCmdLine, syscfg.upload_c, create_chain_file(), directories[nDirectoryNum].path, stripfn(u->filename), szComPort, "");
        ExecuteExternalProgram(szCmdLine, GetApplication()->GetSpawnOptions( SPWANOPT_ULCHK ) );

		WFile file( directories[nDirectoryNum].path, stripfn( u->filename ) );
		if ( !file.Exists() )
        {
            sysoplogf( "File \"%s\" to %s deleted by UL event.", u->filename, directories[nDirectoryNum].name );
			GetSession()->bout << u->filename << " was deleted by the upload event.\r\n";
            return false;
        }
    }
    return true;
}

char (*devices)[9];
int num_devices;

#define NUM_OS2_DEVICES 17
static char *OS2DeviceNames[] =
{
    "KBD$", "PRN", "COM1", "COM2", "COM3", "COM4", "COM5", "COM6", "COM7",
    "COM8", "LPT1", "LPT2", "LPT3", "CLOCK$", "SCREEN$", "POINTER$", "MOUSE$"
};


void finddevs(char (*devs)[9], int *count)
{
    *count = NUM_OS2_DEVICES;
    if (devs)
    {
        for (int i = 0; i < NUM_OS2_DEVICES; i++)
        {
            strcpy(devs[i], OS2DeviceNames[i]);
        }
    }
}


void find_devices()
{
    finddevs(NULL, &num_devices);
    devices = (char (*)[9]) (BbsAllocA((num_devices + 2) * 9));
    finddevs(devices, &num_devices);
}


bool okfn(const char *pszFileName)
{
	if ( !pszFileName || !*pszFileName )
	{
		return false;
	}

    int nStringLen = strlen( pszFileName );
    if ( pszFileName[0] == '-' || pszFileName[0] == ' ' || pszFileName[0] == '.' || pszFileName[0] == '@' )
    {
        return false;
    }

    for ( int i = 0; i < nStringLen; i++ )
    {
        unsigned char ch = pszFileName[i];
        if ( ch == ' '  || ch == '/' || ch == '\\' || ch == ':'  ||
             ch == '>'  || ch == '<' || ch == '|'  || ch == '+'  ||
             ch == ','  || ch == ';' || ch == '^'  || ch == '\"' ||
             ch == '\'' || ch > 126 )
        {
            return false;
        }
    }

    for (int i1 = 0; i1 < num_devices; i1++)
    {
        int nDeviceLen = strlen(devices[i1]);
        if (strncmp(devices[i1], pszFileName, nDeviceLen) == 0)
        {
            if ( pszFileName[nDeviceLen] == 0 || pszFileName[nDeviceLen] == '.' || nDeviceLen == 8 )
            {
                return false;
            }
        }
    }
    return true;
}


void print_devices()
{
    for ( int i = 0; i < num_devices; i++ )
    {
        GetSession()->bout << devices[ i ];
		nl();
    }
}


void get_arc_cmd( char *pszOutBuffer, const char *pszArcFileName, int cmd, const char *ofn )
{
    char szArcCmd[ MAX_PATH ];

    pszOutBuffer[0] = '\0';
    const char* ss = strrchr( pszArcFileName, '.' );
    if ( ss == NULL )
    {
        return;
    }
    ++ss;
    for ( int i = 0; i < MAX_ARCS; i++ )
    {
        if ( wwiv::stringUtils::IsEqualsIgnoreCase(ss, arcs[i].extension) )
        {
            switch (cmd)
            {
            case 0:
                strcpy(szArcCmd, arcs[i].arcl);
                break;
            case 1:
                strcpy(szArcCmd, arcs[i].arce);
                break;
            case 2:
                strcpy(szArcCmd, arcs[i].arca);
                break;
            case 3:
                strcpy(szArcCmd, arcs[i].arcd);
                break;
            case 4:
                strcpy(szArcCmd, arcs[i].arck);
                break;
            case 5:
                strcpy(szArcCmd, arcs[i].arct);
                break;
            }

            if ( szArcCmd[0] == 0 )
            {
                return;
            }
            stuff_in( pszOutBuffer, szArcCmd, pszArcFileName, ofn, "", "", "" );
            WWIV_make_abs_cmd( pszOutBuffer );
            return;
        }
    }
    BbsFreeMemory( arcs );
}


int list_arc_out(const char *pszFileName, const char *pszDirectory)
{
    char szFileNameToDelete[81];
    int nRetCode = 0;

    szFileNameToDelete[0] = 0;

    char szFullPathName[ MAX_PATH ];
    sprintf(szFullPathName, "%s%s", pszDirectory, pszFileName);
    if (directories[udir[GetSession()->GetCurrentFileArea()].subnum].mask & mask_cdrom)
    {
        sprintf(szFullPathName, "%s%s", syscfgovr.tempdir, pszFileName);
        if (!WFile::Exists(szFullPathName))
        {
            char szFullPathNameInDir[ MAX_PATH ];
            sprintf(szFullPathNameInDir, "%s%s", pszDirectory, pszFileName);
            copyfile(szFullPathNameInDir, szFullPathName, false);
            strcpy(szFileNameToDelete, szFullPathName);
        }
    }
    char szArchiveCmd[ MAX_PATH ];
    get_arc_cmd(szArchiveCmd, szFullPathName, 0, "");
    if (!okfn(pszFileName))
    {
        szArchiveCmd[0] = 0;
    }

    if (WFile::Exists(szFullPathName) && (szArchiveCmd[0] != 0))
    {
        nl( 2 );
		GetSession()->bout << "Archive listing for " << pszFileName << wwiv::endl;
        nl();
        nRetCode = ExecuteExternalProgram(szArchiveCmd, GetApplication()->GetSpawnOptions( SPWANOPT_ARCH_L ) );
        nl();
    }
    else
    {
        nl();
        GetApplication()->GetLocalIO()->LocalPuts("Unknown archive: ");
        GetSession()->bout << pszFileName;
        nl( 2 );
        nRetCode = 0;
    }

    if ( szFileNameToDelete[0] )
    {
        WFile::Remove( szFileNameToDelete );
    }

    return nRetCode;
}


bool ratio_ok()
{
    bool bRetValue = true;

    if ( !GetSession()->thisuser.isExemptRatio() )
    {
        if ((syscfg.req_ratio > 0.0001) && (ratio() < syscfg.req_ratio))
        {
            bRetValue = false;
            ClearScreen();
            nl();
            bprintf( "Your up/download ratio is %-5.3f.  You need a ratio of %-5.3f to download.\r\n\n",
                     ratio(), syscfg.req_ratio );
        }
    }
    if ( !GetSession()->thisuser.isExemptPost() )
    {
        if ((syscfg.post_call_ratio > 0.0001) && (post_ratio() < syscfg.post_call_ratio))
        {
            bRetValue = false;
            ClearScreen();
            nl();
            bprintf( "%s %-5.3f.  %s %-5.3f %s.\r\n\n",
                 "Your post/call ratio is", post_ratio(),
                 "You need a ratio of", syscfg.post_call_ratio,
                 "to download." );
        }
    }
    return bRetValue;
}


bool dcs()
{
    return ( cs() || GetSession()->thisuser.GetDsl() >= 100 ) ? true : false;
}


void dliscan1(int nDirectoryNum)
{
    sprintf(g_szDownloadFileName, "%s%s.dir", syscfg.datadir, directories[nDirectoryNum].filename);
	WFile fileDownload( g_szDownloadFileName );
	fileDownload.Open( WFile::modeBinary|WFile::modeCreateFile|WFile::modeReadWrite, WFile::shareUnknown, WFile::permReadWrite );
	int nNumRecords = fileDownload.GetLength() / sizeof(uploadsrec);
    uploadsrec u;
    if (nNumRecords == 0)
    {
        memset(&u, 0, sizeof(uploadsrec));
        strcpy(u.filename, "|MARKER|");
        time_t tNow;
        time(&tNow);
        u.daten = static_cast<unsigned long>(tNow);
        FileAreaSetRecord( fileDownload, 0 );
		fileDownload.Write( &u, sizeof( uploadsrec ) );
    }
    else
    {
        FileAreaSetRecord( fileDownload, 0 );
		fileDownload.Read( &u, sizeof( uploadsrec ) );
        if ( !wwiv::stringUtils::IsEquals( u.filename, "|MARKER|" ) )
        {
            GetSession()->numf = u.numbytes;
            memset(&u, 0, sizeof(uploadsrec));
            strcpy(u.filename, "|MARKER|");
            time_t l;
            time(&l);
            u.daten = static_cast<unsigned long>(l);
            u.numbytes = GetSession()->numf;
            FileAreaSetRecord( fileDownload, 0 );
			fileDownload.Write( &u, sizeof( uploadsrec ) );
        }
    }
	fileDownload.Close();
    GetSession()->numf = u.numbytes;
    this_date = u.daten;
    GetSession()->m_DirectoryDateCache[nDirectoryNum] = this_date;

    sprintf( g_szExtDescrFileName, "%s%s.ext", syscfg.datadir, directories[nDirectoryNum].filename );
    zap_ed_info();
}


void dliscan_hash(int nDirectoryNum)
{
    uploadsrec u;

    if ( nDirectoryNum >= GetSession()->num_dirs || GetSession()->m_DirectoryDateCache[nDirectoryNum] )
    {
        return;
    }

	WFile file( syscfg.datadir, directories[nDirectoryNum].filename );
	if ( !file.Open( WFile::modeBinary | WFile::modeReadOnly ) )
    {
        time((time_t *) &(GetSession()->m_DirectoryDateCache[nDirectoryNum]));
        return;
    }
	int nNumRecords = file.GetLength() / sizeof(uploadsrec);
    if (nNumRecords < 1)
    {
        time((time_t *) &(GetSession()->m_DirectoryDateCache[nDirectoryNum]));
    }
    else
    {
        FileAreaSetRecord( file, 0 );
		file.Read( &u, sizeof( uploadsrec ) );
        if ( wwiv::stringUtils::IsEquals( u.filename, "|MARKER|" ) )
        {
            GetSession()->m_DirectoryDateCache[nDirectoryNum] = u.daten;
        }
        else
        {
            time((time_t *) &(GetSession()->m_DirectoryDateCache[nDirectoryNum]));
        }
    }
	file.Close();
}


void dliscan()
{
    dliscan1(udir[GetSession()->GetCurrentFileArea()].subnum);
}


void add_extended_description(const char *pszFileName, const char *pszDescription)
{
    ext_desc_type ed;

    strcpy( ed.name, pszFileName );
    ed.len = static_cast<short>( wwiv::stringUtils::GetStringLength( pszDescription ) );

	WFile file( g_szExtDescrFileName );
	file.Open(	WFile::modeReadWrite | WFile::modeBinary | WFile::modeCreateFile,
				WFile::shareUnknown,
				WFile::permReadWrite );
	file.Seek( 0L, WFile::seekEnd );
	file.Write( &ed, sizeof( ext_desc_type ) );
	file.Write( pszDescription, ed.len );
	file.Close();

    zap_ed_info();
}


void delete_extended_description(const char *pszFileName)
{
    ext_desc_type ed;

    char* ss = static_cast<char *>( BbsAllocA( 10240L ) );
    if ( ss == NULL )
    {
        return;
    }

	WFile fileExtDescr( g_szExtDescrFileName );
	fileExtDescr.Open( WFile::modeBinary|WFile::modeCreateFile|WFile::modeReadWrite, WFile::shareUnknown, WFile::permReadWrite );
	long lFileSize = fileExtDescr.GetLength();
    long r = 0, w = 0;
    while (r < lFileSize)
    {
		fileExtDescr.Seek( r, WFile::seekBegin );
		fileExtDescr.Read( &ed, sizeof( ext_desc_type ) );
        if (ed.len < 10000)
        {
			fileExtDescr.Read( ss, ed.len );
            if ( !wwiv::stringUtils::IsEquals( pszFileName, ed.name ) )
            {
                if (r != w)
                {
					fileExtDescr.Seek( w, WFile::seekBegin );
					fileExtDescr.Write( &ed, sizeof( ext_desc_type ) );
					fileExtDescr.Write( ss, ed.len );
                }
                w += sizeof( ext_desc_type ) + ed.len;
            }
        }
        r += sizeof( ext_desc_type ) + ed.len;
    }
	fileExtDescr.SetLength( w );
	fileExtDescr.Close();
    BbsFreeMemory( ss );
    zap_ed_info();
}


char *read_extended_description(const char *pszFileName)
{
    get_ed_info();

    if ( ed_got && ed_info )
    {
        for (int i = 0; i < ed_num; i++)
        {
            if ( wwiv::stringUtils::IsEquals( pszFileName, ed_info[i].name ) )
            {
				WFile fileExtDescr( g_szExtDescrFileName );
				if ( !fileExtDescr.Open( WFile::modeBinary | WFile::modeReadOnly ) )
                {
                    return NULL;
                }
				fileExtDescr.Seek( ed_info[i].offset, WFile::seekBegin );
                ext_desc_type ed;
                int nNumRead = fileExtDescr.Read( &ed, sizeof( ext_desc_type ) );
                if ( nNumRead == sizeof( ext_desc_type ) &&
                     wwiv::stringUtils::IsEquals( pszFileName, ed.name ) )
                {
                    char* ss = static_cast<char *>( BbsAllocA( ed.len + 10 ) );
                    if (ss)
                    {
						fileExtDescr.Read( ss, ed.len );
                        ss[ed.len] = 0;
                    }
					fileExtDescr.Close();
                    return ss;
                }
                else
                {
                    zap_ed_info();
					fileExtDescr.Close();
                    break;
                }
            }
        }
    }
    if ( ed_got != 1 )
    {
		WFile fileExtDescr( g_szExtDescrFileName );
		if ( fileExtDescr.Open( WFile::modeBinary | WFile::modeReadOnly ) )
        {
			long lFileSize = fileExtDescr.GetLength();
			long lCurPos = 0;
            while (lCurPos < lFileSize)
            {
				fileExtDescr.Seek( lCurPos, WFile::seekBegin );
                ext_desc_type ed;
				lCurPos += static_cast<long>( fileExtDescr.Read( &ed, sizeof( ext_desc_type ) ) );
                if ( wwiv::stringUtils::IsEquals( pszFileName, ed.name ) )
                {
                    char* ss = static_cast<char *>( BbsAllocA( ed.len + 10 ) );
                    if (ss)
                    {
						fileExtDescr.Read( ss, ed.len );
                        ss[ed.len] = 0;
                    }
					fileExtDescr.Close();
                    return ss;
                }
                else
                {
                    lCurPos += static_cast<long>( ed.len );
                }
            }
			fileExtDescr.Close();
        }
    }
    return NULL;
}



void print_extended(const char *pszFileName, bool *abort, int numlist, int indent)
{
    bool next = false;
    int numl = 0;
    int cpos = 0;
    char ch, s[81];
    int i;

    char* ss = read_extended_description(pszFileName);
    if ( ss )
    {
        ch = ( indent != 2 ) ? 10 : 0;
        while ( ss[cpos] && !(*abort) && numl < numlist )
        {
            if ( ch == SOFTRETURN )
            {
                if ( indent == 1 )
                {
                    for ( i = 0; i < INDENTION; i++ )
                    {
                        if ( i == 12 || i == 18 )
                        {
                            s[ i ] = ( okansi() ? 'บ' : '|' );
                        }
                        else
                        {
                            s[ i ] = SPACE;
                        }
                    }
                    s[INDENTION] = '\0';
                    ansic( GetSession()->thisuser.isUseExtraColor() && !( *abort ) ? FRAME_COLOR : 0 );
                    osan( s, abort, &next );
                    if ( GetSession()->thisuser.isUseExtraColor() && !( *abort ) )
                    {
                        ansic( 1 );
                    }
                }
                else
                {
                    if ( indent == 2 )
                    {
                        for ( i = 0; i < 13; i++ )
                        {
                            s[ i ] = SPACE;
                        }
                        s[13] = '\0';
                        osan( s, abort, &next );
                        if ( GetSession()->thisuser.isUseExtraColor() && !( *abort ) )
                        {
                            ansic( 2 );
                        }
                    }
                }
            }
            bputch( ch = ss[ cpos++ ] );
            checka( abort, &next );
            if ( ch == SOFTRETURN )
            {
                ++numl;
            }
            else if ( ch != RETURN && GetApplication()->GetLocalIO()->WhereX() >= 78 )
            {
                osan( "\r\n", abort, &next );
                ch = SOFTRETURN;
            }
        }
        if ( GetApplication()->GetLocalIO()->WhereX() )
        {
            nl();
        }
        BbsFreeMemory( ss );
    }
    else if ( GetApplication()->GetLocalIO()->WhereX() )
    {
        nl();
    }
}


void align( char *pszFileName )
{
    // TODO Modify this to handle long filenames
    char szFileName[40], szExtension[40];

    bool bInvalid = false;
    if ( pszFileName[ 0 ] == '.' )
    {
        bInvalid = true;
    }

    for ( int i = 0; i < wwiv::stringUtils::GetStringLength(pszFileName); i++ )
    {
        if ( pszFileName[i] == '\\' || pszFileName[i] == '/' ||
             pszFileName[i] == ':'  || pszFileName[i] == '<' ||
             pszFileName[i] == '>'  || pszFileName[i] == '|' )
        {
            bInvalid = true;
        }
    }
    if ( bInvalid )
    {
        strcpy(pszFileName, "        .   ");
        return;
    }
    char* s2 = strrchr( pszFileName, '.' );
    if ( s2 == NULL || strrchr( pszFileName, '\\' ) > s2 )
    {
        szExtension[0] = '\0';
    }
    else
    {
        strcpy( szExtension, &( s2[ 1 ] ) );
        szExtension[3]  = '\0';
        s2[0]           = '\0';
    }
    strcpy( szFileName, pszFileName );

    for ( int j = strlen( szFileName ); j < 8; j++ )
    {
        szFileName[j] = SPACE;
    }
    szFileName[8] = '\0';
    bool bHasWildcard = false;
    bool bHasSpace    = false;
    for ( int k = 0; k < 8; k++ )
    {
        if ( szFileName[k] == '*' )
        {
            bHasWildcard = true;
        }
        if ( szFileName[k] == ' ' )
        {
            bHasSpace = true;
        }
        if ( bHasSpace )
        {
            szFileName[k] = ' ';
        }
        if ( bHasWildcard )
        {
            szFileName[k] = '?';
        }
    }

    for ( int i2 = strlen( szExtension ); i2 < 3; i2++ )
    {
        szExtension[ i2 ] = SPACE;
    }
    szExtension[ 3 ] = '\0';
    bHasWildcard = false;
    for ( int i3 = 0; i3 < 3; i3++ )
    {
        if ( szExtension[i3] == '*' )
        {
            bHasWildcard = true;
        }
        if ( bHasWildcard )
        {
            szExtension[i3] = '?';
        }
    }

    char szBuffer[ MAX_PATH ];
    for ( int i4 = 0; i4 < 12; i4++ )
    {
        szBuffer[ i4 ] = SPACE;
    }
    strcpy( szBuffer, szFileName );
    szBuffer[8] = '.';
    strcpy( &( szBuffer[9] ), szExtension );
    strcpy( pszFileName, szBuffer );
    for ( int i5 = 0; i5 < 12; i5++ )
    {
        pszFileName[ i5 ] = wwiv::UpperCase<char>( pszFileName[ i5 ] );
    }
}


bool compare( const char *pszFileName1, const char *pszFileName2 )
{
    for ( int i = 0; i < 12; i++ )
    {
        if ( pszFileName1[i] != pszFileName2[i] && pszFileName1[i] != '?' && pszFileName2[i] != '?' )
        {
            return false;
        }
    }
    return true;
}


void printinfo(uploadsrec * u, bool *abort)
{
    char s[85], s1[40], s2[81];
    int i;
    bool next;

    if ( GetSession()->titled != 0 )
    {
        printtitle( abort );
    }
    if ( GetSession()->tagging == 0 && !x_only )
    {
        return;
    }
    if ( GetSession()->tagging == 1 && !GetSession()->thisuser.isUseNoTagging() && !x_only )
    {
        if ( !filelist )
        {
            filelist = static_cast<tagrec *>( BbsAllocA( 50 * sizeof( tagrec ) ) );
            GetSession()->tagptr = 0;
        }
        else
        {
            filelist[GetSession()->tagptr].u = *u;
            filelist[GetSession()->tagptr].directory = udir[GetSession()->GetCurrentFileArea()].subnum;
            filelist[GetSession()->tagptr].dir_mask = directories[udir[GetSession()->GetCurrentFileArea()].subnum].mask;
            GetSession()->tagptr++;
            sprintf( s, "\r|#%d%2ld|#%d%c",
                    (check_batch_queue(filelist[GetSession()->tagptr - 1].u.filename)) ? 6 : 0,
                    GetSession()->tagptr, GetSession()->thisuser.isUseExtraColor() ? FRAME_COLOR : 0, okansi() ? 'บ' : '|' );
            osan( s, abort, &next );
        }
    }
    else if ( !x_only )
    {
        GetSession()->bout << "\r";
    }
    ansic( 1 );
    strncpy( s, u->filename, 8 );
    s[8] = '\0';
    osan( s, abort, &next );
    strncpy( s, &( ( u->filename )[ 8 ] ), 4 );
    s[4] = '\0';
    ansic( 1 );
    osan( s, abort, &next );
    ansic( GetSession()->thisuser.isUseExtraColor() ? FRAME_COLOR : 0 );
    osan( ( okansi() ? "บ" : "|" ), abort, &next );

    sprintf( s1, "%ld""k", bytes_to_k( u->numbytes ) );

    if ( !( directories[ udir[ GetSession()->GetCurrentFileArea() ].subnum ].mask & mask_cdrom ) )
    {
        strcpy( s2, directories[ udir[ GetSession()->GetCurrentFileArea() ].subnum ].path );
        strcat( s2, u->filename );
        if ( !WFile::Exists( s2 ) )
        {
            strcpy( s1, "N/A" );
        }
    }
    for ( i = 0; i < 5 - wwiv::stringUtils::GetStringLength( s1 ); i++ )
    {
        s[i] = SPACE;
    }
    s[i] = '\0';
    strcat( s, s1 );
    ansic( 2 );
    osan( s, abort, &next );

    if ( GetSession()->tagging == 1 && !GetSession()->thisuser.isUseNoTagging() && !x_only )
    {
        ansic( GetSession()->thisuser.isUseExtraColor() ? FRAME_COLOR : 0 );
        osan( ( okansi() ? "บ" : "|" ), abort, &next );
        sprintf( s1, "%d", u->numdloads );

        for ( i = 0; i < 4 - wwiv::stringUtils::GetStringLength( s1 ); i++ )
        {
            s[i] = SPACE;
        }
        s[i] = '\0';
        strcat( s, s1 );
        ansic( 2 );
        osan( s, abort, &next );
    }
    ansic( GetSession()->thisuser.isUseExtraColor() ? FRAME_COLOR : 0 );
    osan( ( okansi() ? "บ" : "|" ), abort, &next );
    sprintf( s, "|#%d%s", ( u->mask & mask_extended ) ? 1 : 2, u->description );
    if ( GetSession()->tagging && !GetSession()->thisuser.isUseNoTagging() && !x_only )
    {
        plal( s, GetSession()->thisuser.GetScreenChars() - 28, abort );
    }
    else
    {
        plal( s, strlen( s ), abort );
        if ( !*abort && GetSession()->thisuser.GetNumExtended() && ( u->mask & mask_extended ) )
        {
            print_extended( u->filename, abort, GetSession()->thisuser.GetNumExtended(), 1 );
        }
    }
    if ( !( *abort ) )
    {
        ++g_num_listed;
    }
    else
    {
        GetSession()->tagptr = 0;
        GetSession()->tagging = 0;
    }
}


void printtitle( bool *abort )
{
    char szBuffer[ 255 ], *ss;

    if ( x_only )
    {
        nl();
        ss = "";
    }
    else
    {
        ss = "\r";
    }
    if ( lines_listed >= GetSession()->screenlinest - 7 && !x_only &&
         !GetSession()->thisuser.isUseNoTagging() && filelist && g_num_listed )
    {
        tag_files();
        if (GetSession()->tagging == 0)
        {
            return;
        }
    }
    sprintf( szBuffer, "%s%s - #%s, %ld %s.", ss, directories[udir[GetSession()->GetCurrentFileArea()].subnum].name,
             udir[GetSession()->GetCurrentFileArea()].keys, GetSession()->numf, "files");
    ansic( GetSession()->thisuser.isUseExtraColor() ? FRAME_COLOR : 0 );
    if ( ( g_num_listed == 0 && GetSession()->tagptr == 0 ) || GetSession()->tagging == 0 ||
          ( GetSession()->thisuser.isUseNoTagging() && g_num_listed == 0 ) )
    {
        if ( okansi() )
        {
			GetSession()->bout << ss << "ออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ" << wwiv::endl;
        }
        else
        {
			GetSession()->bout << ss << "------------------------------------------------------------------------------" << wwiv::endl;
        }
    }
    else if ( lines_listed )
    {
        if ( GetSession()->titled != 2 && GetSession()->tagging == 1 && !GetSession()->thisuser.isUseNoTagging() )
        {
            if ( okansi() )
            {
				GetSession()->bout << ss << "ออสออออออออออออสอออออสออออสอออออออออออออออออออออออออออออออออออออออออออออออออออ" << wwiv::endl;
            }
            else
            {
				GetSession()->bout << ss << "--+------------+-----+----+---------------------------------------------------" << wwiv::endl;
            }
        }
        else
        {
            if ( ( GetSession()->thisuser.isUseNoTagging() || GetSession()->tagging == 2 ) && g_num_listed != 0 )
            {
                ansic( GetSession()->thisuser.isUseExtraColor() ? FRAME_COLOR : 0 );
                if ( okansi() )
                {
					GetSession()->bout << ss << "ออออออออออออสอออออสอออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ" << wwiv::endl;
                }
                else
                {
					GetSession()->bout << ss << "------------+-----+-----------------------------------------------------------" << wwiv::endl;
                }
            }
        }
    }
    if ( GetSession()->thisuser.isUseExtraColor() )
    {
        ansic( 2 );
    }
    pla( szBuffer, abort );
    if ( GetSession()->tagging == 1 && !GetSession()->thisuser.isUseNoTagging() && !x_only )
    {
        ansic( GetSession()->thisuser.isUseExtraColor() ? FRAME_COLOR : 0 );
        if ( okansi() )
        {
			GetSession()->bout << "\r" << "ออหออออออออออออหอออออหออออหอออออออออออออออออออออออออออออออออออออออออออออออออออ" << wwiv::endl;
        }
        else
        {
			GetSession()->bout << "\r" << "--+------------+-----+----+---------------------------------------------------" << wwiv::endl;
        }
    }
    else
    {
        ansic( GetSession()->thisuser.isUseExtraColor() ? FRAME_COLOR : 0 );
        if ( okansi() )
        {
			GetSession()->bout << "\r" << "ออออออออออออหอออออหอออออออออออออออออออออออออออออออออออออออออออออออออออออออออออ" << wwiv::endl;
        }
        else if ( x_only )
        {
			GetSession()->bout << "------------+-----+-----------------------------------------------------------" << wwiv::endl;
        }
        else
        {
			GetSession()->bout << "\r" << "------------+-----+-----------------------------------------------------------" << wwiv::endl;
        }
    }
    GetSession()->titled = 0;
}


void file_mask( char *pszFileMask )
{
    nl();
    GetSession()->bout << "|#2File mask: ";
    input( pszFileMask, 12 );
    if ( pszFileMask[0] == '\0' )
    {
        strcpy( pszFileMask, "*.*" );
    }
    if ( strchr( pszFileMask, '.' ) == NULL )
    {
        strcat(pszFileMask, ".*");
    }
    align( pszFileMask );
    nl();
}


void listfiles()
{
    if ( ok_listplus() )
    {
        listfiles_plus( LIST_DIR );
        return;
    }

    dliscan();
    char szFileMask[81];
    file_mask( szFileMask );
    g_num_listed = 0;
    GetSession()->titled = 1;
    lines_listed = 0;

	WFile fileDownload( g_szDownloadFileName );
	fileDownload.Open( WFile::modeBinary | WFile::modeReadOnly );
    bool abort = false;
    for ( int i = 1; i <= GetSession()->numf && !abort && !hangup && GetSession()->tagging != 0; i++ )
    {
        FileAreaSetRecord( fileDownload, i );
        uploadsrec u;
		fileDownload.Read( &u, sizeof( uploadsrec ) );
        if ( compare( szFileMask, u.filename ) )
        {
			fileDownload.Close();
            printinfo(&u, &abort);
			fileDownload.Open( WFile::modeBinary | WFile::modeReadOnly );
        }
        else if ( bkbhit() )
        {
            bool next = false;
            checka(&abort, &next);
        }
    }
	fileDownload.Close();
    endlist( 1 );
}


void nscandir( int nDirNum, bool *abort )
{
    if ( GetSession()->m_DirectoryDateCache[udir[nDirNum].subnum] && GetSession()->m_DirectoryDateCache[udir[nDirNum].subnum] < static_cast<unsigned long>( nscandate ) )
    {
        return;
    }

    int nOldCurDir = GetSession()->GetCurrentFileArea();
    GetSession()->SetCurrentFileArea( nDirNum );
    dliscan();
    if ( this_date >= nscandate )
    {
        if ( ok_listplus() )
        {
            *abort = listfiles_plus(NSCAN_DIR) ? 1 : 0;
            GetSession()->SetCurrentFileArea( nOldCurDir );
            return;
        }
		WFile fileDownload( g_szDownloadFileName );
		fileDownload.Open( WFile::modeBinary | WFile::modeReadOnly );
        for ( int i = 1; i <= GetSession()->numf && !(*abort) && !hangup && GetSession()->tagging != 0; i++ )
        {
            CheckForHangup();
            FileAreaSetRecord( fileDownload, i );
            uploadsrec u;
			fileDownload.Read( &u, sizeof( uploadsrec ) );
            if ( u.daten >= static_cast<unsigned long>( nscandate ) )
            {
				fileDownload.Close();
                printinfo( &u, abort );
                fileDownload.Open( WFile::modeBinary | WFile::modeReadOnly );
            }
            else if ( bkbhit() )
            {
                bool next = false;
                checka( abort, &next );
            }
        }
		fileDownload.Close();
    }
    GetSession()->SetCurrentFileArea( nOldCurDir );
}


void nscanall()
{
    bool bScanAllConfs = false;
    g_flags |= g_flag_scanned_files;

    if ( uconfdir[1].confnum != -1 && okconf( &GetSession()->thisuser ) )
    {
        if ( !x_only )
        {
            nl();
            GetSession()->bout << "|#5All conferences? ";
            bScanAllConfs = yesno();
            nl();
        }
        else
        {
            bScanAllConfs = true;
        }
        if ( bScanAllConfs )
        {
            tmp_disable_conf( true );
        }
    }
    if ( ok_listplus() )
    {
        int save_dir = GetSession()->GetCurrentFileArea();
        listfiles_plus( NSCAN_NSCAN );
        if ( bScanAllConfs )
        {
            tmp_disable_conf( false );
        }
        GetSession()->SetCurrentFileArea( save_dir );
        return;
    }
    bool abort      = false;
    g_num_listed    = 0;
    int count       = 0;
    int color       = 3;
    if ( !x_only )
    {
        GetSession()->bout << "\r" << "|#2Searching ";
    }
    for ( int i = 0; i < GetSession()->num_dirs && !abort && udir[i].subnum != -1 &&
                GetSession()->tagging != 0; i++ )
    {
        count++;
        if ( !x_only )
        {
            GetSession()->bout << "|#" << color << ".";
            if ( count >= NUM_DOTS )
            {
                GetSession()->bout << "\r" << "|#2Searching ";
                color++;
                count = 0;
                if ( color == 4 )
                {
                    color++;
                }
                if ( color == 10 )
                {
                    color = 0;
                }
            }
        }
        int nSubNum = udir[i].subnum;
        if ( qsc_n[nSubNum / 32] & ( 1L << ( nSubNum % 32 ) ) )
        {
            GetSession()->titled = 1;
            nscandir( i, &abort );
        }
    }
    endlist( 2 );
    if ( bScanAllConfs )
    {
        tmp_disable_conf( false );
    }
}


void searchall()
{
    char szFileMask[81];

    if ( ok_listplus() )
    {
        listfiles_plus( SEARCH_ALL );
        return;
    }

    bool bScanAllConfs = false;
    if ( uconfdir[1].confnum != -1 && okconf( &GetSession()->thisuser ) )
    {
        if ( !x_only )
        {
            nl();
            GetSession()->bout << "|#5All conferences? ";
            bScanAllConfs = yesno();
            nl();
        }
        else
        {
            bScanAllConfs = true;
        }
        if ( bScanAllConfs )
        {
            tmp_disable_conf( true );
        }
    }
    bool abort = false;
    int nOldCurDir = GetSession()->GetCurrentFileArea();
    if ( x_only )
    {
        strcpy(szFileMask, "*.*");
        align(szFileMask);
    }
    else
    {
        nl( 2 );
        GetSession()->bout << "Search all directories.\r\n";
        file_mask(szFileMask);
        if ( !x_only )
        {
            nl();
            GetSession()->bout << "|#2Searching ";
        }
    }
    g_num_listed = 0;
    lines_listed = 0;
    int count = 0;
    int color = 3;
    for ( int i = 0; i < GetSession()->num_dirs && !abort && !hangup && ( GetSession()->tagging || x_only )
        && udir[i].subnum != -1; i++ )
    {
        int nDirNum = udir[i].subnum;
        bool bIsDirMarked = false;
        if ( qsc_n[nDirNum / 32] & ( 1L << ( nDirNum % 32 ) ) )
        {
            bIsDirMarked = true;
        }
        bIsDirMarked = true;
        // remove bIsDirMarked=true to search only marked directories
        if ( bIsDirMarked )
        {
            if ( !x_only )
            {
                count++;
                GetSession()->bout << "|#" << color << ".";
                if ( count >= NUM_DOTS )
                {
                    GetSession()->bout << "\r" << "|#2Searching ";
                    color++;
                    count = 0;
                    if ( color == 4 )
                    {
                        color++;
                    }
                    if ( color == 10 )
                    {
                        color = 0;
                    }
                }
            }
            GetSession()->SetCurrentFileArea( i );
            dliscan();
            GetSession()->titled = 1;
			WFile fileDownload( g_szDownloadFileName );
			fileDownload.Open( WFile::modeBinary | WFile::modeReadOnly );
            for ( int i1 = 1; i1 <= GetSession()->numf && !abort && !hangup && ( GetSession()->tagging || x_only ); i1++ )
            {
                FileAreaSetRecord( fileDownload, i1 );
                uploadsrec u;
				fileDownload.Read( &u, sizeof( uploadsrec ) );
                if ( compare( szFileMask, u.filename ) )
                {
					fileDownload.Close();
                    printinfo( &u, &abort );
					fileDownload.Open( WFile::modeBinary | WFile::modeReadOnly );
                }
                else if ( bkbhit() )
                {
                    bool next = false;
                    checka( &abort, &next );
                }
            }
            fileDownload.Close();
        }
    }
    GetSession()->SetCurrentFileArea( nOldCurDir );
    endlist( 1 );
    if ( bScanAllConfs )
    {
        tmp_disable_conf( false );
    }
}


int recno( const char *pszFileMask )
{
    return nrecno( pszFileMask, 0 );
}


int nrecno( const char *pszFileMask, int nStartingRec )
{
    int nRecNum = nStartingRec + 1;
    if ( GetSession()->numf < 1 || nStartingRec >= GetSession()->numf )
    {
        return -1;
    }

	WFile fileDownload( g_szDownloadFileName );
	fileDownload.Open( WFile::modeBinary | WFile::modeReadOnly );
    FileAreaSetRecord( fileDownload, nRecNum );
    uploadsrec u;
	fileDownload.Read( &u, sizeof( uploadsrec ) );
    while ( ( nRecNum < GetSession()->numf ) && ( compare( pszFileMask, u.filename ) == 0 ) )
    {
        ++nRecNum;
        FileAreaSetRecord( fileDownload, nRecNum );
		fileDownload.Read( &u, sizeof( uploadsrec ) );
    }
	fileDownload.Close();
    return ( compare( pszFileMask, u.filename ) ) ? nRecNum : -1;
}


int printfileinfo( uploadsrec * u, int nDirectoryNum )
{
    double d = XFER_TIME( u->numbytes );
	GetSession()->bout << "Filename   : " << stripfn( u->filename ) << wwiv::endl;
	GetSession()->bout << "Description: " << u->description << wwiv::endl;
	GetSession()->bout << "File size  : " << bytes_to_k(u->numbytes) << wwiv::endl;
	GetSession()->bout << "Apprx. time: " << ctim( d ) << wwiv::endl;
	GetSession()->bout << "Uploaded on: " << u->date << wwiv::endl;
    if ( u->actualdate[2] == '/' && u->actualdate[5] == '/' )
    {
		GetSession()->bout << "File date  : " << u->actualdate << wwiv::endl;
    }
	GetSession()->bout << "Uploaded by: " << u->upby << wwiv::endl;
	GetSession()->bout << "Times D/L'd: " << u->numdloads << wwiv::endl;
    nl();
    bool abort = false;
    if ( u->mask & mask_extended )
    {
        GetSession()->bout << "Extended Description: \r\n";
        print_extended( u->filename, &abort, 255, 0 );
    }
    char szFileName[ MAX_PATH ];
    sprintf( szFileName, "%s%s", directories[nDirectoryNum].path, u->filename );
    StringRemoveWhitespace( szFileName );
    if ( !WFile::Exists( szFileName ) )
    {
        GetSession()->bout << "\r\n-=>FILE NOT THERE<=-\r\n\n";
        return -1;
    }
    return ( nsl() >= d ) ? 1 : 0;
}


void remlist( const char *pszFileName )
{
    char szFileName[MAX_PATH], szListFileName[MAX_PATH];

    sprintf( szFileName, "%s", pszFileName );
    align( szFileName );

    if ( filelist )
    {
        for ( int i = 0; i < GetSession()->tagptr; i++ )
        {
            strcpy( szListFileName, filelist[i].u.filename );
            align( szListFileName );
            if ( wwiv::stringUtils::IsEquals( szFileName, szListFileName ) )
            {
                for ( int i2 = i; i2 < GetSession()->tagptr - 1; i2++ )
                {
                    filelist[ i2 ] = filelist[ i2 + 1 ];
                }
                GetSession()->tagptr--;
                g_num_listed--;
            }
        }
    }
}


int FileAreaSetRecord( WFile &file, int nRecordNumber )
{
	return file.Seek( nRecordNumber * sizeof( uploadsrec ), WFile::seekBegin );
}

