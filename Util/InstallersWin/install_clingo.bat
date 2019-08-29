@echo off
setlocal

rem BAT script that downloads and installs Clingo library
rem Run it through a cmd with the x64 Visual C++ Toolset enabled.

set LOCAL_PATH=%~dp0
set "FILE_N=    -[%~n0]:"

rem Print batch params (debug purpose)
echo %FILE_N% [Batch params]: %*

rem ============================================================================
rem -- Parse arguments ---------------------------------------------------------
rem ============================================================================

set BUILD_DIR=.
set DEL_SRC=false

:arg-parse
if not "%1"=="" (
    if "%1"=="--build-dir" (
        set BUILD_DIR=%~2
        shift
    )

    if "%1"=="--delete-src" (
        set DEL_SRC=true
    )

    shift
    goto :arg-parse
)

set CLINGO_SRC=clingo-src
set CLINGO_SRC_DIR=%BUILD_DIR%%CLINGO_SRC%\
set CLINGO_INSTALL=clingo-install
set CLINGO_INSTALL_DIR=%BUILD_DIR%%CLINGO_INSTALL%\
set CLINGO_BUILD_DIR=%CLINGO_SRC_DIR%build

if exist "%CLINGO_INSTALL_DIR%" (
    goto already_build
)

if not exist "%CLINGO_SRC_DIR%" (
    echo %FILE_N% Cloning "Clingo"

    call git clone https://github.com/potassco/clingo.git %CLINGO_SRC_DIR%
    cd %CLINGO_SRC_DIR%
    call git submodule update --init --recursive
    if %errorlevel% neq 0 goto error_git
) else (
    echo %FILE_N% Not cloning "Clingo" because already exists a folder called "%CLINGO_SRC%".
)

if not exist "%CLINGO_BUILD_DIR%" (
    echo %FILE_N% Creating "%CLINGO_BUILD_DIR%"
    mkdir "%CLINGO_BUILD_DIR%"
)

echo %FILE_N% Generating build...

cmake -H%CLINGO_SRC_DIR%^
    -B%CLINGO_BUILD_DIR%^
    -DCLINGO_BUILD_APPS=OFF^
    -G "Visual Studio 15 2017 Win64"^
    -DCMAKE_BUILD_TYPE=Release^
    -DCMAKE_CXX_FLAGS_RELEASE="/MD /MP"^
    -DCMAKE_INSTALL_PREFIX=%CLINGO_INSTALL_DIR%
if %errorlevel%  neq 0 goto error_cmake

echo %FILE_N% Building...
cmake --build %CLINGO_BUILD_DIR% --config Release --target install

if errorlevel  neq 0 goto error_install

rem Remove the downloaded Clingo source if no more needed
if %DEL_SRC% == true (
    rd /s /q "%CLINGO_SRC_DIR%"
)

goto success

rem ============================================================================
rem -- Messages and Errors -----------------------------------------------------
rem ============================================================================

:success
    echo.
    echo %FILE_N% "Clingo" has been successfully installed in "%CLINGO_INSTALL_DIR%"!
    goto good_exit

:already_build
    echo %FILE_N% A "Clingo installation already exists.
    echo %FILE_N% Delete "%CLINGO_INSTALL_DIR%" if you want to force a rebuild.
    goto good_exit

:error_git
    echo.
    echo %FILE_N% [GIT ERROR] An error ocurred while executing the git.
    echo %FILE_N% [GIT ERROR] Possible causes:
    echo %FILE_N%              - Make sure "git" is installed.
    echo %FILE_N%              - Make sure it is available on your Windows "path".
    goto bad_exit

:error_cmake
    echo.
    echo %FILE_N% [CMAKE ERROR] An error ocurred while executing the cmake.
    echo %FILE_N% [CMAKE ERROR] Possible causes:
    echo %FILE_N%                - Make sure "CMake" is installed.
    echo %FILE_N%                - Make sure it is available on your Windows "path".
    goto bad_exit

:error_install
    echo.
    echo %FILE_N% [Visual Studio 15 2017 Win64 ERROR] An error ocurred while installing using Visual Studio 15 2017 Win64.
    echo %FILE_N% [Visual Studio 15 2017 Win64 ERROR] Possible causes:
    echo %FILE_N%                - Make sure you have Visual Studio installed.
    echo %FILE_N%                - Make sure you have the "x64 Visual C++ Toolset" in your path.
    echo %FILE_N%                  For example using the "Visual Studio x64 Native Tools Command Prompt",
    echo %FILE_N%                  or the "vcvarsall.bat".
    goto bad_exit

:good_exit
    echo %FILE_N% Exiting...
    endlocal & set install_clingo=%CLINGO_INSTALL_DIR%
    exit /b 0

:bad_exit
    if exist "%CLINGO_INSTALL_DIR%" rd /s /q "%CLINGO_INSTALL_DIR%"
    echo %FILE_N% Exiting with error...
    endlocal
    exit /b %errorlevel%
