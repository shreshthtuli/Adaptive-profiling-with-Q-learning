@echo off

setlocal

rem Run pinadx-vsextension.msi
echo Installing PinAdx...
start /wait extras\pinadx-vsplugin\pinadx-vsextension-3.6.97554-g31f0a167d.msi

rem x64 cpu
if "AMD64" == "%PROCESSOR_ARCHITECTURE%" (
  rem If VS2017 is installed then integrate PinUi
  if exist "%ProgramFiles(x86)%\Microsoft Visual Studio\2017" (
    echo Integrating PinUi to VS2017...
    start /wait extras\pinadx-vsplugin\pin-vs-extension_v15.vsix
  )
  goto end
)

rem x86 cpu
if "x86" == "%PROCESSOR_ARCHITECTURE%" (
  rem If VS2017 is installed then integrate PinUi
  if exist "%ProgramFiles%\Microsoft Visual Studio\2017" (
    echo Integrating PinUi to VS2017...
    start /wait extras\pinadx-vsplugin\pin-vs-extension_v15.vsix
  )
  goto end
)

:end
endlocal
