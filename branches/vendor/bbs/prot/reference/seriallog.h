/*	$Id$	*/

#ifndef	SERIALLOG_H
#define	SERIALLOG_H


extern	FILE	*SerialLogFile ;
extern	void	SerialLogFlush() ;
extern	void	SerialLog(const void *data, int len, int w) ;

#endif
