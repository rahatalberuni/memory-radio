// Memory Radio - LCDWIKI ES3C28P / ESP32-S3 N16R8.
// Gemini analyzes stories and follow-ups; the device records, plays, and explores.
#include <Arduino.h>
#include <SPI.h>
#include <Wire.h>
#include <ESP_I2S.h>
#include <SD_MMC.h>
#include <mbedtls/base64.h>
#include <math.h>
#include <atomic>

constexpr int W=240,H=320,CS=10,DC=46,BL=45,TOUCH_RST=18,AMP=1;
constexpr uint16_t BG=0x1064,CARD=0x2148,CYAN=0x8e7f,WHITE=0xff3a,DIM=0xad14,PINK=0xfb8d,GREEN=0xb710;
SPIClass display(FSPI);I2SClass audio;uint16_t *fb=nullptr;
bool touchOK=false,codecOK=false,audioOK=false;
#include "hardware.h"

constexpr size_t MAX_SAMPLES=16000*60;
int16_t *capture=nullptr,*original=nullptr,*reply=nullptr;
SemaphoreHandle_t audioLock=nullptr;
enum AudioMode {QUIET,RECORD,PLAY};
std::atomic<int> audioMode{QUIET},audioDone{QUIET};
std::atomic<size_t> recordCount{0},playPosition{0};
std::atomic<uint32_t> micPeak{0},micBlocks{0};
std::atomic<uint32_t> speakerFrames{0},speakerNonzeroFrames{0},speakerErrors{0},speakerPeak{0};
std::atomic<uint32_t> speakerPendingId{0},speakerStartedId{0},speakerFailedId{0},speakerAckBytes{0};
std::atomic<bool> speakerFault{false};
int16_t speakerTone[14400];
String loadedAudioKind="",loadedAudioMemoryId="";size_t loadedAudioSamples=0;
const int16_t *playBuffer=nullptr;size_t playLength=0,recordLimit=MAX_SAMPLES,originalCount=0;
struct Item{String name,detail,quote,source;float start=0,end=0;};
struct Memory{String title,summary,note,questions[3];Item items[3][4];int counts[3]={0,0,0};};
Memory memory,pending;bool hasMemory=false;
enum Screen {HOME,RECORDING,REVIEW,PROCESSING,SUMMARY,LIST,DETAIL,QUESTIONS,QUESTION,PROBLEM};
Screen screen=HOME,returnScreen=SUMMARY;
int category=0,itemIndex=0,questionIndex=0,page=0;
int answerNumber=0;
bool followup=false,waiting=false;
String notice="",busyLabel="",recordPrompt="";
uint32_t frames=0,lastBackend=0,requestId=0,waitStart=0;
size_t receiveLength=0,receiveCount=0;bool receiveValid=false;
uint32_t receiveId=0,receiveStarted=0;
bool uploadActive=false,uploadAccepted=false;
const uint8_t *uploadBuffer=nullptr;
size_t uploadLength=0,uploadOffset=0,uploadChunkSize=0;
uint32_t uploadLastSent=0,uploadHash=0;int uploadRetries=0;
String uploadHeader="";
bool sdOK=false,sdSaved=false,restoringSD=false;
String memoryId="",memoryFolder="",requestKind="";
void serialCommand(String line);
void stopAudio();
#include "storage.h"

bool online(){return lastBackend&&millis()-lastBackend<15000;}
void stopAudio(){if(audioLock)xSemaphoreTake(audioLock,portMAX_DELAY);audioMode=QUIET;audioDone=QUIET;speakerPendingId=0;if(audioLock)xSemaphoreGive(audioLock);}
void playPcm(const int16_t *samples,size_t count,uint32_t responseId=0){
  if(!audioOK||!samples||!count){notice="SPEAKER AUDIO IS NOT AVAILABLE.";screen=PROBLEM;if(responseId)Serial.printf("AUDIO_ERROR %u\n",responseId);return;}
  stopAudio();digitalWrite(AMP,LOW);playBuffer=samples;playLength=count;playPosition=0;speakerAckBytes=count*2;speakerPendingId=responseId;audioMode=PLAY;
}
void testSpeaker(){
  if(waiting||screen==RECORDING)return;stopAudio();
  for(int i=0;i<14400;i++){int part=i/4800,local=i%4800;float envelope=min(1.0f,min(local/160.0f,(4800-local)/160.0f));float sample=local<4000?sinf(2*PI*(part==0?523:part==1?659:784)*local/16000)*5000*envelope:0;speakerTone[i]=(int16_t)sample;}
  playPcm(speakerTone,14400);Serial.println("SPEAKER_TEST_STARTED three_tones=1");
}
void audioTask(void*){
  int16_t tx[512],rx[256];
  for(;;){
    xSemaphoreTake(audioLock,portMAX_DELAY);int mode=audioMode.load();size_t pos=playPosition.load();
    for(int i=0;i<256;i++){int16_t v=(mode==PLAY&&pos<playLength)?playBuffer[pos++]:0;tx[2*i]=tx[2*i+1]=v;}
    if(mode==PLAY)playPosition=pos;
    size_t written=audio.write((uint8_t*)tx,sizeof(tx));
    if(mode==PLAY){uint32_t peak=0;for(int i=0;i<512;i++)peak=max(peak,(uint32_t)abs((int)tx[i]));speakerPeak=peak;
      if(written==sizeof(tx)){speakerFrames++;if(peak)speakerNonzeroFrames++;uint32_t id=speakerPendingId.exchange(0);if(id)speakerStartedId=id;}
      else{speakerErrors++;speakerFault=true;speakerFailedId=speakerPendingId.exchange(0);audioMode=QUIET;}}
    size_t n=audio.readBytes((char*)rx,sizeof(rx))/2;
    uint32_t peak=0;for(size_t i=0;i<n;i++)peak=max(peak,(uint32_t)abs((int)rx[i]));micPeak=peak;micBlocks++;
    if(mode==RECORD&&n){size_t used=recordCount.load(),take=min(n,recordLimit-used);memcpy(capture+used,rx,take*2);recordCount=used+take;if(used+take>=recordLimit){audioMode=QUIET;audioDone=RECORD;}}
    else if(mode==PLAY&&pos>=playLength){audioMode=QUIET;audioDone=PLAY;}
    xSemaphoreGive(audioLock);vTaskDelay(1);
  }
}
String decode(const String &encoded,size_t limit=1500){
  if(encoded=="-")return "";
  size_t n=0;unsigned char buf[1501];
  if(mbedtls_base64_decode(buf,min(limit,(size_t)1500),&n,(const unsigned char*)encoded.c_str(),encoded.length())!=0)return "";
  buf[n]=0;return String((char*)buf);
}
int linesOf(const String &input,String *lines,int maxLines=70,int width=34){
  String rest=input;rest.toUpperCase();int n=0;
  while(rest.length()&&n<maxLines){int cut=min(width,(int)rest.length());if(cut<(int)rest.length()){int space=rest.lastIndexOf(' ',cut);if(space>0)cut=space;}lines[n++]=rest.substring(0,cut);rest=rest.substring(cut);rest.trim();}
  return n;
}
void wrapped(int x,int y,const String &s,int width=34,int maxLines=3,uint16_t color=WHITE){String lines[16];int n=linesOf(s,lines,min(maxLines,16),width);for(int i=0;i<n;i++)text(x,y+i*14,lines[i].c_str(),1,color);}
void panel(int x,int y,int w,int h){roundRect(x,y,w,h,9,CARD);}
void btn(int x,int y,int w,const char *label,uint16_t color=CYAN){panel(x,y,w,32);text(x+(w-(int)strlen(label)*6)/2,y+12,label,1,color);}
void bodyPages(const String &content,int y=100,int perPage=8){
  String lines[70];int n=linesOf(content,lines);int pages=max(1,(n+perPage-1)/perPage);page=constrain(page,0,pages-1);
  for(int i=0;i<perPage&&page*perPage+i<n;i++)text(16,y+i*14,lines[page*perPage+i].c_str(),1,WHITE);
  char label[20];snprintf(label,sizeof(label),"%d OF %d",page+1,pages);center(220,label,1,DIM);
  if(page>0)text(16,220,"PREV",1,CYAN);if(page+1<pages)text(198,220,"NEXT",1,CYAN);
}
void tabs(){const char *labels[]={"STORY","PEOPLE","PLACES","EVENTS"};for(int i=0;i<4;i++){panel(8+i*58,278,54,32);text(10+i*58+(50-(int)strlen(labels[i])*6)/2,290,labels[i],1,(screen==SUMMARY?i==0:i==category+1)?GREEN:DIM);}}
void drawRadio(){
  panel(39,69,162,115);rect(54,84,103,25,BG);for(int i=0;i<11;i++)rect(60+i*8,91,2,11,DIM);
  circle(178,97,10,CYAN);circle(178,97,4,CARD);
  for(int i=0;i<8;i++){int h=8+(int)(10+9*sinf(i*1.6f+millis()/550.0f));rect(59+i*16,137-h/2,5,h,i%2?PINK:CYAN);}
  rect(151,65,3,17,DIM);rect(154,63,31,3,DIM);
}
void draw(){
  rect(0,0,W,H,BG);text(12,14,"MEMORY",2,WHITE);text(97,14,"RADIO",2,CYAN);
  text(12,40,online()?"GEMINI CONNECTED":"CONNECT USB + GEMINI",1,online()?GREEN:DIM);
  text(12,55,sdOK?(sdSaved?"SD: SAVED":"SD: READY"):"SD: NOT AVAILABLE",1,sdOK?GREEN:PINK);
  text(118,55,audioMode==PLAY?"SPEAKER PLAYING":audioOK?"SPEAKER READY":"NO AUDIO",1,audioMode==PLAY?PINK:DIM);
  if(screen!=HOME&&screen!=RECORDING&&screen!=PROCESSING){text(211,40,"HOME",1,DIM);}
  if(screen==HOME){
    drawRadio();btn(12,190,216,audioMode==PLAY?"STOP SPEAKER":"TEST SPEAKER",PINK);
    btn(12,231,216,"RECORD A MEMORY",GREEN);btn(12,274,216,hasMemory?"EXPLORE YOUR MEMORY":"OPEN LATEST SAVED",CYAN);
  }else if(screen==RECORDING){
    wrapped(18,67,followup?recordPrompt:"TELL ME ABOUT A MOMENT YOU WANT TO REMEMBER.",34,4,DIM);
    char timer[20];int left=max(0,(int)(recordLimit/16000-recordCount.load()/16000));snprintf(timer,sizeof(timer),"%02d SEC",left);text(48,143,timer,3,PINK);
    for(int i=0;i<22;i++){int h=5+min(40,(int)(micPeak.load()/80))*(0.3f+0.7f*fabsf(sinf(i*0.8f+millis()/110.0f)));rect(15+i*10,199-h/2,5,h,i%3?CYAN:PINK);}
    btn(12,237,216,"FINISH RECORDING",GREEN);btn(12,278,216,"DISCARD",DIM);
  }else if(screen==REVIEW){
    panel(12,69,216,93);text(24,84,"VOICE CAPTURED",2,GREEN);
    char d[40];snprintf(d,sizeof(d),"%u SECONDS IN YOUR OWN VOICE",(unsigned)(originalCount/16000));wrapped(24,115,d,31,2,DIM);
    btn(12,177,216,audioMode==PLAY?"STOP PLAYBACK":"PLAY MY RECORDING");btn(12,220,216,"ORGANIZE WITH GEMINI",GREEN);btn(12,278,216,"RECORD AGAIN",DIM);
  }else if(screen==PROCESSING){
    drawRadio();wrapped(20,195,busyLabel,33,3,CYAN);text(20,249,"YOUR RECORDING IS STILL SAFE",1,DIM);btn(12,278,216,"CANCEL",DIM);
  }else if(screen==SUMMARY){
    wrapped(16,65,memory.title,34,2,CYAN);bodyPages(memory.summary+(memory.note.length()?"  UNCLEAR: "+memory.note:""));
    btn(12,238,102,"QUESTIONS",GREEN);btn(126,238,102,audioMode==PLAY?"STOP AUDIO":"PLAY AI AUDIO",PINK);tabs();
  }else if(screen==LIST){
    const char *names[]={"PEOPLE IN YOUR STORY","PLACES YOU REMEMBER","YOUR MEMORY TIMELINE"};text(16,68,names[category],1,CYAN);
    for(int i=0;i<memory.counts[category];i++){int y=88+i*35;panel(12,y,216,32);wrapped(22,y+6,memory.items[category][i].name,32,2,WHITE);}
    if(!memory.counts[category])wrapped(20,110,"NONE CLEARLY MENTIONED. ANSWER A FOLLOW-UP TO ADD MORE.",32,4,DIM);
    btn(12,238,216,"FOLLOW-UP QUESTIONS",GREEN);tabs();
  }else if(screen==DETAIL){
    Item &item=memory.items[category][itemIndex];wrapped(16,65,item.name,34,2,CYAN);
    bodyPages(item.detail+"  YOU SAID: "+item.quote);
    btn(12,238,216,"HEAR ORIGINAL MOMENT",PINK);btn(12,278,216,"BACK TO EXPLORER",DIM);
  }else if(screen==QUESTIONS){
    text(16,68,"GEMINI WANTS TO KNOW...",1,CYAN);
    for(int i=0;i<3;i++){int y=87+i*54;panel(12,y,216,48);wrapped(21,y+8,memory.questions[i],32,3,WHITE);}
    btn(12,278,216,"BACK TO STORY",DIM);
  }else if(screen==QUESTION){
    text(16,68,"ONE MORE DETAIL",1,CYAN);wrapped(18,96,memory.questions[questionIndex],34,8,WHITE);
    btn(12,198,216,"HEAR QUESTION",CYAN);btn(12,238,216,"RECORD MY ANSWER",GREEN);btn(12,278,216,"BACK",DIM);
  }else{
    text(16,75,"LET'S TRY AGAIN",2,PINK);wrapped(18,111,notice,34,7,WHITE);
    btn(12,238,216,originalCount?"BACK TO MY RECORDING":"BACK HOME",GREEN);btn(12,278,216,"HOME",DIM);
  }
  flush();frames++;
}
void problem(const String &message){waiting=false;uploadActive=false;receiveValid=false;notice=message;screen=PROBLEM;}
void beginRecord(bool answer=false){
  if(!audioOK){problem("MICROPHONE IS NOT AVAILABLE.");return;}
  stopAudio();followup=answer;recordCount=0;recordLimit=(answer?30:60)*16000;
  if(answer)recordPrompt=memory.questions[questionIndex];screen=RECORDING;audioMode=RECORD;
}
void beginWait(const String &label,Screen back){stopAudio();waiting=true;requestId++;waitStart=millis();busyLabel=label;returnScreen=back;screen=PROCESSING;}
bool needBackend(){if(online())return true;problem("OPEN CONNECT GEMINI ON YOUR COMPUTER. ENTER YOUR KEY THERE, THEN TRY AGAIN.");return false;}
void uploadAudio(bool answer){
  if(!needBackend())return;
  size_t samples=answer?recordCount.load():originalCount;
  if(samples<16000){problem("PLEASE RECORD AT LEAST ONE SECOND OF SPEECH.");return;}
  beginWait(answer?"GEMINI IS WEAVING YOUR NEW DETAIL INTO THE STORY...":"GEMINI IS LISTENING AND BUILDING YOUR MEMORY...",answer?QUESTION:REVIEW);
  const int16_t *buffer=answer?capture:original;size_t bytes=samples*2;
  String source=answer?"answer"+String(answerNumber):"story";
  uploadBuffer=(const uint8_t*)buffer;uploadLength=bytes;uploadOffset=0;uploadChunkSize=0;uploadAccepted=false;uploadRetries=0;uploadActive=true;
  uploadHash=2166136261u;for(size_t i=0;i<bytes;i++)uploadHash=(uploadHash^uploadBuffer[i])*16777619u;
  uploadHeader="UPLOAD2 "+String(requestId)+" "+(answer?"ANSWER":"STORY")+" "+String(questionIndex)+" "+String(bytes)+" "+memoryId+" "+source;Serial.println(uploadHeader);
  uploadLastSent=millis();
}
void sendUploadChunk(){
  if(!uploadActive||!uploadAccepted)return;
  if(uploadOffset>=uploadLength){Serial.printf("UPLOAD_END2 %u %08X\n",requestId,uploadHash);uploadActive=false;return;}
  uploadChunkSize=min((size_t)768,uploadLength-uploadOffset);unsigned char encoded[1025];size_t n=0;
  if(mbedtls_base64_encode(encoded,sizeof(encoded),&n,uploadBuffer+uploadOffset,uploadChunkSize)!=0){problem("USB AUDIO ENCODING FAILED. YOUR RECORDING IS SAVED.");return;}
  encoded[n]=0;Serial.printf("CHUNK %u %u %s\n",requestId,(unsigned)uploadOffset,encoded);uploadLastSent=millis();
}
void uploadPoll(){
  if(!uploadActive||millis()-uploadLastSent<2000)return;
  if(++uploadRetries>6){problem("USB TRANSFER TIMED OUT. RESTART CONNECT GEMINI, THEN RETRY YOUR SAVED RECORDING.");return;}
  if(uploadAccepted)sendUploadChunk();
  else{Serial.println(uploadHeader);uploadLastSent=millis();}
}
void finishRecord(){
  stopAudio();if(followup){answerNumber++;sdSaved=saveAudio(String("answer")+String(answerNumber),capture,recordCount.load());uploadAudio(true);}else{originalCount=recordCount.load();memcpy(original,capture,originalCount*2);hasMemory=false;memory=Memory();answerNumber=0;newFolder();sdSaved=saveAudio("story",original,originalCount);saveLatest();screen=REVIEW;}
}
void request(const char *kind,int a=0,int b=0){
  if(!needBackend())return;Screen previous=screen;
  beginWait(String(kind)=="LOAD"?"OPENING YOUR LAST SAVED MEMORY...":String(kind)=="EXCERPT"?"FINDING THAT MOMENT IN YOUR ORIGINAL VOICE...":"GEMINI IS PREPARING YOUR NARRATION...",previous);
  requestKind=kind;Serial.printf("REQUEST %u %s %d %d %s\n",requestId,kind,a,b,memoryId.length()?memoryId.c_str():"LATEST");
}
void playAiAudio(){
  if(audioMode==PLAY){stopAudio();return;}
  if(!audioOK){problem("THE SPEAKER AUDIO DRIVER IS NOT READY. TRY TEST SPEAKER ON THE HOME SCREEN.");return;}
  if(loadedAudioKind=="NARRATE"&&loadedAudioMemoryId==memoryId&&loadedAudioSamples){playPcm(reply,loadedAudioSamples);return;}
  stopAudio();size_t count=loadWav("recap",reply);
  if(count){loadedAudioKind="NARRATE";loadedAudioMemoryId=memoryId;loadedAudioSamples=count;playPcm(reply,count);return;}
  if(online()){request("NARRATE");return;}
  problem("NO AI AUDIO SAVED YET. CONNECT GEMINI, OPEN YOUR SUMMARY, THEN TAP PLAY AI AUDIO.");
}
void cancelWait(){waiting=false;uploadActive=false;requestId++;receiveValid=false;screen=returnScreen;Serial.println("CANCEL");}
void handleTouch(int x,int y){
  Serial.printf("TOUCH %d %d\n",x,y);
  if(screen!=RECORDING&&screen!=PROCESSING&&screen!=HOME&&y>=30&&y<55&&x>205){stopAudio();screen=HOME;return;}
  if(screen==HOME){if(y>=190&&y<222){if(audioMode==PLAY)stopAudio();else testSpeaker();}else if(y>=231&&y<263)beginRecord();else if(y>=274&&y<306){if(hasMemory){screen=SUMMARY;page=0;}else if(originalCount){screen=REVIEW;}else if(!loadSD())request("LOAD");}}
  else if(screen==RECORDING){if(y>=237&&y<269)finishRecord();else if(y>=278){stopAudio();screen=followup?QUESTION:HOME;}}
  else if(screen==REVIEW){if(y>=177&&y<209){if(audioMode==PLAY)stopAudio();else playPcm(original,originalCount);}else if(y>=220&&y<252)uploadAudio(false);else if(y>=278)beginRecord();}
  else if(screen==PROCESSING){if(y>=278)cancelWait();}
  else if(screen==SUMMARY||screen==LIST){
    if(y>=278){stopAudio();int t=constrain((x-8)/58,0,3);page=0;if(t==0)screen=SUMMARY;else{category=t-1;screen=LIST;}return;}
    if(y>=238&&y<270){if(screen==LIST||x<120){screen=QUESTIONS;}else{playAiAudio();}return;}
    if(screen==SUMMARY&&y>=212&&y<233){page+=x<120?-1:1;page=max(0,page);}
    if(screen==LIST&&y>=88&&y<228){int i=(y-88)/35;if(i<memory.counts[category]){itemIndex=i;page=0;screen=DETAIL;}}
  }else if(screen==DETAIL){if(y>=212&&y<233){page=max(0,page+(x<120?-1:1));}else if(y>=238&&y<270){if(online())request("EXCERPT",category,itemIndex);else{Item &item=memory.items[category][itemIndex];const int16_t *data=original;size_t count=originalCount;if(item.source!="story"){count=loadWav(item.source,reply);data=reply;}size_t start=(size_t)(max(0.0f,item.start-0.4f)*16000),end=min(count,(size_t)((item.end+0.4f)*16000));if(end>start)playPcm(data+start,end-start);}}else if(y>=278){stopAudio();screen=LIST;}}
  else if(screen==QUESTIONS){if(y>=87&&y<249){int q=(y-87)/54;if(q<3){questionIndex=q;screen=QUESTION;}}else if(y>=278){screen=SUMMARY;page=0;}}
  else if(screen==QUESTION){if(y>=198&&y<230)request("QUESTION",questionIndex);else if(y>=238&&y<270)beginRecord(true);else if(y>=278){stopAudio();screen=QUESTIONS;}}
  else if(screen==PROBLEM){if(y>=238&&y<270)screen=originalCount?REVIEW:HOME;else if(y>=278)screen=HOME;}
}
void touchPoll(){static bool down=false;static int x=0,y=0;if(!touchOK)return;int count=regRead(0x38,2);if(count<0)return;bool touched=count>0&&count<3;
  if(touched&&!down){Wire.beginTransmission(0x38);Wire.write(3);if(Wire.endTransmission(false)!=0)return;if(Wire.requestFrom((uint8_t)0x38,(size_t)4)!=4)return;x=(Wire.read()&15)<<8;x|=Wire.read();y=(Wire.read()&15)<<8;y|=Wire.read();down=true;}
  else if(!touched&&down){down=false;if(x>=0&&x<W&&y>=0&&y<H)handleTouch(x,y);}
}
void status(){Serial.printf("STATUS screen=%d touch=%d codec=%d audio=%d psram=%u heap=%u mic=%u blocks=%u original=%u recording=%u mode=%d memory=%d people=%d places=%d events=%d waiting=%d request=%u backend=%d frames=%u sd=%d saved=%d id=%s speakerFrames=%u speakerNonzero=%u speakerErrors=%u speakerPeak=%u amplifier=%d playPosition=%u playLength=%u\n",screen,touchOK,codecOK,audioOK,ESP.getPsramSize(),ESP.getFreeHeap(),micPeak.load(),micBlocks.load(),(unsigned)originalCount,(unsigned)recordCount.load(),audioMode.load(),hasMemory,memory.counts[0],memory.counts[1],memory.counts[2],waiting,requestId,online(),frames,sdOK,sdSaved,memoryId.c_str(),speakerFrames.load(),speakerNonzeroFrames.load(),speakerErrors.load(),speakerPeak.load(),digitalRead(AMP)==LOW,(unsigned)playPosition.load(),(unsigned)playLength);}
void serialCommand(String line){
  line.trim();unsigned id=0;int a=0,b=0;size_t bytes=0;
  if(line=="READY"){lastBackend=millis();}
  else if(sscanf(line.c_str(),"ACCEPT %u",&id)==1){if(uploadActive&&waiting&&id==requestId&&!uploadAccepted){uploadAccepted=true;uploadRetries=0;sendUploadChunk();}}
  else if(sscanf(line.c_str(),"ACK %u %d",&id,&a)==2){if(waiting&&id==requestId&&a>=0&&(size_t)a<=uploadLength){if(uploadActive){if((size_t)a==uploadOffset+uploadChunkSize){uploadOffset=a;uploadRetries=0;sendUploadChunk();}else if((size_t)a==uploadOffset){sendUploadChunk();}}else if(uploadAccepted&&(size_t)a==uploadLength)Serial.printf("UPLOAD_END2 %u %08X\n",requestId,uploadHash);}}
  else if(line=="STATUS")status();
  else if(line=="PENDING")Serial.printf("PENDING title=%u summary=%u\n",pending.title.length(),pending.summary.length());
  else if(line=="FRAME"){Serial.printf("FRAME %d\n",W*H*2);Serial.write((uint8_t*)fb,W*H*2);Serial.println("\nFRAME_END");}
  else if(line=="RECORD")beginRecord();
  else if(line=="STOP"&&screen==RECORDING)finishRecord();
  else if(line=="ANALYZE"&&screen==REVIEW)uploadAudio(false);
  else if(line=="ANALYZE_SAVED"&&originalCount>0&&!waiting)uploadAudio(false);
  else if(line=="PLAY")playPcm(original,originalCount);
  else if(line=="SPEAKER_TEST")testSpeaker();
  else if(line=="SPEAKER_STREAM_TEST"&&!waiting)request("SPEAKER_CHECK");
  else if(line=="PLAY_AI"&&hasMemory)playAiAudio();
  else if(line=="LOAD_SD")loadSD();
  else if(line=="SD_LIST")listSD();
  else if(line=="SD_VERIFY")verifySD();
  else if(line=="SD_JSON"){if(sdOK&&validId(memoryId)){File f=SD_MMC.open(memoryFolder+"/memory.json",FILE_READ);if(f){Serial.println("JSON_BEGIN");while(f.available())Serial.write(f.read());Serial.println("\nJSON_END");f.close();}}}
  else if(line=="REBOOT"){stopAudio();Serial.println("REBOOTING");delay(100);ESP.restart();}
  else if(line=="CANCEL"&&waiting)cancelWait();
  else if(line=="RESET"){stopAudio();waiting=false;uploadActive=false;requestId++;receiveValid=false;screen=HOME;}
  else if(sscanf(line.c_str(),"TAP %d %d",&a,&b)==2){handleTouch(a,b);}
  else if(sscanf(line.c_str(),"BEGIN %u",&id)==1){if(waiting&&id==requestId)pending=Memory();}
  else if(line.startsWith("FIELD ")){
    int s1=line.indexOf(' ',6),s2=line.indexOf(' ',s1+1);id=line.substring(6,s1).toInt();
    if(waiting&&id==requestId&&s1>6&&s2>s1){String key=line.substring(s1+1,s2),value=decode(line.substring(s2+1));
      if(key=="TITLE")pending.title=value.substring(0,68);else if(key=="SUMMARY")pending.summary=value.substring(0,1400);else if(key=="NOTE")pending.note=value.substring(0,300);else if(key.length()==2&&key[0]=='Q'&&key[1]>='0'&&key[1]<='2')pending.questions[key[1]-'0']=value.substring(0,180);
      Serial.printf("FIELD_RECEIVED key=%s bytes=%u\n",key.c_str(),value.length());
    }
  }else if(line.startsWith("ITEM ")){
    int pos=5;String fields[9];for(int i=0;i<9;i++){int at=line.indexOf(' ',pos);if(at<0)at=line.length();fields[i]=line.substring(pos,at);pos=at+1;}
    id=fields[0].toInt();a=fields[1].toInt();b=fields[2].toInt();
    if(waiting&&id==requestId&&a>=0&&a<3&&b>=0&&b<(a==2?4:3)){
      pending.items[a][b]={decode(fields[3],100),decode(fields[4],500),decode(fields[5],300),decode(fields[6],30),fields[7].toFloat(),fields[8].toFloat()};pending.counts[a]=max(pending.counts[a],b+1);
    }
  }else if(sscanf(line.c_str(),"DONE %u",&id)==1){
    if(waiting&&id==requestId){if(!pending.title.length()||!pending.summary.length()){problem("GEMINI RETURNED AN INCOMPLETE MEMORY. PLEASE TRY AGAIN.");return;}bool changed=memory.summary!=pending.summary;memory=pending;hasMemory=true;waiting=false;screen=SUMMARY;page=0;if(!restoringSD){if(changed){loadedAudioKind="";loadedAudioSamples=0;if(sdOK&&SD_MMC.exists(memoryFolder+"/recap.wav"))SD_MMC.rename(memoryFolder+"/recap.wav",memoryFolder+"/recap-previous-"+String(millis())+".wav");}lastBackend=millis();sdSaved=saveMetadata();}}
  }else if(line.startsWith("ERROR ")){
    int split=line.indexOf(' ',6);id=line.substring(6,split).toInt();if(waiting&&id==requestId)problem(decode(line.substring(split+1)));
  }else if(sscanf(line.c_str(),"AUDIO_OUT %u %u",&id,(unsigned*)&bytes)==2){
    if(bytes==0||bytes>MAX_SAMPLES*2||bytes%2){if(waiting&&id==requestId)problem("INVALID AUDIO RESPONSE.");return;}
    receiveLength=bytes;receiveCount=0;receiveId=id;receiveValid=waiting&&id==requestId;receiveStarted=millis();stopAudio();
  }else if(sscanf(line.c_str(),"AUDIO_OUT_END %u",&id)==1){
    if(receiveValid&&receiveCount==receiveLength&&id==receiveId&&waiting&&id==requestId){waiting=false;screen=returnScreen;String audioName=requestKind=="NARRATE"?"recap":requestKind=="QUESTION"?"question"+String(questionIndex):requestKind=="SPEAKER_CHECK"?"speaker-test":"excerpt-"+String(category)+"-"+String(itemIndex);sdSaved=saveAudio(audioName,reply,receiveLength/2);loadedAudioKind=requestKind;loadedAudioMemoryId=memoryId;loadedAudioSamples=receiveLength/2;playPcm(reply,receiveLength/2,id);lastBackend=millis();}
    receiveValid=false;receiveLength=0;receiveCount=0;
  }
}
void serialPoll(){
  static String line="";
  while(Serial.available()){
    if(receiveLength&&receiveCount<receiveLength){uint8_t trash[512];size_t n=min((size_t)Serial.available(),min((size_t)512,receiveLength-receiveCount));uint8_t *target=receiveValid?((uint8_t*)reply)+receiveCount:trash;size_t got=Serial.readBytes((char*)target,n);receiveCount+=got;continue;}
    char c=Serial.read();if(c=='\n'){serialCommand(line);line="";}else if(c!='\r'){if(line.length()<7000)line+=c;else line="";}
  }
  if(receiveLength&&receiveCount<receiveLength&&millis()-receiveStarted>20000){receiveLength=receiveCount=0;receiveValid=false;line="";if(waiting)problem("AUDIO TRANSFER TIMED OUT. PLEASE RECONNECT THE BRIDGE.");}
}
void setup(){
  Serial.setRxBufferSize(16384);Serial.setTxBufferSize(2048);Serial.begin(115200);delay(300);pinMode(AMP,OUTPUT);digitalWrite(AMP,HIGH);audioLock=xSemaphoreCreateMutex();
  fb=(uint16_t*)heap_caps_malloc(W*H*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  capture=(int16_t*)heap_caps_malloc(MAX_SAMPLES*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);original=(int16_t*)heap_caps_malloc(MAX_SAMPLES*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);reply=(int16_t*)heap_caps_malloc(MAX_SAMPLES*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);
  if(!fb||!capture||!original||!reply||!audioLock){Serial.println("FATAL PSRAM ALLOCATION");fb=nullptr;return;}
  lcdInit();draw();pinMode(TOUCH_RST,OUTPUT);digitalWrite(TOUCH_RST,LOW);delay(15);digitalWrite(TOUCH_RST,HIGH);delay(200);
  Wire.begin(16,15,400000);Wire.setTimeOut(30);touchOK=probe(0x38);codecOK=probe(0x18);
  if(codecOK){audio.setPins(5,7,8,6,4);audioOK=audio.begin(I2S_MODE_STD,16000,I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_STEREO);
    if(audioOK)audioOK=audio.configureRX(16000,I2S_DATA_BIT_WIDTH_16BIT,I2S_SLOT_MODE_MONO,I2S_RX_TRANSFORM_NONE,I2S_STD_SLOT_LEFT)&&codecInit();
    if(audioOK){audio.setTimeout(50);digitalWrite(AMP,LOW);if(xTaskCreatePinnedToCore(audioTask,"MemoryAudio",4096,nullptr,3,nullptr,0)!=pdPASS){audioOK=false;digitalWrite(AMP,HIGH);}}
  }
  SD_MMC.setPins(38,40,39);sdOK=SD_MMC.begin("/sdcard",true,false,20000);
  if(sdOK){sdOK=SD_MMC.cardType()!=CARD_NONE&&SD_MMC.mkdir("/memory-radio");if(sdOK){loadSD();screen=HOME;}}
  Serial.println("MEMORY RADIO READY");status();
}
void loop(){
  if(!fb){delay(1000);return;}serialPoll();touchPoll();uploadPoll();int done=audioDone.exchange(QUIET);if(done==RECORD&&screen==RECORDING)finishRecord();
  uint32_t playingId=speakerStartedId.exchange(0);if(playingId)Serial.printf("AUDIO_PLAYING %u %u\n",playingId,speakerAckBytes.load());
  if(speakerFault.exchange(false)){uint32_t failed=speakerFailedId.exchange(0);if(failed)Serial.printf("AUDIO_ERROR %u\n",failed);problem("SPEAKER OUTPUT FAILED. PLEASE RESTART THE DEVICE AND TRY TEST SPEAKER.");}
  if(waiting&&millis()-waitStart>150000){waiting=false;requestId++;problem("GEMINI TOOK TOO LONG. YOUR RECORDING IS AVAILABLE TO RETRY.");}
  static uint32_t lastFrame=0;if(millis()-lastFrame>=100){lastFrame=millis();draw();}delay(3);
}
