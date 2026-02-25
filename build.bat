@echo off

echo Compiling RayRider...
g++ main.cpp -o RayRider.exe -I./include -L./lib -lraylib -lopengl32 -lgdi32 -lwinmm -std=c++17

if %errorlevel% neq 0 (
    echo Compilation Failed for RayRider!
    pause
    exit /b %errorlevel%
)
echo Compilation Successful!
echo Run RayRider.exe to play!
