
#include <MD_MAX72xx.h>
#include <SPI.h>

#define  delay_t  50  // in milliseconds

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4

//Hardware SPI Arduino UNO
// CLK Pin  > 13 SCK
// Data Pin > 11 MOSI

#define CS_PIN    10

//JoyStick
#define joyXa A0
#define joyYa A1
#define joySWa 2
int joyX;
int joyY;
int joySW;
int joyDir; //JoyStick direction 0=none, 1=down, 2=up, 3=left, 4=right

// Hardware SPI connection
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

byte sn[8][4]={
  {0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00},
  {0x00, 0x00, 0x00, 0x00}
  };

byte snak[8][32];
int x;
int y;
int bx;
int by;
int count;

unsigned int timestamp;
unsigned int bef_timestamp;

/*a számos mátrixba haladni 8 asával és minden sort egyből kiiratni (ott helybe
létre hozni a B0010000 es formátumot)*/

void setup() {
  randomSeed(analogRead(0));
  pinMode(joyXa, INPUT);
  pinMode(joyYa, INPUT);
  pinMode(joySW, INPUT_PULLUP);
  joyX=0;
  joyY=0;
  joySW=0;
  joyDir=4;

  count=1;
   
  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, 0);
  mx.clear();
  
  for(int i=0;i<8;i++){
    for(int j=0;j<32;j++){
      snak[i][j]=0;  
    }  
  }
  x=2;
  y=0;
  bx=16;
  by=16;
  snak[y][x]=1;
  
  Serial.begin(38400);
  while (!Serial);
  GetFood();
  timestamp=millis();
  bef_timestamp=millis();
}

void loop() {
  timestamp=millis();
  GetJoyStickDirection();
  if(timestamp-bef_timestamp>400){
    drawShape();
    Step();
    bef_timestamp=timestamp;
  }
  

  //delay(500);
}

void Step(){
  bx=x;
  by=y;
  if(joyDir==3){
    if(x<31){
      x++;  
    }else{
      x=0;
    }
  }else if(joyDir==4){
    if(x>0){
      x--;  
    }else{
      x=31;
    }
  }else if(joyDir==1){
    if(y<7){
      y++;  
    }else{
      y=0;
    }
  }else if(joyDir==2){
    if(y>0){
      y--;  
    }else{
      y=7;
    }
  }
  
  if(snak[y][x]==255){
    count++;
    snak[y][x]=1;
    for(int i=0;i<8;i++){
      for(int j=0;j<32;j++){
        if(snak[i][j]>1 && snak[i][j]<255){
          snak[i][j]++;
        }
      }  
    }
    snak[by][bx]=2;
    GetFood();
  }else if(snak[y][x]>0){
    for(int i=0;i<8;i++){
      for(int j=0;j<32;j++){
        snak[i][j]=0;  
      }  
    }
    x=2;
    y=0;
    bx=16;
    by=16;
    snak[y][x]=1;
    count=1;
    GetFood();
  }else{
    snak[y][x]=1;

    if(count==1){
      snak[by][bx]=0;
    }else{
      snak[by][bx]=2;
    }
    
    for(int i=0;i<8;i++){
      for(int j=0;j<32;j++){
        if(snak[i][j]>1 && snak[i][j]<count){
          snak[i][j]++;
        }else if(snak[i][j]==count && snak[i][j]!=1){
          snak[i][j]=0;
        }
      }  
    }
    
  }
  //snak[y][x]=1;
  
  
}

void GetFood(){
 byte r=0;
 while(r==0){ 
  int rX=random(0,31);
  int rY=random(0,7);
  if(snak[rY][rX]==0){
    snak[rY][rX]=255;
    r=1;
  }
 }
}

void GetJoyStickDirection(){
  joyX=analogRead(joyXa);
  joyY=analogRead(joyYa);
  joySW=digitalRead(joySWa);
  
  /*if(joySW==0 && joyDir!=0){
      joyDir=0;
  }*/
  if(joyX>900 && joyY%500<250 && joyDir!=2){
    joyDir=1;  
  }else if(joyX<200 && joyY%500<250 && joyDir!=1){
    joyDir=2;  
  }else if(joyY>900 && joyX%500<250 && joyDir!=4){
    joyDir=3;  
  }if(joyY<200 && joyX%500<250 && joyDir!=3){
    joyDir=4;  
  } 
}

void GetBform(){
  int i=0;
  int j=0;
  int k=0;
  for(i=0;i<8;i++){
    for(j=0;j<4;j++){
      byte aa=0;
      int aaa=0;
      for(k=0;k<8;k++){
        if(snak[i][j*8+k]>0){
          aa+=pow(2,k);
          if(k>1 && aaa==0){ //a random pow 2 fölött pl 2^3=7,999 ezért 1 et hozzá kell adni
            aa+=1;
          }
        }
      }
      
      /*if(aa!=0){
        Serial.println(aa,DEC); 
      }*/
      
      sn[i][3-j]=aa;
    }  
  }
  
}

void drawShape() {
  GetBform();
  int i=0;
  int j=0;
  
  for(i=0;i<8;i++){
    for(j=0;j<4;j++){
        mx.setRow(3-j, i, sn[i][j]);
    }  
  }

}
