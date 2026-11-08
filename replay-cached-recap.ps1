param([string]$Port='COM3')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'bridge.ps1') -Library
$serial=New-Object IO.Ports.SerialPort $Port,115200,'None',8,'One'
$serial.ReadTimeout=10000;$serial.WriteTimeout=15000
try{
  $serial.Open();$serial.DtrEnable=$true;$serial.WriteLine('STATUS')
  do{$status=$serial.ReadLine().Trim()}while(!$status.StartsWith('STATUS '))
  if($status -notmatch ' id=(M[A-Fa-f0-9]{8}) '){throw 'No current memory on the ESP32'}
  $memoryId=$Matches[1];$folder=Join-Path (Join-Path $PSScriptRoot 'memories') $memoryId
  $cache=Get-ChildItem -LiteralPath $folder -Filter 'narrate-*.wav' -File|Sort-Object LastWriteTime -Descending|Select-Object -First 1
  if(!$cache){throw 'No generated recap is cached for this memory'}
  $wav=Read-PcmWav ([IO.File]::ReadAllBytes($cache.FullName));[byte[]]$pcm=Resample $wav.Pcm $wav.Rate
  if($pcm.Length -gt 1920000){throw 'Cached recap exceeds the device playback limit'}
  $serial.WriteLine('READY');Start-Sleep -Milliseconds 150;$serial.WriteLine('PLAY_AI')
  $until=[DateTime]::UtcNow.AddSeconds(3);$request=$null
  while([DateTime]::UtcNow -lt $until){$line=$serial.ReadLine().Trim();if($line -match '^REQUEST (\d+) NARRATE '){$request=$Matches[1];break}}
  if(!$request){throw 'Device did not request a recap; it may already have one saved locally'}
  $serial.WriteLine("AUDIO_OUT $request $($pcm.Length)")
  for($at=0;$at -lt $pcm.Length;$at+=1024){$serial.Write($pcm,$at,[Math]::Min(1024,$pcm.Length-$at));[Threading.Thread]::Sleep(5)}
  $serial.WriteLine('');$serial.WriteLine("AUDIO_OUT_END $request")
  do{$line=$serial.ReadLine().Trim()}while(!$line.StartsWith('AUDIO_PLAYING '))
  if($line -ne "AUDIO_PLAYING $request $($pcm.Length)"){throw 'Playback confirmation did not match the recap'}
  Write-Output "CACHED_AI_RECAP_PLAYING bytes=$($pcm.Length) duration_seconds=$([Math]::Round($pcm.Length/32000.0,1)) sd_file=recap.wav"
}finally{if($serial.IsOpen){$serial.Close()};$serial.Dispose()}
