@echo off
echo Compiling GLFW project...
C:/MinGW/bin/g++.exe -g -std=c++17 -I"include" -L"lib" "src/main.cpp" "src/glad.c" -lglfw3dll -lopengl32 -lgdi32 -o main.exe
if %ERRORLEVEL% equ 0 (
    echo Compilation successful! Running program...
    main.exe
) else (
    echo Compilation failed!
    pause
)
