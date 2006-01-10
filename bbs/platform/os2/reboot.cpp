/****************************************************************************/
/*                                                                          */
/*                             WWIV Version 5.0x                            */
/*            Copyright (C) 1998-2003 by WWIV Software Services             */
/*                       Copyright (c) 1996, Mike Deweese                   */
/*                           Used with permission                           */
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


void WWIV_RebootComputer()
{
    {
        HFILE hf;
        ULONG dummy;
        
        ULONG rc = DosOpen("DOS$", &hf, &dummy, 0L, FILE_NORMAL, FILE_OPEN,
            OPEN_ACCESS_WRITEONLY | OPEN_SHARE_DENYNONE |
            OPEN_FLAGS_FAIL_ON_ERROR, NULL);

        if (!rc) 
        {
            // Flush the caches
            DosShutdown(1);
            
            // Shut down the file system
            DosShutdown(1);
            rc = DosShutdown(0);
            if (!rc) 
            {
                // Reboot the machine
                DosDevIOCtl(hf, 0xd5, 0xab, NULL, 0, NULL, NULL, 0, NULL);
            }
        }
    }
}


