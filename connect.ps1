param([string]$Port='COM3')
$ErrorActionPreference='Stop'
$keyPath=Join-Path $PSScriptRoot '.gemini-key.xml'
if(!$env:GEMINI_API_KEY -and ![Environment]::GetEnvironmentVariable('GEMINI_API_KEY','User') -and !(Test-Path -LiteralPath $keyPath)){
  & (Join-Path $PSScriptRoot 'configure.ps1')
}
if(!$env:GEMINI_API_KEY -and ![Environment]::GetEnvironmentVariable('GEMINI_API_KEY','User') -and !(Test-Path -LiteralPath $keyPath)){exit}
try{& (Join-Path $PSScriptRoot 'bridge.ps1') -Port $Port}catch{
  Add-Type -AssemblyName System.Windows.Forms
  [Windows.Forms.MessageBox]::Show('Memory Radio could not connect. Close Arduino Serial Monitor, check your API key and internet connection, and try again. Details are in bridge-error.log.','Memory Radio')|Out-Null
  throw
}
