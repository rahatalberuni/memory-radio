param([string]$Port='COM3')
$ErrorActionPreference='Stop'
$image=Join-Path $PSScriptRoot 'build\MemoryRadio.bin'
if((Get-Item -LiteralPath $image).Length -gt 0x300000){throw 'Firmware exceeds existing application partition'}
# Preserve the existing partition table, model partition, and all data outside the app.
& 'C:\Users\alber\AppData\Local\Arduino15\packages\esp32\tools\esptool_py\5.3.1\esptool.exe' --chip esp32s3 --port $Port --baud 460800 write-flash 0x10000 $image
if($LASTEXITCODE -ne 0){throw 'Firmware upload failed'}
