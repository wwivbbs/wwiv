	###############################################################################
#
# WWIV 5.0 Test Utilities
# Copyright 2000 Rushfan
#
#

RM	= del
DOS_CC	= tcc
MSVC_CC	= cl
GCC_CC	= gcc
DIFF	= diff

default:
	@echo Please select a target:
	@echo  .
	@echo Targets:
	@echo     size

size:
	$(DOS_CC) testsize.cpp
	TESTSIZE size.dos
	$(MSVC_CC) testsize.cpp
	testsize.exe size.w32
	$(GCC_CC) testsize.cpp -o testsize.exe
	testsize.exe size.gcc
	@echo: Comparing Borland DOS Compiler with MS Visual C++
	diff size.dos size.w32
	@echo: Comparing Borland DOS Compiler with GCC
	diff size.dos size.gcc
	@echo: Comparing MS Visual C++ with GCC
	diff size.w32 size.gcc


clean:
	-$(RM) testsize.exe
	-$(RM) testsize.obj
	-$(RM) testsize.o

.PHONY:	size


