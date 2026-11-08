function Get-GeminiFailure($Record,[string]$Secret=''){
  $response=$null;$exception=$Record.Exception
  while($exception){if($exception.Response){$response=$exception.Response;break};$exception=$exception.InnerException}
  $code=0;if($response){try{$code=[int]$response.StatusCode}catch{}}
  $body=[string]$Record.ErrorDetails.Message
  if(!$body -and $response){try{$reader=New-Object IO.StreamReader ($response.GetResponseStream());$body=$reader.ReadToEnd();$reader.Dispose()}catch{}}
  $status='';$retry='';$message=[string]$Record.Exception.Message
  if($body){
    try{
      $parsed=$body|ConvertFrom-Json
      if($parsed.error){
        if(!$code -and $parsed.error.code -match '^\d+$'){$code=[int]$parsed.error.code}
        $status=[string]$parsed.error.status
        if(!$status){$status=[string]$parsed.error.code}
        if($parsed.error.message){$message=[string]$parsed.error.message}
        foreach($detail in @($parsed.error.details)){if($detail.retryDelay){$retry=[string]$detail.retryDelay}}
      }
    }catch{}
  }
  if($Secret){$message=$message.Replace($Secret,'[REDACTED]')}
  $message=[regex]::Replace($message,'AIza[0-9A-Za-z_-]{20,}','[REDACTED]')
  $message=[regex]::Replace($message,'(?i)([?&]key=)[^\s&"'']+','$1[REDACTED]')
  $message=[regex]::Replace($message,'\s+',' ').Trim()
  if(!$code){
    $cause=$Record.Exception;$timedOut=$false
    while($cause){
      if($cause -is [TimeoutException] -or ($cause -is [Net.WebException] -and $cause.Status -eq [Net.WebExceptionStatus]::Timeout)){$timedOut=$true}
      $cause=$cause.InnerException
    }
    if($timedOut -or $message -match '(?i)\btimed out\b|\btimeout\b'){$status='TIMEOUT'}
  }
  if($message.Length -gt 3500){$message=$message.Substring(0,3500)}
  return [pscustomobject]@{http_code=$code;status=$status;message=$message;retry_delay=$retry}
}
function Format-GeminiFailure($Failure){
  if($Failure.http_code){return ('Gemini HTTP '+$Failure.http_code+' '+$Failure.status+': '+$Failure.message).Trim()}
  return $Failure.message
}
function Save-GeminiStatus([string]$Path,[string]$Model,[string]$Stage,$Failure){
  $record=[ordered]@{time_utc=[DateTime]::UtcNow.ToString('o');model=$Model;stage=$Stage;http_code=$Failure.http_code;status=$Failure.status;message=$Failure.message;retry_delay=$Failure.retry_delay}
  [IO.File]::WriteAllText($Path,($record|ConvertTo-Json -Depth 5),[Text.Encoding]::UTF8)
}
