#pragma once
// Files are confined to /memory-radio. The card is never formatted.
String encode(const String &s){unsigned char out[2101];size_t n=0;if(!s.length())return "-";if(mbedtls_base64_encode(out,2100,&n,(const unsigned char*)s.c_str(),s.length())!=0)return "-";out[n]=0;return String((char*)out);}
String jsonString(const String &s){String out="\"";for(size_t i=0;i<s.length();i++){char c=s[i];if(c=='"'||c=='\\'){out+='\\';out+=c;}else if(c=='\n')out+="\\n";else if(c=='\r')out+="\\r";else if((uint8_t)c>=32)out+=c;}return out+'"';}
bool validId(const String &s){if(s.length()!=9||s[0]!='M')return false;for(int i=1;i<9;i++)if(!isxdigit(s[i]))return false;return true;}
void newFolder(){
  char id[16];do{snprintf(id,sizeof(id),"M%08X",esp_random());memoryId=id;memoryFolder="/memory-radio/"+memoryId;}while(sdOK&&SD_MMC.exists(memoryFolder));
  if(sdOK&&!SD_MMC.mkdir(memoryFolder))sdSaved=false;
}
bool saveLatest(){if(!sdOK||!validId(memoryId))return false;File f=SD_MMC.open("/memory-radio/latest.txt",FILE_WRITE);if(!f)return false;bool ok=f.println(memoryId)>0;f.close();return ok;}
bool saveAudio(String name,const int16_t *data,size_t count){
  if(!sdOK||!validId(memoryId)||!count||count>MAX_SAMPLES)return false;
  name.toLowerCase();for(size_t i=0;i<name.length();i++)if(!isalnum(name[i])&&name[i]!='-')return false;
  String path=memoryFolder+"/"+name+".wav";File f=SD_MMC.open(path+".tmp",FILE_WRITE);if(!f)return false;
  uint8_t header[44]={0};memcpy(header,"RIFF",4);uint32_t v=count*2+36;memcpy(header+4,&v,4);memcpy(header+8,"WAVEfmt ",8);v=16;memcpy(header+16,&v,4);header[20]=1;header[22]=1;v=16000;memcpy(header+24,&v,4);v=32000;memcpy(header+28,&v,4);header[32]=2;header[34]=16;memcpy(header+36,"data",4);v=count*2;memcpy(header+40,&v,4);
  bool ok=f.write(header,44)==44;size_t offset=0;
  while(ok&&offset<count*2){size_t n=min((size_t)4096,count*2-offset);ok=f.write(((const uint8_t*)data)+offset,n)==n;offset+=n;delay(1);}
  f.flush();f.close();if(ok){if(SD_MMC.exists(path))SD_MMC.remove(path);ok=SD_MMC.rename(path+".tmp",path);}
  Serial.printf("SD_AUDIO %s bytes=%u ok=%d\n",path.c_str(),(unsigned)(count*2+44),ok);return ok;
}
bool saveMetadata(){
  if(!sdOK||!validId(memoryId))return false;
  File f=SD_MMC.open(memoryFolder+"/memory.txt.tmp",FILE_WRITE);if(!f)return false;
  f.println("TITLE "+encode(memory.title));f.println("SUMMARY "+encode(memory.summary));f.println("NOTE "+encode(memory.note));
  for(int i=0;i<3;i++)f.println("Q"+String(i)+" "+encode(memory.questions[i]));
  for(int c=0;c<3;c++)for(int i=0;i<memory.counts[c];i++){Item &item=memory.items[c][i];f.println("I "+String(c)+" "+String(i)+" "+encode(item.name)+" "+encode(item.detail)+" "+encode(item.quote)+" "+encode(item.source)+" "+String(item.start,3)+" "+String(item.end,3));}
  bool ok=!f.getWriteError();f.flush();f.close();if(!ok)return false;
  String target=memoryFolder+"/memory.txt",backup=memoryFolder+"/memory.bak";
  if(SD_MMC.exists(backup))SD_MMC.remove(backup);
  if(SD_MMC.exists(target)&&!SD_MMC.rename(target,backup))return false;
  if(!SD_MMC.rename(target+".tmp",target))return false;
  File json=SD_MMC.open(memoryFolder+"/memory.json",FILE_WRITE);if(!json)return false;
  json.print("{\"title\":"+jsonString(memory.title)+",\"summary\":"+jsonString(memory.summary)+",\"uncertainty\":"+jsonString(memory.note)+",\"questions\":[");
  for(int i=0;i<3;i++){if(i)json.print(',');json.print(jsonString(memory.questions[i]));}json.print("],\"people\":[");
  for(int c=0;c<3;c++){if(c)json.print(c==1?"],\"places\":[":"],\"events\":[");for(int i=0;i<memory.counts[c];i++){if(i)json.print(',');Item &item=memory.items[c][i];json.print("{\"name\":"+jsonString(item.name)+",\"detail\":"+jsonString(item.detail)+",\"quote\":"+jsonString(item.quote)+",\"source\":"+jsonString(item.source)+",\"start_seconds\":"+String(item.start,3)+",\"end_seconds\":"+String(item.end,3)+"}");}}
  json.print("]}");ok=!json.getWriteError();json.flush();json.close();saveLatest();return ok;
}
bool loadSD(){
  if(!sdOK)return false;File index=SD_MMC.open("/memory-radio/latest.txt",FILE_READ);if(!index)return false;
  String id=index.readStringUntil('\n');id.trim();index.close();if(!validId(id))return false;
  String folder="/memory-radio/"+id;File wav=SD_MMC.open(folder+"/story.wav",FILE_READ);if(!wav)return false;
  uint8_t header[44];if(wav.read(header,44)!=44||memcmp(header,"RIFF",4)||memcmp(header+8,"WAVE",4)||memcmp(header+36,"data",4)){wav.close();return false;}
  uint32_t bytes=0;memcpy(&bytes,header+40,4);if(bytes>MAX_SAMPLES*2||bytes%2||wav.size()<bytes+44){wav.close();return false;}
  stopAudio();size_t offset=0;while(offset<bytes){size_t n=wav.read(((uint8_t*)original)+offset,min((size_t)4096,(size_t)bytes-offset));if(!n)break;offset+=n;}wav.close();if(offset!=bytes)return false;
  memoryId=id;memoryFolder=folder;originalCount=bytes/2;sdSaved=true;hasMemory=false;answerNumber=0;
  File listing=SD_MMC.open(folder);for(File f=listing.openNextFile();f;f=listing.openNextFile()){String name=f.name();if(name.startsWith("answer"))answerNumber=max(answerNumber,(int)name.substring(6).toInt());f.close();}listing.close();
  File meta=SD_MMC.open(folder+"/memory.txt",FILE_READ);if(!meta)meta=SD_MMC.open(folder+"/memory.bak",FILE_READ);
  if(meta){restoringSD=true;pending=Memory();waiting=true;requestId++;
    while(meta.available()){String line=meta.readStringUntil('\n');line.trim();if(line.startsWith("I "))serialCommand("ITEM "+String(requestId)+" "+line.substring(2));else serialCommand("FIELD "+String(requestId)+" "+line);}
    meta.close();serialCommand("DONE "+String(requestId));restoringSD=false;waiting=false;
  }
  Serial.printf("SD_RESTORED id=%s samples=%u memory=%d\n",memoryId.c_str(),(unsigned)originalCount,hasMemory);return true;
}
void listSD(){if(!sdOK){Serial.println("SD_NOT_AVAILABLE");return;}File dir=SD_MMC.open(memoryFolder.length()?memoryFolder:"/memory-radio");for(File f=dir.openNextFile();f;f=dir.openNextFile()){Serial.printf("SD_FILE %s %u\n",f.name(),(unsigned)f.size());f.close();}dir.close();}
size_t loadWav(const String &name,int16_t *target){
  if(target==reply){loadedAudioKind="";loadedAudioSamples=0;}
  if(!sdOK||!name.length())return 0;for(size_t i=0;i<name.length();i++)if(!isalnum(name[i])&&name[i]!='-')return 0;
  File f=SD_MMC.open(memoryFolder+"/"+name+".wav",FILE_READ);if(!f)return 0;
  uint8_t h[44];if(f.read(h,44)!=44||memcmp(h,"RIFF",4)||memcmp(h+36,"data",4)){f.close();return 0;}
  uint32_t bytes=0;memcpy(&bytes,h+40,4);if(bytes>MAX_SAMPLES*2||bytes%2||f.size()<44+bytes){f.close();return 0;}
  size_t at=0;while(at<bytes){size_t n=f.read(((uint8_t*)target)+at,min((size_t)4096,(size_t)bytes-at));if(!n)break;at+=n;}f.close();return at==bytes?bytes/2:0;
}
void verifySD(){
  size_t count=loadWav("story",reply);uint32_t a=2166136261u,b=2166136261u;
  for(size_t i=0;i<originalCount*2;i++){a=(a^((uint8_t*)original)[i])*16777619u;}
  for(size_t i=0;i<count*2;i++){b=(b^((uint8_t*)reply)[i])*16777619u;}
  Serial.printf("SD_VERIFY samples=%u loaded=%u originalHash=%08X savedHash=%08X pass=%d\n",(unsigned)originalCount,(unsigned)count,a,b,originalCount>0&&count==originalCount&&a==b);
}
