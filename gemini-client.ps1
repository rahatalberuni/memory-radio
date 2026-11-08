function Get-GeminiAttemptModels([string]$Primary,[string[]]$Available){
  $preferred=if($Primary -match 'tts'){@('gemini-3.1-flash-tts-preview','gemini-2.5-flash-preview-tts')}else{@('gemini-3.5-flash-lite','gemini-2.5-flash','gemini-2.5-flash-lite','gemini-3-flash-preview','gemini-3.5-flash')}
  $fallbacks=@($preferred|Where-Object {$_ -ne $Primary -and $Available -contains $_}|Select-Object -First 2)
  $plan=@($Primary)+$fallbacks
  while($plan.Count -lt 3){$plan+=($plan[-1])}
  return $plan
}
function Invoke-MemoryRadioGemini($Body,[string]$Primary,[string[]]$Available,[string]$BaseUrl,$ApiHeaders,[string]$Secret,[string]$StatusPath){
  $models=@(Get-GeminiAttemptModels $Primary $Available)
  $json=$Body|ConvertTo-Json -Depth 25 -Compress
  $deadline=[DateTime]::UtcNow.AddSeconds(85)
  for($attempt=0;$attempt -lt $models.Count;$attempt++){
    $candidate=$models[$attempt]
    $remaining=[int][Math]::Floor(($deadline-[DateTime]::UtcNow).TotalSeconds)
    if($remaining -lt 1){throw 'Gemini processing reached its retry deadline. Your recording is saved; please try again later.'}
    $timeout=[Math]::Min(35,$remaining)
    $uri=$BaseUrl+'/models/'+[Uri]::EscapeDataString($candidate)+':generateContent'
    try{
      $response=Invoke-RestMethod -Method Post -Uri $uri -Headers $ApiHeaders -ContentType 'application/json' -Body ([Text.Encoding]::UTF8.GetBytes($json)) -TimeoutSec $timeout
      $success=[pscustomobject]@{http_code=200;status='OK';message='Gemini generation succeeded.';retry_delay=''}
      if($StatusPath){Save-GeminiStatus $StatusPath $candidate 'generate_content' $success}
      return [pscustomobject]@{Response=$response;Model=$candidate}
    }catch{
      $failure=Get-GeminiFailure $_ $Secret
      if($StatusPath){Save-GeminiStatus $StatusPath $candidate 'generate_content' $failure}
      $retryable=$failure.http_code -in @(500,503,504) -or $failure.status -eq 'TIMEOUT'
      if(!$retryable -or $attempt+1 -ge $models.Count){throw (Format-GeminiFailure $failure)}
      $next=$models[$attempt+1]
      $reason=if($failure.status -eq 'TIMEOUT'){'request timed out'}else{'HTTP '+$failure.http_code}
      if($next -eq $candidate){Write-Host "Gemini $candidate is temporarily unavailable ($reason). Retrying..."}
      else{Write-Host "Gemini $candidate failed ($reason). Trying available model $next..."}
      [Threading.Thread]::Sleep(2000)
    }
  }
}
