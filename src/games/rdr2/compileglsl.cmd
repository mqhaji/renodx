@echo off
setlocal ENABLEDELAYEDEXPANSION

REM Path to glslangValidator from Vulkan SDK
set GLSLANG="C:\VulkanSDK\1.4.313.0\Bin\glslangValidator.exe"

REM Look for all .glsl files in the current folder
for %%F in (*.glsl) do (
    echo 📄 Compiling: %%F
    %GLSLANG% -V --target-env vulkan1.3 -o %%~nF.spv "%%F"
    if errorlevel 1 (
        echo ❌ Failed to compile %%F
    ) else (
        echo ✅ Output:%%~nF.spv
    )
)
