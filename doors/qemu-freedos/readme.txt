Simple instructions for QEMU/FreeDOS Doors under WWIV.

1) Install QEMU according to your Linux Distributions instructions.
   On debian this is:
   sudo apt-get install qemu-kvm qemu

2) Make a Disk Image for FreeDos:
   qemu-img create -f raw freedos.img 100M

3) Download FreeDOS ISO.
   I grabbed the latest 1.3RC4 Live CD ISO from:
   https://www.freedos.org/download/

4) Install FreeDOS.  Pick any configuration, but I didn't need
   a big one, so choose the basic system without sources.
   Here's the command I used.
   qemu-system-x86_64 freedos.img -cdrom ./FD13LIVE.iso -boot d

5) You will need to reboot after partitioning (DOS) and then should
   have a basic functioning system.

6) Download BNU FOSSIL driver and put it in a place where you can
   get it to as drive D: (i.e. put it in wwiv/doors), run QEMU
   and copy it to C:\BNU.  Also replace FDAUTO.BAT and FDCONFIG.SYS
   with the ones here.

If you want to look at the disk image read only from linux, you can
use the following command:

sudo mount -o loop,offset=32256 /wwiv/freedos.img /mnt


Use:

  1) The way this works is D: should be mounted under QEMU where the doors
     are installed.
  2) E: should be the node's temp directory.
  3) When running a door, write out the command to use into a batch file
     located in the node's temp dir called DOOR.BAT, this will be invoked
     by the end of FreeDOS's FDAUTO.BAT on bootup.
  4) Also note that since the C: drive contents are not modified while using
     a door, the gw.sh script copies it to the temp directory to use it, this
     way multiple nodes can be in doors at the same time. Just make sure that
     two nodes don't use the same door at the same time, since drive D is
     shared across the two (but writes from one VM are not visible immediately
     to the other).

Contents
	* gw.sh - sample script to use for Global WAR
	* wwivqemu.sh - wrapper to spawn QEMU seting most of the defaults
	  we want.
	* local.sh - local wrapper script to run QEMU locally to access
	  the FreeDOS contents.

This worked for me under Ubuntu and a Debian VM in the cloud.
