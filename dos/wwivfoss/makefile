#######################################################################
# WWIVFOSS Makefile 
# 

MSVC_HOME	= C:\MSVC
INCLUDE 	= $(MSVC_HOME)\INCLUDE
LIB		= $(MSVC_HOME)\LIB
# Note: /G3 for 386 compile caused this to crash.
CPPFLAGS	= /nologo -I $(INCLUDE) -L $(LIB) /W4 /G0
LDFLAGS		= 
BINDIR		= bin\

all:	$(BINDIR)\WWIVFOSS.EXE \
		$(BINDIR)\UTILTEST.EXE

clean:
	-@DEL /Q $(BINDIR)\*.*
	-@DEL /Q .\*.COD

.cpp{$(BINDIR)}.obj:
    $(MSVC_HOME)\BIN\CL.EXE $(CPPFLAGS) /Zi /Od /Fo$@ /c $<

WWIVFOSS_OBJS	= $(BINDIR)\WWIVFOSS.OBJ \
		  $(BINDIR)\FOSSIL.OBJ \
		  $(BINDIR)\UTIL.OBJ \
		  $(BINDIR)\PIPE.OBJ

$(BINDIR)\WWIVFOSS.EXE::   $(WWIVFOSS_OBJS)
	-@MKDIR $(BINDIR)
	LINK /NOLOGO /CODEVIEW $(LDFLAGS) @<<$*.CRF
$(WWIVFOSS_OBJS: =+^
)
$@
$*.MAP


<<


UTILTEST_OBJS	= $(BINDIR)\UTILTEST.OBJ \
		  $(BINDIR)\UTIL.OBJ


$(BINDIR)\UTILTEST.EXE::   $(UTILTEST_OBJS)
	-@MKDIR $(BINDIR)
	LINK /NOLOGO $(LDFLAGS) @<<$*.CRF
$(UTILTEST_OBJS: =+^
)
$@
$*.MAP


<<
