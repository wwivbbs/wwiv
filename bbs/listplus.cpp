/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2007, WWIV Software Services             */
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
#include "listplus.h"


user_config config_listing;
int list_loaded;

int bulk_move = 0;
int bulk_dir = -1;

int extended_desc_used, lc_lines_used;
bool ext_is_on;

struct listplus_config lp_config;

static char _on_[] = "ON!";
static char _off_[] = "OFF";

int lp_config_loaded;

#ifdef FILE_POINTS
long fpts;
#endif	// FILE_POINTS

char *lp_color_list[] =
{
		"Black   ",
		"Blue    ",
		"Green   ",
		"Cyan    ",
		"Red     ",
		"Purple  ",
		"Brown   ",
		"L-Gray  ",
		"D-Gray  ",
		"L-Blue  ",
		"L-Green ",
		"L-Cyan  ",
		"L-Red   ",
		"L-Purple",
		"Yellow  ",
		"White   "
};

// TODO remove this hack and fix the real problem of fake spaces in filenames everywhere
bool ListPlusExist( const char *pszFileName )
{
    char szRealFileName[ MAX_PATH ];
    strcpy( szRealFileName, pszFileName );
    StringRemoveWhitespace( szRealFileName );
    return ( *szRealFileName ) ? WFile::Exists( szRealFileName ) : false;
}


void colorize_foundtext(char *text, struct search_record * search_rec, int color)
{
	int size;
	char *pszTempBuffer, found_color[10], normal_color[10], *tok;
	char find[101], word[101];

	sprintf(found_color, "|%02d|%02d", lp_config.found_fore_color, lp_config.found_back_color);
	sprintf(normal_color, "|16|%02d", color);

	if (lp_config.colorize_found_text)
	{
		strcpy(find, search_rec->search);
		tok = strtok(find, "&|!()");

		while (tok)
		{
			pszTempBuffer = text;
			strcpy(word, tok);
			StringTrim(word);

			while (pszTempBuffer && word[0])
			{
				if ((pszTempBuffer = stristr(pszTempBuffer, word)) != NULL)
				{
					size = strlen(pszTempBuffer) + 1;
					memmove(&pszTempBuffer[6], &pszTempBuffer[0], size);
					strncpy(pszTempBuffer, found_color, 6);
					pszTempBuffer += strlen(word) + 6;
					size = strlen(pszTempBuffer) + 1;
					memmove(&pszTempBuffer[6], &pszTempBuffer[0], size);
					strncpy(pszTempBuffer, normal_color, 6);
					pszTempBuffer += 6;
					++pszTempBuffer;
				}
			}
			tok = strtok(NULL, "&|!()");
		}
	}
}


void printtitle_plus_old()
{
	GetSession()->bout << "|16|15" << charstr( 79, 'Ü' ) << wwiv::endl;

	char szBuffer[255];
	sprintf(szBuffer, "Area %d : %-30.30s (%ld files)", atoi(udir[GetSession()->GetCurrentFileArea()].keys),
			directories[udir[GetSession()->GetCurrentFileArea()].subnum].name, GetSession()->numf);
	GetSession()->bout.WriteFormatted( "|23|01 ù %-56s Space=Tag/?=Help ù \r\n", szBuffer );

	if (config_listing.lp_options & cfl_header)
	{
		build_header();
	}

	GetSession()->bout << "|16|08" << charstr(79, 'ß') << wwiv::endl;
	GetSession()->bout.Color( 0 );
}


void printtitle_plus()
{
	if (GetSession()->localIO()->WhereY() != 0 || GetSession()->localIO()->WhereX() != 0)
	{
		GetSession()->bout.ClearScreen();
	}

	if (config_listing.lp_options & cfl_header)
	{
		printtitle_plus_old();
	}
	else
	{
		char szBuffer[255];
		sprintf( szBuffer, "Area %d : %-30.30s (%ld files)", atoi(udir[GetSession()->GetCurrentFileArea()].keys),
				 directories[udir[GetSession()->GetCurrentFileArea()].subnum].name, GetSession()->numf );
		GetSession()->bout.DisplayLiteBar( " %-54s Space=Tag/?=Help ", szBuffer );
		GetSession()->bout.Color( 0 );
	}
}


void build_header()
{
	char szHeader[255];
	int desc_pos = 30;

	strcpy(szHeader, " Tag # ");

	if (config_listing.lp_options & cfl_fname)
	{
		strcat( szHeader, "FILENAME" );
	}

	if (config_listing.lp_options & cfl_extension)
	{
		strcat(szHeader, ".EXT ");
	}

	if (config_listing.lp_options & cfl_dloads)
	{
		strcat(szHeader, " DL ");
	}

	if (config_listing.lp_options & cfl_kbytes)
	{
		strcat(szHeader, "Bytes ");
	}

#ifdef FILE_POINTS
	if (config_listing.lp_options & cfl_file_points)
	{
		strcat(szHeader, "Fpts ");
	}
#endif

	if (config_listing.lp_options & cfl_days_old)
	{
		strcat(szHeader, "Age ");
	}

	if (config_listing.lp_options & cfl_times_a_day_dloaded)
	{
		strcat(szHeader, "DL'PD ");
	}

	if (config_listing.lp_options & cfl_days_between_dloads)
	{
		strcat(szHeader, "DBDLS ");
	}

	if (config_listing.lp_options & cfl_description)
	{
		desc_pos = strlen( szHeader );
		strcat( szHeader, "Description" );
	}
	StringJustify( szHeader, 79, ' ', JUSTIFY_LEFT );
	GetSession()->bout << "|23|01" << szHeader << wwiv::endl;

	szHeader[0] = '\0';

	if (config_listing.lp_options & cfl_date_uploaded)
	{
		strcat(szHeader, "Date Uploaded      ");
	}
	if (config_listing.lp_options & cfl_upby)
	{
		strcat(szHeader, "Who uploaded");
	}

	if ( szHeader[0] )
	{
		StringJustify(szHeader, desc_pos + strlen(szHeader), ' ', JUSTIFY_RIGHT);
		StringJustify(szHeader, 79, ' ', JUSTIFY_LEFT);
		GetSession()->bout << "|23|01" << szHeader << wwiv::endl;
	}
}


int first_file_pos()
{
	int i = FIRST_FILE_POS;

	if ( config_listing.lp_options & cfl_header )
	{
		i += lp_configured_lines();
	}

	return i;
}


int lp_configured_lines()
{
	if (config_listing.lp_options & cfl_date_uploaded
		|| config_listing.lp_options & cfl_upby)
	{
		return 2;
	}

	return 1;
}


void print_searching(struct search_record * search_rec)
{
	if (GetSession()->localIO()->WhereY() != 0 || GetSession()->localIO()->WhereX() != 0)
	{
		GetSession()->bout.ClearScreen();
	}

	if (strlen(search_rec->search) > 3)
	{
		GetSession()->bout << "|#1Search keywords : |#2" << search_rec->search;
		GetSession()->bout.NewLine( 2 );
	}
	GetSession()->bout << "|#1<Space> aborts  : ";
	GetSession()->bout.WriteFormatted(" |B1|15%-40.40s|B0|#0", directories[udir[GetSession()->GetCurrentFileArea()].subnum].name);
}


void catch_divide_by_zero(int x)
{
	if ( x != x )
	{
		x = x;
	}
	sysoplog( "Caught divide by 0" );
}


int listfiles_plus(int type)
{
	int save_topdata = GetSession()->topdata;
	int save_dir = GetSession()->GetCurrentFileArea();
	long save_status = GetSession()->GetCurrentUser()->GetStatus();

	GetSession()->tagging = 0;

	ext_is_on = GetSession()->GetCurrentUser()->GetFullFileDescriptions();
	signal(SIGFPE, catch_divide_by_zero);

	GetSession()->topdata = WLocalIO::topdataNone;
	GetApplication()->UpdateTopScreen();
	GetSession()->bout.ClearScreen();

	int nReturn = listfiles_plus_function(type);

#ifdef FAST_EXTENDED_DESCRIPTION
	lp_zap_ed_info();
#endif

	GetSession()->bout.Color( 0 );
	GetSession()->bout.GotoXY( 1, GetSession()->GetCurrentUser()->GetScreenLines() - 3 );
	GetSession()->bout.NewLine( 3 );

	lines_listed = 0;

	if (type != NSCAN_NSCAN)
    {
		tmp_disable_conf( false );
    }

	GetSession()->GetCurrentUser()->SetStatus( save_status );

    if ( type == NSCAN_DIR || type == SEARCH_ALL )    // change Build3
    {
	    GetSession()->SetCurrentFileArea( save_dir );
    }
	dliscan();

	GetSession()->topdata = save_topdata;
	GetApplication()->UpdateTopScreen();

	return nReturn;
}


void undrawfile(int filepos, int filenum)
{
	lines_listed = 0;
	GetSession()->bout.GotoXY(4, filepos + first_file_pos());
	GetSession()->bout.WriteFormatted("|%2d%3d|#0", lp_config.file_num_color, filenum);
}


int lp_add_batch(const char *pszFileName, int dn, long fs)
{
	double t;

	if (find_batch_queue(pszFileName) > -1)
	{
		return 0;
	}

	if (GetSession()->numbatch >= GetSession()->max_batch)
	{
		GetSession()->bout.GotoXY(1, GetSession()->GetCurrentUser()->GetScreenLines() - 1);
		GetSession()->bout << "No room left in batch queue.\r\n";
		pausescr();
	}
	else if (!ratio_ok())
	{
		pausescr();
	}
	else
	{
		if (modem_speed && fs)
		{
			t = (9.0) / ((double) (modem_speed)) * ((double) (fs));
		}
		else
		{
			t = 0.0;
		}

#ifdef FILE_POINTS
        if ( ( GetSession()->GetCurrentUser()->GetFilePoints() < ( batchfpts + fpts ) )
			&& !GetSession()->GetCurrentUser()->IsExemptRatio() )
		{
			GetSession()->bout.GotoXY(1, GetSession()->GetCurrentUser()->GetScreenLines() - 1);
			GetSession()->bout << "Not enough file points to download this file\r\n";
			pausescr();
		}
		else
#endif

			if ((nsl() <= (batchtime + t)) && (!so()))
			{
				GetSession()->bout.GotoXY(1, GetSession()->GetCurrentUser()->GetScreenLines() - 1);
				GetSession()->bout << "Not enough time left in queue.\r\n";
				pausescr();
			}
			else
			{
				if (dn == -1)
				{
					GetSession()->bout.GotoXY(1, GetSession()->GetCurrentUser()->GetScreenLines() - 1);
					GetSession()->bout << "Can't add temporary file to batch queue.\r\n";
					pausescr();
				}
				else
				{
					batchtime += static_cast<float>( t );

#ifdef FILE_POINTS
					batchfpts += fpts;
#endif

					strcpy(batch[GetSession()->numbatch].filename, pszFileName);
					batch[GetSession()->numbatch].dir = static_cast<short>( dn );
					batch[GetSession()->numbatch].time = static_cast<float>( t );
					batch[GetSession()->numbatch].sending = 1;
					batch[GetSession()->numbatch].len = fs;

#ifdef FILE_POINTS
					batch[GetSession()->numbatch].filepoints = fpts;
#endif

					GetSession()->numbatch++;

#ifdef KBPERDAY
					kbbatch += bytes_to_k(fs);
#endif
					++GetSession()->numbatchdl;
					return 1;
				}
			}
	}
	return 0;
}


int printinfo_plus(uploadsrec * u, int filenum, int marked, int LinesLeft, struct search_record * search_rec)
{
	char szBuffer[MAX_PATH], szFileName[MAX_PATH], szFileExt[MAX_EXT];
	char szFileInformation[1024], element[150];
	int chars_this_line = 0, numl = 0, cpos = 0, will_fit = 78;
    char ch = 0;
	int char_printed = 0, extdesc_pos;

	strcpy(szFileName, u->filename);
	if ( !szFileName[0] )
	{
		// Make sure the filename isn't empty, if it is then lets bail!
		return numl;
	}

	char * str = strchr(szFileName, '.');
	if ( str && *str )
	{
		str[0] = 0;
		++str;
		strcpy(szFileExt, str);
	}
	else
	{
		strcpy( szFileExt, "   " );
	}

	time_t tTimeNow = time(NULL);
	long lDiffTime = static_cast<long>( difftime(tTimeNow, u->daten) );
	int nDaysOld = lDiffTime / SECONDS_PER_DAY;

	sprintf(szFileInformation, "|%2d %c |%2d%3d ", lp_config.tagged_color, marked ? '\xFE' : ' ', lp_config.file_num_color, filenum);
	int width = 7;
	lines_listed = 0;

	if (config_listing.lp_options & cfl_fname)
	{
		strcpy( szBuffer, szFileName );
		StringJustify(szBuffer, 8, ' ', JUSTIFY_LEFT);
		if (search_rec)
		{
			colorize_foundtext(szBuffer, search_rec, config_listing.lp_colors[0]);
		}
		sprintf(element, "|%02d%s", config_listing.lp_colors[0], szBuffer);
		strcat(szFileInformation, element);
		width += 8;
	}
	if (config_listing.lp_options & cfl_extension)
	{
		strcpy(szBuffer, szFileExt);
		StringJustify(szBuffer, 3, ' ', JUSTIFY_LEFT);
		if (search_rec)
		{
			colorize_foundtext(szBuffer, search_rec, config_listing.lp_colors[1]);
		}
		sprintf(element, "|%02d.%s", config_listing.lp_colors[1], szBuffer);
		strcat(szFileInformation, element);
		width += 4;
	}
	if (config_listing.lp_options & cfl_dloads)
	{
		sprintf(szBuffer, "%3d", u->numdloads);
		szBuffer[3] = 0;
		sprintf(element, " |%02d%s", config_listing.lp_colors[2], szBuffer);
		strcat(szFileInformation, element);
		width += 4;
	}
	if (config_listing.lp_options & cfl_kbytes)
	{
		sprintf(szBuffer, "%4ldk", static_cast<long>( bytes_to_k( u->numbytes ) ) );
		szBuffer[5] = 0;
		if (!(directories[udir[GetSession()->GetCurrentFileArea()].subnum].mask & mask_cdrom))
		{
			char szTempFile[MAX_PATH];

			strcpy(szTempFile, directories[udir[GetSession()->GetCurrentFileArea()].subnum].path);
			strcat(szTempFile, u->filename);
            unalign( szTempFile );
			if (lp_config.check_exist)
			{
				if (!ListPlusExist(szTempFile))
				{
					strcpy(szBuffer, "OFFLN");
				}
			}
		}
		sprintf(element, " |%02d%s", config_listing.lp_colors[3], szBuffer);
		strcat(szFileInformation, element);
		width += 6;
	}
#ifdef FILE_POINTS
	if (config_listing.lp_options & cfl_file_points)
	{
		if (u->mask & mask_validated)
		{
			if (u->filepoints)
			{
				sprintf(szBuffer, "%4u", u->filepoints);
				szBuffer[4] = 0;
			}
			else
			{
				sprintf(szBuffer, "Free");
			}
		}
		else
		{
			sprintf(szBuffer, "9e99");
		}
		sprintf(element, " |%02d%s", config_listing.lp_colors[5], szBuffer);
		strcat(szFileInformation, element);
		width += 5;
	}
#endif

	if (config_listing.lp_options & cfl_days_old)
	{
		sprintf(szBuffer, "%3d", nDaysOld);
		szBuffer[3] = 0;
		sprintf(element, " |%02d%s", config_listing.lp_colors[6], szBuffer);
		strcat(szFileInformation, element);
		width += 4;
	}
	if (config_listing.lp_options & cfl_times_a_day_dloaded)
	{
		float t;

		t = nDaysOld ? (float) u->numdloads / (float) nDaysOld : (float) 0.0;
		sprintf(szBuffer, "%2.2f", t);
		szBuffer[5] = 0;
		sprintf(element, " |%02d%s", config_listing.lp_colors[8], szBuffer);
		strcat(szFileInformation, element);
		width += 6;
	}
	if (config_listing.lp_options & cfl_days_between_dloads)
	{
		float t;

		t = nDaysOld ? (float) u->numdloads / (float) nDaysOld : (float) 0.0;
		t = t ? (float) 1 / (float) t : (float) 0.0;
		sprintf(szBuffer, "%3.1f", t);
		szBuffer[5] = 0;
		sprintf(element, " |%02d%s", config_listing.lp_colors[9], szBuffer);
		strcat(szFileInformation, element);
		width += 6;
	}
	if (config_listing.lp_options & cfl_description)
	{
		++width;
		strcpy(szBuffer, u->description);
		if (search_rec)
		{
			colorize_foundtext(szBuffer, search_rec, config_listing.lp_colors[10]);
		}
		sprintf(element, " |%02d%s", config_listing.lp_colors[10], szBuffer);
		strcat(szFileInformation, element);
		extdesc_pos = width;
	}
	else
	{
		extdesc_pos = -1;
	}

	strcat(szFileInformation, "\r\n");
	cpos = 0;
	while (szFileInformation[cpos] && numl < LinesLeft)
	{
		do
		{
			ch = szFileInformation[cpos];
			if (!ch)
			{
				continue;
			}
			++cpos;
		} while (ch == '\r' && ch);

		if (!ch)
		{
			break;
		}

		if ( ch == SOFTRETURN )
		{
			GetSession()->bout.NewLine();
			chars_this_line = 0;
			char_printed = 0;
			++numl;
		}
		else if (chars_this_line > will_fit && ch)
		{
			do
			{
				ch = szFileInformation[cpos++];
			} while (ch != '\n' && ch != 0);
			--cpos;
		}
		else if (ch)
		{
			chars_this_line += bputch(ch);
		}
	}

	if (extdesc_pos > 0)
	{
		int num_extended, lines_left;
		int lines_printed;

		lines_left = LinesLeft - numl;
		num_extended = GetSession()->GetCurrentUser()->GetNumExtended();
		if (num_extended < lp_config.show_at_least_extended)
		{
			num_extended = lp_config.show_at_least_extended;
		}
		if (num_extended > lines_left)
		{
			num_extended = lines_left;
		}
		if (ext_is_on && mask_extended & u->mask)
		{
			lines_printed = print_extended_plus(u->filename, num_extended, -extdesc_pos, config_listing.lp_colors[10], search_rec);
		}
		else
		{
			lines_printed = 0;
		}

		if (lines_printed)
		{
			numl += lines_printed;
			chars_this_line = 0;
			char_printed = 0;
		}
	}
	if (GetSession()->localIO()->WhereX())
	{
		if (char_printed)
		{
			GetSession()->bout.NewLine();
			++numl;
		}
		else
		{
			bputch('\r');
		}
	}
	szBuffer[0] = 0;
	szFileInformation[0] = 0;

	if (config_listing.lp_options & cfl_date_uploaded)
	{
		if ((u->actualdate[2] == '/') && (u->actualdate[5] == '/'))
		{
			sprintf(szBuffer, "UL: %s  New: %s", u->date, u->actualdate );
			StringJustify(szBuffer, 27, ' ', JUSTIFY_LEFT);
		}
		else
		{
			sprintf(szBuffer, "UL: %s", u->date);
			StringJustify(szBuffer, 12, ' ', JUSTIFY_LEFT);
		}
		sprintf(element, "|%02d%s  ", config_listing.lp_colors[4], szBuffer);
		strcat(szFileInformation, element);
	}

	if (config_listing.lp_options & cfl_upby)
	{
		if (config_listing.lp_options & cfl_date_uploaded)
		{
			StringJustify(szFileInformation, strlen(szFileInformation) + width, ' ', JUSTIFY_RIGHT);
			GetSession()->bout << szFileInformation;
			GetSession()->bout.NewLine();
			++numl;
		}
        std::string tmp = u->upby;
        tmp = properize( tmp ).substr(0, 15);
		sprintf(element, "|%02dUpby: %s", config_listing.lp_colors[7], tmp);
		strcpy(szFileInformation, element);
	}

	if (szBuffer[0])
	{
		StringJustify(szFileInformation, strlen(szFileInformation) + width, ' ', JUSTIFY_RIGHT);
		GetSession()->bout << szFileInformation;
		GetSession()->bout.NewLine();
		++numl;
	}
	return numl;
}

void CheckLPColors()
{
	for ( int i = 0; i < 32; i++ )
	{
		if ( config_listing.lp_colors[i] == 0 )
		{
			config_listing.lp_colors[i] = 7;
		}
	}
}

int load_config_listing(int config)
{
	int len;

	unload_config_listing();
	memset( &config_listing, 0, sizeof( user_config ) );

	for (int fh = 0; fh < 32; fh++)
	{
		config_listing.lp_colors[fh] = 7;
	}

	config_listing.lp_options = cfl_fname | cfl_extension | cfl_dloads | cfl_kbytes | cfl_description;

	if (!config)
	{
		return 0;
	}

    WFile fileConfig( syscfg.datadir, CONFIG_USR );

    if ( fileConfig.Exists() )
    {
	    WUser user;
        GetApplication()->GetUserManager()->ReadUser( &user, config );
		if ( fileConfig.Open( WFile::modeBinary | WFile::modeReadOnly ) )
		{
			fileConfig.Seek( config * sizeof(user_config), WFile::seekBegin );
			len = fileConfig.Read( &config_listing, sizeof( user_config ) );
			fileConfig.Close();
			if ( len != sizeof( user_config ) ||
                 !wwiv::stringUtils::IsEqualsIgnoreCase( config_listing.name, user.GetName() ) )
			{
				memset( &config_listing, 0, sizeof( config_listing ) );
				strcpy( config_listing.name, user.GetName() );
				CheckLPColors();
				return 0;
			}
			list_loaded = config;
			extended_desc_used = config_listing.lp_options & cfl_description;
			config_listing.lp_options |= cfl_fname;
			config_listing.lp_options |= cfl_description;
			CheckLPColors();
			return 1;
		}
	}
	CheckLPColors();
	return 0;
}


void write_config_listing(int config)
{
	if (!config)
	{
		return;
	}

    WUser user;
    GetApplication()->GetUserManager()->ReadUser( &user, config );
    strcpy( config_listing.name, user.GetName() );

	WFile fileUserConfig( syscfg.datadir, CONFIG_USR );
	if ( !fileUserConfig.Open( WFile::modeBinary|WFile::modeCreateFile|WFile::modeReadWrite, WFile::shareUnknown, WFile::permReadWrite ) )
	{
		return;
	}

	config_listing.lp_options |= cfl_fname;
	config_listing.lp_options |= cfl_description;

	fileUserConfig.Seek( config * sizeof(user_config), WFile::seekBegin );
	fileUserConfig.Write( &config_listing, sizeof( user_config ) );
	fileUserConfig.Close();
}


void unload_config_listing()
{
	list_loaded = 0;
	memset(&config_listing, 0, sizeof(user_config));
}


int print_extended_plus(const char *pszFileName, int numlist, int indent, int color, struct search_record * search_rec)
{
	int numl = 0;
	int cpos = 0;
	int chars_this_line = 0;

	int will_fit = 80 - abs(indent) - 2;

	char * ss = READ_EXTENDED_DESCRIPTION(pszFileName);

	if (!ss)
	{
		return 0;
	}

	int strip_pos = strlen( ss ) - 1;

	while ( ss[strip_pos] && strip_pos > 0 )
	{
		int temp_char = ss[strip_pos];
		if (isspace(temp_char))
		{
			ss[strip_pos] = '\0';
		}
		else
		{
			break;
		}
		--strip_pos;
	}

	if ( ss )
	{
		int nBufferSize = strlen( ss );
		if ( nBufferSize > MAX_EXTENDED_SIZE )
		{
			nBufferSize = MAX_EXTENDED_SIZE;
			ss[nBufferSize] = '\0';
		}
		char* new_ss = static_cast<char *>( BbsAllocA( ( nBufferSize * 4 ) + 30 ) );
		WWIV_ASSERT( new_ss );
		if ( new_ss )
		{
			strcpy( new_ss, ss );
			if ( search_rec )
			{
				colorize_foundtext( new_ss, search_rec, color );
			}
			if ( indent > -1 && indent != 16 )
			{
				GetSession()->bout << "  |#1Extended Description:\n\r";
			}
			char ch = SOFTRETURN;

			while ( new_ss[cpos] && numl < numlist && !hangup )
			{
				if ( ch == SOFTRETURN && indent )
				{
					GetSession()->bout.SystemColor(color);
					bputch('\r');
					GetSession()->bout << "\x1b[" << abs( indent ) << "C";
				}
				do
				{
					ch = new_ss[cpos++];
				} while ( ch == '\r' && !hangup );

				if ( ch == SOFTRETURN )
				{
					GetSession()->bout.NewLine();
					chars_this_line = 0;
					++numl;
				}
				else if ( chars_this_line > will_fit )
				{
					do
					{
						ch = new_ss[cpos++];
					} while ( ch != '\n' && ch != 0 );
					--cpos;
				}
				else
				{
						chars_this_line += bputch( ch );
				}
			}

			if ( GetSession()->localIO()->WhereX() )
			{
				GetSession()->bout.NewLine();
				++numl;
			}
			BbsFreeMemory( new_ss );
			BbsFreeMemory( ss );	// frank's gpf is here.
		}
	}
	GetSession()->bout.Color( 0 );
	return numl;
}


void show_fileinfo(uploadsrec * u)
{
	GetSession()->bout.ClearScreen();
	repeat_char( 'Í', 78 );
	GetSession()->bout << "  |#1Filename    : |#2" << u->filename << wwiv::endl;
	GetSession()->bout << "  |#1Uploaded on : |#2" << u->date << " by |#2" << u->upby << wwiv::endl;
	if ( u->actualdate[2] == '/' && u->actualdate[5] == '/' )
	{
		GetSession()->bout << "  |#1Newest file : |#2" << u->actualdate << wwiv::endl;
	}
	GetSession()->bout << "  |#1Size        : |#2" << bytes_to_k( u->numbytes ) << wwiv::endl;
	GetSession()->bout << "  |#1Downloads   : |#2" << u->numdloads << "|#1" << wwiv::endl;
	GetSession()->bout << "  |#1Description : |#2" << u->description << wwiv::endl;
	print_extended_plus(u->filename, 255, 16, YELLOW, NULL);
	repeat_char( 'Í', 78 );
	pausescr();
}

int check_lines_needed(uploadsrec * u)
{
	char *ss, *tmp;
	int elines = 0;
	int max_extended;

	lc_lines_used = lp_configured_lines();
	int max_lines = calc_max_lines();
	int num_extended = GetSession()->GetCurrentUser()->GetNumExtended();

	if (num_extended < lp_config.show_at_least_extended)
	{
		num_extended = lp_config.show_at_least_extended;
	}

	if (max_lines > num_extended)
	{
		max_lines = num_extended;
	}

	if (extended_desc_used)
	{
		max_extended = GetSession()->GetCurrentUser()->GetNumExtended();

		if (max_extended < lp_config.show_at_least_extended)
		{
			max_extended = lp_config.show_at_least_extended;
		}

		if (ext_is_on && mask_extended & u->mask)
		{
			ss = READ_EXTENDED_DESCRIPTION(u->filename);
		}
		else
		{
			ss = NULL;
		}

		if (ss)
		{
			tmp = ss;
			while ((elines < max_extended) && ((tmp = strchr(tmp, '\r')) != NULL))
			{
				++elines;
				++tmp;
			}
			BbsFreeMemory(ss);
		}
	}
	if (lc_lines_used + elines > max_lines)
	{
		return max_lines - 1;
	}

	return lc_lines_used + elines;
}

static int ed_num;
static ext_desc_rec *ed_info;
static char last_g_szExtDescrFileName[131];
static WFile fileExt;

char *lp_read_extended_description(const char *pszFileName)
{
	ext_desc_type ed;
	long l;
	char *ss = NULL;

	lp_get_ed_info();

	if ( ed_info && fileExt.IsOpen() )
	{
		for (int i = 0; i < ed_num; i++)
		{
			if ( wwiv::stringUtils::IsEquals( pszFileName, ed_info[i].name ) )
			{
				fileExt.Seek( ed_info[i].offset, WFile::seekBegin );
				l = fileExt.Read( &ed, sizeof( ext_desc_type ) );

				if ( l == sizeof( ext_desc_type ) && wwiv::stringUtils::IsEquals(pszFileName, ed.name) )
				{
					ss = static_cast<char *>( BbsAllocA( ed.len + 10 ) );
					WWIV_ASSERT( ss );
					if (ss)
					{
						fileExt.Read( ss, ed.len );
						ss[ed.len] = 0;
					}
					return ss;
				}
				else
				{
					break;
				}
			}
		}
	}
	return NULL;
}

void lp_zap_ed_info()
{
	if (ed_info)
	{
		BbsFreeMemory(ed_info);
		ed_info = NULL;
	}
	if ( fileExt.IsOpen() )
	{
		fileExt.Close();
	}

	last_g_szExtDescrFileName[0] = 0;
	ed_num = 0;
}

void lp_get_ed_info()
{
	long l, l1;
	ext_desc_type ed;

	if ( !wwiv::stringUtils::IsEquals( last_g_szExtDescrFileName, g_szExtDescrFileName ) )
	{
		lp_zap_ed_info();
		strcpy( last_g_szExtDescrFileName, g_szExtDescrFileName );

		if ( !GetSession()->numf )
		{
			return;
		}

		l = 0;

		fileExt.SetName( g_szExtDescrFileName );
		fileExt.Open( WFile::modeBinary | WFile::modeReadOnly );

		if ( fileExt.IsOpen() )
		{
			l1 = fileExt.GetLength();
			if (l1 > 0)
			{
				ed_info = static_cast<ext_desc_rec *>( BbsAllocA( GetSession()->numf * sizeof( ext_desc_rec ) ) );
				WWIV_ASSERT(ed_info);
				if (ed_info == NULL)
				{
					fileExt.Close();
					return;
				}
				ed_num = 0;
				while ((l < l1) && (ed_num < GetSession()->numf))
				{
					fileExt.Seek( l, WFile::seekBegin );
					if ( fileExt.Read(&ed, sizeof( ext_desc_type ) ) == sizeof( ext_desc_type ) )
					{
						strcpy(ed_info[ed_num].name, ed.name);
						ed_info[ed_num].offset = l;
						l += static_cast<long>( ed.len + sizeof( ext_desc_type ) );
						ed_num++;
					}
				}
			}
		}
	}
}


void prep_menu_items(char **menu_items)
{
	strcpy(menu_items[0], "Next");
	strcpy(menu_items[1], "Prev");
	strcpy(menu_items[2], "Tag");
	strcpy(menu_items[3], "Info");
	strcpy(menu_items[4], "ViewZip");

	if (GetSession()->using_modem != 0)
	{
		strcpy(menu_items[5], "Dload");
	}
	else
	{
		strcpy(menu_items[5], "Move");
	}

	strcpy(menu_items[6], "+Dir");
	strcpy(menu_items[7], "-Dir");
	strcpy(menu_items[8], "Full-Desc");
	strcpy(menu_items[9], "Quit");
	strcpy(menu_items[10], "?");
	if (so())
	{
		strcpy(menu_items[11], "Sysop");
		menu_items[13][0] = 0;
	}
	else
	{
		menu_items[12][0] = 0;
	}
}

int prep_search_rec(struct search_record * search_rec, int type)
{
	memset(search_rec, 0, sizeof(struct search_record));
	search_rec->search_extended = lp_config.search_extended_on;

	if (type == LIST_DIR)
	{
		file_mask(search_rec->filemask);
		search_rec->alldirs = THIS_DIR;
	}
	else if (type == SEARCH_ALL)
	{
		search_rec->alldirs = ALL_DIRS;
		if (!search_criteria(search_rec))
		{
			return 0;
		}
	}
	else if (type == NSCAN_DIR)
	{
		search_rec->alldirs = THIS_DIR;
		search_rec->nscandate = nscandate;
	}
	else if (type == NSCAN_NSCAN)
	{
		g_flags |= g_flag_scanned_files;
		search_rec->nscandate = nscandate;
		search_rec->alldirs = ALL_DIRS;
	}
	else
	{
		sysoplog("Undef LP type");
		return 0;
	}

	if (!search_rec->filemask[0] && !search_rec->nscandate && !search_rec->search[0])
	{
		return 0;
	}

	if (search_rec->filemask[0])
	{
		if (strchr(search_rec->filemask, '.') == NULL)
		{
			strcat(search_rec->filemask, ".*");
		}
	}
	align(search_rec->filemask);
	return 1;
}

int calc_max_lines()
{
	int max_lines;

	if ( lp_config.max_screen_lines_to_show )
    {
		max_lines = std::min<int>( GetSession()->GetCurrentUser()->GetScreenLines(),
                                   lp_config.max_screen_lines_to_show ) -
                    ( first_file_pos() + 1 + STOP_LIST );
	}
    else
    {
		max_lines = GetSession()->GetCurrentUser()->GetScreenLines() - ( first_file_pos() + 1 + STOP_LIST );
    }

	return max_lines;
}

void sysop_configure()
{
    short color = 0;
	bool done = false;
	char s[201];

	if (!so())
    {
		return;
    }

	load_lp_config();

	while ( !done && !hangup )
    {
		GetSession()->bout.ClearScreen();
		printfile( LPSYSOP_NOEXT );
		GetSession()->bout.GotoXY( 38, 2 );
		GetSession()->bout.SystemColor( lp_config.normal_highlight );
		GetSession()->bout.WriteFormatted( "%3d", lp_config.normal_highlight );
		GetSession()->bout.GotoXY( 77, 2 );
		GetSession()->bout.SystemColor( lp_config.normal_menu_item );
		GetSession()->bout.WriteFormatted( "%3d", lp_config.normal_menu_item );
		GetSession()->bout.GotoXY( 38, 3 );
		GetSession()->bout.SystemColor( lp_config.current_highlight );
		GetSession()->bout.WriteFormatted( "%3d", lp_config.current_highlight );
		GetSession()->bout.GotoXY( 77, 3 );
		GetSession()->bout.SystemColor( lp_config.current_menu_item );
		GetSession()->bout.WriteFormatted( "%3d", lp_config.current_menu_item );
		GetSession()->bout.Color( 0 );
		GetSession()->bout.GotoXY( 38, 6 );
		GetSession()->bout.WriteFormatted( "|%2d%2d", lp_config.tagged_color, lp_config.tagged_color );
		GetSession()->bout.GotoXY(77, 6);
		GetSession()->bout.WriteFormatted("|%2d%2d", lp_config.file_num_color, lp_config.file_num_color);
		GetSession()->bout.GotoXY(38, 7);
		GetSession()->bout.WriteFormatted("|%2d%2d", lp_config.found_fore_color, lp_config.found_fore_color);
		GetSession()->bout.GotoXY(77, 7);
		GetSession()->bout.WriteFormatted("|%2d%2d", lp_config.found_back_color, lp_config.found_back_color);
		GetSession()->bout.GotoXY(38, 8);
		GetSession()->bout.SystemColor( lp_config.current_file_color );
		GetSession()->bout.WriteFormatted("%3d", lp_config.current_file_color);
		GetSession()->bout.GotoXY(38, 11);
		GetSession()->bout.WriteFormatted("|#4%2d", lp_config.max_screen_lines_to_show);
		GetSession()->bout.GotoXY(77, 11);
		GetSession()->bout.WriteFormatted("|#4%2d", lp_config.show_at_least_extended);
		GetSession()->bout.GotoXY(74, 14);
		GetSession()->bout.WriteFormatted("|#4%s", lp_config.request_file ? _on_ : _off_);
		GetSession()->bout.GotoXY(74, 15);
		GetSession()->bout.WriteFormatted("|#4%s", lp_config.search_extended_on ? _on_ : _off_);
		GetSession()->bout.GotoXY(74, 16);
		GetSession()->bout.WriteFormatted("|#4%s", lp_config.edit_enable ? _on_ : _off_);
		GetSession()->bout.Color( 0 );
		GetSession()->bout.GotoXY(29, 14);
		GetSession()->bout.WriteFormatted("|#4%s", lp_config.no_configuration ? _on_ : _off_);
		GetSession()->bout.GotoXY(29, 15);
		GetSession()->bout.WriteFormatted("|#4%s", lp_config.colorize_found_text ? _on_ : _off_);
		GetSession()->bout.GotoXY(29, 16);
		GetSession()->bout.WriteFormatted("|#4%s", lp_config.simple_search ? _on_ : _off_);
		GetSession()->bout.GotoXY(29, 17);
		GetSession()->bout.WriteFormatted("|#4%s", lp_config.check_exist ? _on_ : _off_);
		GetSession()->bout.GotoXY(1, 19);
		GetSession()->bout << "|#1Q-Quit ";
		char key = onek( "Q\rABCDEFGHIJKLMNOPRS", true );
		switch (key)
		{
		case 'Q':
		case '\r':
			done = true;
			break;
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'I':
			color = SelectColor( 2 );
			if (color >= 0)
			{
				switch (key)
				{
				case 'A':
					lp_config.normal_highlight = color;
					break;
				case 'B':
					lp_config.normal_menu_item = color;
					break;
				case 'C':
					lp_config.current_highlight = color;
					break;
				case 'D':
					lp_config.current_menu_item = color;
					break;
				case 'I':
					lp_config.current_file_color = color;
					break;
				}
			}
			break;
		case 'E':
		case 'F':
		case 'G':
		case 'H':
			color = SelectColor( 1 );
			if (color >= 0)
			{
				switch (key)
				{
				case 'E':
					lp_config.tagged_color = color;
					break;
				case 'F':
					lp_config.file_num_color = color;
					break;
				case 'G':
					lp_config.found_fore_color = color;
					break;
				case 'H':
					lp_config.found_back_color = color + 16;
					break;
				}
			}
			break;
		case 'J':
			GetSession()->bout << "Enter max amount of lines to show (0=disabled) ";
			input( s, 2, true );
            lp_config.max_screen_lines_to_show = wwiv::stringUtils::StringToShort(s);
			break;
		case 'K':
			GetSession()->bout << "Enter minimum extended description lines to show ";
			input( s, 2, true );
			lp_config.show_at_least_extended = wwiv::stringUtils::StringToShort(s);
			break;
		case 'L':
			lp_config.no_configuration = !lp_config.no_configuration;
			break;
		case 'M':
			lp_config.request_file = !lp_config.request_file;
			break;
		case 'N':
			lp_config.colorize_found_text = !lp_config.colorize_found_text;
			break;
		case 'O':
			lp_config.search_extended_on = !lp_config.search_extended_on;
			break;
		case 'P':
			lp_config.simple_search = !lp_config.simple_search;
			break;
		case 'R':
			lp_config.edit_enable = !lp_config.edit_enable;
			break;
		case 'S':
			lp_config.check_exist = !lp_config.check_exist;
			break;
		}
  }
  save_lp_config();
  load_lp_config();
}


short SelectColor(int which)
{
	char ch, nc;

	GetSession()->bout.NewLine();

	if ( GetSession()->GetCurrentUser()->HasColor() )
	{
		color_list();
		GetSession()->bout.Color( 0 );
		GetSession()->bout.NewLine();
		GetSession()->bout << "|#2Foreground? ";
		ch = onek("01234567");
		nc = ch - '0';

		if (which == 2)
		{
			GetSession()->bout << "|#2Background? ";
			ch = onek("01234567");
			nc = nc | ((ch - '0') << 4);
		}
	}
	else
	{
		GetSession()->bout.NewLine();
		GetSession()->bout << "|#5Inversed? ";
		if (yesno())
		{
			if ((GetSession()->GetCurrentUser()->GetBWColor( 1 ) & 0x70) == 0)
			{
				nc = 0 | ((GetSession()->GetCurrentUser()->GetBWColor( 1 ) & 0x07) << 4);
			}
			else
			{
				nc = (GetSession()->GetCurrentUser()->GetBWColor( 1 ) & 0x70);
			}
		}
		else
		{
			if ((GetSession()->GetCurrentUser()->GetBWColor( 1 ) & 0x70) == 0)
			{
				nc = 0 | (GetSession()->GetCurrentUser()->GetBWColor( 1 ) & 0x07);
			}
			else
			{
				nc = ((GetSession()->GetCurrentUser()->GetBWColor( 1 ) & 0x70) >> 4);
			}
		}
	}

	GetSession()->bout << "|#5Intensified? ";
	if (yesno())
	{
		nc |= 0x08;
	}

	if (which == 2)
	{
		GetSession()->bout << "|#5Blinking? ";
		if (yesno())
		{
			nc |= 0x80;
		}
	}
	GetSession()->bout.NewLine();
	GetSession()->bout.SystemColor(nc);
	GetSession()->bout << DescribeColorCode( nc );
	GetSession()->bout.Color( 0 );
	GetSession()->bout.NewLine();

	GetSession()->bout << "|#5Is this OK? ";
	if (yesno())
	{
		return nc;
	}
	return -1;
}

void check_listplus()
{
    GetSession()->bout << "|#5Use listplus file GetSession()->tagging? ";

	if (noyes())
    {
		if ( GetSession()->GetCurrentUser()->IsUseListPlus() )
        {
            GetSession()->GetCurrentUser()->ClearStatusFlag( WUser::listPlus );
        }
	}
    else
    {
		if ( !GetSession()->GetCurrentUser()->IsUseListPlus() )
        {
            GetSession()->GetCurrentUser()->SetStatusFlag( WUser::listPlus );
        }
	}
}

void config_file_list()
{
	int key, which = -1;
	unsigned long bit = 0L;
	char action[51];
	uploadsrec u;

	strcpy( u.filename, "WWIV430.ZIP" );
	strcpy( u.description, "This is a sample description!" );
	strcpy( u.date, date() );
	strcpy( reinterpret_cast<char*>( u.upby ), GetSession()->GetCurrentUser()->GetUserNameAndNumber( GetSession()->usernum ) );
	u.numdloads = 50;
	u.numbytes = 655535L;
	u.daten = time( NULL ) - 10000;

	load_lp_config();

	if ( GetSession()->usernum != list_loaded )
	{
		if ( !load_config_listing( GetSession()->usernum ) )
		{
			load_config_listing( 1 );
		}
	}

	GetSession()->bout.ClearScreen();
	printfile( LPCONFIG_NOEXT );
	if ( !config_listing.lp_options & cfl_fname )
	{
		config_listing.lp_options |= cfl_fname;
	}

	if ( !( config_listing.lp_options & cfl_description ) )
	{
		config_listing.lp_options |= cfl_description;
	}

	action[0] = '\0';
	bool done = false;
	while ( !done && !hangup )
	{
		update_user_config_screen( &u, which );
		key = onek( "Q2346789H!@#$%^&*(" );
		switch ( key )
		{
		case '2':
		case '3':
		case '4':
		case '6':
		case '7':
		case '8':
		case '9':
		case 'H':
			switch ( key )
			{
			case '2':
				bit = cfl_extension;
				which = 2;
				break;
			case '3':
				bit = cfl_dloads;
				which = 3;
				break;
			case '4':
				bit = cfl_kbytes;
				which = 4;
				break;
			case '6':
				bit = cfl_date_uploaded;
				which = 6;
				break;
			case '7':
				bit = cfl_file_points;
				which = 7;
				break;
			case '8':
				bit = cfl_days_old;
				which = 8;
				break;
			case '9':
				bit = cfl_upby;
				which = 9;
				break;
			case 'H':
				bit = cfl_header;
				which = 10;
				break;
			}

			if ( config_listing.lp_options & bit )
			{
				config_listing.lp_options &= ~bit;
			}
			else
			{
				config_listing.lp_options |= bit;
			}
			break;

		case '!':
		case '@':
		case '#':
		case '$':
		case '%':
		case '^':
		case '&':
		case '*':
		case '(':
			switch ( key )
			{
			case '!':
				bit = 0;
				which = 1;
				break;
			case '@':
				bit = 1;
				which = 2;
				break;
			case '#':
				bit = 2;
				which = 3;
				break;
			case '$':
				bit = 3;
				which = 4;
				break;
			case '%':
				bit = 10;
				which = 5;
				break;
			case '^':
				bit = 4;
				which = 6;
				break;
			case '&':
				bit = 5;
				which = 7;
				break;
			case '*':
				bit = 6;
				which = 8;
				break;
			case '(':
				bit = 7;
				which = 9;
				break;
			}

			++config_listing.lp_colors[bit];
			if ( config_listing.lp_colors[bit] > 15 )
			{
				config_listing.lp_colors[bit] = 1;
			}
			break;
		case 'Q':
			done = true;
			break;
		}
	}
	list_loaded = GetSession()->usernum;
	write_config_listing(GetSession()->usernum);
	GetSession()->bout.NewLine( 4 );
}


void update_user_config_screen( uploadsrec * u, int which )
{
	struct search_record sr;

	memset( &sr, 0, sizeof( struct search_record ) );

	if ( which < 1 || which == 1 )
	{
		GetSession()->bout.GotoXY(37, 4);
		GetSession()->bout.SystemColor( static_cast<BYTE>( config_listing.lp_options & cfl_fname ? RED + ( BLUE << 4 ) : BLACK + ( BLUE << 4 ) ) );
		GetSession()->bout << "\xFE ";
		GetSession()->bout.SystemColor( BLACK + ( BLUE << 4 ) );
		GetSession()->bout << lp_color_list[ config_listing.lp_colors[ 0 ] ];
	}
	if ( which < 1 || which == 2 )
	{
		GetSession()->bout.GotoXY( 37, 5 );
		GetSession()->bout.SystemColor( static_cast<BYTE>( config_listing.lp_options & cfl_extension ? RED + ( BLUE << 4 ) : BLACK + ( BLUE << 4 ) ) );
		GetSession()->bout << "\xFE ";
		GetSession()->bout.SystemColor( BLACK + ( BLUE << 4 ) );
		GetSession()->bout << lp_color_list[ config_listing.lp_colors[ 1 ] ];
	}
	if ( which < 1 || which == 3 )
	{
		GetSession()->bout.GotoXY( 37, 6 );
		GetSession()->bout.SystemColor( static_cast<BYTE>( config_listing.lp_options & cfl_dloads ? RED + ( BLUE << 4 ) : BLACK + ( BLUE << 4 ) ) );
		GetSession()->bout << "\xFE ";
		GetSession()->bout.SystemColor( BLACK + ( BLUE << 4 ) );
		GetSession()->bout << lp_color_list[ config_listing.lp_colors[ 2 ] ];
	}
	if ( which < 1 || which == 4 )
	{
		GetSession()->bout.GotoXY( 37, 7 );
		GetSession()->bout.SystemColor( static_cast<BYTE>( config_listing.lp_options & cfl_kbytes ? RED + ( BLUE << 4 ) : BLACK + ( BLUE << 4 ) ) );
		GetSession()->bout << "\xFE ";
		GetSession()->bout.SystemColor( BLACK + ( BLUE << 4 ) );
		GetSession()->bout << lp_color_list[ config_listing.lp_colors[ 3 ] ];
	}
	if ( which < 1 || which == 5 )
	{
		GetSession()->bout.GotoXY( 37, 8 );
		GetSession()->bout.SystemColor( static_cast<BYTE>( config_listing.lp_options & cfl_description ? RED + ( BLUE << 4 ) : BLACK + ( BLUE << 4 ) ) );
		GetSession()->bout << "\xFE ";
		GetSession()->bout.SystemColor( BLACK + ( BLUE << 4 ) );
		GetSession()->bout << lp_color_list[ config_listing.lp_colors[ 10 ] ];
	}
	if ( which < 1 || which == 6 )
	{
		GetSession()->bout.GotoXY( 37, 9 );
		GetSession()->bout.SystemColor( static_cast<BYTE>( config_listing.lp_options & cfl_date_uploaded ? RED + ( BLUE << 4 ) : BLACK + ( BLUE << 4 ) ) );
		GetSession()->bout << "\xFE ";
		GetSession()->bout.SystemColor( BLACK + ( BLUE << 4 ) );
		GetSession()->bout << lp_color_list[ config_listing.lp_colors[ 4 ] ];
	}
	if ( which < 1 || which == 7 )
	{
		GetSession()->bout.GotoXY( 37, 10 );
		GetSession()->bout.SystemColor( static_cast<BYTE>( config_listing.lp_options & cfl_file_points ? RED + ( BLUE << 4 ) : BLACK + ( BLUE << 4 ) ) );
		GetSession()->bout << "\xFE ";
		GetSession()->bout.SystemColor( BLACK + ( BLUE << 4 ) );
		GetSession()->bout << lp_color_list[config_listing.lp_colors[5]];
	}
	if ( which < 1 || which == 8 )
	{
		GetSession()->bout.GotoXY(37, 11);
		GetSession()->bout.SystemColor( (config_listing.lp_options & cfl_days_old ? RED + (BLUE << 4) : BLACK + (BLUE << 4)) );
		GetSession()->bout << "\xFE ";
		GetSession()->bout.SystemColor(BLACK + (BLUE << 4));
		GetSession()->bout << lp_color_list[config_listing.lp_colors[6]];
	}
	if ( which < 1 || which == 9 )
	{
		GetSession()->bout.GotoXY( 37, 12 );
		GetSession()->bout.SystemColor( static_cast<BYTE>( config_listing.lp_options & cfl_upby ? RED + ( BLUE << 4 ) : BLACK + ( BLUE << 4 ) ) );
		GetSession()->bout << "\xFE ";
		GetSession()->bout.SystemColor( BLACK + ( BLUE << 4 ) );
		GetSession()->bout << lp_color_list[config_listing.lp_colors[7]];
	}
	if ( which < 1 || which == 10 )
	{
		GetSession()->bout.GotoXY( 37, 13 );
		GetSession()->bout.SystemColor( static_cast<BYTE>( config_listing.lp_options & cfl_header ? RED + ( BLUE << 4 ) : BLACK + ( BLUE << 4) ) );
		GetSession()->bout << "\xFE ";
		GetSession()->bout.SystemColor( BLACK + ( BLUE << 4 ) );
	}
	GetSession()->bout.SystemColor( YELLOW );
	GetSession()->bout.GotoXY( 1, 21 );
	GetSession()->bout.ClearEOL();
	GetSession()->bout.NewLine();
	GetSession()->bout.ClearEOL();
	GetSession()->bout.GotoXY( 1, 21 );
	printinfo_plus( u, 1, 1, 30, &sr );
	GetSession()->bout.GotoXY( 30, 17 );
	GetSession()->bout.SystemColor( YELLOW );
	GetSession()->bout.BackSpace();
}


int rename_filename( const char *pszFileName, int dn )
{
	char s[81], s1[81], s2[81], *ss, s3[81], ch;
	int i, cp, ret = 1;
	uploadsrec u;

#ifdef FAST_EXTENDED_DESCRIPTION
	lp_zap_ed_info();
#endif

	dliscan1( dn );
	strcpy( s, pszFileName );

	if ( s[0] == '\0' )
	{
		return ret;
	}

	if ( strchr( s, '.' ) == NULL )
	{
		strcat( s, ".*" );
	}
	align( s );

	strcpy( s3, s );
	i = recno( s );
	while ( i > 0 )
	{
		WFile fileDownload( g_szDownloadFileName );
		if ( !fileDownload.Open( WFile::modeBinary | WFile::modeReadOnly ) )
		{
			break;
		}
		cp = i;
		FileAreaSetRecord( fileDownload, i );
		fileDownload.Read( &u, sizeof( uploadsrec ) );
		fileDownload.Close();
		GetSession()->bout.NewLine();
		printfileinfo( &u, dn );
		GetSession()->bout.NewLine();
		GetSession()->bout << "|#5Change info for this file (Y/N/Q)? ";
		ch = ynq();
		if ( ch == 'Q' )
		{
			ret = 0;
			break;
		}
		else if ( ch == 'N' )
		{
			i = nrecno( s3, cp );
			continue;
		}
		GetSession()->bout.NewLine();
		GetSession()->bout << "|#2New filename? ";
		input( s, 12 );
		if ( !okfn( s ))
        {
			s[0] = '\0';
        }
		if ( s[0] )
        {
			align( s );
			if ( !wwiv::stringUtils::IsEquals( s, "        .   " ) )
            {
				strcpy( s1, directories[dn].path );
				strcpy( s2, s1 );
				strcat( s1, s );
				if ( ListPlusExist( s1 ) )
                {
					GetSession()->bout << "Filename already in use; not changed.\r\n";
                }
				else
                {
					strcat( s2, u.filename );
					WFile::Rename( s2, s1 );
					if ( ListPlusExist( s1 ) )
					{
						ss = read_extended_description( u.filename );
						if ( ss )
						{
							delete_extended_description (u.filename );
							add_extended_description( s, ss );
							BbsFreeMemory( ss );
						}
						strcpy( u.filename, s );
					}
					else
					{
						GetSession()->bout << "Bad filename.\r\n";
					}
				}
			}
		}
		GetSession()->bout.NewLine();
		GetSession()->bout << "New description:\r\n|#2: ";
		Input1( s, u.description, 58, false, INPUT_MODE_FILE_MIXED );
		if ( s[0] )
		{
			strcpy( u.description, s );
		}
		ss = read_extended_description(u.filename);
		GetSession()->bout.NewLine( 2 );
		GetSession()->bout << "|#5Modify extended description? ";
		if ( yesno() )
		{
			GetSession()->bout.NewLine();
			if ( ss )
			{
				GetSession()->bout << "|#5Delete it? ";
				if ( yesno() )
				{
					BbsFreeMemory( ss );
					delete_extended_description( u.filename );
					u.mask &= ~mask_extended;
				}
				else
				{
					u.mask |= mask_extended;
					modify_extended_description( &ss, directories[dn].name, u.filename );
					if ( ss )
					{
						delete_extended_description( u.filename );
						add_extended_description( u.filename, ss );
						BbsFreeMemory( ss );
					}
				}
			}
			else
			{
				modify_extended_description(&ss, directories[dn].name, u.filename);
				if (ss)
				{
					add_extended_description(u.filename, ss);
					BbsFreeMemory(ss);
					u.mask |= mask_extended;
				}
				else
				{
					u.mask &= ~mask_extended;
				}
			}
		}
		else if ( ss )
		{
				BbsFreeMemory( ss );
				u.mask |= mask_extended;
		}
		else
		{
			u.mask &= ~mask_extended;
		}
		if ( fileDownload.Open( WFile::modeBinary | WFile::modeCreateFile | WFile::modeReadWrite, WFile::shareUnknown, WFile::permReadWrite ) )
		{
			FileAreaSetRecord( fileDownload, i );
			fileDownload.Write( &u, sizeof( uploadsrec ) );
			fileDownload.Close();
		}
		i = nrecno( s3, cp );
	}
	return ret;
}


int remove_filename( const char *pszFileName, int dn )
{
	int ret = 1;
	char szTempFileName[MAX_PATH];
	uploadsrec u;
    memset( &u, 0, sizeof( uploadsrec ) );

#ifdef FAST_EXTENDED_DESCRIPTION
	lp_zap_ed_info();
#endif

	dliscan1( dn );
	strcpy( szTempFileName, pszFileName );

	if ( szTempFileName[0] == '\0' )
	{
		return ret;
	}
	if (strchr(szTempFileName, '.') == NULL)
	{
		strcat(szTempFileName, ".*");
	}
	align(szTempFileName);
	int i = recno(szTempFileName);
	bool abort = false;
    bool rdlp = false;
	while ( !hangup && i > 0 && !abort )
	{
		WFile fileDownload( g_szDownloadFileName );
		if ( fileDownload.Open( WFile::modeReadOnly | WFile::modeBinary ) )
		{
			FileAreaSetRecord( fileDownload, i );
			fileDownload.Read( &u, sizeof( uploadsrec ) );
			fileDownload.Close();
		}
		if ( dcs() || ( u.ownersys == 0 && u.ownerusr == GetSession()->usernum ) )
		{
			GetSession()->bout.NewLine();
			printfileinfo(&u, dn);
			GetSession()->bout << "|#9Remove (|#2Y/N/Q|#9) |#0: |#2";
			char ch = ynq();
			if (ch == 'Q')
			{
				ret = 0;
				abort = true;
			}
			else if (ch == 'Y')
			{
				rdlp = true;
                bool rm = false;
				if (dcs())
				{
					GetSession()->bout << "|#5Delete file too? ";
					rm = yesno();
					if ( rm && ( u.ownersys == 0 ) )
					{
						GetSession()->bout << "|#5Remove DL points? ";
						rdlp = yesno();
					}
					if ( GetApplication()->HasConfigFlag( OP_FLAGS_FAST_SEARCH) )
					{
						GetSession()->bout << "|#5Remove from ALLOW.DAT? ";
						if ( yesno() )
						{
							modify_database(szTempFileName, false);
						}
					}
				}
				else
				{
					rm = true;
					modify_database( szTempFileName, false );
				}
				if ( rm )
				{
					WFile::Remove( directories[dn].path, u.filename );
					if ( rdlp && u.ownersys == 0 )
					{
                        WUser user;
                        GetApplication()->GetUserManager()->ReadUser( &user, u.ownerusr );
                        if ( !user.IsUserDeleted() )
						{
                            if ( date_to_daten( user.GetFirstOn() ) < static_cast<signed int>( u.daten ) )
							{
                                user.SetFilesUploaded( user.GetFilesUploaded() - 1 );
                                user.SetUploadK( user.GetUploadK() - bytes_to_k( u.numbytes ) );

#ifdef FILE_POINTS
								if ( u.mask & mask_validated )
								{
                                    if ( ( u.filepoints * 2 ) > user.GetFilePoints() )
									{
                                        user.SetFilePoints( 0 );
									}
									else
									{
                                        user.SetFilePoints( user.GetFilePoints() - ( u.filepoints * 2 ) );
									}
								}
								GetSession()->bout << "Removed " << (u.filepoints * 2) << " file points\r\n";
#endif
                                GetApplication()->GetUserManager()->WriteUser( &user, u.ownerusr );
							}
						}
					}
				}
				if ( u.mask & mask_extended )
				{
					delete_extended_description( u.filename );
				}
				sysoplogf( "- \"%s\" removed off of %s", u.filename, directories[dn].name );

				if ( fileDownload.Open( WFile::modeBinary|WFile::modeCreateFile|WFile::modeReadWrite, WFile::shareUnknown, WFile::permReadWrite ) )
				{
					for ( int i1 = i; i1 < GetSession()->numf; i1++ )
					{
						FileAreaSetRecord( fileDownload, i1 + 1 );
						fileDownload.Read( &u, sizeof( uploadsrec ) );
						FileAreaSetRecord( fileDownload, i1 );
						fileDownload.Write( &u, sizeof( uploadsrec ) );
					}
					--i;
					--GetSession()->numf;
					FileAreaSetRecord( fileDownload, 0 );
					fileDownload.Read( &u, sizeof( uploadsrec ) );
					u.numbytes = GetSession()->numf;
					FileAreaSetRecord( fileDownload, 0 );
					fileDownload.Write( &u, sizeof( uploadsrec ) );
					fileDownload.Close();
				}
			}
		}
		i = nrecno( szTempFileName, i );
	}
	return ret;
}


int move_filename( const char *pszFileName, int dn )
{
    char szTempMoveFileName[81], szSourceFileName[MAX_PATH], szDestFileName[MAX_PATH], *ss;
    int nDestDirNum = -1, ret = 1;
    uploadsrec u, u1;

#ifdef FAST_EXTENDED_DESCRIPTION
    lp_zap_ed_info();
#endif

    strcpy(szTempMoveFileName, pszFileName);
    dliscan1(dn);
    align(szTempMoveFileName);
    int nRecNum = recno(szTempMoveFileName);
    if (nRecNum < 0)
    {
        GetSession()->bout << "\r\nFile not found.\r\n";
        return ret;
    }
    bool done = false;
    bool ok = false;

    tmp_disable_conf( true );

    while ( !hangup && nRecNum > 0 && !done )
    {
        int cp = nRecNum;
		WFile fileDownload( g_szDownloadFileName );
		if ( fileDownload.Open( WFile::modeBinary | WFile::modeReadOnly ) )
        {
            FileAreaSetRecord( fileDownload, nRecNum );
			fileDownload.Read( &u, sizeof( uploadsrec ) );
			fileDownload.Close();
        }
        GetSession()->bout.NewLine();
        printfileinfo(&u, dn);
        GetSession()->bout.NewLine();
        GetSession()->bout << "|#5Move this (Y/N/Q)? ";
        char ch = 0;
        if (bulk_move)
        {
            tmp_disable_pause( true );
            GetSession()->bout.Color( 1 );
            GetSession()->bout << YesNoString( true );
			GetSession()->bout.NewLine();
            ch = 'Y';
        }
        else
        {
            ch = ynq();
        }

        if ( ch == 'Q' )
        {
            done = true;
            ret = 0;
        }
        else if ( ch == 'Y' )
        {
            sprintf( szSourceFileName, "%s%s", directories[dn].path, u.filename );
            if ( !bulk_move )
            {
                do
                {
                    GetSession()->bout.NewLine( 2 );
                    GetSession()->bout << "|#2To which directory? ";
                    ss = mmkey( 1 );
                    if ( ss[0] == '?' )
                    {
                        dirlist( 1 );
                        dliscan1( dn );
                    }
                } while ( !hangup && ss[0] == '?' );

                nDestDirNum = -1;
                if ( ss[0] )
                {
                    for ( int i1 = 0; ( i1 < GetSession()->num_dirs ) && ( udir[i1].subnum != -1 ); i1++ )
                    {
                        if ( wwiv::stringUtils::IsEquals( udir[i1].keys, ss ) )
                        {
                            nDestDirNum = i1;
                        }
                    }
                }

                if ( GetSession()->numbatch > 1 )
                {
                    GetSession()->bout << "|#5Move all tagged files? ";
                    if (yesno())
                    {
                        bulk_move = 1;
                        bulk_dir = nDestDirNum;
                        tmp_disable_pause( true );
                    }
                }
            }
            else
            {
                nDestDirNum = bulk_dir;
            }

            if (nDestDirNum != -1)
            {
                ok = true;
                nDestDirNum = udir[nDestDirNum].subnum;
                dliscan1(nDestDirNum);
                if (recno(u.filename) > 0)
                {
                    ok = false;
                    GetSession()->bout.NewLine();
                    GetSession()->bout << "Filename already in use in that directory.\r\n";
                }
                if (GetSession()->numf >= directories[nDestDirNum].maxfiles)
                {
                    ok = false;
                    GetSession()->bout << "\r\nToo many files in that directory.\r\n";
                }
                if (freek1(directories[nDestDirNum].path) < static_cast<double>((u.numbytes / 1024L) + 3) )
                {
                    ok = false;
                    GetSession()->bout << "\r\nNot enough disk space to move it.\r\n";
                }
                dliscan();
            }
            else
            {
                ok = false;
            }
        }
        else
        {
            ok = false;
        }
        if (ok && !done)
        {
            if (!bulk_move)
            {
                GetSession()->bout << "|#5Reset upload time for file? ";
                if (yesno())
                {
                    u.daten = static_cast<unsigned long>(time(NULL));
                }
            }
            else
            {
                u.daten = static_cast<unsigned long>(time(NULL));
            }
            --cp;
			if ( fileDownload.Open( WFile::modeBinary|WFile::modeCreateFile|WFile::modeReadWrite, WFile::shareUnknown, WFile::permReadWrite ) )
            {
                for (int i2 = nRecNum; i2 < GetSession()->numf; i2++)
                {
                    FileAreaSetRecord( fileDownload, i2 + 1 );
					fileDownload.Read( &u1, sizeof( uploadsrec ) );
                    FileAreaSetRecord( fileDownload, i2 );
					fileDownload.Write( &u1, sizeof( uploadsrec ) );
                }
                --GetSession()->numf;
                FileAreaSetRecord( fileDownload, 0 );
				fileDownload.Read( &u1, sizeof( uploadsrec ) );
                u1.numbytes = GetSession()->numf;
                FileAreaSetRecord( fileDownload, 0 );
				fileDownload.Write( &u1, sizeof( uploadsrec ) );
				fileDownload.Close();
            }
            ss = read_extended_description(u.filename);
            if (ss)
            {
                delete_extended_description(u.filename);
            }
            sprintf(szDestFileName, "%s%s", directories[nDestDirNum].path, u.filename);
            dliscan1(nDestDirNum);
			if ( fileDownload.Open( WFile::modeBinary|WFile::modeCreateFile|WFile::modeReadWrite, WFile::shareUnknown, WFile::permReadWrite ) )
            {
                for (int i = GetSession()->numf; i >= 1; i--)
                {
                    FileAreaSetRecord( fileDownload, i );
					fileDownload.Read( &u1, sizeof( uploadsrec ) );
                    FileAreaSetRecord( fileDownload, i + 1 );
					fileDownload.Write( &u1, sizeof( uploadsrec ) );
                }
                FileAreaSetRecord( fileDownload, 1 );
				fileDownload.Write( &u, sizeof( uploadsrec ) );
                ++GetSession()->numf;
                FileAreaSetRecord( fileDownload, 0 );
				fileDownload.Read( &u1, sizeof( uploadsrec ) );
                u1.numbytes = GetSession()->numf;
                if (u.daten > u1.daten)
                {
                    u1.daten = u.daten;
                    GetSession()->m_DirectoryDateCache[nDestDirNum] = u.daten;
                }
                FileAreaSetRecord( fileDownload, 0 );
				fileDownload.Write( &u1, sizeof( uploadsrec ) );
				fileDownload.Close();
            }
            if (ss)
            {
                add_extended_description(u.filename, ss);
                BbsFreeMemory(ss);
            }
            if ( !wwiv::stringUtils::IsEquals( szSourceFileName, szDestFileName ) &&
                 ListPlusExist( szSourceFileName ) )
            {
                StringRemoveWhitespace( szSourceFileName );
                StringRemoveWhitespace( szDestFileName );
                bool bSameDrive = false;
                if ((szSourceFileName[1] != ':') && (szDestFileName[1] != ':'))
                {
                    bSameDrive = true;
                }
                if ((szSourceFileName[1] == ':') && (szDestFileName[1] == ':') && (szSourceFileName[0] == szDestFileName[0]))
                {
                    bSameDrive = true;
                }
                if ( bSameDrive )
                {
                    WFile::Rename(szSourceFileName, szDestFileName);
                    if (ListPlusExist(szDestFileName))
                    {
                        WFile::Remove(szSourceFileName);
                    }
                    else
                    {
                        copyfile(szSourceFileName, szDestFileName, false);
                        WFile::Remove(szSourceFileName);
                    }
                }
                else
                {
                    copyfile(szSourceFileName, szDestFileName, false);
                    WFile::Remove(szSourceFileName);
                }
            }
            GetSession()->bout << "\r\nFile moved.\r\n";
        }
        dliscan();
        nRecNum = nrecno(szTempMoveFileName, cp);
    }

    tmp_disable_conf( false );
    tmp_disable_pause( false );

    return ret;
}


void do_batch_sysop_command(int mode, const char *pszFileName)
{
	int save_curdir = GetSession()->GetCurrentFileArea();
	int pos = 0;

	GetSession()->bout.ClearScreen();

	if (GetSession()->numbatchdl)
	{
		bool done = false;
		while (pos < GetSession()->numbatch && !done)
		{
			if (batch[pos].sending)
			{
				switch (mode)
				{
				case SYSOP_DELETE:
					if (!remove_filename(batch[pos].filename, batch[pos].dir))
					{
						done = true;
					}
					else
					{
						delbatch(pos);
					}
					break;

				case SYSOP_RENAME:
					if (!rename_filename(batch[pos].filename, batch[pos].dir))
					{
						done = true;
					}
					else
					{
						delbatch(pos);
					}
					break;

				case SYSOP_MOVE:
					if (!move_filename(batch[pos].filename, batch[pos].dir))
					{
						done = true;
					}
					else
					{
						delbatch(pos);
					}
					break;
				}
			}
			else
			{
				++pos;
			}
		}
	}
	else
	{                                  // Just act on the single file
		switch (mode)
		{
		case SYSOP_DELETE:
			remove_filename(pszFileName, udir[GetSession()->GetCurrentFileArea()].subnum);
			break;

		case SYSOP_RENAME:
			rename_filename(pszFileName, udir[GetSession()->GetCurrentFileArea()].subnum);
			break;

		case SYSOP_MOVE:
			move_filename(pszFileName, udir[GetSession()->GetCurrentFileArea()].subnum);
			break;
		}
	}


	GetSession()->SetCurrentFileArea( save_curdir );
	dliscan();
}

void load_lp_config()
{
	if (!lp_config_loaded)
	{
		WFile fileConfig( syscfg.datadir, LISTPLUS_CFG );
		if ( !fileConfig.Open( WFile::modeBinary | WFile::modeReadOnly ) )
		{
			memset( &lp_config, 0, sizeof( struct listplus_config ) );
			lp_config.fi = lp_config.lssm = static_cast<long>(time(NULL));

			lp_config.normal_highlight  = (YELLOW + (BLACK << 4));
			lp_config.normal_menu_item  = (CYAN + (BLACK << 4));
			lp_config.current_highlight = (BLUE + (LIGHTGRAY << 4));
			lp_config.current_menu_item = (BLACK + (LIGHTGRAY << 4));

			lp_config.tagged_color      = LIGHTGRAY;
			lp_config.file_num_color    = GREEN;

			lp_config.found_fore_color  = RED;
			lp_config.found_back_color  = (LIGHTGRAY) + 16;

			lp_config.current_file_color = BLACK + (LIGHTGRAY << 4);

			lp_config.max_screen_lines_to_show = 24;
			lp_config.show_at_least_extended = 5;

			lp_config.edit_enable = 1;            // Do or don't let users edit their config
			lp_config.request_file = 1;           // Do or don't use file request
			lp_config.colorize_found_text = 1;    // Do or don't colorize found text
			lp_config.search_extended_on = 0;     // Defaults to either on or off in adv search, or is either on or off in simple search
			lp_config.simple_search = 1;          // Which one is entered when searching, can switch to other still
			lp_config.no_configuration = 0;       // Toggles configurable menus on and off
			lp_config.check_exist = 1;            // Will check to see if the file exists on hardrive and put N/A if not
			lp_config_loaded = 1;
			lp_config.forced_config = 0;

			save_lp_config();
		}
		else
		{
			fileConfig.Read( &lp_config, sizeof( struct listplus_config ) );
			lp_config_loaded = 1;
			fileConfig.Close();
		}
	}
	check_lp_colors();
}


void save_lp_config()
{
	if (lp_config_loaded)
	{
		WFile fileConfig( syscfg.datadir, LISTPLUS_CFG );
		if ( fileConfig.Open( WFile::modeBinary | WFile::modeCreateFile | WFile::modeTruncate | WFile::modeReadWrite, WFile::shareUnknown, WFile::permReadWrite ) )
		{
			fileConfig.Write( &lp_config, sizeof( struct listplus_config ) );
			fileConfig.Close();
		}
	}
}

int search_criteria(struct search_record * sr)
{
	int x = 0;
	int all_conf = 1, useconf;
	char s1[81];

	useconf = ( uconfdir[1].confnum != -1 && okconf( GetSession()->GetCurrentUser() ) );


LP_SEARCH_HELP:
	sr->search_extended = lp_config.search_extended_on;

	GetSession()->bout.ClearScreen();
	printfile(LPSEARCH_NOEXT);

	bool done = false;
	while (!done)
    {
		GetSession()->bout.GotoXY(1, 15);
		for (int i = 0; i < 9; i++)
        {
			GetSession()->bout.GotoXY(1, 15 + i);
			GetSession()->bout.ClearEOL();
		}
		GetSession()->bout.GotoXY(1, 15);

		GetSession()->bout << "|#1A)|#2 Filename (wildcards) :|02 " << sr->filemask << wwiv::endl;
		GetSession()->bout << "|#1B)|#2 Text (no wildcards)  :|02 " << sr->search << wwiv::endl;
		if ( okconf( GetSession()->GetCurrentUser() ) )
		{
			sprintf(s1, "%s", stripcolors(directories[udir[GetSession()->GetCurrentFileArea()].subnum].name));
		}
		else
		{
			sprintf(s1, "%s", stripcolors(directories[udir[GetSession()->GetCurrentFileArea()].subnum].name));
		}
		GetSession()->bout << "|#1C)|#2 Which Directories    :|02 " << ( sr->alldirs == THIS_DIR ? s1 : sr->alldirs == ALL_DIRS ? "All dirs" : "Dirs in NSCAN" ) << wwiv::endl;
		sprintf(s1, "%s", stripcolors( reinterpret_cast<char*>( dirconfs[uconfdir[GetSession()->GetCurrentConferenceFileArea()].confnum].name ) ) );
		GetSession()->bout << "|#1D)|#2 Which Conferences    :|02 " << ( all_conf ? "All Conferences" : s1 ) << wwiv::endl;
		GetSession()->bout << "|#1E)|#2 Extended Description :|02 " << ( sr->search_extended ? "Yes" : "No " ) << wwiv::endl;
		GetSession()->bout.NewLine();
		GetSession()->bout << "|15Select item to change |#2<CR>|15 to start search |#2Q=Quit|15:|#0 ";

		x = onek("QABCDE\r?");

		switch (x)
		{
		case 'A':
			GetSession()->bout << "Filename (wildcards okay) : ";
			input( sr->filemask, 12, true );
			if (sr->filemask[0])
			{
				if (okfn(sr->filemask))
				{
					if (strlen(sr->filemask) < 8)
					{
						sysoplogf( "Filespec: %s", sr->filemask);
					}
					else
					{
						if (strstr(sr->filemask, "."))
						{
							sysoplogf("Filespec: %s", sr->filemask);
						}
						else
						{
							GetSession()->bout << "|#6Invalid filename: " << sr->filemask << wwiv::endl;
							pausescr();
							sr->filemask[0] = '\0';
						}
					}
				}
				else
				{
					GetSession()->bout << "|#6Invalid filespec: " << sr->filemask << wwiv::endl;
					pausescr();
					sr->filemask[0] = 0;
				}
			}
			break;

		case 'B':
			GetSession()->bout << "Keyword(s) : ";
			input( sr->search, 60, true );
			if (sr->search[0])
            {
				sysoplogf( "Keyword: %s", sr->search);
			}
			break;

		case 'C':
			++sr->alldirs;
			if (sr->alldirs >= 3)
			{
				sr->alldirs = 0;
			}
			break;

		case 'D':
			all_conf = !all_conf;
			break;

		case 'E':
			sr->search_extended = !sr->search_extended;
			break;

		case 'Q':
			return 0;

		case '\r':
			done = true;
			break;

		case '?':
			goto LP_SEARCH_HELP;

		}
	}

	if (sr->search[0] == ' ' && sr->search[1] == 0)
    {
		sr->search[0] = 0;
    }


	if (useconf && all_conf)                  // toggle conferences off
    {
		tmp_disable_conf( true );
    }

	return x;
}


void load_listing()
{

	if (GetSession()->usernum != list_loaded)
    {
		load_config_listing(GetSession()->usernum);
    }

	if (!list_loaded)
    {
		load_config_listing( 1 );                 // default to what the sysop has
	}                                         // config it for themselves
	// force filename to be shown
	if (!config_listing.lp_options & cfl_fname)
    {
		config_listing.lp_options |= cfl_fname;
    }
}


void view_file(const char *pszFileName)
{
	char szCommandLine[MAX_PATH];
	char szBuffer[30];
	long osysstatus;
	int i, i1;
	uploadsrec u;

	GetSession()->bout.ClearScreen();

	strcpy(szBuffer, pszFileName);
	unalign(szBuffer);

	if (WFile::Exists("AVIEWCOM.EXE"))
    {
		if (incom)
        {
			sprintf( szCommandLine, "AVIEWCOM.EXE %s%s -p%s -a1 -d",
				     directories[udir[GetSession()->GetCurrentFileArea()].subnum].path, szBuffer, syscfgovr.tempdir );
			osysstatus = GetSession()->GetCurrentUser()->GetStatus();
			if ( GetSession()->GetCurrentUser()->HasPause() )
            {
                GetSession()->GetCurrentUser()->ToggleStatusFlag( WUser::pauseOnPage );
            }
			ExecuteExternalProgram(szCommandLine, EFLAG_INTERNAL | EFLAG_TOPSCREEN | EFLAG_COMIO | EFLAG_NOPAUSE);
			GetSession()->GetCurrentUser()->SetStatus( osysstatus );
		}
        else
        {
			sprintf( szCommandLine, "AVIEWCOM.EXE %s%s com0 -o%u -p%s -a1 -d",
				     directories[udir[GetSession()->GetCurrentFileArea()].subnum].path, szBuffer,
				     GetApplication()->GetInstanceNumber(), syscfgovr.tempdir);
			ExecuteExternalProgram( szCommandLine, EFLAG_NOPAUSE | EFLAG_TOPSCREEN );
		}
	}
    else
    {
		dliscan();
		bool abort = false;
		bool next = false;
		i = recno(pszFileName);
		do
        {
			if (i > 0)
            {
				WFile fileDownload( g_szDownloadFileName );
				if ( fileDownload.Open( WFile::modeBinary | WFile::modeReadOnly ) )
                {
					FileAreaSetRecord( fileDownload, i );
					fileDownload.Read( &u, sizeof( uploadsrec ) );
					fileDownload.Close();
				}
				i1 = list_arc_out(stripfn(u.filename), directories[udir[GetSession()->GetCurrentFileArea()].subnum].path);
				if (i1)
                {
					abort = true;
                }
				checka(&abort, &next);
				i = nrecno(pszFileName, i);
			}
		} while ( i > 0 && !hangup && !abort );
		GetSession()->bout.NewLine();
		pausescr();
	}
}


int lp_try_to_download( const char *pszFileMask, int dn )
{
	int i, rtn, ok2;
    bool abort = false, next = false;
	uploadsrec u;
	char s1[81], s3[81];

	dliscan1(dn);
	i = recno(pszFileMask);
	if (i <= 0)
    {
		abort = next = 0;
		checka(&abort, &next);
        return ( abort ) ? -1 : 0;
	}
	bool ok = true;

	foundany = 1;
	do
    {
		GetSession()->localIO()->tleft( true );
		WFile fileDownload( g_szDownloadFileName );
		fileDownload.Open( WFile::modeBinary | WFile::modeReadOnly );
		FileAreaSetRecord( fileDownload, i );
		fileDownload.Read( &u, sizeof( uploadsrec ) );
		fileDownload.Close();

		ok2 = 0;
		if ( strncmp( u.filename, "WWIV4", 5 ) == 0 &&
			 !GetApplication()->HasConfigFlag( OP_FLAGS_NO_EASY_DL ) )
        {
			ok2 = 1;
        }

		if ( !ok2 && (!(u.mask & mask_no_ratio)))
        {
			if (!ratio_ok())
            {
				return -2;
            }
        }

		write_inst(INST_LOC_DOWNLOAD, udir[GetSession()->GetCurrentFileArea()].subnum, INST_FLAGS_ONLINE);
		sprintf(s1, "%s%s", directories[dn].path, u.filename);
		sprintf(s3, "%-40.40s", u.description);
		abort = 0;
		rtn = add_batch(s3, u.filename, dn, u.numbytes);
		s3[0] = 0;

		if ( abort || ( rtn == -3 ) )
        {
			ok = false;
        }
		else
        {
			i = nrecno( pszFileMask, i );
        }
	} while ( ( i > 0 ) && ok && !hangup );
	returning = true;
	if (rtn == -2)
    {
		return -2;
    }
	if ( abort || rtn == -3 )
    {
		return -1;
    }
	else
    {
		return 1;
    }
}


void download_plus(const char *pszFileName)
{
	char szFileName[MAX_PATH];

	if (GetSession()->numbatchdl != 0)
    {
		GetSession()->bout.NewLine();
		GetSession()->bout << "|#2Download files in your batch queue (|#1Y/n|#2)? ";
		if (noyes())
        {
			batchdl( 1 );
			return;
		}
	}
	strcpy(szFileName, pszFileName);
	if ( szFileName[0] == '\0' )
    {
		return;
    }
	if (strchr(szFileName, '.') == NULL)
    {
		strcat(szFileName, ".*");
    }
	align( szFileName );
	if (lp_try_to_download(szFileName, udir[GetSession()->GetCurrentFileArea()].subnum) == 0)
    {
		GetSession()->bout << "\r\nSearching all directories.\r\n\n";
		int dn = 0;
		int count = 0;
		int color = 3;
		foundany = 0;
		if (!x_only)
		{
			GetSession()->bout << "\r|#2Searching ";
		}
		while ((dn < GetSession()->num_dirs) && (udir[dn].subnum != -1))
        {
			count++;
			if (!x_only)
            {
				GetSession()->bout << "|#" << color << ".";
				if ( count == NUM_DOTS )
                {
					GetSession()->bout << "\r";
					GetSession()->bout << "|#2Searching ";
					color++;
					count = 0;
					if (color == 4)
                    {
						color++;
                    }
					if (color == 10)
                    {
						color = 0;
                    }
				}
			}
			if (lp_try_to_download(szFileName, udir[dn].subnum) < 0)
            {
				break;
            }
			else
            {
				dn++;
            }
		}
		if (!foundany)
        {
			GetSession()->bout << "File not found.\r\n\n";
		}
	}
}


void request_file(const char *pszFileName)
{
	GetSession()->bout.ClearScreen();
	GetSession()->bout.NewLine();

	printfile(LPFREQ_NOEXT);

    GetSession()->bout << "|#2File missing.  Request it? ";

	if (noyes())
    {
		ssm( 1, 0, "%s is requesting file %s", GetSession()->GetCurrentUser()->GetName(), pszFileName );
		GetSession()->bout << "File request sent\r\n";
	}
    else
    {
		GetSession()->bout << "File request NOT sent\r\n";
    }

	pausescr();

}


bool ok_listplus()
{
	if ( !okansi() )
    {
		return false;
    }

#ifndef FORCE_LP
	if ( GetSession()->GetCurrentUser()->IsUseNoTagging() )
    {
		return false;
    }

    if ( GetSession()->GetCurrentUser()->IsUseListPlus() )
    {
		return false;
    }

	if ( x_only )
    {
		return false;
    }
#endif

	return 1;
}


void check_lp_colors()
{
	for (int i = 0; i < 32; i++)
    {
		if (!config_listing.lp_colors[i])
        {
			config_listing.lp_colors[i] = 1;
        }
	}
}

