@echo on

if not exist %1 rem: goto :usage
if not exist %2 rem: goto :usage

REM Enable echo lines below if you need to debug this script
REM echo %0
REM echo $(IntDir) = %1
REM echo $(OutDir) = %2
REM echo $(PlatformArchitecture) = %3
REM echo $(TargetExt) = %4

set $=%ProgramW6432%\7-Zip\;%ProgramFiles%\7-Zip\;%ProgramFiles(x86)%\7-Zip\
for %%$ in (7z.exe) do set SevenZip=%%~$$:$

set ArchivePath=%~dp0..\..\7z-src.7z
if not exist "%ArchivePath%" "%SevenZip%" a "%ArchivePath%" -m0=LZMA2 -r "%~dp0..\..\7-Zip\*"

goto :PostBuild$%3%4

:PostBuild$32.exe
:PostBuild$64.exe
"%~27zSfxHtm.exe" /create /target 4.0 "%~27zSfxHtmExeTest.exe" /adjunct "%0" /manifest "%~dp0res\requireAdministrator.manifest" "%ArchivePath%"
"%~27zSfxHtm.exe" /create /dll open /target 4.0 "%~27zSfxHtmExeTestOpen.cpl" /adjunct "%0" "%ArchivePath%"
"%~27zSfxHtm.exe" /create /dll runas /target 4.0 "%~27zSfxHtmExeTestRunAs.cpl" /adjunct "%0" "%ArchivePath%"
goto :PostBuild$

:PostBuild$32.dll
"%windir%\syswow64\rundll32.exe" "%~27zSfxHtm.dll",EntryPoint /create "%~27zSfxHtmDllTest.exe" /adjunct "%0" /manifest "%~dp0res\requireAdministrator.manifest" "%ArchivePath%"
"%windir%\syswow64\rundll32.exe" "%~27zSfxHtm.dll",EntryPoint /create /dll open "%~27zSfxHtmDllTestOpen.cpl" /adjunct "%0" "%ArchivePath%"
"%windir%\syswow64\rundll32.exe" "%~27zSfxHtm.dll",EntryPoint /create /dll runas "%~27zSfxHtmDllTestRunAs.cpl" /adjunct "%0" "%ArchivePath%"
goto :PostBuild$

:PostBuild$64.dll
"%windir%\system32\rundll32.exe" "%~27zSfxHtm.dll",EntryPoint /create "%~27zSfxHtmDllTest.exe" /adjunct "%0" /manifest "%~dp0res\requireAdministrator.manifest" "%ArchivePath%"
"%windir%\system32\rundll32.exe" "%~27zSfxHtm.dll",EntryPoint /create /dll open "%~27zSfxHtmDllTestOpen.cpl" /adjunct "%0" "%ArchivePath%"
"%windir%\system32\rundll32.exe" "%~27zSfxHtm.dll",EntryPoint /create /dll runas "%~27zSfxHtmDllTestRunAs.cpl" /adjunct "%0" "%ArchivePath%"
goto :PostBuild$

:PostBuild$

exit

:usage
echo ###############################################################################
echo # 7zSfxHtm.vcxproj post-build script                                          #
echo # Not intended for direct invocation through user interface                   #
echo # Post-build command line:                                                    #
echo # PostBuild.bat "$(IntDir)" "$(OutDir)" $(PlatformArchitecture) $(TargetExt)  #
echo ###############################################################################
pause
