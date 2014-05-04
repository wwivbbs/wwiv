#include "ARGUMENT.H"
#include "EDITOR.H"
#include <stdlib.h>
#include <string.h>

#define GetFileString(s, stream) fgets(s, 81, stream); \
																 if (strchr(s, '\n'))  \
																	 *strchr(s,'\n')=0;


BREditArguments::BREditArguments(int argc, char **argv)

// BREDIT can accept three different types of command lines:
//
// 1. BREDIT filename.  This is for general purpose editing (perhaps
//    from a //DOS prompt.  It assumes no WWIV handling of codes,
//    sysop mode, and enhanced editing.
//
// 2. BREDIT filename 80 25 200 [params]
//    where [params] is a space-delimited set of options (in any order):
//
//    S   Local Mode
//    D   Disables Enhanced mode
//    E   Disable built-in handling of enhanced mode
//    H   Disables BREDIT's built in extended ANSI handler
//

{
// Set up defaults

	ScreenLines=24;
	MaxColumns=80;
	AnonymousStatus=0;
	MaxLines=300;
	EnhancedMode=AUTO;
        DoEnhancedIO=0;
	WWIVColorChanges=(getenv("BBS")==NULL) ? 0 : 1;
	SysOpMode=0;
	EditFilename=NULL;
	strcpy(HelpFile, argv[0]);
	strcpy(strchr(HelpFile, '.'), ".HLP");
        if (argc==2)
        {
                SysOpMode=1;
                WWIVColorChanges=0;
                EditFilename=argv[1];
        }
        else if (argc > 4)
        {
		EditFilename=argv[1];
		MaxColumns=atoi(argv[2]);
		ScreenLines=atoi(argv[3])-1;
		MaxLines=atoi(argv[4]);

		int i;

		for(i=5;i<argc;i++) {
                        switch(toupper(argv[i][0]))
                        {
			case 'S':
				SysOpMode=1;
				EnhancedMode=ON;
                                DoEnhancedIO=1;
				break;
			case 'D':
				EnhancedMode=OFF;
				break;
			case 'E':
				EnhancedMode=ON;
				break;
			case 'H':
				DoEnhancedIO=0;
				break;
			}
		}
		FILE *stream;
		char s[81];

		TitleChangeable=0;
		EnableTag=1;
                if ((stream=fopen("EDITOR.INF", "rb"))!=NULL)
                {
			TitleChangeable=1;
			GetFileString(Title, stream);
			GetFileString(Destination, stream);
			if (strchr(Destination, '#'))
                        {
				EnableTag=0;
                        }
			GetFileString(s, stream); // User number
			GetFileString(s, stream); // User name
			GetFileString(s, stream); // User real name
			GetFileString(s, stream); // User SL

			if (atoi(s)==255)
                        {
				SysOpMode=1;
                        }
			GetFileString(s, stream);
			if (atoi(s)==1)
                        {
				EnableTag=0;
                        }
			fclose(stream);
		}
        }
        else
        {
		printf("\n\nBREdit - Full Screen Editor version %s\n\n"
					"Invalid parameters given\n\n"
                                        "BREDIT Filename [Columns Rows MaxLines [S D E H]]\n\n"
					"See BREDIT.DOC for more information on paramters.\n\n", VERSION);
		exit(1);
	}
}

BREditArguments::~BREditArguments(void)
{
        if (TitleChangeable)
        {
		FILE *stream;
		stream=fopen("RESULT.ED", "wt");
		fprintf(stream, "%d\n%s\n", AnonymousStatus, Title);
		fclose(stream);
	}
}


