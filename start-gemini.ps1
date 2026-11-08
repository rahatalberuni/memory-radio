param([string]$Port='COM3')
$ErrorActionPreference='Stop'
$pidPath=Join-Path $PSScriptRoot 'build\bridge.pid'
if(Test-Path -LiteralPath $pidPath){$oldId=[int](Get-Content -LiteralPath $pidPath);$old=Get-Process -Id $oldId -ErrorAction SilentlyContinue;if($old -and $old.ProcessName -eq 'powershell'){Write-Output "Memory Radio bridge is already running (PID $oldId).";exit}}
$arguments=@('-NoProfile','-ExecutionPolicy','Bypass','-STA','-File',('"'+(Join-Path $PSScriptRoot 'connect.ps1')+'"'),'-Port',$Port)
$process=Start-Process -FilePath powershell.exe -ArgumentList $arguments -WindowStyle Normal -RedirectStandardOutput (Join-Path $PSScriptRoot 'bridge-output.log') -RedirectStandardError (Join-Path $PSScriptRoot 'bridge-error.log') -PassThru
$process.Id|Set-Content -LiteralPath $pidPath
Write-Output "Memory Radio Gemini bridge launched (PID $($process.Id))."
