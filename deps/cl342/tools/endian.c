/* This is a lowest-common-denominator program which should compile under
   the default cc on any system, no matter how primitive (even the SunOS
   one - note the use of 1970s-vintage octal escapes).  This is necessary
   because it's compiled before we can try and select a decent compiler via
   the makefile */

#include <stdio.h>
#include <stdlib.h>

int main()
	{
	if( *( long * ) "\200\0\0\0\0\0\0\0" < 0 )
		printf( "-DDATA_BIGENDIAN" );
	else
		printf( "-DDATA_LITTLEENDIAN" );

	return( 0 );
	}
