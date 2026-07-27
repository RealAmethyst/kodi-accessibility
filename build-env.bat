@ECHO OFF
REM Build environment for the accessibility fork.
REM Its contents are documented in the project's BUILD.md so it is reproducible.
REM
REM Three things this fixes, all of which otherwise fail confusingly:
REM
REM 1. NoDefaultCurrentDirectoryInExePath. When set, cmd refuses to run a program
REM    found in the current directory. Kodi's build scripts call each other by
REM    bare name (CALL prepare-env.bat), so every one of them fails with
REM    "is not recognized as an internal or external command".
REM
REM 2. CMake. The CMake on the system PATH is 4.x, whose major line dropped
REM    compatibility with cmake_minimum_required below 3.5 — fine for Kodi's own
REM    root, but dependency builds are where old minimums live. The Build Tools
REM    ship 3.31.6, which is what Kodi expects, so prefer that one here and leave
REM    the system CMake alone for everything else.
REM
REM 3. nasm. Required by FFmpeg. winget installs it under AppData\Local\bin,
REM    not Program Files, and does not add it to PATH.

SET "NoDefaultCurrentDirectoryInExePath="

SET "VSCMAKE=C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
SET "NASMDIR=%LOCALAPPDATA%\bin\NASM"

IF NOT EXIST "%VSCMAKE%\cmake.exe" (
  ECHO ERROR: expected CMake from the 2022 Build Tools at:
  ECHO   "%VSCMAKE%"
  EXIT /B 1
)
IF NOT EXIST "%NASMDIR%\nasm.exe" (
  ECHO ERROR: nasm not found at "%NASMDIR%"
  EXIT /B 1
)


REM 4. Java. Kodi generates its Python bindings with a Groovy script, and the
REM    bundled Groovy is 4.0.26, which cannot read Java 25 bytecode -- it fails
REM    with "Unsupported class file major version 69" deep inside an MSBuild
REM    custom-build step, reported only as error MSB8066.
REM    This machine has JDK 25 installed, and Oracle's javapath shim puts it
REM    ahead of everything else on the system PATH, so cmd resolves java to 25
REM    even though Git Bash resolves it to Temurin 21. Pin the build to 21.
SET "JAVA_HOME=C:\Program Files\Eclipse Adoptium\jdk-21.0.10.7-hotspot"
IF NOT EXIST "%JAVA_HOME%\bin\java.exe" (
  ECHO ERROR: expected a Java 21 JDK at:
  ECHO   "%JAVA_HOME%"
  EXIT /B 1
)
SET "PATH=%JAVA_HOME%\bin;%PATH%"

SET "PATH=%VSCMAKE%;%NASMDIR%;%PATH%"

ECHO Build environment ready.
FOR /f "tokens=*" %%v IN ('cmake --version ^| findstr /R "^cmake"') DO ECHO   %%v
FOR /f "tokens=*" %%v IN ('nasm -v') DO ECHO   %%v

FOR /f "tokens=*" %%v IN ('java -version 2^>^&1 ^| findstr /R "version"') DO ECHO   %%v
