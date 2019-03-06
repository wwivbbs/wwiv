WWIV BBS
========

WWIV is compiled with the following compilers:
  
- MS Visual C++ 2017 Community Edition.
- GCC 6.3 (or later) on Linux 
  (Debian9, Centos7 (with GCC 7) or Ubuntu)

You will need CMake 3.9 or later to build WWIV.

***

# Building WWIV BBS
***

## Windows Builds
This assumes you already have github desktop installed.
We prefer contributors to FORK WWIVBBS repositories to their account and work from there.
If you're on Windows this is likely in this folder: "Documents\GitHub\WWIV"

### Download Visual Studio
WWIV is compiled with the VS2017 compiler for windows. You can download [Microsoft Visual Studio 2017 Community](https://www.visualstudio.com/downloads/)

### Install VS2017
Choose a custom install and select the following components:
```
C++
   Common Tools for C++
Common Tools
   Git Fow Windows <- You should have this already
   GitHub for VS
```

## Build WWIV
* From the VS2017 menu, select File and then Open from Source Control
On the bottom, you should see your local GIT repositories already.
Above that you will see Login to GitHub, do that.
* Now in your Local repositories (Documents\GitHub\WWIV), open the
  folder WWIV in Visual Studio. It should recognize the CMake build
  and be able to build WWIV.
* When VS says "READY" on the bottom, go to Build on the menu and select Build Solution(F7). If you have any build errors, run Build one more time and see if that resolves itself as there can be timing issues on some machines.
* You select whether or not you are building DEBUG or RELEASE on the toolbar. Those binaries and other built files will be places in a \debug and \release folder along side your github source files. ex: ```Documents\GitHub\WWIV\debug``` or ```Documents\GitHub\WWIV\release```.

## Linux Builds
This only builds the binaries, it does NOT include the supporting files.  Please follow the 
[Linux Installation](http://docs.wwivbbs.org/en/latest/linux_installation/) instructions for getting the supporting files in place.

** NOTE:** Do these steps as a non-root user; your BBS user would be the easiest from a file permissions perspective later on.  root should never be used to compile binaries.

### Things you need:

Package | Comments
------- | ----------
git | to grab the source code for compiling  
ncurses | ncurses-devel, libncurses5-dev, etc depending on your distro
cmake | 3.9 or later
make | (unless you want to experiment with ninja)
g++ 6.3.0 or later | 

### Build Steps
There are two primary ways to get the files for building; download a zip of the project or clone the repo.  In both cases, you will end up with the following files in the build directory:  

* bbs/bbs  
* wwivconfig/wwivconfig  
* wwivd/wwivd  
* wwivutil/wwivutil  
* network/network  
* networkb/networkb
* networkc/networkc
* networkf/networkf
* network1/network1
* network2/network2
* network3/network3

#### Using a downloaded .zip (no git required)
* If you don't want to worry about managing a git repo and just want the files, you can download the zipped project file from GitHub.  Go to 
https://github.com/wwivbbs/wwiv and click on the [Download Zip](https://github.com/wwivbbs/wwiv/archive/master.zip) Button
* unzip the zip file. It will create a wwiv-master directory in your current location
* navigate to wwiv-master

#### Using a git repository
If you plan to have an active repo, we prefer contributors to FORK WWIVBBS repositories to their account and work from there.  
* [Fork](https://help.github.com/articles/fork-a-repo/), then clone your fork
    
    ```bash
    # Create a directory for your fork's clone.
    mkdir git
    chdir git
    # Clone your fork into the current directory (git).
    # Use your GitHub username instead of <em>YOUR-USERNAME</em>
    git clone --recursive https://github.com/<em>YOUR-USERNAME</em>/wwiv.git
    ```
* Navigate to wwiv

#### Compiling WWIV

No matter which way you used (source zip or git repository), compiling WWIV is the same.

You need to compile the dependencies first. Enter the cloned repository, change to the ```deps/cl342``` directory and then do

```
make
```

* If you want to create a debug version, run ```./debug.sh``` instead of
  ```cmake ..``` in the next step.
* run the following:
  ```mkdir _build && cd _build && cmake .. && cmake --build . -- -j$(cat /proc/cpuinfo | awk '/^processor/{print $3}' | wc -l)``` 
  (don't forget the ".")


Now you can enter the ```bbs/admin/unix``` directory and run ```sudo ./install.sh```
*** 

Installation and SysOp Instructions
====================

All the installation and SysOp administration information you 
need is in the [WWIV Documentation](https://docs.wwivbbs.org/)

Get Involved
====================

If you want to help out with WWIV BBS:

* Read the [Contributors Guidelines](contributing.md)
* Check out the [WWIVBBS Homepage](https://www.wwivbbs.org) and find us on IRC.
* Jump into the [Issues List](https://github.com/wwivbbs/wwiv/issues).
