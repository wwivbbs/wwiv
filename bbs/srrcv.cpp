/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2014, WWIV Software Services             */
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
#include <cmath>

#include "wwiv.h"

bool NewZModemReceiveFile( const char *pszFileName );


char modemkey(int *tout) {
	if (bkbhitraw()) {
		char ch = bgetchraw();
		calc_CRC(ch);
		return ch;
	}
	if (*tout) {
		return 0;
	}
	double d1 = timer();
	while ( fabs(timer() - d1) < 0.5 && !bkbhitraw() && !hangup ) {
		CheckForHangup();
	}
	if (bkbhitraw()) {
		char ch = bgetchraw();
		calc_CRC(ch);
		return ch;
	}
	*tout = 1;
	return 0;
}



int receive_block(char *b, unsigned char *bln, bool bUseCRC ) {
	bool abort = false;
	unsigned char ch = gettimeout(5.0, &abort);
	int err = 0;

	if (abort) {
		return CF;
	}
	int tout = 0;
	if (ch == 0x81) {
		unsigned char bn = modemkey(&tout);
		unsigned char bn1 = modemkey(&tout);
		if ((bn ^ bn1) == 0xff) {
			b[0] = bn;
			*bln = bn;
			return 8;
		} else {
			return 3;
		}
	} else if (ch == 1) {
		unsigned char bn = modemkey(&tout);
		unsigned char bn1 = modemkey(&tout);
		if ((bn ^ bn1) != 0xff) {
			err = 3;
		}
		*bln = bn;
		crc = 0;
		checksum = 0;
		for (int i = 0; (i < 128) && (!hangup); i++) {
			b[i] = modemkey(&tout);
		}
		if ( !bUseCRC && !hangup ) {
			unsigned char cs1 = checksum;
			bn1 = modemkey(&tout);
			if (bn1 != cs1) {
				err = 2;
			}
		} else if (!hangup) {
			int cc1 = crc;
			bn = modemkey(&tout);
			bn1 = modemkey(&tout);
			if ((bn != (unsigned char) (cc1 >> 8)) || (bn1 != (unsigned char) (cc1 & 0x00ff))) {
				err = 2;
			}
		}
		if (tout) {
			return 7;
		}
		return err;
	} else if (ch == 2) {
		unsigned char bn = modemkey(&tout);
		unsigned char bn1 = modemkey(&tout);
		crc = 0;
		checksum = 0;
		if ((bn ^ bn1) != 0xff) {
			err = 3;
		}
		*bln = bn;
		for (int i = 0; (i < 1024) && (!hangup); i++) {
			b[i] = modemkey(&tout);
		}
		if ( !bUseCRC && !hangup ) {
			unsigned char cs1 = checksum;
			bn1 = modemkey(&tout);
			if (bn1 != cs1) {
				err = 2;
			}
		} else if (!hangup) {
			int cc1 = crc;
			bn = modemkey(&tout);
			bn1 = modemkey(&tout);
			if ((bn != (unsigned char) (cc1 >> 8)) || (bn1 != (unsigned char) (cc1 & 0x00ff))) {
				err = 2;
			}
		}
		if (tout) {
			return 7;
		}
		return ( err == 0 ) ? 1 : err;
	} else if (ch == CX) {
		return 4;
	} else if (ch == 4) {
		return 5;
	} else if (ch == 0) {
		return 7;
	} else {
		return 9;
	}
}


void xymodem_receive(const char *pszFileName, bool *received, bool bUseCRC ) {
	char b[1025], x[81], ch;
	unsigned char bln;
	int i1, i2, i3;

	WFile::Remove( pszFileName );
	bool ok = true;
	bool lastcan = false;
	bool lasteot = false;
	int  nTotalErrors = 0;
	int  nConsecErrors = 0;

	WFile file( pszFileName );
	if ( !file.Open( WFile::modeBinary|WFile::modeCreateFile|WFile::modeReadWrite, WFile::shareUnknown, WFile::permReadWrite ) ) {
		GetSession()->bout << "\r\n\nDOS error - Can't create file.\r\n\n";
		*received = false;
		return;
	}
	long pos = 0L;
	long reallen = 0L;
	time_t filedatetime = 0L;
	unsigned int bn = 1;
	bool done = false;
	double tpb = (12.656) / ((double) (modem_speed));
	GetSession()->bout << "\r\n-=> Ready to receive, Ctrl+X to abort.\r\n";
	int nOldXPos = GetSession()->localIO()->WhereX();
	int nOldYPos = GetSession()->localIO()->WhereY();
	GetSession()->localIO()->LocalXYPuts(52, 0, "\xB3 Filename :               ");
	GetSession()->localIO()->LocalXYPuts(52, 1, "\xB3 Xfer Time:               ");
	GetSession()->localIO()->LocalXYPuts(52, 2, "\xB3 File Size:               ");
	GetSession()->localIO()->LocalXYPuts(52, 3, "\xB3 Cur Block: 1 - 1k        ");
	GetSession()->localIO()->LocalXYPuts(52, 4, "\xB3 Consec Errors: 0         ");
	GetSession()->localIO()->LocalXYPuts(52, 5, "\xB3 Total Errors : 0         ");
	GetSession()->localIO()->LocalXYPuts(52, 6, "\xC0\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4\xC4");
	GetSession()->localIO()->LocalXYPuts(65, 0, stripfn( pszFileName ) );
	int nNumStartTries = 0;
	do {
		if (nNumStartTries++ > 9) {
			*received = false;
			file.Close();
			file.Delete();
			return;
		}
		if (bUseCRC) {
			rputch('C');
		} else {
			rputch( CU );
		}

		double d1 = timer();
		while ( fabs(timer() - d1) < 10.0 && !bkbhitraw() && !hangup ) {
			CheckForHangup();
			if (GetSession()->localIO()->LocalKeyPressed()) {
				ch = GetSession()->localIO()->getchd();
				if (ch == 0) {
					GetSession()->localIO()->getchd();
				} else if (ch == ESC) {
					done = true;
					ok = false;
				}
			}
		}
	} while ( !bkbhitraw() && !hangup );

	int i = 0;
	do {
		bln = 255;
		GetSession()->localIO()->LocalXYPrintf( 69, 4, "%d  ", nConsecErrors );
		GetSession()->localIO()->LocalXYPrintf( 69, 5, "%d", nTotalErrors );
		GetSession()->localIO()->LocalXYPrintf( 65, 3, "%ld - %ldk", pos / 128 + 1, pos / 1024 + 1);
		if (reallen) {
			GetSession()->localIO()->LocalXYPuts( 65, 1, ctim( ( static_cast<double>( reallen - pos ) ) * tpb ) );
		}
		i = receive_block(b, &bln, bUseCRC);
		if ( i == 0 || i == 1 ) {
			if ( bln == 0 && pos == 0L ) {
				i1 = strlen(b) + 1;
				i3 = i1;
				while ( b[i3] >= '0' && b[i3] <= '9' &&  (i3 - i1) < 15 ) {
					x[i3 - i1] = b[i3++];
				}
				x[i3 - i1] = '\0';
				reallen = atol(x);
				GetSession()->localIO()->LocalXYPrintf( 65, 2, "%ld - %ldk", ( reallen + 127 ) / 128, bytes_to_k( reallen ) );
				while ((b[i1] != SPACE) && (i1 < 64)) {
					++i1;
				}
				if (b[i1] == SPACE) {
					++i1;
					while ((b[i1] >= '0') && (b[i1] <= '8')) {
						filedatetime = ( filedatetime * 8 ) + static_cast<long>( b[i1] - '0' );
						++i1;
					}
					i1 += timezone + 5 * 60 * 60;
				}
				rputch( CF );
			} else if ( ( bn & 0x00ff ) == static_cast<unsigned int>( bln ) ) {
				file.Seek( pos, WFile::seekBegin );
				long lx = reallen - pos;
				i2 = ( i == 0 ) ? 128 : 1024;
				if ( ( static_cast<long>( i2 ) > lx ) && reallen ) {
					i2 = static_cast<int>( lx );
				}
				file.Write( b, i2 );
				pos += static_cast<long>( i2 );
				++bn;
				rputch( CF );
			} else if (((bn - 1) & 0x00ff) == static_cast<unsigned int>( bln ) ) {
				rputch( CF );
			} else {
				rputch( CX );
				ok = false;
				done = true;
			}
			nConsecErrors = 0;
		} else if ( i == 2 || i == 7 || i == 3 ) {
			if ( pos == 0L && reallen == 0L && bUseCRC ) {
				rputch( 'C' );
			} else {
				rputch( CU );
			}
			++nConsecErrors;
			++nTotalErrors;
			if ( nConsecErrors > 9 ) {
				rputch( CX );
				ok = false;
				done = true;
			}
		} else if ( i == CF ) {
			ok = false;
			done = true;
			rputch( CX );
		} else if (i == 4) {
			if (lastcan) {
				ok = false;
				done = true;
				rputch( CF );
			} else {
				lastcan = true;
				rputch( CU );
			}
		} else  if (i == 5) {
			lasteot = true;
			if (lasteot) {
				done = true;
				rputch( CF );
			} else {
				lasteot = true;
				rputch( CU );
			}
		} else if (i == 8) {
			// This used to be where the filetype was set.
            //*ft = bln;
			rputch( CF );
			nConsecErrors = 0;
		} else if (i == 9) {
			dump();
		}
		if (i != 4) {
			lastcan = false;
		}
		if (i != 5) {
			lasteot = false;
		}
	} while ( !hangup && !done );
	GetSession()->localIO()->LocalGotoXY(nOldXPos, nOldYPos);
	if (ok) {
		if (filedatetime) {
			WWIV_SetFileTime( pszFileName, filedatetime );
		}
		file.Close();
		*received = true;
	} else {
		file.Close();
		file.Delete();
		*received = false;
	}
}


void zmodem_receive(const char *pszFileName, bool *received ) {
	char *pszWorkingFileName = WWIV_STRDUP( pszFileName );
	StringRemoveWhitespace( pszWorkingFileName );

	bool bOldBinaryMode = GetSession()->remoteIO()->GetBinaryMode();
	GetSession()->remoteIO()->SetBinaryMode( true );
	bool bResult = NewZModemReceiveFile( pszWorkingFileName );
	GetSession()->remoteIO()->SetBinaryMode( bOldBinaryMode );

	*received = ( bResult ) ? true : false;
	BbsFreeMemory( pszWorkingFileName );
}

