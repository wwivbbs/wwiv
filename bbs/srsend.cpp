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


bool NewZModemSendFile( const char *pszFileName );



void send_block(char *b, int nBlockType, bool bUseCRC, char byBlockNumber)
{
    int nBlockSize = 0;

    CheckForHangup();
    switch (nBlockType)
    {
    case 5:
        nBlockSize = 128;
        rputch( 1 );
        break;
    case 4:
        rputch( '\x81' );
        rputch(byBlockNumber);
        rputch(byBlockNumber ^ 0xff);
        break;
    case 3:
        rputch(CX);
        break;
    case 2:
        rputch( 4 );
        break;
    case 1:
        nBlockSize = 1024;
        rputch( 2 );
        break;
    case 0:
        nBlockSize = 128;
        rputch( 1 );
    }
    if ( nBlockType > 1 && nBlockType < 5 )
    {
        return;
    }

    rputch(byBlockNumber);
    rputch(byBlockNumber ^ 0xff);
    crc = 0;
    checksum = 0;
    for (int i = 0; i < nBlockSize; i++)
    {
        char ch = b[i];
        rputch(ch);
        calc_CRC(ch);
    }

    if (bUseCRC)
    {
        rputch( static_cast<char>( crc >> 8 ) );
        rputch( static_cast<char>( crc & 0x00ff ) );
    }
    else
    {
        rputch(checksum);
    }
    dump();
}


char send_b(WFile &file, long pos, int nBlockType, char byBlockNumber, bool *bUseCRC, char *pszFileName, int *terr, bool *abort)
{
    char b[1025], szTempBuffer[20];

    int nb = 0;
    if (nBlockType == 0)
    {
        nb = 128;
    }
    if (nBlockType == 1)
    {
        nb = 1024;
    }
    if (nb)
    {
		file.Seek( pos, WFile::seekBegin );
		int nNumRead = file.Read( b, nb );
        for (int i = nNumRead; i < nb; i++)
        {
            b[i] = '\0';
        }
    }
    else if (nBlockType == 5)
    {
        char szFileDate[20];
        memset( b, 0, 128 );
        nb = 128;
        strcpy(b, stripfn(pszFileName));
        sprintf( szTempBuffer, "%ld ", pos );
		// We neede dthis cast to (long) to compile with XCode 1.5 on OS X
        sprintf( szFileDate, "%ld", (long)file.GetFileTime() - (long)timezone );

        strcat(szTempBuffer, szFileDate);
        strcpy(&(b[strlen(b) + 1]), szTempBuffer);
        b[127] = static_cast<unsigned char>((static_cast<int>(pos + 127) / 128) >> 8);
        b[126] = static_cast<unsigned char>((static_cast<int>(pos + 127) / 128) & 0x00ff);
    }
    bool done = false;
    int nNumErrors = 0;
    char ch = 0;
    do
    {
        send_block(b, nBlockType, *bUseCRC, byBlockNumber);
        ch = gettimeout(5.0, abort);
        if ( ch == 'C' && pos == 0 )
        {
            *bUseCRC = true;
        }
        if ( ch == 6 || ch == CX )
        {
            done = true;
        }
        else
        {
            ++nNumErrors;
            ++(*terr);
            if (nNumErrors >= 9)
            {
                done = true;
            }
            GetApplication()->GetLocalIO()->LocalXYPrintf( 69, 4, "%d", nNumErrors );
            GetApplication()->GetLocalIO()->LocalXYPrintf( 69, 5, "%d", *terr );
        }
    } while ( !done && !hangup && !*abort );

    if (ch == 6)
    {
        return 6;
    }
    if (ch == CX)
    {
        return CX;
    }
    return CU;
}


bool okstart(bool *bUseCRC, bool *abort)
{
    double d    = timer();
    bool   ok   = false;
    bool   done = false;

    while ( fabs(timer() - d) < 90.0 && !done && !hangup && !*abort )
    {
        char ch = gettimeout(91.0 - d, abort);
        if (ch == 'C')
        {
            *bUseCRC = true;
            ok = true;
            done = true;
        }
        if ( ch == CU )
        {
            *bUseCRC = false;
            ok = true;
            done = true;
        }
        if (ch == CX)
        {
            ok = false;
            done = true;
        }
    }
    return ok;
}


int GetXYModemBlockSize( bool bBlockSize1K )
{
    return ( bBlockSize1K ) ? 1024 : 128;
}


void xymodem_send(char *pszFileName, bool *sent, double *percent, char ft, bool bUseCRC, bool bUseYModem, bool bUseYModemBatch )
{
    char ch;

    long cp = 0L;
    char byBlockNumber = 1;
    bool abort = false;
    int terr = 0;
    StringRemoveWhitespace( pszFileName );
	WFile file( pszFileName );
	if ( !file.Open( WFile::modeBinary | WFile::modeReadOnly ) )
    {
        if (!bUseYModemBatch)
        {
            sess->bout << "\r\nFile not found.\r\n\n";
        }
        *sent = false;
        *percent = 0.0;
        return;
    }
	long lFileSize = file.GetLength();
    if (!lFileSize)
    {
        lFileSize = 1;
    }
    double tpb = ( 12.656 ) / ( ( double ) modem_speed );

    if (!bUseYModemBatch)
    {
        sess->bout << "\r\n-=> Beginning file transmission, Ctrl+X to abort.\r\n";
    }
    int xx1 = GetApplication()->GetLocalIO()->WhereX();
    int yy1 = GetApplication()->GetLocalIO()->WhereY();
    GetApplication()->GetLocalIO()->LocalXYPuts( 52, 0, "³ Filename :               ");
    GetApplication()->GetLocalIO()->LocalXYPuts( 52, 1, "³ Xfer Time:               ");
    GetApplication()->GetLocalIO()->LocalXYPuts( 52, 2, "³ File Size:               ");
    GetApplication()->GetLocalIO()->LocalXYPuts( 52, 3, "³ Cur Block: 1 - 1k        ");
    GetApplication()->GetLocalIO()->LocalXYPuts( 52, 4, "³ Consec Errors: 0         ");
    GetApplication()->GetLocalIO()->LocalXYPuts( 52, 5, "³ Total Errors : 0         ");
    GetApplication()->GetLocalIO()->LocalXYPuts( 52, 6, "ÀÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄÄ");
    GetApplication()->GetLocalIO()->LocalXYPuts( 65, 0, stripfn(pszFileName));
    GetApplication()->GetLocalIO()->LocalXYPrintf(65, 2, "%ld - %ldk", (lFileSize + 127) / 128, bytes_to_k( lFileSize ) );

    if (!okstart(&bUseCRC, &abort))
    {
        abort = true;
    }
    if ( ft && !abort && !hangup )
    {
        ch = send_b(file, lFileSize, 4, ft, &bUseCRC, pszFileName, &terr, &abort);
        if (ch == CX)
        {
            abort = true;
        }
        if ( ch == CU )
        {
            send_b( file, 0L, 3, 0, &bUseCRC, pszFileName, &terr, &abort);
            abort = true;
        }
    }
    if ( bUseYModem && !abort && !hangup )
    {
        ch = send_b(file, lFileSize, 5, 0, &bUseCRC, pszFileName, &terr, &abort);
        if (ch == CX)
        {
            abort = true;
        }
        if ( ch == CU )
        {
            send_b(file, 0L, 3, 0, &bUseCRC, pszFileName, &terr, &abort);
            abort = true;
        }
    }
    bool bUse1kBlocks = false;
    while ( !hangup && !abort && cp < lFileSize )
    {
        bUse1kBlocks = ( bUseYModem ) ? true : false;
        if ((lFileSize - cp) < 128L)
        {
            bUse1kBlocks = false;
        }
        GetApplication()->GetLocalIO()->LocalXYPrintf( 65, 3, "%ld - %ldk", cp / 128 + 1, cp / 1024 + 1 );
        GetApplication()->GetLocalIO()->LocalXYPuts( 65, 1, ctim(((double) (lFileSize - cp)) * tpb ) );
        GetApplication()->GetLocalIO()->LocalXYPuts( 69, 4, "0" );

        ch = send_b(file, cp, ( bUse1kBlocks ) ? 1 : 0, byBlockNumber, &bUseCRC, pszFileName, &terr, &abort);
        if (ch == CX)
        {
            abort = true;
        }
        else if ( ch == CU )
        {
            wait1(18);
            dump();
            send_b(file, 0L, 3, 0, &bUseCRC, pszFileName, &terr, &abort);
            abort = true;
        }
        else
        {
            ++byBlockNumber;
            cp += GetXYModemBlockSize( bUse1kBlocks );
        }
    }
    if ( !hangup && !abort )
    {
        send_b(file, 0L, 2, 0, &bUseCRC, pszFileName, &terr, &abort);
    }
    if (!abort)
    {
        *sent = true;
        *percent = 1.0;
    }
    else
    {
        *sent = false;
        cp += GetXYModemBlockSize( bUse1kBlocks );
        if (cp >= lFileSize)
        {
            *percent = 1.0;
        }
        else
        {
            cp -= GetXYModemBlockSize( bUse1kBlocks );
            *percent = ((double) (cp)) / ((double) lFileSize);
        }
    }
	file.Close();
    GetApplication()->GetLocalIO()->LocalGotoXY(xx1, yy1);
    if ( *sent && !bUseYModemBatch )
    {
        sess->bout << "-=> File transmission complete.\r\n\n";
    }
}


void zmodem_send(char *pszFileName, bool *sent, double *percent, char ft  )
{
    *sent = false;
    *percent = 0.0;

    StringRemoveWhitespace( pszFileName );

    bool bOldBinaryMode = GetApplication()->GetComm()->GetBinaryMode();
	GetApplication()->GetComm()->SetBinaryMode( true );
	bool bResult = NewZModemSendFile( pszFileName );
	GetApplication()->GetComm()->SetBinaryMode( bOldBinaryMode );

	if ( bResult )
	{
        *sent = true;
        *percent = 100.0;
	}
}

