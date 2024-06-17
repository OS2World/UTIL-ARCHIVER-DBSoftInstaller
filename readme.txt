Ok, this is the DBSoft installer, it uses the freely available ACE compression
source code. To build this installer you need several things.

You need EMX gcc for OS/2. (I have only tested with 0.9d)
You need ACE for OS/2.
You need GNUFU for OS/2. (the GNU file utilities)
You need LXLITE.
You need GNU Make.
You need RC IBM's resource compiler.

All of these should be available from Hobbes or LEO.

Instructions:

1)
Replace the installer.bmp (and icon if desired) with your logo.

2)
Run make in the main directory. This should if everything is setup
properly generate a sfx.exe file.  This it the EXE header to your
self installing archive.

3) 
Run make in the include/ directory.  This should create include.exe
in the main directory.

4) 
Using the template provided sample.cfg, create a configuration file
for your application.  

5)
Use ACE for OS/2 to create a archive of your software.  The -r 
switch may be desirable to save subdirectories.

6)
Run "include <yourconfigfile>", which will take sfx.exe, your ACE
archive(s) and the information provided in the configuration file
to produce a install.exe file.  This file should be self installing.

This software is provided as is, with no warranty.

If you have any questions or comments write me at dbsoft@technologist.com.

Thanks!

Brian Smith


