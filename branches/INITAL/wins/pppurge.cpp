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
	
	char s[121], buf[_MAX_PATH];
	getcwd( buf, _MAX_PATH );
	sprintf(s, "%s\\SENT\\*.*",buf);

	struct _finddata_t ff;
	long hFind = _findfirst( s, &ff );
	int nFindNext = ( hFind != -1 ) ? 0 : -1;
	int howmany = 0, kbytes = 0;
	while ( nFindNext == 0 ) 
	{
		sprintf(s, "%s\\SENT\\%s",buf, ff.name);
		struct _stat fileinfo;
		if ( _stat( s, &fileinfo ) == 0 ) 
		{
			long age = (time(NULL) - fileinfo.st_atime);
			if (age > (86400L * (nNumDaysToKeep+1))) 
			{
				kbytes += (int)fileinfo.st_size/1024;
				++howmany;
				unlink(s);
			}
		}
		nFindNext = _findnext( hFind, &ff );
	}
	_findclose( hFind );
	printf("\n \xFE Deleted %dKB in %d files.\n\n", kbytes, howmany);

	return 0;
}

