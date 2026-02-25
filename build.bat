@echo off

echo Compiling Curious Bike Demo...
g++ main.cpp -o bike_demo.exe -I./include -L./lib -lraylib -lopengl32 -lgdi32 -lwinmm -std=c++17

if %errorlevel% neq 0 (
    echo Compilation Failed for bike_demo!
    pause
    exit /b %errorlevel%
)
echo Compilation Successful!
echo Run bike_demo.exe to play!
