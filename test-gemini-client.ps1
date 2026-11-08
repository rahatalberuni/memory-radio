$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'gemini-errors.ps1')
. (Join-Path $PSScriptRoot 'gemini-client.ps1')
$script:calls=New-Object Collections.Generic.List[string]
$script:scenario='busy';$script:code=503
function Invoke-RestMethod {
  param($Method,$Uri,$Headers,$ContentType,$Body,$TimeoutSec)
  [void]$script:calls.Add($Uri)
  if($script:scenario -in @('busy','timeout') -and $Uri -match 'gemini-2.5-flash:'){return @{candidates=@(@{content=@{parts=@(@{text='OK'})}})}}
  if($script:scenario -eq 'timeout'){throw (New-Object Net.WebException 'The operation has timed out',([Net.WebExceptionStatus]::Timeout))}
  $record=New-Object Management.Automation.ErrorRecord (New-Object Exception 'Simulated provider error'),'ApiTest',([Management.Automation.ErrorCategory]::InvalidOperation),$null
  $record.ErrorDetails=New-Object Management.Automation.ErrorDetails ('{"error":{"code":'+$script:code+',"status":"TEST_ERROR","message":"Simulated provider status"}}')
  throw $record
}
$result=Invoke-MemoryRadioGemini @{contents=@()} 'gemini-3.8-flash' @('gemini-3.8-flash','gemini-2.5-flash') 'https://example.invalid/v1beta' @{} '' ''
if($result.Model -ne 'gemini-2.5-flash' -or $script:calls.Count -ne 2){throw 'Busy model retry and fallback failed'}
if($script:calls[0] -eq $script:calls[1] -or $script:calls[1] -notmatch 'gemini-2.5-flash:'){throw 'Fallback ordering failed'}
$script:calls.Clear();$script:scenario='timeout'
$result=Invoke-MemoryRadioGemini @{} 'gemini-3.8-flash' @('gemini-3.8-flash','gemini-2.5-flash') 'https://example.invalid/v1beta' @{} '' ''
if($result.Model -ne 'gemini-2.5-flash' -or $script:calls.Count -ne 2){throw 'Network timeout did not trigger immediate fallback'}
foreach($code in @(400,403,429)){
  $script:calls.Clear();$script:scenario='error';$script:code=$code;$rejected=$false
  try{Invoke-MemoryRadioGemini @{} 'gemini-3.8-flash' @('gemini-2.5-flash') 'https://example.invalid/v1beta' @{} '' ''|Out-Null}catch{$rejected=$true}
  if(!$rejected -or $script:calls.Count -ne 1){throw "HTTP $code was incorrectly retried"}
}
$plan=@(Get-GeminiAttemptModels 'gemini-3.1-flash-tts-preview' @('gemini-2.5-flash-preview-tts','gemini-2.5-flash'))
if($plan[2] -ne 'gemini-2.5-flash-preview-tts'){throw 'Speech fallback selected a text-only model'}
Write-Output 'GEMINI_CLIENT_TEST=PASS (503 fallback, network timeout fallback, no retries for 400/403/429, compatible speech fallback)'
