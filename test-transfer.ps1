$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'bridge.ps1') -Library
if(-not ('TestNumberedSender' -as [type])){
Add-Type -TypeDefinition @'
using System;
using System.Collections.Generic;
public class TestNumberedSender {
  public int ReadTimeout {get;set;}
  public bool CorruptHash;
  public int DuplicateCount;
  byte[] data;
  Queue<string> lines=new Queue<string>();
  bool duplicateSent=false;
  bool timeoutSent=false;
  public TestNumberedSender(byte[] value,bool corrupt) {data=value;CorruptHash=corrupt;}
  void Chunk(int offset) {
    if(offset==data.Length) {
      uint hash=2166136261;unchecked {foreach(byte b in data)hash=(hash^b)*16777619;}
      if(CorruptHash)hash^=1;
      lines.Enqueue("UPLOAD_END2 42 "+hash.ToString("X8"));return;
    }
    lines.Enqueue("CHUNK 42 "+offset+" "+Convert.ToBase64String(data,offset,Math.Min(768,data.Length-offset)));
  }
  public void WriteLine(string line) {
    string[] fields=line.Split(' ');
    if(fields[0]=="ACCEPT"){Chunk(0);return;}
    if(fields[0]=="ACK") {
      int offset=int.Parse(fields[2]);
      if(offset==768&&!duplicateSent){duplicateSent=true;DuplicateCount++;Chunk(0);return;}
      if(offset==1536&&!timeoutSent){timeoutSent=true;return;}
      Chunk(offset);
    }
  }
  public string ReadLine(){if(lines.Count==0)throw new TimeoutException("Injected lost packet");return lines.Dequeue();}
}
'@
}
[byte[]]$data=New-Object byte[] 1920000
for($i=0;$i -lt $data.Length;$i++){$data[$i]=[byte](($i*17+23)%256)}
$fake=New-Object TestNumberedSender -ArgumentList $data,$false
[byte[]]$received=Receive-Upload $fake '42' $data.Length
if($received.Length -ne $data.Length -or [MemoryRadioWire]::Hash($received) -ne [MemoryRadioWire]::Hash($data) -or $fake.DuplicateCount -ne 1){throw 'Transfer reconstruction failed'}
$bad=New-Object TestNumberedSender -ArgumentList $data,$true
$rejected=$false;try{Receive-Upload $bad '42' $data.Length|Out-Null}catch{if($_.Exception.Message -match 'integrity'){$rejected=$true}else{throw}}
if(!$rejected){throw 'Corrupt recording was accepted'}
if([MemoryRadioWire]::Hash([Text.Encoding]::ASCII.GetBytes('hello')) -ne 0x4F9F2CAB){throw 'Known checksum vector failed'}
Write-Output 'TRANSFER_TEST=PASS (60s payload, duplicate packet, lost packet retry, exact reconstruction, checksum rejection, known hash vector)'
