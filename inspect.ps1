param([string]$Port='COM3',[string]$Name='screen')
$ErrorActionPreference='Stop'
$serial=New-Object IO.Ports.SerialPort $Port,115200,'None',8,'One'
$serial.ReadTimeout=5000;$serial.WriteTimeout=5000
try{
  $serial.Open();$serial.DtrEnable=$true;Start-Sleep -Milliseconds 300
  $serial.DiscardInBuffer();$serial.WriteLine('STATUS');Start-Sleep -Milliseconds 150
  Write-Output $serial.ReadExisting()
  $serial.WriteLine('FRAME')
  do{$header=$serial.ReadLine().Trim()}while(!$header.StartsWith('FRAME '))
  if($header -ne 'FRAME 153600'){throw "Unexpected frame header: $header"}
  $raw=New-Object byte[] 153600;$offset=0
  while($offset -lt $raw.Length){$offset+=$serial.Read($raw,$offset,$raw.Length-$offset)}
  Add-Type -AssemblyName System.Drawing
  $img=New-Object Drawing.Bitmap 240,320
  try{
    for($y=0;$y -lt 320;$y++){for($x=0;$x -lt 240;$x++){
      $v=[int]$raw[2*($y*240+$x)]+256*[int]$raw[2*($y*240+$x)+1]
      $r=(($v -shr 11) -band 31)*255/31;$g=(($v -shr 5) -band 63)*255/63;$b=($v -band 31)*255/31
      $img.SetPixel($x,$y,[Drawing.Color]::FromArgb([int]$r,[int]$g,[int]$b))
    }}
    $img.Save((Join-Path $PSScriptRoot "build\$Name.png"),[Drawing.Imaging.ImageFormat]::Png)
  }finally{$img.Dispose()}
  Write-Output "Saved build\$Name.png"
}finally{if($serial.IsOpen){$serial.Close()};$serial.Dispose()}
