$ErrorActionPreference='Stop'
 $root=$PSScriptRoot
$platform='C:/Users/alber/AppData/Local/Arduino15/packages/esp32/hardware/esp32/3.3.12'
$sdk='C:/Users/alber/AppData/Local/Arduino15/packages/esp32/tools/esp32s3-libs/3.3.12'
$bin='C:/Users/alber/AppData/Local/Arduino15/packages/esp32/tools/esp-x32/2601/bin'
$build=($root+'/build').Replace('\','/')
New-Item -ItemType Directory -Path $build -Force | Out-Null
$libNames=@('Wire','SPI','ESP_I2S','FS','SD_MMC','Network','WiFi','WebServer','Preferences','Hash')
$includes=@("$platform/cores/esp32", "$platform/variants/esp32s3", "$root")
foreach($lib in $libNames){$includes += "$platform/libraries/$lib/src"}
$defs=@('-DF_CPU=240000000L','-DARDUINO=10607','-DARDUINO_ESP32S3_DEV','-DARDUINO_ARCH_ESP32','-DARDUINO_BOARD=\"ESP32S3_DEV\"','-DARDUINO_VARIANT=\"esp32s3\"','-DARDUINO_PARTITION_esp_sr_16','-DBOARD_HAS_PSRAM','-DESP32=ESP32','-DCORE_DEBUG_LEVEL=0','-DARDUINO_USB_MODE=1','-DARDUINO_USB_CDC_ON_BOOT=1','-DARDUINO_USB_MSC_ON_BOOT=0','-DARDUINO_USB_DFU_ON_BOOT=0','-DARDUINO_RUNNING_CORE=1','-DARDUINO_EVENT_RUNNING_CORE=1')
$common=@('-c','-Os','-Werror=return-type',"`"@$sdk/flags/defines`"",'-iprefix',"`"$sdk/include/`"","`"@$sdk/flags/includes`"","`"-I$sdk/dio_opi/include`"")+$defs
foreach($inc in $includes){$common += "`"-I$($inc.Replace('\','/'))`""}
Copy-Item "$root/MemoryRadio.ino" "$build/MemoryRadio.cpp" -Force
$sets=@(@{Dir="$platform/cores/esp32";Core=$true;Name='core'})
foreach($lib in $libNames){$sets += @{Dir="$platform/libraries/$lib/src";Core=$false;Name=$lib}}
$sets += @{Dir=$build;Core=$false;Name='sketch'}
$coreObjects=@();$otherObjects=@()
foreach($set in $sets){
 $files=Get-ChildItem -LiteralPath $set.Dir -Recurse -File | Where-Object { $_.Extension -cin @('.c','.cpp','.S') }
 foreach($file in $files){
  if($set.Name -eq 'sketch' -and $file.Name -ne 'MemoryRadio.cpp'){continue}
  $id=($file.FullName.Substring($set.Dir.Length).TrimStart('\','/') -replace '[^a-zA-Z0-9_.]','_')
  $obj="$build/$($set.Name)_$id.o"
  if($set.Core){$coreObjects += $obj}else{$otherObjects += $obj}
  if($set.Name -ne 'sketch' -and (Test-Path $obj) -and (Get-Item $obj).LastWriteTime -gt $file.LastWriteTime){continue}
  $compiler="$bin/xtensa-esp32s3-elf-gcc.exe";$flags='c_flags';$extra=@()
  if($file.Extension -eq '.cpp'){$compiler="$bin/xtensa-esp32s3-elf-g++.exe";$flags='cpp_flags'}
  elseif($file.Extension -ceq '.S'){$flags='S_flags';$extra=@('-x','assembler-with-cpp')}
  if($set.Core){$extra += '-DARDUINO_CORE_BUILD'}
  $args=$common + "`"@$sdk/flags/$flags`"" + $extra + "`"$($file.FullName.Replace('\','/'))`"" + '-o' + "`"$obj`""
  [IO.File]::WriteAllLines("$build/compile.rsp",$args,[Text.Encoding]::ASCII)
  & $compiler "@$build/compile.rsp"
  if($LASTEXITCODE -ne 0){throw "Compile failed: $($file.Name)"}
  Write-Output "Compiled $($set.Name)/$($file.Name)"
 }
}
[IO.File]::WriteAllLines("$build/archive.rsp",@('cr',"`"$build/core.a`"")+($coreObjects | ForEach-Object {"`"$_`""}),[Text.Encoding]::ASCII)
& "$bin/xtensa-esp32s3-elf-gcc-ar.exe" "@$build/archive.rsp"
if($LASTEXITCODE -ne 0){throw 'Archive failed'}
$link=@("`"-Wl,--Map=$build/MemoryRadio.map`"","`"-L$sdk/lib`"","`"-L$sdk/ld`"","`"-L$sdk/dio_opi`"",'-Wl,--wrap=esp_panic_handler','-Wl,--wrap=esp_bt_mem_release','-Wl,--wrap=esp_bt_controller_mem_release',"`"@$sdk/flags/ld_flags`"","`"@$sdk/flags/ld_scripts`"",'-Wl,--start-group')+($otherObjects | ForEach-Object {"`"$_`""})+@("`"$build/core.a`"","`"@$sdk/flags/ld_libs`"",'-Wl,--end-group','-Wl,-EL','-o',"`"$build/MemoryRadio.elf`"")
[IO.File]::WriteAllLines("$build/link.rsp",$link,[Text.Encoding]::ASCII)
& "$bin/xtensa-esp32s3-elf-g++.exe" "@$build/link.rsp"
if($LASTEXITCODE -ne 0){throw 'Link failed'}
$esptool='C:/Users/alber/AppData/Local/Arduino15/packages/esp32/tools/esptool_py/5.3.1/esptool.exe'
& $esptool --chip esp32s3 elf2image --flash-mode dio --flash-freq 80m --flash-size 16MB --elf-sha256-offset 0xb0 -o "$build/MemoryRadio.bin" "$build/MemoryRadio.elf"
if($LASTEXITCODE -ne 0){throw 'App binary failed'}
& $esptool --chip esp32s3 elf2image --flash-mode dio --flash-freq 80m --flash-size 16MB -o "$build/bootloader.bin" "$sdk/bin/bootloader_dio_80m.elf"
if($LASTEXITCODE -ne 0){throw 'Bootloader failed'}
Copy-Item "$platform/tools/partitions/esp_sr_16.csv" "$build/partitions.csv" -Force
& "$platform/tools/gen_esp32part.exe" -q "$build/partitions.csv" "$build/partitions.bin"
if($LASTEXITCODE -ne 0){throw 'Partition generation failed'}
Write-Output 'BUILD SUCCESS'

