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

#include "wwiv.h"

#if defined( _MSC_VER )
#pragma warning( push )
#pragma warning( disable : 4100 )
#endif

int  set_strings_fn(int filen, char *pszDirectoryName, char *pszFileName, int allowwrite) {
  return false;
}

void put_string(int filen, int n, char *pszText) {
  return;
}

int  cachestat() {
  return 0;
}

int  num_strings(int filen) {
  return 0;
}

const char *getrandomstring(int filen) {
  return "";
}

void close_strfiles() {
  return;
}


#if defined( _MSC_VER )
#pragma warning( pop )
#endif // _MSC_VER



/*


#define CACHE_LEN 4096
#define WRITE_SUPPORT
#define MAX_STRFILES 8

struct strfile_t
{
  int str_f;
  long str_l;
  long str_o;
  long str_num;
  int allowwrite;
  int stayopen;
  char fn[81];
};

static char str_buf[256];

#ifdef CACHE_LEN

struct strhdr
{
  int fn;
  int num;
  int nxt;
};

static char *str_cache;
static int str_first;
static double str_total, str_hits;

#endif

static strfile_t fileinfo[MAX_STRFILES];



static void close_strfile(strfile_t * sfi)
{
  if (sfi->str_f > 0) {
    XXsh_close(sfi->str_f);
    sfi->str_f = 0;
  }
}


static int open_strfile(strfile_t * sfi)
{
  unsigned int i;
  unsigned char ch;

  if (sfi->str_f)
    return sfi->str_f;

  if (sfi->fn[0] == 0)
    return 0;

#ifdef WRITE_SUPPORT
  if (sfi->allowwrite)
    sfi->str_f = XXsh_open(sfi->fn, O_RDWR | O_BINARY | O_CREAT, S_IREAD | S_IWRITE);
  else
    sfi->str_f = XXsh_open1(sfi->fn, O_RDONLY | O_BINARY);
#else
  sfi->str_f = XXsh_open1(sfi->fn, O_RDONLY | O_BINARY);
#endif
  if (sfi->str_f < 0)
    sfi->str_f = 0;
  else {
    if (sfi->str_l == 0) {

#ifdef WRITE_SUPPORT
        // This will need to use WFile
      if ((filelength(sfi->str_f) == 0) && (sfi->allowwrite)) {
        i = 0xffff;
        XXsh_write(sfi->str_f, &i, 2);
        ch = 100;
        XXsh_write(sfi->str_f, &ch, 1);
        XXlseek(sfi->str_f, 0, SEEK_SET);
      }
#endif

      i = 0;
      XXsh_read(sfi->str_f, &i, 2);
      if (i != 0xffff) {
        sfi->str_l = 161;
        sfi->str_o = 0;
      } else {
        XXsh_read(sfi->str_f, &ch, 1);
        sfi->str_l = ch;
        sfi->str_o = 3;
      }
    }
    if (sfi->str_l)
    {
        // This will need to use WFile
      sfi->str_num = (filelength(sfi->str_f) - sfi->str_o) / sfi->str_l;
    }
  }

  return (sfi->str_f);
}


int set_strings_fn(int filen, char *pszDirectoryName, char *pszFileName, int allowwrite)
{
    strfile_t *sfi;
    int stayopen;
#ifdef CACHE_LEN
    strhdr *sp;
#endif

    if (filen < 0) {
      stayopen = 1;
      filen = (-filen);
    } else {
      stayopen = 0;
    }

    if ((filen >= MAX_STRFILES) || (filen < 0))
      return (-1);

#ifndef WRITE_SUPPORT
    if (allowwrite)
      return (-1);
#endif

    sfi = &(fileinfo[filen]);

    close_strfile(sfi);

    memset(sfi, 0, sizeof(strfile_t));

    if (!pszFileName)
      return ( 0 );

    sfi->allowwrite = allowwrite;
    sfi->stayopen = stayopen;

    if (pszDirectoryName && pszDirectoryName[0]) {
      strcpy(sfi->fn, pszDirectoryName);
      if (sfi->fn[strlen(sfi->fn) - 1] != WFile::pathSeparatorChar)
        strcat(sfi->fn, WFile::pathSeparatorString);
    } else
      sfi->fn[0] = 0;
    strcat(sfi->fn, pszFileName);

    if (open_strfile(sfi) == 0) {
      memset(sfi, 0, sizeof(strfile_t));
      return (-1);
    }

#ifdef CACHE_LEN
      if (!str_cache)
        str_cache = static_cast<char *>( malloc( CACHE_LEN ) );

      if (str_cache) {
        sp = (strhdr *) str_cache;
        sp->num = sp->nxt = -1;
        str_first = 0;
        str_total = 0.0;
        str_hits = 0.0;
      }
#endif
    if (!sfi->stayopen)
      close_strfile(sfi);

    return ( 0 );
}


#ifdef WRITE_SUPPORT

void put_string(int filen, int n, char *pszText)
{
    long l;
    strfile_t *sfi;
#ifdef CACHE_LEN
    strhdr *sp;
#endif

    if ((filen >= MAX_STRFILES) || (filen < 0))
       return;

     sfi = &(fileinfo[filen]);

     n--;

    if ((n < 0) || (!open_strfile(sfi)))
       return;

     memset(str_buf, 0, sizeof(str_buf));

    while (sfi->str_num < n) {
      l = (static_cast<long>(sfi->str_num) * sfi->str_l) + sfi->str_o;
      XXlseek(sfi->str_f, l, SEEK_SET);
      XXsh_write(sfi->str_f, str_buf, sfi->str_l);
      sfi->str_num++;
    }

     l = (static_cast<long>(n) * sfi->str_l) + sfi->str_o;

    XXlseek(sfi->str_f, l, SEEK_SET);
    strncpy(str_buf, pszText, sfi->str_l - 1);
    XXsh_write(sfi->str_f, str_buf, sfi->str_l);

    if (sfi->str_num <= n)
      sfi->str_num = n + 1;

#ifdef CACHE_LEN
    if (str_cache) {
      sp = (strhdr *) str_cache;
      sp->num = sp->nxt = -1;
    }
#endif

    if (filen || (!sfi->stayopen))
      close_strfile(sfi);

}
#endif



int cachestat()
{
#ifdef CACHE_LEN
    if (str_total < 1.0)
      return ( 0 );

    return (static_cast<int>((str_hits * 100.0) / str_total));
#else
    return ( 0 );
#endif
}


char *get_stringx(int filen, int n)
{
    char *ss;
    strfile_t *sfi;
#ifdef CACHE_LEN
    int cur_idx, next_idx, last_idx;
    strhdr *sp, *sp1;
#endif

     n--;

    if ((filen >= MAX_STRFILES) || (filen < 0))
       return ("");

     sfi = &(fileinfo[filen]);

    if (n < 0)
       return ("");

    if (n >= sfi->str_num) {
      return ("");
    }
#ifdef CACHE_LEN
    if (str_cache) {
      str_total += 1.0;

      for (last_idx = cur_idx = str_first, sp = (strhdr *) (str_cache + cur_idx);
           sp->nxt != -1;
           last_idx = cur_idx, cur_idx = sp->nxt, sp = (strhdr *) (str_cache + cur_idx)) {
        if ((sp->num == n) && (sp->fn == filen)) {
          str_hits += 1.0;
          return (str_cache + cur_idx + sizeof(strhdr));
        }
      }
    }
#endif
    if (!open_strfile(sfi))
      return ("");

    XXlseek(sfi->str_f, (static_cast<long>(n) * sfi->str_l) + sfi->str_o, SEEK_SET);
    XXsh_read(sfi->str_f, str_buf, sfi->str_l);
    ss = str_buf;

#ifdef CACHE_LEN
    if (str_cache) {
      next_idx = cur_idx + sizeof(strhdr) + strlen(str_buf) + 1;
      if (next_idx + sizeof(strhdr) > CACHE_LEN) {
        sp1 = (strhdr *) (str_cache + last_idx);
        sp1->nxt = 0;
        sp = (strhdr *) str_cache;
        next_idx -= cur_idx;
        while ((str_first > cur_idx) || (str_first < next_idx + static_cast<int>( sizeof(strhdr)))) {
          sp1 = (strhdr *) (str_cache + str_first);
          str_first = sp1->nxt;
        }
        cur_idx = 0;
      } else {
        while ((str_first > cur_idx) && (next_idx + static_cast<int>( sizeof(strhdr) ) > str_first)) {
          sp1 = (strhdr *) (str_cache + str_first);
          str_first = sp1->nxt;
        }
      }
      sp->fn = filen;
      sp->num = n;
      sp->nxt = next_idx;
      ss = str_cache + cur_idx + sizeof(strhdr);
      strcpy(ss, str_buf);
      sp = (strhdr *) (str_cache + next_idx);
      sp->num = sp->nxt = -1;
    }
#endif

    if (!sfi->stayopen)
      close_strfile(sfi);

    return (ss);
}


int num_strings(int filen)
{
    if ((filen >= MAX_STRFILES) || (filen < 0))
      return ( 0 );

    return (fileinfo[filen].str_num);
}


char *getrandomstring(int filen)
{
    strfile_t *sfi;

    if ((filen >= MAX_STRFILES) || (filen < 0))
       return ("");

     sfi = &(fileinfo[filen]);

    if (!open_strfile(sfi))
       return ("");
    else
    {
        // This will need to use WFile
       return (get_stringx(filen, 1 + WWIV_GetRandomNumber(static_cast<int>((filelength(sfi->str_f) - sfi->str_o) / sfi->str_l))));
     }
}


void close_strfiles()
{
    int i;

    for (i = 0; i < MAX_STRFILES; i++) {
      close_strfile(&(fileinfo[i]));
    }
}


*/

