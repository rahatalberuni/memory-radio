param([string]$Port='COM3')
$ErrorActionPreference='Stop'
$serial=New-Object IO.Ports.SerialPort $Port,115200,'None',8,'One'
$serial.ReadTimeout=10000;$serial.WriteTimeout=5000
function Send([string]$Line){$serial.WriteLine($Line);Start-Sleep -Milliseconds 150}
function Read-Until([string]$Prefix){do{$line=$serial.ReadLine().Trim()}while(!$line.StartsWith($Prefix));return $line}
function Status{$serial.DiscardInBuffer();$serial.WriteLine('STATUS');return Read-Until 'STATUS '}
function Number([string]$Line,[string]$Name){return [int]([regex]::Match($Line,'(?:^| )'+$Name+'=(\d+)').Groups[1].Value)}
function Require([bool]$Ok,[string]$Message){if(!$Ok){throw $Message}}
try{
  $serial.Open();$serial.DtrEnable=$true;Send 'RESET';$before=Status;Write-Output $before
  Require ((Number $before 'audio') -eq 1 -and (Number $before 'codec') -eq 1) 'Codec or audio driver is not ready'
  $original=Number $before 'original';$startFrames=Number $before 'speakerFrames';$startNonzero=Number $before 'speakerNonzero';$startErrors=Number $before 'speakerErrors'
  # Exercise the same touch handler used by the new HOME button.
  Send 'TAP 110 206';$playing=Status;Write-Output $playing
  Require ((Number $playing 'mode') -eq 2 -and (Number $playing 'amplifier') -eq 1) 'Speaker button did not enable playback/amplifier'
  Start-Sleep -Milliseconds 1100;$done=Status;Write-Output $done
  Require ((Number $done 'mode') -eq 0 -and (Number $done 'speakerFrames') -gt $startFrames -and (Number $done 'speakerNonzero') -gt $startNonzero -and (Number $done 'speakerErrors') -eq $startErrors) 'Test tones were not delivered to the audio driver'
  # Verify the incoming response path and its acknowledgement after a real I2S write.
  Send 'READY';$serial.DiscardInBuffer();$serial.WriteLine('SPEAKER_STREAM_TEST');$request=Read-Until 'REQUEST '
  Require ($request -match '^REQUEST (\d+) SPEAKER_CHECK ') 'Speaker stream request missing';$id=$Matches[1]
  $tone=New-Object byte[] 32000
  for($i=0;$i -lt 16000;$i++){$envelope=[Math]::Min(1,[Math]::Min($i/160.0,(16000-$i)/160.0));$v=[int16]([Math]::Sin(2*[Math]::PI*660*$i/16000)*4000*$envelope);$tone[2*$i]=[byte]([int]$v -band 255);$tone[2*$i+1]=[byte](([int]$v -shr 8) -band 255)}
  $serial.WriteLine("AUDIO_OUT $id 32000")
  for($at=0;$at -lt $tone.Length;$at+=1024){$serial.Write($tone,$at,[Math]::Min(1024,$tone.Length-$at));[Threading.Thread]::Sleep(5)}
  $serial.WriteLine('');$serial.WriteLine("AUDIO_OUT_END $id")
  $ack=Read-Until 'AUDIO_PLAYING ';Require ($ack -eq "AUDIO_PLAYING $id 32000") 'ESP32 did not confirm streamed speaker audio';Write-Output $ack
  Start-Sleep -Milliseconds 1200;$final=Status;Write-Output $final
  Require ((Number $final 'speakerErrors') -eq $startErrors -and (Number $final 'original') -eq $original) 'Speaker test failed or changed the original recording'
  Send 'RESET';Write-Output 'SPEAKER_TEST=PASS (home button, amplifier enable, nonzero I2S output, completion, streamed response acknowledgement, original recording retained)'
}finally{if($serial.IsOpen){$serial.Close()};$serial.Dispose()}
