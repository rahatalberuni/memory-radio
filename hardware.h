// Hardware driver reused from the locally validated PipPet project.
// Deliberately small authored 5x7 font, stored as row masks.
const uint8_t alphabet[36][7]={
{14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},{30,17,17,17,17,17,30},
{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},{14,17,16,23,17,17,15},{17,17,17,31,17,17,17},
{31,4,4,4,4,4,31},{7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
{17,27,21,21,17,17,17},{17,25,25,21,19,19,17},{14,17,17,17,17,17,14},{30,17,17,30,16,16,16},
{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},{15,16,16,14,1,1,30},{31,4,4,4,4,4,4},
{17,17,17,17,17,17,14},{17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
{17,17,10,4,4,4,4},{31,1,2,4,8,16,31},{14,17,19,21,25,17,14},{4,12,4,4,4,4,14},
{14,17,1,2,4,8,31},{30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
{14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},{14,17,17,15,1,1,14}};
void rect(int x,int y,int w,int h,uint16_t c){int x0=max(0,x),x1=min(W,x+w),y0=max(0,y),y1=min(H,y+h);for(int j=y0;j<y1;j++)for(int i=x0;i<x1;i++)fb[j*W+i]=c;}
void circle(int x,int y,int r,uint16_t c){for(int j=-r;j<=r;j++){int a=(int)sqrtf((float)(r*r-j*j));rect(x-a,y+j,a*2+1,1,c);}}
void roundRect(int x,int y,int w,int h,int r,uint16_t c){r=min(r,min(w,h)/2);rect(x+r,y,w-2*r,h,c);rect(x,y+r,r,h-2*r,c);rect(x+w-r,y+r,r,h-2*r,c);circle(x+r,y+r,r,c);circle(x+w-r-1,y+r,r,c);circle(x+r,y+h-r-1,r,c);circle(x+w-r-1,y+h-r-1,r,c);}
void text(int x,int y,const char *s,int scale=1,uint16_t c=WHITE){for(;*s;s++,x+=6*scale){char ch=toupper(*s);int k=ch>='A'&&ch<='Z'?ch-'A':ch>='0'&&ch<='9'?26+ch-'0':-1;if(k>=0){for(int row=0;row<7;row++)for(int col=0;col<5;col++)if(alphabet[k][row]&(16>>col))rect(x+col*scale,y+row*scale,scale,scale,c);}else if(ch=='!'){rect(x+2*scale,y,scale,5*scale,c);rect(x+2*scale,y+6*scale,scale,scale,c);}else if(ch=='.'||ch==':'){rect(x+2*scale,y+6*scale,scale,scale,c);if(ch==':')rect(x+2*scale,y+2*scale,scale,scale,c);}else if(ch=='-'){rect(x,y+3*scale,4*scale,scale,c);}else if(ch=='?'){text(x,y," ",scale,c);rect(x,y,4*scale,scale,c);rect(x+4*scale,y+scale,scale,2*scale,c);rect(x+2*scale,y+3*scale,2*scale,scale,c);rect(x+2*scale,y+4*scale,scale,scale,c);rect(x+2*scale,y+6*scale,scale,scale,c);}else if(ch=='\''){rect(x+2*scale,y,scale,2*scale,c);}}}
void center(int y,const char*s,int scale=1,uint16_t c=WHITE){text((W-(int)strlen(s)*6*scale)/2,y,s,scale,c);}
void lcdCommand(uint8_t cmd,const uint8_t *data=nullptr,int n=0){digitalWrite(DC,LOW);digitalWrite(CS,LOW);display.write(cmd);if(n){digitalWrite(DC,HIGH);display.writeBytes(data,n);}digitalWrite(CS,HIGH);}
void lcdValue(uint8_t cmd,uint8_t value){lcdCommand(cmd,&value,1);}
void flush(){display.beginTransaction(SPISettings(40000000,MSBFIRST,SPI_MODE0));uint8_t cols[]={0,0,0,239},rows[]={0,0,1,63};lcdCommand(0x2a,cols,4);lcdCommand(0x2b,rows,4);digitalWrite(DC,LOW);digitalWrite(CS,LOW);display.write(0x2c);digitalWrite(DC,HIGH);display.writePixels(fb,W*H*2);digitalWrite(CS,HIGH);display.endTransaction();}
void lcdInit(){pinMode(CS,OUTPUT);pinMode(DC,OUTPUT);pinMode(BL,OUTPUT);digitalWrite(CS,HIGH);digitalWrite(BL,LOW);display.begin(12,13,11,CS);display.beginTransaction(SPISettings(10000000,MSBFIRST,SPI_MODE0));lcdCommand(0x01);delay(150);lcdCommand(0x28);const uint8_t p1[]={0x00,0xc1,0x30},p2[]={0x64,0x03,0x12,0x81},p3[]={0x85,0x00,0x78},p4[]={0x39,0x2c,0x00,0x34,0x02},p5[]={0x00,0x00},p6[]={0x08,0x82,0x27};lcdCommand(0xcf,p1,3);lcdCommand(0xed,p2,4);lcdCommand(0xe8,p3,3);lcdCommand(0xcb,p4,5);lcdValue(0xf7,0x20);lcdCommand(0xea,p5,2);lcdValue(0xc0,0x23);lcdValue(0xc1,0x10);const uint8_t vcom[]={0x3e,0x28};lcdCommand(0xc5,vcom,2);lcdValue(0xc7,0x86);lcdValue(0x36,0x48);lcdValue(0x3a,0x55);const uint8_t fr[]={0x00,0x18};lcdCommand(0xb1,fr,2);lcdCommand(0xb6,p6,3);lcdValue(0xf2,0);lcdValue(0x26,1);lcdCommand(0x11);delay(120);lcdCommand(0x29);display.endTransaction();digitalWrite(BL,HIGH);}

bool probe(uint8_t addr){Wire.beginTransmission(addr);return Wire.endTransmission()==0;}
bool regWrite(uint8_t reg,uint8_t val){Wire.beginTransmission(0x18);Wire.write(reg);Wire.write(val);return Wire.endTransmission()==0;}
int regRead(uint8_t addr,uint8_t reg){Wire.beginTransmission(addr);Wire.write(reg);if(Wire.endTransmission(false)!=0)return -1;if(Wire.requestFrom(addr,(size_t)1)!=1)return -1;return Wire.read();}
bool codecInit(){if(!probe(0x18))return false;bool ok=regWrite(0x00,0x1f);delay(20);ok&=regWrite(0x00,0);ok&=regWrite(0x00,0x80);
// MCLK=16000*256=4096000. Slave I2S, 16-bit ADC/DAC, analog mic.
const uint8_t cfg[][2]={{0x01,0x3f},{0x02,0x00},{0x03,0x10},{0x04,0x10},{0x05,0x00},{0x06,0x03},{0x07,0x00},{0x08,0xff},{0x09,0x0c},{0x0a,0x0c},{0x0d,0x01},{0x0e,0x02},{0x12,0x00},{0x13,0x10},{0x1c,0x6a},{0x37,0x08},{0x14,0x1a},{0x16,0x04},{0x17,0xc8},{0x31,0x00},{0x32,0xbf}};
for(auto &rv:cfg)ok&=regWrite(rv[0],rv[1]);return ok;}
