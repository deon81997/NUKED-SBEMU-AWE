@ECHO OFF
REM Second half of INSTALL.BAT; runs from the hard disk so the floppies can be swapped.
%NSBD%:\NUKEDSB\FLOPPY\HDPMI32I.EXE -r -x > NUL
SET NSBN=1
:NEXT
IF EXIST %NSBS%:\DISK%NSBN%.PAK GOTO HAVE
ECHO.
ECHO  Insert disk %NSBN% in drive %NSBS%: and press any key  (Ctrl+C = stop)
PAUSE > NUL
GOTO NEXT
:HAVE
%NSBD%:\NUKEDSB\FLOPPY\UNPAK %NSBS%:\DISK%NSBN%.PAK %NSBD%:\NUKEDSB
IF ERRORLEVEL 10 GOTO DONE
IF ERRORLEVEL 3 GOTO WFAIL
IF ERRORLEVEL 1 GOTO RFAIL
IF "%NSBN%"=="1" GOTO N2
IF "%NSBN%"=="2" GOTO N3
IF "%NSBN%"=="3" GOTO N4
IF "%NSBN%"=="4" GOTO N5
IF "%NSBN%"=="5" GOTO N6
IF "%NSBN%"=="6" GOTO N7
IF "%NSBN%"=="7" GOTO N8
IF "%NSBN%"=="8" GOTO N9
GOTO DONE
:N2
SET NSBN=2
GOTO NEXT
:N3
SET NSBN=3
GOTO NEXT
:N4
SET NSBN=4
GOTO NEXT
:N5
SET NSBN=5
GOTO NEXT
:N6
SET NSBN=6
GOTO NEXT
:N7
SET NSBN=7
GOTO NEXT
:N8
SET NSBN=8
GOTO NEXT
:N9
SET NSBN=9
GOTO NEXT
:RFAIL
ECHO.
ECHO  Disk %NSBN% could not be read. Clean the drive or write that disk again,
ECHO  then press any key to retry disk %NSBN%  (Ctrl+C = stop)
PAUSE > NUL
GOTO NEXT
:WFAIL
ECHO.
ECHO  Cannot write to %NSBD%:\NUKEDSB - is the drive full? (about 25 MB needed)
GOTO END
:DONE
ECHO.
ECHO  All disks unpacked. Running SETUP %NSBD% ...
%NSBD%:
CD \NUKEDSB
CALL SETUP %NSBD%
ECHO.
ECHO  Done. The unpacked pack stays in %NSBD%:\NUKEDSB (README.TXT is there);
ECHO  you can delete that folder later, as after unzipping.
:END
SET NSBN=
