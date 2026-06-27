@echo off
setlocal EnableDelayedExpansion

set "SRC_DIR=%~1"
if not defined SRC_DIR (
  echo ERROR: LuaJIT source directory not specified.
  exit /b 1
)

set "VCVARS="
for %%V in (18 2022 2019) do (
  if exist "%ProgramFiles%\Microsoft Visual Studio\%%V\Community\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS=%ProgramFiles%\Microsoft Visual Studio\%%V\Community\VC\Auxiliary\Build\vcvars64.bat"
    goto :vcfound
  )
)
for %%V in (18 2022 2019) do (
  if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\%%V\BuildTools\VC\Auxiliary\Build\vcvars64.bat" (
    set "VCVARS=%ProgramFiles(x86)%\Microsoft Visual Studio\%%V\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
    goto :vcfound
  )
)
echo ERROR: vcvars64.bat not found. Install Visual Studio with the C++ workload.
exit /b 1

:vcfound
call "%VCVARS%"
if errorlevel 1 exit /b 1

cd /d "%SRC_DIR%\src"
if errorlevel 1 exit /b 1

call msvcbuild.bat
exit /b %ERRORLEVEL%
