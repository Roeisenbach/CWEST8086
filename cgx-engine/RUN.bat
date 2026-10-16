@echo off
rem ===========================================================================
rem  CGX Tournament - double-click this file.
rem
rem  Put survivor files in the "survivors" folder next to this one, and zombies
rem  in "zombies".  Then press START in the window.
rem
rem  Everything is relative to this folder, so you can put it anywhere and call
rem  it anything.  Nothing is downloaded and nothing is installed.
rem
rem  It never rebuilds the ENGINE - it links against the gate-proven class files
rem  in engine\, the exact bytes that carry the G1/G2/G3/OpSweep proof.  It only
rem  ever compiles the .java files in gui\, and only when you have edited one.
rem ===========================================================================
setlocal
title CGX Tournament

set "HERE=%~dp0"
set "HEREN=%HERE:~0,-1%"
set "CLASSES=%HEREN%\engine"
set "GUISRC=%HEREN%\gui"
set "GUIB=%GUISRC%\buildGui"
set "JAR=%HEREN%\lib\corewars8086-6.0.0.jar"
set "STAMP=%GUIB%\src-stamp"

rem --- find a JDK.  Prefer JAVA_HOME, then anything on PATH, then the usual
rem     install locations.  javac is only needed if a gui .java has changed;
rem     java alone is enough to launch a build that is already current.
set "JAVA="
set "JAVAC="
if defined JAVA_HOME if exist "%JAVA_HOME%\bin\java.exe" (
  set "JAVA=%JAVA_HOME%\bin\java.exe"
  if exist "%JAVA_HOME%\bin\javac.exe" set "JAVAC=%JAVA_HOME%\bin\javac.exe"
)
if not defined JAVA for %%J in (java.exe) do if not defined JAVA set "JAVA=%%~$PATH:J"
if not defined JAVAC for %%J in (javac.exe) do if not defined JAVAC set "JAVAC=%%~$PATH:J"
if not defined JAVA (
  for /d %%D in ("C:\Program Files\Java\jdk*" "C:\Program Files\Eclipse Adoptium\jdk*" "C:\Program Files\Microsoft\jdk*") do (
    if not defined JAVA if exist "%%D\bin\java.exe" (
      set "JAVA=%%D\bin\java.exe"
      set "JAVAC=%%D\bin\javac.exe"
    )
  )
)
if not defined JAVA (
  echo.
  echo   No Java found.  Install a JDK 17 or newer, or set JAVA_HOME.
  echo.
  pause
  exit /b 1
)

if not exist "%JAR%" (
  echo.
  echo   Missing: %JAR%
  echo   The lib folder must sit next to this file.
  echo.
  pause
  exit /b 1
)
if not exist "%CLASSES%\Tourney.class" (
  echo.
  echo   Missing engine classes at: %CLASSES%
  echo.
  pause
  exit /b 1
)

rem --- rebuild the window only if a gui .java is newer than the last build ---
if not exist "%GUIB%\CgxGuiFrame.class" goto :build
if not exist "%STAMP%\" goto :launch
xcopy /L /D /Y "%GUISRC%\*.java" "%STAMP%\" 2>nul | findstr /b /c:"0 File(s)" >nul
if errorlevel 1 goto :build
goto :launch

:build
if not defined JAVAC (
  echo.
  echo   A gui source changed but no javac was found - launching the existing build.
  echo.
  goto :launch
)
echo Building the window ...
if not exist "%GUIB%" mkdir "%GUIB%"
pushd "%GUISRC%"
dir /b /a-d *.java > "%GUIB%\sources.txt"
"%JAVAC%" -nowarn -cp "%CLASSES%;%JAR%" -d "%GUIB%" "@%GUIB%\sources.txt"
popd
if not exist "%GUIB%\CgxGuiFrame.class" (
  echo.
  echo   Build failed - see the messages above.
  echo.
  pause
  exit /b 1
)
if exist "%STAMP%" rmdir /s /q "%STAMP%"
mkdir "%STAMP%"
copy /y "%GUISRC%\*.java" "%STAMP%\" >nul

:launch
"%JAVA%" -Xmx2g -Dcgx.gui.dir="%HEREN%" -cp "%CLASSES%;%GUIB%;%JAR%" CgxGuiFrame %*
if errorlevel 1 pause
endlocal
