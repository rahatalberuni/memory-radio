param([string]$Port='COM3')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'bridge.ps1') -Library
$serial=New-Object IO.Ports.SerialPort $Port,115200,'None',8,'One'
$serial.ReadTimeout=10000;$serial.WriteTimeout=5000
try{
  $serial.Open();$serial.DtrEnable=$true
  $serial.WriteLine('RESET');Start-Sleep -Milliseconds 150
  $serial.WriteLine('READY');Start-Sleep -Milliseconds 150
  $serial.DiscardInBuffer()
  $serial.WriteLine('ANALYZE_SAVED')
  do{$header=$serial.ReadLine().Trim()}while(!$header.StartsWith('UPLOAD2 '))
  if($header -notmatch '^UPLOAD2 (\d+) STORY [0-2] (\d+) (M[A-Fa-f0-9]{8}) story$'){throw 'Unexpected upload header'}
  $request=$Matches[1];$length=[int]$Matches[2]
  [byte[]]$pcm=Receive-Upload $serial $request $length
  Write-Output ('HARDWARE_TRANSFER_PASS bytes='+$pcm.Length+' checksum='+[MemoryRadioWire]::Hash($pcm).ToString('X8'))
  $serial.WriteLine('ERROR '+$request+' '+(Encode 'USB TRANSFER VERIFIED. OPEN CONNECT GEMINI AND TRY ORGANIZE AGAIN.'))
  Start-Sleep -Milliseconds 200;$serial.WriteLine('TAP 50 250');Start-Sleep -Milliseconds 150
  $serial.WriteLine('STATUS');do{$line=$serial.ReadLine().Trim()}while(!$line.StartsWith('STATUS '));Write-Output $line
}finally{if($serial.IsOpen){$serial.Close()};$serial.Dispose()}
