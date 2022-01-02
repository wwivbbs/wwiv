/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5                            */
/*             Copyright (C)2021-2022, WWIV Software Services             */
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
#include "util.h"

#include <conio.h>
#include <ctype.h>
#include <dos.h>
#include <fcntl.h>
#include <io.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#pragma warning(disable : 4505)
#pragma warning(disable : 4127)

#define ASSERT_EQ(x, y) do { \
  if ((x) != (y)) { \
    fprintf(stderr, "FAILED EQ: %s:%d: %d != %d\r\n ", __FILE__, __LINE__, x, y); \
    fflush(stderr); \
    return 1; \
  }  \
} while(0)

#define ASSERT_STR_EQ(x, y) do { \
  if (strcmp((x), (y)) != 0) { \
    fprintf(stderr, "FAILED STR_EQ: %s:%d: %s != %s\r\n ", __FILE__, __LINE__, x, y); \
    fflush(stderr); \
    return 1; \
  }  \
} while(0)


int main(int, char**) {
  Array a(10);
  ASSERT_EQ(0, a.size());

  a.push_back("Hello");
  ASSERT_EQ(1, a.size());
  
  a.push_back("World");
  ASSERT_EQ(2, a.size());
  
  ASSERT_STR_EQ("Hello", a.at(0));
  ASSERT_STR_EQ("World", a.at(1));
  fprintf(stderr, "Success!\r\n");
  return 0;
}
