/****************************************************************************/
/*                                                                          */
/*                             WWIV Version 5.0x                            */
/*            Copyright (C) 1998-2005 by WWIV Software Services             */
/*                                                                          */
/*      Distribution or publication of this source code, it's individual    */
/*       components, or a compiled version thereof, whether modified or     */
/*        unmodified, without PRIOR, WRITTEN APPROVAL of WWIV Software      */
/*        Services is expressly prohibited.  Distribution of compiled       */
/*            versions of WWIV is restricted to copies compiled by          */
/*           WWIV Software Services.  Violators will be procecuted!         */
/*                                                                          */
/****************************************************************************/

extern INT32 debuglevel;

FILE *fsh_open(const char *path, char *mode);
int sh_close(int f);
void fsh_close(FILE *f);

int sh_open(const char *path, int file_access, unsigned mode);
int sh_open1(const char *path, int access);
int sh_read(int handle, void *buf, unsigned len);

#ifndef NETWORK
int sh_write(int handle, const void *buf, unsigned len);
#endif




//#endif // __INCLUDED_WSHARE_H__



