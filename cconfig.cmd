rem WWIV Version 5.x
set CXX=C:\usr\bin\g++
set CC=C:\usr\bin\gcc

echo: Runing cmake on "%1"
cmake -DCMAKE_BUILD_TYPE=Release -DWWIV_ARCH=x86 -DWWIV_DISTRO=os2 %1

