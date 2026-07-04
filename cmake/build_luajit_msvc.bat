@echo off
setlocal EnableDelayedExpansion

set "SRC_DIR=%~1"
if not defined SRC_DIR (
  echo ERROR: LuaJIT source directory not specified.
  exit /b 1
)

REM Target architecture forwarded from CMake (CMAKE_VS_PLATFORM_NAME etc).
REM Defaults to x64 so existing x64 builds keep working if it is omitted.
set "TARGET_ARCH=%~2"
if not defined TARGET_ARCH set "TARGET_ARCH=x64"

REM --- Normalize the CMake target arch to a vcvars target token ---
set "TGT="
if /i "%TARGET_ARCH%"=="ARM64"   set "TGT=arm64"
if /i "%TARGET_ARCH%"=="ARM64EC" set "TGT=arm64"
if /i "%TARGET_ARCH%"=="AMD64"   set "TGT=x64"
if /i "%TARGET_ARCH%"=="x64"     set "TGT=x64"
if /i "%TARGET_ARCH%"=="x86_64"  set "TGT=x64"
if /i "%TARGET_ARCH%"=="Win32"   set "TGT=x86"
if /i "%TARGET_ARCH%"=="x86"     set "TGT=x86"
if not defined TGT (
  echo ERROR: Unsupported target architecture '%TARGET_ARCH%'.
  exit /b 1
)

REM --- Determine the host arch so we can pick a native or cross toolchain ---
set "HOSTRAW=%PROCESSOR_ARCHITECTURE%"
if defined PROCESSOR_ARCHITEW6432 set "HOSTRAW=%PROCESSOR_ARCHITEW6432%"
set "HST="
if /i "%HOSTRAW%"=="ARM64" set "HST=arm64"
if /i "%HOSTRAW%"=="AMD64" set "HST=x64"
if /i "%HOSTRAW%"=="x86"   set "HST=x86"
if not defined HST set "HST=x64"

REM vcvarsall takes "<target>" for a native build and "<host>_<target>" to cross.
if /i "%HST%"=="%TGT%" (
  set "VCARG=%TGT%"
) else (
  set "VCARG=%HST%_%TGT%"
)

REM --- Locate vcvarsall.bat across common editions/install roots ---
set "VCVARSALL="
for %%V in (18 2022 2019) do (
  for %%E in (Community Professional Enterprise BuildTools) do (
    if not defined VCVARSALL if exist "%ProgramFiles%\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvarsall.bat" (
      set "VCVARSALL=%ProgramFiles%\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvarsall.bat"
    )
  )
)
if not defined VCVARSALL for %%V in (18 2022 2019) do (
  for %%E in (Community Professional Enterprise BuildTools) do (
    if not defined VCVARSALL if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvarsall.bat" (
      set "VCVARSALL=%ProgramFiles(x86)%\Microsoft Visual Studio\%%V\%%E\VC\Auxiliary\Build\vcvarsall.bat"
    )
  )
)
if not defined VCVARSALL (
  echo ERROR: vcvarsall.bat not found. Install Visual Studio with the C++ workload.
  exit /b 1
)

echo Building LuaJIT for target '%TGT%' using vcvarsall '%VCARG%'
call "%VCVARSALL%" %VCARG%
if errorlevel 1 exit /b 1

cd /d "%SRC_DIR%\src"
if errorlevel 1 exit /b 1

call msvcbuild.bat
exit /b %ERRORLEVEL%
