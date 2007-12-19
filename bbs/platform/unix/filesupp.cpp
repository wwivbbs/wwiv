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

/*
bool WWIV_CopyFile(const char *szSourceFileName, const char *szDestFileName)
{
  int f1, f2, i;    
  char *b;
  struct ftime ft;

  if ((strcmp(szSourceFileName, szDestFileName) != 0) &&
     (exist(szSourceFileName)) && (!exist(szDestFileName))) {
    if ((b = (char *)malloca(16400)) == NULL)
      return false;
    f1 = sh_open1(szSourceFileName, O_RDONLY);
    getftime(f1, &ft);
    f2 = sh_open(szDestFileName, O_RDWR | O_CREAT | O_TRUNC, S_IREAD | S_IWRITE);
    i = read(f1, (void *) b, 16384);
    while (i > 0) {
      write(f2, (void *) b, i);
      i = read(f1, (void *) b, 16384);
    }
    f1=sh_close(f1);
    f2=sh_close(f2);
    setftime(szDestFileName, &ft);
    bbsfree(b);
  }
#ifdef LAZY_WRITES
  wait1(LAZY_WRITES);
#endif
  return true;
}
*/

bool iscdrom(char ch)
{
  return false;		/* Ugly hack until we can get something better */
}
