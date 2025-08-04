@echo off
echo Building Dino Runner GLUT version...

set GLUT_DIR=libs\freeglut
set INCLUDE_PATH=-I%GLUT_DIR%\include
set LIB_PATH=-L%GLUT_DIR%\lib
set LIBS=-lfreeglut -lopengl32 -lglu32

g++ %INCLUDE_PATH% %LIB_PATH% -o next.exe src\next.cpp %LIBS%

if %ERRORLEVEL% EQU 0 (
    echo Build successful! Run with: next.exe
    echo.
    echo Don't forget to copy freeglut.dll to the same directory as next.exe
    copy %GLUT_DIR%\bin\freeglut.dll . >nul 2>&1
    if exist %GLUT_DIR%\bin\freeglut.dll (
        echo freeglut.dll copied successfully!
    )
    echo.
    echo Starting Dino Runner GLUT version...
    echo Controls: UP arrow/SPACE/W to jump, R to restart, ESC to quit
    echo.
    start next.exe
) else (
    echo Build failed!
    pause
)
