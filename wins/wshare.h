#if !defined( __WSHARE_H__INCLUDED__ )
#define __WSHARE_H__INCLUDED__ 
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

#include <stdio.h>

FILE *fsh_open(const char *path, char *mode);
int sh_close(int f);
void fsh_close(FILE *f);

int sh_open(const char *path, int file_access, unsigned mode);
int sh_open1(const char *path, int access);
int sh_read(int handle, void *buf, unsigned len);
long sh_lseek(int handle, long offset, int fromwhere);
size_t fsh_read(void *ptr, size_t size, size_t n, FILE * stream);
size_t fsh_write(const void *ptr, size_t size, size_t n, FILE * stream);
int sh_open25(const char *path, int file_access, int share, unsigned mode);


#ifndef NETWORK
int sh_write(int handle, const void *buf, unsigned len);
#endif




#endif // __WSHARE_H__INCLUDED__ 
