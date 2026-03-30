@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "NODE_DIR=%~dp0node"
if exist "%NODE_DIR%\node.exe" set "PATH=%NODE_DIR%;%PATH%"

:: Read build counter and increment
for /f "tokens=*" %%i in ('node -e "var fs=require('fs');var c=0;try{c=parseInt(fs.readFileSync('.buildcount','utf8').trim(),10)||0;}catch(e){}console.log(c+1);"') do set "BUILD=%%i"

:: Version is 0.1.{build}
set "NEW_VER=0.1.%BUILD%"
echo Building version: %NEW_VER%

:: Update version in package.json
node -e "var fs=require('fs');var p=JSON.parse(fs.readFileSync('package.json','utf8'));p.version='%NEW_VER%';fs.writeFileSync('package.json',JSON.stringify(p,null,2)+'\n');"

:: Build and package
echo.
echo Building...
call npm run build
if errorlevel 1 goto err

echo.
echo Packaging...
call npx electron-builder --win nsis --x64
if errorlevel 1 goto err

:: Check output files exist
if not exist "out\Sharkov Setup %NEW_VER%.exe" (
    echo ERROR: Installer not found at out\Sharkov Setup %NEW_VER%.exe
    goto err
)
if not exist "out\latest.yml" (
    echo ERROR: latest.yml not found in out\
    goto err
)

echo.
echo Build complete:
echo   out\Sharkov Setup %NEW_VER%.exe
echo   out\latest.yml
echo.

:: Confirm release
set /p "CONFIRM=Create GitHub release v%NEW_VER%? (y/n): "
if /i not "%CONFIRM%"=="y" (
    echo Aborted. Files are in out\ if you want to release manually.
    goto end
)

:: Commit version bump
git add package.json package-lock.json
git commit -m "release: v%NEW_VER%"
git push origin main

:: Create release via WSL gh
echo Creating GitHub release...
wsl -- gh release create "v%NEW_VER%" --repo daelsc/sharkov --title "v%NEW_VER%" --generate-notes
if errorlevel 1 (
    echo ERROR: Failed to create release. Check gh auth.
    goto err
)

:: Upload assets
echo Uploading installer...
wsl -- gh release upload "v%NEW_VER%" --repo daelsc/sharkov "./out/Sharkov Setup %NEW_VER%.exe"
echo Uploading latest.yml...
wsl -- gh release upload "v%NEW_VER%" --repo daelsc/sharkov "./out/latest.yml"

echo.
echo Release v%NEW_VER% published!
echo https://github.com/daelsc/sharkov/releases/tag/v%NEW_VER%
goto end

:err
echo.
echo Release failed.
pause
exit /b 1

:end
pause
