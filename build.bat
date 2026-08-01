@echo off
setlocal enabledelayedexpansion

REM gVRMod OpenXR — Windows build (x64)
REM Texture share: D3D9 CreateTexture hook from vrmod-module-master main
REM Submit: OpenXR XR_KHR_D3D11_enable, flip=0

REM ***************************** edit if needed ******************************
set vcvarsallpath="C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvarsall.bat"
if not exist %vcvarsallpath% set vcvarsallpath="C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"
if not exist %vcvarsallpath% set vcvarsallpath="C:\Program Files\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvarsall.bat"
REM ***************************************************************************

echo === gVRMod Windows Build (OpenXR + D3D11, flip=0) ===

if not exist deps\gmod\Interface.h (
    echo [+] Downloading GMod module base headers...
    mkdir deps\gmod 2>NUL
    powershell -command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest https://github.com/Facepunch/gmod-module-base/archive/15bf18f369a41ac3d4eba29ee0679f386ec628b7.zip -OutFile deps\gmod\tmp.zip; Expand-Archive deps\gmod\tmp.zip -DestinationPath deps\gmod\tmp -Force; Move-Item deps\gmod\tmp\gmod-module-base-15bf18f369a41ac3d4eba29ee0679f386ec628b7\include\GarrysMod\Lua\* deps\gmod\ -Force; Remove-Item deps\gmod\tmp.zip; Remove-Item deps\gmod\tmp -Recurse -Force"
)

if not exist deps\openxr\openxr\openxr.h (
    echo [+] Downloading OpenXR-SDK headers...
    mkdir deps\openxr 2>NUL
    powershell -command "[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12; Invoke-WebRequest https://github.com/KhronosGroup/OpenXR-SDK/archive/refs/tags/release-1.1.60.zip -OutFile deps\openxr\tmp.zip; Expand-Archive deps\openxr\tmp.zip -DestinationPath deps\openxr\tmp -Force; New-Item -ItemType Directory -Force deps\openxr\openxr | Out-Null; Copy-Item deps\openxr\tmp\OpenXR-SDK-release-1.1.60\include\openxr\* deps\openxr\openxr\ -Force; Remove-Item deps\openxr\tmp.zip; Remove-Item deps\openxr\tmp -Recurse -Force"
)

mkdir install\GarrysMod\garrysmod\lua\bin 2>NUL
mkdir build_win64 2>NUL

call %vcvarsallpath% x64
if errorlevel 1 (
    echo ERROR: vcvarsall failed — set path to Visual Studio at top of build.bat
    exit /b 1
)

set INC=/I..\deps /I..\src
set SRC=^
  ..\src\core\vrmod_log.cpp ^
  ..\src\input\xr_input.cpp ^
  ..\src\rendering\d3d\d3d_hooks.cpp ^
  ..\src\rendering\openxr\xr_session.cpp ^
  ..\src\rendering\openxr\xr_render_d3d.cpp ^
  ..\src\lua\lua_interface.cpp

set FLAGS=/nologo /O2 /W3 /wd4996 /MT /EHsc /std:c++17 /D_CRT_SECURE_NO_WARNINGS /DNOMINMAX /DWIN32_LEAN_AND_MEAN
set LIBS=d3d11.lib dxgi.lib user32.lib shell32.lib

pushd build_win64
cl %FLAGS% %INC% %SRC% /link %LIBS% /DLL /OUT:..\install\GarrysMod\garrysmod\lua\bin\gmcl_vrmod_win64.dll
if errorlevel 1 (
    echo BUILD FAILED
    popd
    exit /b 1
)
del /q *.obj 2>NUL
del /q ..\install\GarrysMod\garrysmod\lua\bin\gmcl_vrmod_win64.exp 2>NUL
del /q ..\install\GarrysMod\garrysmod\lua\bin\gmcl_vrmod_win64.lib 2>NUL
popd

echo.
echo [+] Built: install\GarrysMod\garrysmod\lua\bin\gmcl_vrmod_win64.dll
echo     Texture hook: D3D9 CreateTexture (vrmod-module-master main)
echo     Graphics API: OpenXR XR_KHR_D3D11_enable
echo     RT flip: 0 (Windows top-left)
echo.
echo Runtime: place openxr_loader.dll next to the module or on PATH.
echo          SteamVR / Oculus / WMR OpenXR runtime must be active.
echo          Install addon: copy addon\gvrmod → garrysmod\addons\gvrmod
echo.
endlocal
