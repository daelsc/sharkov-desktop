@echo off
setlocal enabledelayedexpansion
cd /d "%~dp0"

set "NODE_DIR=%~dp0node"
if exist "%NODE_DIR%\node.exe" set "PATH=%NODE_DIR%;%PATH%"

:: Require GH_TOKEN
if "%GH_TOKEN%"=="" (
    echo ERROR: GH_TOKEN environment variable is not set.
    echo Create a GitHub personal access token at https://github.com/settings/tokens
    echo with "repo" scope, then set it: setx GH_TOKEN "ghp_yourtoken"
    goto err
)

:: Read current version, increment patch
for /f "tokens=*" %%i in ('node -e "var p=require('./package.json');var v=p.version.split('.');v[2]=parseInt(v[2],10)+1;console.log(v.join('.'))"') do set "NEW_VER=%%i"
echo.
echo === Releasing v%NEW_VER% ===
echo.

:: Save old version for rollback
for /f "tokens=*" %%i in ('node -e "console.log(require('./package.json').version)"') do set "OLD_VER=%%i"

:: Update version in package.json
node -e "var fs=require('fs');var p=JSON.parse(fs.readFileSync('package.json','utf8'));p.version='%NEW_VER%';fs.writeFileSync('package.json',JSON.stringify(p,null,2)+'\n');"

:: Build
echo [1/4] Building...
call npm run build
if errorlevel 1 (
    echo ERROR: Build failed.
    goto rollback
)

:: Clean and package + publish in one step
echo [2/4] Packaging and publishing to GitHub...
if exist "out\win-unpacked" rmdir /s /q "out\win-unpacked"
call npx electron-builder --win nsis --x64 --publish always
if errorlevel 1 (
    echo ERROR: electron-builder publish failed.
    goto rollback
)

:: Verify the release exists on GitHub
echo [3/4] Verifying release...
wsl -- gh release view "v%NEW_VER%" --repo daelsc/sharkov-desktop --json assets --jq ".assets | length" > nul 2>&1
if errorlevel 1 (
    echo ERROR: Release v%NEW_VER% not found on GitHub after publish.
    goto rollback
)

:: Git commit (AFTER successful publish)
echo [4/4] Committing version bump...
git add package.json
git commit -m "release: v%NEW_VER%"
git push origin main

echo.
echo === Release v%NEW_VER% published successfully ===
echo https://github.com/daelsc/sharkov-desktop/releases/tag/v%NEW_VER%
echo.
echo Users on the installed version will auto-update on next launch.
goto end

:rollback
echo.
echo Rolling back version to %OLD_VER%...
node -e "var fs=require('fs');var p=JSON.parse(fs.readFileSync('package.json','utf8'));p.version='%OLD_VER%';fs.writeFileSync('package.json',JSON.stringify(p,null,2)+'\n');"
echo Version reverted to %OLD_VER%.

:err
echo.
echo Release failed.
pause
exit /b 1

:end
pause
