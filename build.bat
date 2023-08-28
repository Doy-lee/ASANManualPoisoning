@echo off
set script_dir_backslash=%~dp0
set script_dir=%script_dir_backslash:~0,-1%

if not exist %script_dir%\Build mkdir %script_dir%\Build
pushd %script_dir%\Build
cl %script_dir%\asan_example.cpp -Zi -fsanitize=address -nologo -link || exit /b 1
popd
