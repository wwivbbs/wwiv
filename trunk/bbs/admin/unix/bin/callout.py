#!/usr/bin/python2.7
#
# callout.py
#
# loops over all the processed *.uue files created for sending
# and sends them all out.  Once done, it moves them to the sent
# directory as a backup in case something goes wrong.

import os
import sys
import telnetlib
import time
import shutil

#config variables
HOST="skulls.wwivbbs.com"
PORT=2525
queuedir="REPLACE-WWIVBASE/data/wwivnet/mqueue/"
sentdir="REPLACE-WWIVBASE/data/wwivnet/sent/"

def sendstuff():
     tn=telnetlib.Telnet( HOST, PORT )
     #tn.set_debuglevel(10)
     
     print tn.read_until("220")
     print ("EHLO skulls.wwivbbs.com\n")
     tn.write("EHLO skulls.wwivbbs.com\n")
     print tn.read_until("250 HELP")
     for i in mqueue:
          if i[-3:]=='uue':
               f=open(queuedir+i)
               f.readline()
               s=f.readline()
               print ("Delivering %s %s " % (i, s) )
               f.close()
               tn.write("MAIL FROM: <bbsname@skulls.wwivbbs.com>\n")
               tn.read_until("OK")
               tn.write("RCPT %s" % s)
               tn.read_until("OK")
               tn.write("DATA\n")
               tn.read_until('354 Enter mail, end with "." on a line by itself')
               f=open(queuedir+i)
               tn.write(f.read())
               tn.write(".\r\n")
               tn.read_until("250")
               f.close()
               shutil.move(queuedir+i,sentdir+i)
     tn.write("QUIT\n")
     print tn.read_all()
     tn.close()

if __name__ == '__main__':
    mqueue=os.listdir(queuedir)
    if (len(mqueue) > 0):
        sendstuff()
    else:
    	print 'nothing to send'
