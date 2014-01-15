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
#include <vector>

const unsigned int GetTimeLeft();


// Replacable parameters:
// ~~~~~~~~~~~~~~~~~~~~~~
//
// Param     Description                       Example
// ---------------------------------------------------------------------
//  %%       A single '%'                      "%"
//  %1-%5    Specified passed-in parameter
//  %A       callinfo full pathname            "c:\wwiv\temp\callinfo.bbs"
//  %C       chain.txt full pathname           "c:\wwiv\temp\chain.txt"
//  %D       doorinfo full pathname            "c:\wwiv\temp\dorinfo1.def"
//  %E       door32.sys full pathname          "C:\wwiv\temp\door32.sys"
//  %K       gfiles comment file for archives  "c:\wwiv\gfiles\comment.txt"
//  %M       Modem baud rate                   "14400"
//  %N       Instance number                   "1"
//  %O       pcboard full pathname             "c:\wwiv\temp\pcboard.sys"
//  %P       Com port number                   "1"
//  %R       door full pathname                "c:\wwiv\temp\door.sys"
//  %S       Com port baud rate                "38400"
//  %T       Time remaining (min)              "30"
//

/**
 * @todo Document this
 */

const std::string stuff_in( const std::string commandline, const std::string arg1,
                            const std::string arg2, const std::string arg3, const std::string arg4, const std::string arg5 ) {
	std::vector<std::string> flags;
	flags.push_back( arg1 );
	flags.push_back( arg2 );
	flags.push_back( arg3 );
	flags.push_back( arg4 );
	flags.push_back( arg5 );

	std::string::const_iterator iter = commandline.begin();
	std::ostringstream os;
	while ( iter != commandline.end() ) {
		if ( *iter == '%') {
			++iter;
			char ch = wwiv::UpperCase<char>(*iter);
			switch (ch) {
			// fixed strings
			case '%':
				os << "%";
				break;
			// replacable parameters
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
				os << flags.at(ch - '1');
				break;
			// call-specific numbers
			case 'M':
				os << modem_speed;
				break;
			case 'K':
				os << syscfg.gfilesdir << COMMENT_TXT;
				break;
			case 'P':
				os << ((incom)? syscfgovr.primaryport : 0);
				break;
			case 'N':
				os << GetApplication()->GetInstanceNumber();
				break;
			case 'S':
				os << ((com_speed == 1) ? 115200 : com_speed);
				break;
			case 'T':
				os << GetTimeLeft();
				break;
			// chain.txt type filenames
			case 'C':
				os << create_filename( CHAINFILE_CHAIN );
				break;
			case 'D':
				os << create_filename( CHAINFILE_DORINFO );
				break;
			case 'O':
				os << create_filename( CHAINFILE_PCBOARD );
				break;
			case 'A':
				os << create_filename( CHAINFILE_CALLINFO );
				break;
			case 'R':
				os << create_filename( CHAINFILE_DOOR );
				break;
			case 'E':
				os << create_filename( CHAINFILE_DOOR32 );
				break;
			}
			++iter;
		} else {
			os << *iter++;
		}
	}
	return std::string( os.str() );
}

const unsigned int GetTimeLeft() {
	double d = nsl();
	if (d < 0) {
		d += HOURS_PER_DAY_FLOAT * SECONDS_PER_HOUR_FLOAT;
	}
	return static_cast<int>( d ) / MINUTES_PER_HOUR;
}