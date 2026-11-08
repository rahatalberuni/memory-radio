param([string]$Port='COM3')
$ErrorActionPreference='Stop'
$serial=New-Object IO.Ports.SerialPort $Port,115200,'None',8,'One'
$serial.ReadTimeout=5000;$serial.WriteTimeout=5000
function Send([string]$Line){$serial.WriteLine($Line);Start-Sleep -Milliseconds 160}
function Status{$serial.DiscardInBuffer();$serial.WriteLine('STATUS');do{$line=$serial.ReadLine().Trim()}while(!$line.StartsWith('STATUS '));return $line}
function Number([string]$Line,[string]$Name){return [int]([regex]::Match($Line,'(?:^| )'+$Name+'=(\d+)').Groups[1].Value)}
function Require([bool]$Ok,[string]$Message){if(!$Ok){throw $Message}}
try{
  $serial.Open();$serial.DtrEnable=$true;Start-Sleep -Seconds 2;Send 'RESET'
  $initial=Status;Require ((Number $initial 'memory') -eq 1) 'Open a saved memory before testing pause/resume'
  $original=Number $initial 'original';$errors=Number $initial 'speakerErrors'
  Send 'TAP 110 290';Send 'TAP 120 254';Start-Sleep -Milliseconds 400;$playing=Status;Write-Output $playing
  Require ((Number $playing 'mode') -eq 2 -and (Number $playing 'playPosition') -gt 0) 'Play button did not start cached narration'
  Send 'TAP 198 254';$paused=Status;$position=Number $paused 'playPosition';$frames=Number $paused 'speakerFrames';Write-Output $paused
  Require ((Number $paused 'mode') -eq 3 -and $position -gt 0) 'Pause button did not enter paused state'
  Start-Sleep -Milliseconds 800;$held=Status
  Require ((Number $held 'mode') -eq 3 -and (Number $held 'playPosition') -eq $position -and (Number $held 'speakerFrames') -eq $frames) 'Playback advanced while paused'
  Send 'TAP 120 254';$resumed=Status;Write-Output $resumed
  Require ((Number $resumed 'mode') -eq 2 -and (Number $resumed 'playPosition') -gt $position -and (Number $resumed 'playPosition') -lt $position+16000) 'Resume reset or skipped the playback position'
  Send 'TAP 198 254';$pausedAgain=Status;$secondPosition=Number $pausedAgain 'playPosition'
  Start-Sleep -Milliseconds 400;$heldAgain=Status
  Require ((Number $heldAgain 'mode') -eq 3 -and (Number $heldAgain 'playPosition') -eq $secondPosition) 'Repeated pause was not stable'
  Send 'TAP 120 254';$until=[DateTime]::UtcNow.AddSeconds(65)
  do{Start-Sleep -Milliseconds 800;$finished=Status}while((Number $finished 'mode') -eq 2 -and [DateTime]::UtcNow -lt $until)
  Require ((Number $finished 'mode') -eq 0 -and (Number $finished 'playPosition') -eq (Number $finished 'playLength')) 'Resumed narration did not finish normally'
  Send 'TAP 120 254';$replayed=Status
  Require ((Number $replayed 'mode') -eq 2 -and (Number $replayed 'playPosition') -lt 16000) 'Play did not restart a completed narration'
  Send 'TAP 198 254';$final=Status
  Require ((Number $final 'mode') -eq 3 -and (Number $final 'original') -eq $original -and (Number $final 'speakerErrors') -eq $errors) 'Final pause, original recording, or audio output check failed'
  Write-Output $final
  Write-Output 'PAUSE_RESUME_TEST=PASS (touch controls, frozen position/frame count, exact-position resume, repeated pause, completion, replay, original retained)'
}finally{if($serial.IsOpen){$serial.Close()};$serial.Dispose()}
