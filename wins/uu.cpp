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
#include "uu.h"

#define UUENC(c) ((c) ? ((c) & 077) + ' ': '`')
#define UUDEC(c)  (((c)!='`')?(((c)-' ')&077):(0))   /* single char decode */
#define MAXLINE 1024


char *strippath(char *fname)
{
  char *p;

  p = (char *)strrchr(fname, '\\');
  if (!p)
    p = (char *)strrchr(fname, ':');
  if (!p)
    p = fname;
  return (p);
}

long uuencode_aux(FILE *in, FILE *out)
{
  long len = 0L;
  char buf[80];
  int c1, c2, c3, c4, i, n;

  for (;;) {
    /* 1 (up to) 45 character line */
    n = fread(buf, 1, 45, in);
    putc(UUENC(n), out);
    for (i = 0; i < n; i += 3) {
      c1 = buf[i] >> 2;
      c2 = ((buf[i] << 4) & 060) | ((buf[i + 1] >> 4) & 017);
      c3 = ((buf[i + 1] << 2) & 074) | ((buf[i + 2] >> 6) & 03);
      c4 = buf[i + 2] & 077;
      putc(UUENC(c1), out);
      putc(UUENC(c2), out);
      putc(UUENC(c3), out);
      putc(UUENC(c4), out);
    }
    putc('\n', out);
    if (n <= 0)
      break;
    else
      len += n;
  }
  return (len);
}

long uuencode(FILE *in, FILE *out, char *name)
{
  long len = 0L;
  struct stat sbuf;
  int mode;
  char s[12], s1[5];

  setmode(fileno(in), O_BINARY);
  if (fstat(fileno(in), &sbuf) < 0)
    mode = 0666 & ~umask(0666);
  else
    mode = sbuf.st_mode & 0777;
  _splitpath(name, NULL, NULL, s, s1);
  fprintf(out, "begin %o %s%s\n", mode, s, s1);
  len = uuencode_aux(in, out);
  fprintf(out, "end\n\n");
  if (out != NULL)
    fclose(out);
  return (len);
}

void *uudecode_aux(char *in, unsigned char *out)
{
  out[0] = ( unsigned char ) ( (UUDEC(in[0]) << 2) | (UUDEC(in[1]) >> 4) );
  out[1] = ( unsigned char ) ( (UUDEC(in[1]) << 4) | (UUDEC(in[2]) >> 2) );
  out[2] = ( unsigned char ) ( (UUDEC(in[2]) << 6) | (UUDEC(in[3])) );
  return out;
}

#define UU_FREE_ALL free(fname); free(buf);

int uudecode(FILE *in, char *path, char *name)
{
  FILE *out;
  int i = 0, done = 0, len, wlen;
  char *buf, dest[121], *fname;
  unsigned char   temp[4];
  unsigned mode;

  if ((fname = ((char *) malloc(strlen(path) + 15))) <= 0) {
    return (UU_NO_MEM);
  }
  if ((buf = (char *) malloc(MAXLINE)) <= 0) {
    free(fname);
    return (UU_NO_MEM);
  }
  if (fscanf(in, "begin %o %s\n", &mode, dest) < 2) {
    UU_FREE_ALL;
    return (UU_BAD_BEGIN);
  }
  if (name)
    strcpy(name, dest);
  sprintf(fname, "%s%s", path, strippath(dest));
  if ((out = fsh_open(fname, "wb")) <= 0) {
    UU_FREE_ALL;
    return (UU_CANT_OPEN);
  }
  while (fgets(buf, MAXLINE, in)) {
    trim(buf);
    if (strcmpi(buf, "end") == 0) {
      done = 1;
      break;
    }
    if ((strlen(buf) == 22) && ((buf[0] == '.') && (buf[1] == '.')))
      strcpy(buf, buf + 1);
    if (((int)strlen(buf) - 1) != (len = (((UUDEC(buf[0]) + 2) / 3) * 4))) {
      UU_FREE_ALL;
      if (out != NULL)
        fclose(out);
      unlink(fname);
      return (UU_CHECKSUM);
    }
    wlen=UUDEC(buf[0]);
    for (i = 1; i < len; i += 4) {
      fwrite(uudecode_aux(&buf[i], temp), 1, min(wlen, 3), out);
      wlen -= 3;
    }
  }
  UU_FREE_ALL;
  if (out != NULL)
    fclose(out);
  if (done)
    return (UU_SUCCESS);
  unlink(fname);
  return (UU_BAD_END);
}
