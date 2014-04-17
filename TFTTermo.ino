//#include <TFTv2.h>
#include <SPI.h>
#include <NRF24.h>

#include <I2C_SoftwareLibrary.h>

#include "RTClibSS.h"
#include "TFT_ILI9341.h"
#include "hist.h"

//-------- #define DS1307_ADDRESS 0x68


// F5529 PINOUT
// SW I2C
#define SW_SCL_PIN P2_4 ///< pin for SCL
#define SW_SDA_PIN P2_3 ///< pin for SDA

#define NRF_CE_PIN P3_7
#define NRF_SS_CSN_PIN P1_4
#define NRF_IRQ_PIN P2_0

// SCK=P3_2, MI=P3_1, MO=P3_0, IRQ=P2_0
#define TFT_CS_PIN P2_2
#define TFT_DC_PIN P1_6
#define TFT_BL_PIN P2_5
  
#define BUTTON_1  P2_1
#define BUTTON_2  P1_1

/*
// 5523 PINOUT
// SW I2C
#define SW_SCL_PIN P2_4 ///< pin for SCL
#define SW_SDA_PIN P2_3 ///< pin for SDA

#define NRF_CE_PIN P2_1
#define NRF_SS_CSN_PIN P1_4
#define NRF_IRQ_PIN P2_0

// SCK=P1_5, MI=P1_6, MO=P1_7

#define TFT_CS_PIN P2_2
#define TFT_DC_PIN P1_0
#define TFT_BL_PIN P2_5
  
#define BUTTON_1  P2_1
#define BUTTON_2  P1_1
*/

SoftwareWire Wire(SW_SDA_PIN, SW_SCL_PIN); ///< Instantiate SoftwareWire
RTC_DS1307_SS RTC;

NRF24 nrf24(NRF_CE_PIN, NRF_SS_CSN_PIN);

struct wt_msg {
  uint8_t msgt;
  uint8_t sid;
  int16_t temp;
  int16_t vcc;
}; 

#define WS_CHAN  1
#define WS_MSG_TEMP  0xFE

const char *addr="wserv";

wt_msg msg = {0xFF, 0xFF, 0xFFFF, 0xFFFF};
uint8_t err=0;
volatile uint8_t rf_read=0;

unsigned long rf_millis=0;

uint8_t alarms=0;
uint16_t alarm_val=0;
uint8_t alarm_cnt[5]; 
uint8_t alarm_cnt_na=0;
uint16_t msgcnt=0;

#define WS_ALR_WFAIL  0x1
#define WS_ALR_BADMSG 0x2
#define WS_ALR_BAD_TEMP 0x4
#define WS_ALR_TO     0x8
#define WS_ALR_LOWVCC 0x16

#define WS_ALR_WFAIL_IDX  0
#define WS_ALR_BADMSG_IDX 1
#define WS_ALR_BAD_TEMP_IDX 2
#define WS_ALR_TO_IDX     3
#define WS_ALR_LOWVCC_IDX 4
#define WS_ALR_LAST_IDX 4

#define WS_SENS_TIMEOUT (10L*60*1000)

#define DS18_MEAS_FAIL	9999  // Temporarily!

// ************************ HIST

int16_t last_tmp=TH_NODATA, prev_tmp=TH_NODATA;
int16_t last_vcc=0, prev_vcc=-1;
unsigned long last_millis_temp=0;

TempHistory mHist;

// ************************ UI

#define WS_NUILEV 6
#define WS_UI_MAIN  0
#define WS_UI_CHART 1
#define WS_UI_CHART60 2
/*
#define WS_UI_CHART30 5
#define WS_UI_CHART60 6
#define WS_UI_AVG   7
//#define WS_UI_AVG_2 5
//#define WS_UI_ACC   6
*/
#define WS_UI_STAT 3
#define WS_UI_SET   4
#define WS_UI_HIST  5

const char *lnames[WS_NUILEV] = {"main", "chart", "chr60", "stat", "set", "hist"};

#define WS_CHAR_UP 128
#define WS_CHAR_DN 129
#define WS_CHAR_TIME_SZ  4
#define WS_CHAR_TIME_SET_SZ  6

const int chart_width=240;
const int chart_height=204;
const int chart_top=19;

#define WS_BUT_NONE  0x00
#define WS_BUT_PRES  0x01

unsigned long mbloff;
unsigned long mdisp;
unsigned long mui;
uint8_t ledon=0;
uint8_t bt1cnt=0, bt2cnt=0;
uint8_t uilev=0;
uint8_t editmode=0;
const int WS_BL_TO=20000;

long dif=0;

char buf[32];

unsigned long interval(unsigned long prev) {
  unsigned long ms=millis();
  if(ms>prev) return ms-prev;
  return (~prev)+ms;
}  

void setup()
{
  delay(100);
  pinMode(RED_LED, OUTPUT);
  pinMode(GREEN_LED, OUTPUT);
  digitalWrite(RED_LED, LOW);
  digitalWrite(GREEN_LED, LOW);
  pinMode(BUTTON_1, INPUT_PULLUP);
  pinMode(BUTTON_2, INPUT_PULLUP);
    
  Tft.TFTinit(TFT_CS_PIN, TFT_DC_PIN, TFT_BL_PIN); 
  Tft.setBl(true);
  Tft.setOrientation(LCD_LANDSCAPE);
  dispStat("INIT I2C");
    
  Wire.begin(); // I2C Init
  RTC.begin();
  
  dispStat("INIT NRF");
  err = radioSetup();
  pinMode(NRF_IRQ_PIN, INPUT);  
  attachInterrupt(NRF_IRQ_PIN, radioIRQ, FALLING); 
  
  if(!err) {
    dispStat("INIT OK.");  
    digitalWrite(RED_LED, HIGH); delay(1000); digitalWrite(RED_LED, LOW);
  }
  else {
    dispErr();   
    for(int i=0; i<err; i++) {
      digitalWrite(RED_LED, HIGH); delay(500); digitalWrite(RED_LED, LOW);
    }
  }

 if(digitalRead(BUTTON_1)==LOW) {
   dispStat("TEST MOD");
 }

 mHist.init();
 
 mdisp=mui=millis();
 
 updateScreen(true);
}

void loop()
{ 
  if(rf_read) { 
   rf_read=0;
   err=radioRead();  
   if(err) dispErr();
   else {
     updateScreen(false);
     dispStat("READ OK.");
   }
   Tft.drawString("R",0,180,1, err ? RED : GREEN, BLACK, true);
   digitalWrite(err ? RED_LED : GREEN_LED, HIGH);
   ledon=1;
  }

 unsigned long ms=millis(); 
 if(ms-mui>50 || ms<mui) { // UI cycle
   mui=ms;
   if(ledon && ms-rf_millis>100) { //blink
      Tft.drawString(" ",0,180,1, BLUE, BLACK, true);
      digitalWrite(RED_LED, LOW);
      digitalWrite(GREEN_LED, LOW);
      ledon=0;
   }
      
   if(uilev>0 && mdisp>mbloff && !editmode) {  // dimmer
     uilev=0;
     updateScreen(true);
   }
    
   if(digitalRead(BUTTON_1)==LOW) {
     if(!bt1cnt) bt1cnt=1;
     else if(bt1cnt<100) bt1cnt++;
   } 
   else if(bt1cnt) {
     if(bt1cnt<2) {;} // debounce
     else if(bt1cnt<20) processShortClick(); 
     else processLongClick();
     bt1cnt=0;
   }
   
   if(digitalRead(BUTTON_2)==LOW) {
     if(!bt2cnt) bt2cnt=1;
     else if(bt2cnt<100) bt2cnt++;
   } else 
   if(bt2cnt) {
     if(bt2cnt<2) {;} // debounce
     else if(bt2cnt<20) processShortRightClick();
     else { ; } // longclick
     bt2cnt=0;
   }
     
   if(ms-mdisp > 500 || mdisp>ms) { // 0.5 sec screen update
     mdisp=ms;   
     if(!(alarms&WS_ALR_TO) && millis()-rf_millis>WS_SENS_TIMEOUT) { //!!!
       alarms |= WS_ALR_TO;
       alarm_val=(millis()-rf_millis)/60000;
       alarm_cnt[WS_ALR_TO_IDX]++;
       updateScreen(false);
     }
     updateScreenTime(false);       
   }  
 } // UI cycle
}

void processShortClick() {
  if(!editmode) { 
    mbloff=millis()+WS_BL_TO;
    uilev=(uilev+1)%WS_NUILEV;
    updateScreen(true);
  }
  else {
    if(uilev==WS_UI_SET) {
      dispStat("EDIT MOV");
      Tft.drawRectangle(FONT_SPACE*WS_CHAR_TIME_SET_SZ*(editmode<3?editmode-1:editmode), 40, FONT_SPACE*WS_CHAR_TIME_SET_SZ, FONT_Y*WS_CHAR_TIME_SET_SZ, BLACK);  
      editmode++;
      if(editmode>4) editmode=1;
      Tft.drawRectangle(FONT_SPACE*WS_CHAR_TIME_SET_SZ*(editmode<3?editmode-1:editmode), 40, FONT_SPACE*WS_CHAR_TIME_SET_SZ, FONT_Y*WS_CHAR_TIME_SET_SZ, WHITE);
    }
  }
}

void processLongClick() {
  if(uilev==WS_UI_SET) {
    if(!editmode) {
      dispStat("EDIT  ON");
      editmode=1;
      Tft.drawRectangle(0, 40, FONT_SPACE*WS_CHAR_TIME_SET_SZ, FONT_Y*WS_CHAR_TIME_SET_SZ, WHITE);
    } else {
      dispStat("EDIT OFF");
      timeStore();
      editmode=0;      
      Tft.drawRectangle(FONT_SPACE*WS_CHAR_TIME_SET_SZ*(editmode<3?editmode-1:editmode), 40, FONT_SPACE*WS_CHAR_TIME_SET_SZ, FONT_Y*WS_CHAR_TIME_SET_SZ, BLACK);        
      dispStat("TIME STOR");
    }
  }
}

void processShortRightClick() {
  if(!editmode) return;
  if(uilev!=WS_UI_SET) return;
  timeUp(editmode-1, WS_CHAR_TIME_SET_SZ);
  Tft.drawRectangle(FONT_SPACE*WS_CHAR_TIME_SET_SZ*(editmode<3?editmode-1:editmode), 40, FONT_SPACE*WS_CHAR_TIME_SET_SZ, FONT_Y*WS_CHAR_TIME_SET_SZ, WHITE);
}       

void updateScreen(bool refresh) {
  if(refresh) {
    Tft.fillScreen();
    Tft.drawString(lnames[uilev], 240, 0 ,2, WHITE, BLACK, true);
  }
  switch(uilev) {
    case WS_UI_MAIN: {
     uint16_t ent, frc;     
     if((prev_tmp!=last_tmp || refresh) && last_tmp!=TH_NODATA) {
       int16_t diff = mHist.getDiff(last_tmp, 1);
       Tft.drawString(printTemp(buf, last_tmp, 1), 0, 80,6, alarms&WS_ALR_TO ? RED : GREEN, BLACK, true);
       Tft.drawChar( diff==0? ' ': diff>0 ? WS_CHAR_UP : WS_CHAR_DN, FONT_SPACE*6*7, 80, 4, YELLOW, BLACK, true);
     }
     if(prev_vcc!=last_vcc || refresh) {
       prev_vcc=last_vcc;
       ent=last_vcc/100; frc=last_vcc%100; 
       sprintf(buf, " v=%d.%02.2d", ent, frc);          
       Tft.drawString(buf,200,211,2, GREEN, BLACK, true);     
     }
     if(refresh) updateScreenTime(true);  
    }    
    break;
    
    case WS_UI_HIST: {
      printHist(1);
    }
    break;
    
    case WS_UI_CHART: {
      chartHist(1);
    }
    break; 
    case WS_UI_CHART60: {
      chartHist60(1);
    }
    break;     
    case WS_UI_STAT: {      
      Tft.drawString("UPT: ",0,0,2, GREEN, BLACK, true); dispTimeout(millis(), true, 64, 0, 2);
      sprintf(buf, "CNT: %d", (int)msgcnt);
      Tft.drawString(buf,0,16, 2, GREEN, BLACK, true); 
      
      for(int i=0; i<=WS_ALR_LAST_IDX; i++) {
        sprintf(buf, "A%d: %d", i, (int)alarm_cnt[i]);
        Tft.drawString(buf,0, 32+16*i, 2, GREEN, BLACK, true); 
      }            
    }
    break;
    case WS_UI_SET: {  
      updateScreenTime(true);  
    }
    break;
    default: break;
  }
}

void updateScreenTime(bool reset) {
  DateTime now = RTC.now();
  if(uilev==WS_UI_MAIN) {
    //printDate(reset, WS_CHAR_TIME_SZ);   
    printTime(&now, reset, 0, 40, WS_CHAR_TIME_SZ, true);   
    dispTimeout(millis()-last_millis_temp, reset, 0, 160, 2);
  } else if(uilev==WS_UI_SET) {
      if(editmode) {
        ;
      }
      //else printDate(reset, WS_CHAR_TIME_SET_SZ);   
      else printTime(&now, reset, 0, 40, WS_CHAR_TIME_SET_SZ, false);   
  }
}

void dispTimeout(unsigned long dt, bool reset, int x, int y, int sz) {
  static byte p_to[3]={-1,-1,-1};
  static byte p_days=-1;
  byte tmp[3];
  
  if(reset) p_days=-1;
   
  unsigned long ts=dt/1000;
  tmp[2]=ts%60; tmp[1]=(ts/60)%60; tmp[0]=(ts/3600)%24;
  uint8_t days=ts/3600/24;  
  if(days>0 && days!=p_days) {
    sprintf(buf, "> %d days", days);
    Tft.drawString(buf,x,y,sz, GREEN, BLACK, true);
    p_days=days;
  }
  else {
     disp_dig(reset, 3, 2, tmp, p_to, x, y, sz, GREEN, ':', 0);
  }     
}

char *printTemp(char *buf, int16_t disptemp, int8_t showg) {
  char s='+';
  if(disptemp<0) {
    s='-';
    disptemp=-disptemp;
  } else if(disptemp==0) s=' ';
  if(showg)
    sprintf(buf, "%c%d.%d%cc", s, disptemp/10, disptemp%10, (uint8_t)127);
  else  
    sprintf(buf, "%c%d.%d", s, disptemp/10, disptemp%10);
  //-xx.xgC = 7 chars
  while(strlen(buf)<7) strcat(buf, " "); // UGLY 
  return buf;
} 

static byte p_date[3]={-1,-1,-1};
static byte p_time[3]={-1,-1,-1};

void printTime(DateTime *pDT, bool reset, int x, int y, int sz, bool blinkd){
  byte tmp[3];  
  char sbuf[8];  
  tmp[0]=pDT->hour(); tmp[1]=pDT->minute(); tmp[2]=pDT->second();
  disp_dig(reset, 2, 2, tmp, p_time, x, y, sz, YELLOW, (!blinkd || p_time[2]%2) ? ':' : ' ', tmp[2]!=p_time[2]);
  p_time[2]=tmp[2];
  /*
  tmp[0]=now.day(); tmp[1]=now.month(); tmp[2]=now.year()-2000;
  disp_dig(reset, 3, 2, tmp, p_date, 0, 0, 2, RED, '/', false);
  */
}

void timeUp(int dig, int sz) {
  int ig=dig/2;
  int id=(dig+1)%2; 
  byte val=p_time[ig];
  byte maxv[3]={24, 60, 60};
  if(id) { val+=10; if(val>maxv[ig]) val=val%10; /*val=maxv[ig];*/}  
  else { val=(val/10)*10+((val%10)+1)%10; if(val>maxv[ig]) val=(val/10)*10;} 
  byte tmp[3];
  memcpy(tmp, p_time, 3);
  tmp[ig]=val;
  disp_dig(false, 2, 2, tmp, p_time, 0, 40, sz, YELLOW, ':', true);
}

void timeStore() {
  DateTime now=RTC.now();
  DateTime set(now.year(), now.month(), now.day(), p_time[0], p_time[1], 0);
  RTC.adjust(set); 
}

void disp_dig(byte redraw, byte ngrp, byte nsym, byte *data, byte *p_data, int x, int y, int sz, int color, char delim, byte drdrm) {
  char sbuf[8];
  int posX=x;
  for(byte igrp=0; igrp<ngrp; igrp++) {  
    if(igrp) {
      if(redraw || drdrm) 
        Tft.drawChar(delim, posX, y, sz, color, BLACK, true);  
      posX+=FONT_SPACE*sz;
    }
    for(byte isym=0; isym<nsym; isym++) {
       byte div10=(isym==0?10:1);
       if(redraw || (data[igrp]/div10)%10!=(p_data[igrp]/div10)%10)
         Tft.drawChar('0'+(data[igrp]/div10)%10,posX,y,sz, color, BLACK, true); 
       posX+=FONT_SPACE*sz;
    }
    p_data[igrp]=data[igrp];
  }    
}

void dispErr() {
  sprintf(buf, "ERR=%04.4d", err);
  Tft.drawString(buf,0,211,2, RED, BLACK, true);
}

void dispStat(char *pbuf) {
  Tft.drawString(pbuf,0,211,2, GREEN, BLACK, true);
}

/****************** REPORTS ****************/

void printHist(uint8_t sid) {    
  mHist.iterBegin();  
  if(!mHist.movePrev()) {
    Tft.drawString("NO DATA", 40,100, 6, RED, BLACK, false);
    return;
  }
  uint8_t vpos=0;
  do {
    Tft.drawString(printTemp(buf, mHist.getPrev()->temp, 0),0,vpos, 1, GREEN, BLACK, false);
    sprintf(buf, "-%d min", mHist.getPrevMinsBefore()); 
    Tft.drawString(buf,100, vpos, 1, GREEN, BLACK, false);
    vpos+=FONT_SPACE+2;
  } while(mHist.movePrev() && vpos<230);
  
}

void chartHist(uint8_t sid) { // this is not a correct time-chart...just a sequence of measurements
  
  int16_t mint, maxt;
  mHist.iterBegin();  
  if(!mHist.movePrev()) {
    Tft.drawString("NO DATA", 40,100, 6, RED, BLACK, false);
    return;
  }
    
  maxt=mint=mHist.getPrev()->temp;
  do {
    int16_t t = mHist.getPrev()->temp;
    if(t>maxt) maxt=t;
    if(t<mint) mint=t;
  } while(mHist.movePrev());
  
  uint16_t mbefore=mHist.getPrevMinsBefore(); // minutes passed after the earliest measurement
  sprintf(buf, "%d mins", (int)mbefore); Tft.drawString(buf, 160, 0, 2, GREEN, BLACK, false);
  
  int16_t tdiff=prepChart(&mint, &maxt);
  
  int16_t y0, x0, xr;  
  const int chart_xstep_denom=15;
  const int chart_xstep_nom=2;
 
  xr=mbefore*chart_xstep_nom/chart_xstep_denom;
  if(xr>=chart_width) xr=chart_width-1;
  
  Tft.drawStraightDashedLine(LCD_VERTICAL, xr, chart_top, chart_height,BLUE,BLACK, 0x0f); // now line
  DateTime now = RTC.now();
  printTime(&now, true, xr, 224, 2, false);   

  int cnt=0;
  x0=y0=0;
  mHist.iterBegin();
  mHist.movePrev();
  do {
    int y1=(int32_t)(maxt-mHist.getPrev()->temp)*chart_height/tdiff;
    int x1=xr-(mHist.getPrevMinsBefore())*chart_xstep_nom/chart_xstep_denom;
    if(cnt)  {
      /*
      if(cnt<3) {
        sprintf(buf, "%d -> %d", x1, x0); Tft.drawString(buf, 140, FONT_Y*2*cnt, 2, RED, BLACK, false);
      }
      */
     if(x1>0 && x0>0) 
       Tft.drawLineThick(x1,chart_top+y1,x0,chart_top+y0, WHITE, 3);
    }
    y0=y1;
    x0=x1;
    cnt++;
  } while(mHist.movePrev() && x0>0);    
  
 if(xr>36) {
   DateTime start=DateTime(now.unixtime()-mHist.getPrevMinsBefore()*60);
   printTime(&start, true, 0, 224, 2, false);   
 }  
}

void chartHist60(uint8_t sid) 
{  
  const int DUR_24=24;
  const int DUR_MIN=60;
  int16_t alldur=DUR_24*DUR_MIN;
  int32_t acc[DUR_24];
  uint8_t cnt[DUR_24];
  
  int16_t mint, maxt;
  uint16_t mbefore;
 
  mHist.iterBegin();  
  if(!mHist.movePrev()) {
    Tft.drawString("NO DATA", 40,100, 6, RED, BLACK, false);
    return;
  }     
  for(int i=0; i<DUR_24; i++) {
      cnt[i]=0;
      acc[i]=0;   
  }
  maxt=mint=mHist.getPrev()->temp;  
  do {
    uint8_t islot=mHist.getPrevMinsBefore()/DUR_MIN;
    int16_t t = mHist.getPrev()->temp;    
    if(islot>=0 && islot<DUR_24) {
      cnt[islot]++;
      acc[islot]+=t;
    }    
    if(t>maxt) maxt=t;
    if(t<mint) mint=t;    
  } while(mHist.movePrev());

  sprintf(buf, "%d mins", (int)mbefore); Tft.drawString(buf, 160, 0, 2, GREEN, BLACK, false); 
  int16_t tdiff=prepChart(&mint, &maxt);
 
  int xstep = chart_width/DUR_24;
  int y_z=(int32_t)(maxt-0)*chart_height/tdiff; // from top
  int x=0; 

  //int vpos = FONT_Y*2;
  
  for(int i=0; i<DUR_24; i++) {
    // should be up from zerolinr if positive, down if negative
    int islot=DUR_24-1-i; // oldest at left
    if(cnt[islot]) {
      int val=acc[islot]/cnt[islot];
      int y0=0;
      int h;
      if(val<0) {
        if(maxt<0) { y0=0; h=(int32_t)(maxt-val)*chart_height/tdiff; } else { y0=y_z; h=(int32_t)(-val)*chart_height/tdiff; }
      }
      else {
        if(mint>0) { y0=chart_height; h=(int32_t)(val-mint)*chart_height/tdiff;} else { y0=y_z; h=(int32_t)(val)*chart_height/tdiff; }
        y0-=h;
      }

/*      
      sprintf(buf, "* %d %d ", islot, cnt[islot]);
      Tft.drawString(buf, 0, vpos, 2, GREEN, BLACK, false);
      Tft.drawString(printTemp(buf, val, 0), 60, vpos, 2, GREEN, BLACK, false);
      sprintf(buf, "%d %d ", y0, h);
      Tft.drawString(buf, 124, vpos, 2, GREEN, BLACK, false);
      vpos+=FONT_Y*2;  
*/
      Tft.fillRectangle(x+1, chart_top+y0, xstep-2, h, WHITE);
    }
    x+=xstep;
  }
  
  DateTime now = RTC.now();
  printTime(&now, true, chart_width, 224, 2, false);   

}

int16_t  adjMinMax(int16_t *pmint, int16_t *pmaxt) {
  if(*pmint%50) {
    if(*pmint>0) *pmint=(*pmint/50)*50;
     else *pmint=(*pmint/50-1)*50;
  }  
  if(*pmaxt%50) {
     if(*pmaxt>0) *pmaxt=(*pmaxt/50+1)*50;
     else *pmaxt=(*pmaxt/50)*50;
  }  
  if(*pmaxt==*pmint) {*pmint-=50; *pmaxt+=50;}  
  return *pmaxt-*pmint;
}

int16_t prepChart(int16_t *pmint, int16_t *pmaxt) {
  Tft.drawString(printTemp(buf, *pmint, 0), 0, 0, 2, GREEN, BLACK, false);
  Tft.drawString(printTemp(buf, *pmaxt, 0), 80, 0, 2, GREEN, BLACK, false);

  int16_t tdiff=adjMinMax(pmint, pmaxt);
  
  Tft.drawRectangle(0, chart_top, chart_width, chart_height, WHITE); 
  int16_t y0;  
  y0=(*pmaxt-*pmint)>100 ? 50 : 10;  
  for(int16_t ig=*pmint; ig<=*pmaxt; ig+=y0) { // degree lines
     int16_t yl=chart_top+(int32_t)(*pmaxt-ig)*chart_height/(*pmaxt==*pmint? 1 : *pmaxt-*pmint);
     if(ig>*pmint && ig<*pmaxt) Tft.drawStraightDashedLine(LCD_HORIZONTAL, 0, yl, chart_width, ig==0? BLUE : GREEN, BLACK, 0x0f);
     Tft.drawString(printTemp(buf, ig, 0), chart_width, yl-FONT_Y, 2, GREEN, BLACK, false);
  }
  
  return tdiff;
}

/****************** HIST ****************/

uint8_t addHistAcc(struct wt_msg *pmsg) {
  prev_vcc=last_vcc;
  last_vcc=pmsg->vcc;
  if(DS18_MEAS_FAIL==pmsg->temp) {
    alarms |= WS_ALR_BAD_TEMP; alarm_val=err; alarm_cnt[WS_ALR_BAD_TEMP_IDX]++;
    return 1;
  }
  msgcnt++;
  prev_tmp=last_tmp;
  last_tmp=pmsg->temp; 
  last_millis_temp=millis();
  mHist.addAcc(pmsg->temp);
  return 0;
}

/****************** RADIO ****************/

uint8_t radioSetup() {
  if (!nrf24.init())
    err=1;
  else if (!nrf24.setChannel(WS_CHAN))
    err=2;
  else if (!nrf24.setThisAddress((uint8_t*)addr, strlen(addr)))
    err=3;
  else if (!nrf24.setPayloadSize(sizeof(wt_msg)))
    err=4;
  //else if (!nrf24.setRF(NRF24::NRF24DataRate2Mbps, NRF24::NRF24TransmitPower0dBm))
  else if (!nrf24.setRF(NRF24::NRF24DataRate250kbps, NRF24::NRF24TransmitPower0dBm)) 
    err=5;          
  nrf24.spiWriteRegister(NRF24_REG_00_CONFIG, nrf24.spiReadRegister(NRF24_REG_00_CONFIG)|NRF24_MASK_TX_DS|NRF24_MASK_MAX_RT); // only DR interrupt
  /*
  if(err) {alarms |= WS_ALR_WFAIL; alarm_val=err; alarm_cnt[WS_ALR_WFAIL_IDX]++;}
  */  
  return err;
}

uint8_t radioRead() {
  rf_millis=millis();
  err = 0;
  uint8_t len=sizeof(msg); 
  if(!nrf24.available()) { 
   err=6; alarms |= WS_ALR_WFAIL; alarm_cnt[WS_ALR_WFAIL_IDX]++;
   if(++alarm_cnt_na>=3) {
     radioSetup();
     dispStat("RST RAD");
   }
   return err;
  }
  alarm_cnt_na=0;
  if(!nrf24.recv((uint8_t*)&msg, &len)) { err=7; alarms |= WS_ALR_WFAIL; alarm_cnt[WS_ALR_WFAIL_IDX]++; return err;}
  if(WS_MSG_TEMP != msg.msgt) { err=8; alarms |= WS_ALR_BADMSG; alarm_cnt[WS_ALR_BADMSG_IDX]++; return err;}
  if(0==addHistAcc(&msg)) alarms = 0; // at the moment
  return err;
}

void radioIRQ() {
  rf_read=1; 
}
