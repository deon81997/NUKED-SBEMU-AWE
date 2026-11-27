@ECHO OFF
REM Second half of INSTALL.BAT; runs from the hard disk so the floppies can be swapped.
%NSBD%:\NUKEDSB\FLOPPY\HDPMI32I.EXE -r -x > NUL
%NSBD%:\NUKEDSB\FLOPPY\UNPAK %NSBS%:\DISK1.PAK %NSBD%:\NUKEDSB
IF ERRORLEVEL 3 GOTO WFAIL
IF ERRORLEVEL 1 GOTO END
GOTO DONE
:WFAIL
ECHO.
ECHO  Cannot write to %NSBD%:\NUKEDSB - is the drive full?
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
