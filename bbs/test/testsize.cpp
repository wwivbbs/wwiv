/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.0x                         */
/*             Copyright (C)1998-2004, WWIV Software Services             */
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
/////////////////////////////////////////////////////////////////////////////
// WWIV 5.0 Structure Size test application

#include <stdio.h>
#ifdef __MSDOS__
#include <io.h>
#endif

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define STRICT
#include <io.h>
#include <windows.h>
#endif // _WIN32

#include "../vardec.h"
#include "../net.h"

#define EXIT_NORMAL     0
#define EXIT_BAD_PARAMS 1
#define EXIT_BAD_FNAME  2

#define WRITE_SIZE(x) fprintf(hOut, "sizeof (%s) = %d\n", #x, sizeof(x));

int main(int argc, char * argv[])
{
        FILE* hOut = NULL;
        if (argc != 2)
        {
                printf("WWIV 5.0 structure size test utility\r\n");
                printf("Copyright 2000-2001 Rushfan\r\n\n");
                printf("Usage:\n\tTESTSIZE <output filename>\n\n");
                return EXIT_BAD_PARAMS;
        }

        hOut = fopen(argv[1], "wt");
        if (NULL == hOut)
        {
                printf("* ERROR: Unable to open file '%s'\r\n\n", argv[1]);
                return EXIT_BAD_FNAME;
        }

        WRITE_SIZE(net_header_rec);
        WRITE_SIZE(userrec);
        WRITE_SIZE(slrec);
        WRITE_SIZE(valrec);
        WRITE_SIZE(arcrec);
        WRITE_SIZE(configrec);
        WRITE_SIZE(configoverrec);
        WRITE_SIZE(statusrec);
        WRITE_SIZE(colorrec);
        WRITE_SIZE(subboardrec);
        WRITE_SIZE(directoryrec);
        WRITE_SIZE(smalrec);
        WRITE_SIZE(messagerec);
        WRITE_SIZE(postrec);
        WRITE_SIZE(mailrec);
        WRITE_SIZE(tmpmailrec);
        WRITE_SIZE(shortmsgrec);
        WRITE_SIZE(voting_response);
        WRITE_SIZE(uploadsrec);
        WRITE_SIZE(tagrec);
        WRITE_SIZE(zlogrec);
        WRITE_SIZE(chainfilerec);
        WRITE_SIZE(chainregrec);
        WRITE_SIZE(newexternalrec);
        WRITE_SIZE(editorrec);
        WRITE_SIZE(usersubrec);
        WRITE_SIZE(userconfrec);
        WRITE_SIZE(batchrec);
        WRITE_SIZE(ext_desc_type);
        WRITE_SIZE(gfiledirrec);
        WRITE_SIZE(gfilerec);
        WRITE_SIZE(languagerec);
        WRITE_SIZE(result_info);
        WRITE_SIZE(modem_info);
        WRITE_SIZE(filestatusrec);
        WRITE_SIZE(asv_rec);
        WRITE_SIZE(adv_asv_rec);
        WRITE_SIZE(cbv_rec);
        WRITE_SIZE(phonerec);
        WRITE_SIZE(eventsrec);
        WRITE_SIZE(ext_desc_rec);
        WRITE_SIZE(instancerec);
        fclose(hOut);
        return EXIT_NORMAL;
}
