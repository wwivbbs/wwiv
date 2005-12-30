##############################################################################
#
# WWIV 5.0 MAKEFILE for OS/2
# Copyright (c) 2000-2006 WWIV Software Services
# All Rights Reserved.
##

OBJFILES = bbs.o batch.o bbsovl1.o bbsovl2.o \
	 bbsovl3.o \
    bbsutl.o bbsutl1.o bbsutl2.o callback.o \
    chat.o chnedit.o circbuf.o com.o \
    conf.o connect1.o crc.o defaults.o \
    diredit.o dosemu.o events.o \
    gfiles.o gfledit.o ini.o instmsg.o lilo.o \
    listplus.o menu.o menu_pd.o menuedit.o \
    menuspec.o menusupp.o misccmd.o modem.o \
    msgbase.o msgbase1.o multinst.o multmail.o \
    netsup.o newuser.o readmail.o sr.o srrcv.o \
    srsend.o strings.o subacc.o subedit.o \
    subreq.o subxtr.o sysopf.o tcpip.o uedit.o \
    utility.o version.o voteedit.o WComm.o \
    Wios.o Wiot.o xfer.o xferovl.o xferovl1.o \
    xfertmp.o xinit.o wshare.o wutil.o \
    shortmsg.o attach.o automsg.o bbslist.o \
    chains.o colors.o datetime.o dupphone.o \
    inetmsg.o memory.o asv.o status.o syschat.o \
    sysoplog.o user.o vote.o wqscn.o WLocalIO.o \
    makewnd.o reboot.o filesupp.o
INCLUDEPATH = platform\OS2;platform

.cpp.o:
    gcc $(CFLAG1) -I$(INCLUDEPATH) -D__OS2__ -c { $< } 2>>make.log


all: clearlog WWIV50o.exe

WWIV50o.exe: 	$(OBJFILES)

clearlog:
	-del make.log

