/*
 * Copyright 2001,2004 Frank Reid
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *      http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */ 

#include "pppproj.h"

int main(int argc, char *argv[])
{
	fprintf(stderr, "\n\nPPP Sent Packet Purger %s", VERSION);
	fprintf(stderr, "\nContact Frank Reid at edare@home.com or 1@8213.WWIVnet for support\n");
	
	if (!argv[1])  
	{
		printf("\n \xFE Run PPPurge <days> to delete packets older than <days>.\n\n ");
		return 0;
	}
	
	int nNumDaysToKeep = ( argc > 1 ) ? atoi(argv[1]) : 99;
	printf("\n \xFE Purging sent packets older than %d days.", nNumDaysToKeep);
	
	char szFileName[_MAX_PATH], szCurrentDirectory[_MAX_PATH];
	_getcwd( szCurrentDirectory, _MAX_PATH );
	_snprintf(szFileName, _MAX_PATH, "%s\\SENT\\*.*", szCurrentDirectory);

	struct _finddata_t ff;
	long hFind = _findfirst( szFileName, &ff );
	int nFindNext = ( hFind != -1 ) ? 0 : -1;
	int howmany = 0, kbytes = 0;
	while ( nFindNext == 0 ) 
	{
		sprintf(szFileName, "%s\\SENT\\%s", szCurrentDirectory, ff.name);
		struct _stat fileinfo;
		if ( _stat( szFileName, &fileinfo ) == 0 ) 
		{
			time_t tAge = (time(NULL) - fileinfo.st_atime);
			if (tAge > (SECONDS_PER_DAY * (nNumDaysToKeep+1))) 
			{
				kbytes += static_cast<int>(fileinfo.st_size/1024);
				++howmany;
				_unlink(szFileName);
			}
		}
		nFindNext = _findnext( hFind, &ff );
	}
	_findclose( hFind );
	printf("\n \xFE Deleted %dKB in %d files.\n\n", kbytes, howmany);

	return 0;
}

