@echo off
cd /d "%~dp0"
set "NODE_DIR=%~dp0node"
if exist "%NODE_DIR%\node.exe" set "PATH=%NODE_DIR%;%PATH%"

if not exist "node_modules" (
    echo Installing dependencies...
    call npm install
    if errorlevel 1 goto err
)

echo Building for local testing (no publish)...
call npm run build
if errorlevel 1 goto err

echo.
echo Creating executable(s)...
if exist "out\win-unpacked" rmdir /s /q "out\win-unpacked"
call npx electron-builder --win nsis portable --x64
if errorlevel 1 goto err

echo.
echo Done. Output in out\
dir /b out\*.exe 2>nul
goto end

:err
echo.
echo Build failed.
pause
exit /b 1

:end
pause
