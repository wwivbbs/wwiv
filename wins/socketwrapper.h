#if !defined( __SOCKETWRAPPER_H__INCLUDED__ )
#define __SOCKETWRAPPER_H__INCLUDED__
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


/////////////////////////////////////////////////////////////////////////////
//
// Defines
//
//



//  -- n.b. Used to be 1st like in SOCK_READ_ERR macro
//  sock_err:                                                                     

#define SOCK_READ_ERR(PROTOCOL, ACTION)                                         \
    switch (PROTOCOL##_stat) {                                                  \
      case 1 :                                                                  \
        PROTOCOL##_Err_Cond = PROTOCOL##_OK;                                    \
        fprintf(stderr, "\n ! "#PROTOCOL"> Session error : sockerr");           \
        ACTION;                                                                 \
        aborted = 1;                                                            \
        exit(EXIT_SUCCESS);                                                     \
      case -1:                                                                  \
        PROTOCOL##_Err_Cond = PROTOCOL##_OK;                                    \
        fprintf(stderr, "\n ! "#PROTOCOL"> Timeout : sockerr");                 \
        ACTION;                                                                 \
        aborted = 1;                                                            \
        exit(EXIT_SUCCESS);                                                     \
    }

#define SOCK_GETS(PROTOCOL)                                                     \
  sock_gets(sock, _temp_buffer, sizeof(_temp_buffer));         \
  if (DEBUG) fprintf(stderr, "\n"#PROTOCOL"> %s\n", _temp_buffer);                  \
  PROTOCOL##_Err_Cond = atoi(_temp_buffer);                                     \

#define SMTP_FAIL_ON(NUM, ACTION)                                               \
  if (SMTP_Err_Cond == NUM) {                                                   \
    if (DEBUG) fprintf(stderr, "\nSMTP Failure> '" #NUM "'\n");                   \
    sock_puts(sock, "QUIT");                                         \
    ACTION;                                                                     \
    aborted = 1;                                                                \
    return 0;                                                                   \
  }

#define SMTP_RESET_ON(NUM, ACTION)                                              \
  if (SMTP_Err_Cond == NUM) {                                                   \
    if (DEBUG) fprintf(stderr, "\nSMTP Failure> '" #NUM "'\n");                   \
    sock_puts(sock, "RSET");                                         \
    sock_gets(sock, _temp_buffer, sizeof(_temp_buffer));             \
    ACTION;                                                                     \
    aborted = 1;                                                                \
    return(0);                                                                  \
  }


// HACK!!
#define NOOP



/////////////////////////////////////////////////////////////////////////////
//
// Function Prototypes
//
//

int sock_tbused( SOCKET sock );
void sock_flushbuffer( SOCKET s );
int sock_read( SOCKET s, char *pszText, int nBufSize );
int sock_gets( SOCKET s, char * pszText, int nBufSize );
int sock_puts( SOCKET s, const char *pszText );
int sock_write( SOCKET s, const char *pszText );
bool InitializeWinsock();

#endif // __SOCKETWRAPPER_H__INCLUDED__


