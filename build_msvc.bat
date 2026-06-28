@echo off
setlocal

call "E:\Visual Studio\Community\VC\Auxiliary\Build\vcvars64.bat"
if errorlevel 1 exit /b %errorlevel%

cl /nologo /std:c++17 /utf-8 /EHsc cppks.cpp tin.cpp /Fe:tin_app.exe
