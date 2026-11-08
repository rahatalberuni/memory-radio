param([string]$Port='COM3')
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'bridge.ps1') -Library
$serial=New-Object IO.Ports.SerialPort $Port,115200,'None',8,'One'
$serial.ReadTimeout=10000;$serial.WriteTimeout=5000;$serial.ReadBufferSize=262144
function Send([string]$Line){$serial.WriteLine($Line);Start-Sleep -Milliseconds 200}
function Read-Until([string]$Prefix){do{$line=$serial.ReadLine().Trim()}while(!$line.StartsWith($Prefix));return $line}
function Status{$serial.DiscardInBuffer();$serial.WriteLine('STATUS');return Read-Until 'STATUS '}
function Require([bool]$Condition,[string]$Message){if(!$Condition){throw $Message}}
function B64([string]$Value){return [Convert]::ToBase64String([Text.Encoding]::UTF8.GetBytes($Value))}
try{
  $serial.Open();$serial.DtrEnable=$true;Start-Sleep -Milliseconds 300
  Send 'RESET';$s=Status;Write-Output $s;Require ($s -match 'touch=1 codec=1 audio=1.*sd=1') 'Hardware or SD mount failed'
  Send 'RECORD';Start-Sleep -Seconds 3;Send 'STOP';$s=Status;Write-Output $s
  Require ($s -match 'screen=2.*sd=1 saved=1 id=(M[A-Fa-f0-9]{8})') 'Recording was not saved to SD'
  $memoryId=$Matches[1];$samples=[int]([regex]::Match($s,'original=(\d+)').Groups[1].Value);Require ($samples -gt 32000) 'Recording was too short'
  $serial.DiscardInBuffer();Send 'SD_VERIFY';$check=Read-Until 'SD_VERIFY ';Write-Output $check;Require ($check -match 'pass=1') 'Saved WAV differs from capture'
  Send 'PLAY';$s=Status;Require ($s -match 'mode=2') 'Original playback did not start'
  Start-Sleep -Seconds 4
  Send 'READY';$serial.DiscardInBuffer();$serial.WriteLine('ANALYZE');$header=Read-Until 'UPLOAD2 '
  Require ($header -match '^UPLOAD2 (\d+) STORY 0 (\d+) (M[A-Fa-f0-9]{8}) story$') 'Audio upload framing failed'
  $id=[int]$Matches[1];$length=[int]$Matches[2];Require ($length -eq $samples*2) 'Audio upload length mismatch'
  [byte[]]$pcm=Receive-Upload $serial ([string]$id) $length
  $peak=0;for($i=0;$i -lt $pcm.Length;$i+=2){$peak=[Math]::Max($peak,[Math]::Abs([int][BitConverter]::ToInt16($pcm,$i)))};Require ($peak -gt 0) 'Microphone PCM contains only zeros';Write-Output "MIC_CAPTURE samples=$samples peak=$peak"
  Send "BEGIN $id"
  # Explicit test fixture, not a claimed Gemini response.
  Send ("FIELD $id TITLE "+(B64 'DEVICE HARDWARE TEST'))
  Send ("FIELD $id SUMMARY "+(B64 'This is a device test fixture. It verifies touchscreen exploration, original audio playback, microSD persistence, and safe USB framing. It is not an AI-generated memory. Record your own story to begin.'))
  Send ("FIELD $id NOTE "+(B64 'Test data only.'))
  for($q=0;$q -lt 3;$q++){Send ("FIELD $id Q$q "+(B64 'What would you like to remember?'))}
  Send ("ITEM $id 0 0 "+(B64 'TEST PERSON')+' '+(B64 'A test entry for the people screen.')+' '+(B64 'Test fixture')+' '+(B64 'story')+' 0.1 1.2')
  Send ("ITEM $id 1 0 "+(B64 'TEST PLACE')+' '+(B64 'A test location for the places screen.')+' '+(B64 'Test fixture')+' '+(B64 'story')+' 0.1 1.2')
  Send ("ITEM $id 2 0 "+(B64 'TEST EVENT')+' '+(B64 'An event used to check the timeline screen.')+' '+(B64 'Test fixture')+' '+(B64 'story')+' 0.1 1.2')
  Write-Output $serial.ReadExisting();Send 'PENDING';Write-Output $serial.ReadExisting()
  Send "DONE $id";$s=Status;Write-Output $s;Require ($s -match 'screen=4.*memory=1 people=1 places=1 events=1.*saved=1') 'Structured memory was not applied and saved'
  $serial.DiscardInBuffer();Send 'SD_JSON';[void](Read-Until 'JSON_BEGIN');$json='';do{$line=$serial.ReadLine().Trim();if($line -ne 'JSON_END'){$json+=$line}}while($line -ne 'JSON_END')
  $stored=$json|ConvertFrom-Json;Require ($stored.title -eq 'DEVICE HARDWARE TEST' -and $stored.events[0].source -eq 'story') 'SD JSON metadata invalid'
  # Exercise the real touch handlers through the diagnostic input.
  Send 'TAP 82 292';$s=Status;Require ($s -match 'screen=5') 'People tab failed'
  Send 'TAP 50 100';$s=Status;Require ($s -match 'screen=6') 'Detail view failed'
  Send 'RESET';Send 'TAP 50 290';$s=Status;Require ($s -match 'screen=4') 'Open memory failed'
  Send 'TAP 45 253';Send 'TAP 50 100';$s=Status;Require ($s -match 'screen=8') 'Follow-up question view failed'
  # Simulated narration transport checks binary receive and SD WAV output without calling Gemini.
  Send 'READY';$serial.DiscardInBuffer();Send 'TAP 50 212';$request=Read-Until 'REQUEST ';Require ($request -match '^REQUEST (\d+) QUESTION ') 'Question narration request failed';$voiceId=[int]$Matches[1]
  $tone=New-Object byte[] 32000;for($i=0;$i -lt 16000;$i++){$v=[int16]([Math]::Sin(2*[Math]::PI*440*$i/16000)*700);$tone[2*$i]=[byte]([int]$v -band 255);$tone[2*$i+1]=[byte](([int]$v -shr 8) -band 255)}
  $serial.WriteLine("AUDIO_OUT $voiceId 32000");for($at=0;$at -lt $tone.Length;$at+=1024){$serial.Write($tone,$at,[Math]::Min(1024,$tone.Length-$at));[Threading.Thread]::Sleep(5)};$serial.WriteLine('');Send "AUDIO_OUT_END $voiceId";$s=Status;Require ($s -match 'screen=8.*mode=2.*saved=1') 'Narration receive, playback, or SD save failed'
  Start-Sleep -Seconds 2
  $serial.DiscardInBuffer();Send 'REBOOT';Start-Sleep -Seconds 2;$s=Status;Write-Output $s
  Require ($s -match "screen=0.*original=$samples .*memory=1.*sd=1 saved=1 id=$memoryId") 'Card restore after reboot failed'
  $serial.DiscardInBuffer();Send 'SD_VERIFY';$check=Read-Until 'SD_VERIFY ';Require ($check -match 'pass=1') 'Restored recording hash mismatch'
  Send 'SD_LIST';Start-Sleep -Milliseconds 200;Write-Output $serial.ReadExisting()
  Write-Output 'DEVICE_TEST=PASS (microphone, original WAV hash, upload framing, structured fixture, UI handlers, metadata JSON, narration receive/save/playback, reboot restore)'
}finally{if($serial.IsOpen){$serial.Close()};$serial.Dispose()}
