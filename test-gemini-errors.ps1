$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'gemini-errors.ps1')
$record=New-Object Management.Automation.ErrorRecord (New-Object Exception 'The remote server returned an error.'),'GeminiTest',([Management.Automation.ErrorCategory]::InvalidOperation),$null
$record.ErrorDetails=New-Object Management.Automation.ErrorDetails '{"error":{"code":429,"status":"RESOURCE_EXHAUSTED","message":"Quota exceeded for model. secret-value","details":[{"retryDelay":"30s"}]}}'
$failure=Get-GeminiFailure $record 'secret-value'
if($failure.http_code -ne 429 -or $failure.status -ne 'RESOURCE_EXHAUSTED' -or $failure.retry_delay -ne '30s'){throw 'Provider error details were lost'}
if($failure.message.Contains('secret-value') -or !$failure.message.Contains('[REDACTED]')){throw 'Secret redaction failed'}
if((Format-GeminiFailure $failure) -notmatch '^Gemini HTTP 429 RESOURCE_EXHAUSTED:'){throw 'Readable error formatting failed'}
$record.ErrorDetails=New-Object Management.Automation.ErrorDetails '{"error":{"code":400,"status":"INVALID_ARGUMENT","message":"Unsupported response schema"}}'
$failure=Get-GeminiFailure $record
if($failure.http_code -ne 400 -or $failure.message -ne 'Unsupported response schema'){throw 'Invalid request diagnosis failed'}
$record=New-Object Management.Automation.ErrorRecord (New-Object Net.WebException 'The operation has timed out',([Net.WebExceptionStatus]::Timeout)),'TimeoutTest',([Management.Automation.ErrorCategory]::OperationTimeout),$null
$failure=Get-GeminiFailure $record
if($failure.http_code -ne 0 -or $failure.status -ne 'TIMEOUT'){throw 'Transport timeout classification failed'}
Write-Output 'GEMINI_ERROR_TEST=PASS (HTTP details, quota reason, retry delay, credential redaction, request errors, network timeout classification)'
