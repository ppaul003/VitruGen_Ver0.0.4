@echo off
setlocal

set "ROOT=%~dp0.."
set "SOURCE=%ROOT%\VitruGen_ver004"
set "TESTS=%ROOT%\tests"
set "BUILD=%ROOT%\build\mvp-tests"

call "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b %errorlevel%

if not exist "%BUILD%" mkdir "%BUILD%"
pushd "%BUILD%"

cl.exe /nologo /std:c++14 /EHsc /W4 /WX /I"%SOURCE%" ^
    "%TESTS%\MvpAssetPipelineTests.cpp" ^
    "%SOURCE%\MvpAssetPipeline.cpp" ^
    /Fe:MvpAssetPipelineTests.exe
if errorlevel 1 (
    popd
    exit /b 1
)

MvpAssetPipelineTests.exe
set "TEST_RESULT=%ERRORLEVEL%"
if not "%TEST_RESULT%"=="0" (
    popd
    exit /b %TEST_RESULT%
)

cl.exe /nologo /std:c++14 /EHsc /W4 /WX /wd4127 ^
    /I"%SOURCE%" /I"C:\Users\richa\cuda-samples-master\Common" ^
    /I"C:\vcpkg\installed\x64-windows\include" ^
    "%TESTS%\MvpNavigationTests.cpp" ^
    "%SOURCE%\TheArbiter.cpp" ^
    /Fe:MvpNavigationTests.exe /link /LIBPATH:"%SOURCE%"
if errorlevel 1 (
    popd
    exit /b 1
)

MvpNavigationTests.exe
set "TEST_RESULT=%ERRORLEVEL%"
popd
exit /b %TEST_RESULT%
