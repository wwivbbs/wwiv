/****************************************************************************/
/*                                                                          */
/*                             WWIV Version 5.0x                            */
/*            Copyright (C) 1998-2003 by WWIV Software Services             */
/*                                                                          */
/*      Distribution or publication of this source code, it's individual    */
/*       components, or a compiled version thereof, whether modified or     */
/*        unmodified, without PRIOR, WRITTEN APPROVAL of WWIV Software      */
/*        Services is expressly prohibited.  Distribution of compiled       */
/*            versions of WWIV is restricted to copies compiled by          */
/*           WWIV Software Services.  Violators will be procecuted!         */
/*                                                                          */
/****************************************************************************/
#include "../../wwiv.h"


bool WWIV_CopyFile(const char * szSourceFileName, const char * szDestFileName)
{
    // Note: This won't work probably.. Needs to be re-written.. There
    // SHOULD be a OS/2 API call to do this w/o needing this lame code.
    int d1, d2;
    char s[81], s1[81], *b;
//
//
// Start of file copy code... This needs to be moved to a platform function
// to copy the file.   Copying from file "s" to "s1"
//
//
    strcpy(s, szSourceFileName);
    strcpy(s1, szDestFileName);

    if ((b = (char *) malloca(16400)) == NULL) {
        f = sh_close(f);
        return;
    }
    d1 = sh_open1(s,
        O_RDONLY | O_BINARY);
    d2 = sh_open(s1,
        O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
    if ((d1 > -1) && (d2 > -1)) {
        i = sh_read(d1, (void *) b, 16384);
        npr("Copying.");
        while (i > 0) {
            npr(".");
            sh_write(d2, (void *) b, i);
            i = sh_read(d1, (void *) b, 16384);
        }
        d1 = sh_close(d1);
        d2 = sh_close(d2);

#ifdef __OS2__

#error "Unwritten portion of the OS/2 port"
// NOTE: This portion of the code sets the date of the 2nd file to the same
// as the 1st file.

#else

    h1 = CreateFile(s, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    h2 = CreateFile(s1, GENERIC_WRITE, FILE_SHARE_WRITE, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    GetFileTime(h1, &ftCreat, &ftAccess, &ftWrite);
    SetFileTime(h2, &ftCreat, &ftAccess, &ftWrite);
    CloseHandle(h1);
    CloseHandle(h2);
#endif	// __OS2__


    bbsfree(b);


//
// End of the copy code
//


}