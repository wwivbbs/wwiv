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
#if !defined( __PPPPROJ_H_INCLUDED__ )
#define __PPPPROJ_H_INCLUDED__ 

#define _CRT_SECURE_NO_DEPRECATE

#include <cstdio>
#include <stdlib.h>
#include <cstring>
#include <stdarg.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <ctype.h>
#include <io.h>
#include <conio.h>
#include <share.h>
#include <errno.h>
#include <malloc.h>
#include <time.h>
#include <process.h>
#include <direct.h>
#include <cassert>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <wininet.h>

extern "C"
{
#include <winsock2.h>
}

#include "vardec.h"
#include "net.h"
#include "version.h"
#include "retcode.h"
#include "wshare.h"
#include "utility.h"


//
// Defined port numbers.
//
#define QOTD_PORT 17
#define TIME_PORT 37
#define NNTP_PORT 119
#define DEFAULT_POP_PORT 110
#define DEFAULT_SMTP_PORT 25


//
// Assert support
//
#if defined (_DEBUG)

#define PPP_ASSERT(x) assert(x)

#else // !_DEBUG

#define PPP_ASSERT(x)
#if !defined ( NDEBUG )
#define NDEBUG
#endif // NDEBUG

#endif // _DEBUG



#endif // __PPPPROJ_H_INCLUDED__
