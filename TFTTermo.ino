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

unsigned long rf_millis=0; // get rid off

uint8_t alarms=0;
//uint16_t alarm_val=0;
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

#define WS_SENS_TIMEOUT_M 10 // minutes

#define DS18_MEAS_FAIL	9999  // Temporarily!

// ************************ HIST
#define TH_NODATA 0x0FFF
int16_t last_tmp=TH_NODATA, prev_tmp=TH_NODATA;
int16_t last_vcc=0, prev_vcc=-1;
unsigned long last_millis_temp=0;

TempHistory mHist;

// ************************ UI

#define WS_NUILEV 7
#define WS_UI_MAIN  0
#define WS_UI_CHART 1
#define WS_UI_CHART60 2
#define WS_UI_CHART_VCC 3
#define WS_UI_STAT 4
#define WS_UI_SET   5
#define WS_UI_HIST  6

const char *lnames[] = {"main", "chart", "chr60", "vcc", "stat", "set", "hist"};

#define WS_CHAR_UP 128
#define WS_CHAR_DN 129
#define WS_CHAR_TIME_SZ  4
#define WS_CHAR_TIME_SET_SZ  6

#define WS_UI_CYCLE 50 
#define WS_DISP_CNT    10  // in UI_CYCLEs (=0.5s)
#define WS_INACT_CNT   40  // in DISP_CYCLEs (= 20s)

#define WS_CHART_NLEV 4

#define WS_BUT_NONE  0x00
#define WS_BUT_PRES  0x01

#define CHART_WIDTH 240
#define CHART_HEIGHT 204
#define CHART_TOP 19

unsigned long mui;

uint8_t inact_cnt=0;
uint8_t disp_cnt=0;

uint8_t ledon=0;
uint8_t editmode=0;

uint8_t bt1cnt=0, bt2cnt=0;
uint8_t uilev=0;
uint8_t pageidx=0;
uint8_t chrt=TH_HIST_VAL_T;

uint32_t rts=0; 
char buf[32];

uint8_t processClick(uint8_t id, uint8_t cnt, void (*pShort)(), void (*pLong)());

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

 mHist.init(); 
 mui=millis();
 rts = RTC.now().unixtime();
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
 if(ms-mui>WS_UI_CYCLE || ms<mui) { // UI cycle
   mui=ms;
   if(ledon && ms-rf_millis>100) { //blink
      Tft.drawString(" ",0,180,1, BLUE, BLACK, true);
      digitalWrite(RED_LED, LOW);
      digitalWrite(GREEN_LED, LOW);
      ledon=0;
   }
      
   if(uilev>0 && !editmode && !inact_cnt) {  // back to main screen after user inactivity timeout
     uilev=0;
     updateScreen(true);
   }
     
   bt1cnt=processClick(BUTTON_1, bt1cnt, processShortClick, processLongClick); 
   bt2cnt=processClick(BUTTON_2, bt2cnt, processShortRightClick, NULL); 
   
   if(++disp_cnt>=WS_DISP_CNT) { // 0.5 sec screen update
     disp_cnt=0;   
     if(inact_cnt) inact_cnt--;
     if(!(alarms&WS_ALR_TO) && TempHistory::interval_m(last_millis_temp/60000L)>WS_SENS_TIMEOUT_M) { // Alarm condition on no-data timeout          
       alarms |= WS_ALR_TO;
       alarm_cnt[WS_ALR_TO_IDX]++;
       updateScreen(true);       
     }
     updateScreenTime(false);       
   }  
 } // UI cycle
}

uint8_t processClick(uint8_t id, uint8_t cnt, void (*pShort)(), void (*pLong)()) {
   if(digitalRead(id)==LOW) {
     if(!cnt) cnt=1;
     else if(cnt<100) cnt++;
   } 
   else if(cnt) {
     if(cnt<2) {;} // debounce
     else {
        inact_cnt=WS_INACT_CNT;
        if(cnt<20) { if(pShort) (*pShort)(); }
        else { if(pLong) (*pLong)(); }
      }
     cnt=0;
   }
  return cnt;
}


void processShortClick() {
  if(!editmode) { 
    uilev=(uilev+1)%WS_NUILEV;
    updateScreen(true);
  }
  else {
    if(uilev==WS_UI_SET) {
      dispStat("EDIT MOV");
      hiLightDigit(BLACK);
      if(++editmode>4) editmode=1;
      hiLightDigit(WHITE);
    }
  }
}

void processLongClick() {
  if(uilev==WS_UI_SET) {
    if(!editmode) {
      dispStat("EDIT  ON");
      editmode=1;
      hiLightDigit(WHITE);
    } else {
      dispStat("EDIT OFF");
      timeStore();
      hiLightDigit(BLACK);
      editmode=0;
      dispStat("TIME STOR");
    }
  }
}

void processShortRightClick() {
  if(uilev==WS_UI_CHART || uilev==WS_UI_CHART_VCC) {
    if(++pageidx>=WS_CHART_NLEV) pageidx=0;
    Tft.fillScreen();
    chartHist(1, pageidx, chrt);
    return;
  }
  if(uilev==WS_UI_HIST) {
    Tft.fillScreen();
    pageidx=printHist(1, pageidx);
    return;
  }
  if(uilev!=WS_UI_SET) return;
  if(!editmode) return;
  timeUp(editmode-1, WS_CHAR_TIME_SET_SZ);
  hiLightDigit(WHITE);
}       

void updateScreen(bool refresh) {
  if(refresh) {
    Tft.fillScreen();
    Tft.drawString(lnames[uilev], 240, 0 ,2, WHITE, BLACK, true);
  }
  
  line_init(GREEN, 2);
  
  switch(uilev) {
    case WS_UI_MAIN: {
     if((prev_tmp!=last_tmp || refresh) && last_tmp!=TH_NODATA) { // prev_tmp=last_tmp ????
       prev_tmp=last_tmp;
       int16_t diff = mHist.getDiff(last_tmp, 1);
       printTemp(last_tmp); strcat(buf, "\177c "); //"degree(DEC=127)C "
       Tft.drawString(buf, 0, 80,6, alarms&WS_ALR_TO ? RED : GREEN, BLACK, true);
       Tft.drawChar( diff==0? ' ': diff>0 ? WS_CHAR_UP : WS_CHAR_DN, FONT_SPACE*6*7, 80, 4, YELLOW, BLACK, true);
     }
     if(prev_vcc!=last_vcc || refresh) {
       prev_vcc=last_vcc;
       printVcc(last_vcc); strcat(buf, "v");
       line_print_at(buf, 200, 211);
     }
     if(refresh) updateScreenTime(true);  
     }    
     break;
    
    case WS_UI_HIST: 
      pageidx=printHist(1, 0);
      break;
    
    case WS_UI_CHART: {
      pageidx=0;
      chrt=TH_HIST_VAL_T;
      chartHist(1, pageidx, chrt);
      }
      break; 
    case WS_UI_CHART60: 
      chartHist60(1);
      break;
    case WS_UI_CHART_VCC: {
      pageidx=0;
      chrt=TH_HIST_VAL_V;
      chartHist(1, pageidx, chrt);
      }
      break;     
    case WS_UI_STAT: {      
      uint32_t rtdur = RTC.now().unixtime()-rts;
      //line_init(GREEN, 2);      
      dispTimeout(millis()/1000, true, 64, line_getpos(), 2); line_print("UPT: ");
      dispTimeout(rtdur, true, 64, line_getpos(), 2); line_print("RTT: ");
      mHist.iterBegin();  
      while(mHist.movePrev());
      dispTimeout((int)mHist.getPrevMinsBefore()*60, true, 64, line_getpos(), 2); line_print("DUR: ");
      sprintf(buf, "CNT=%d HSZ=%d", (int)msgcnt, (int)mHist.getSz()); line_print(buf);
      sprintf(buf, "HP=%d TP=%d", (int)mHist._getHeadPtr(), (int)mHist._getTailPtr()); line_print(buf);
      sprintf(buf, "SHA1=%d LHAP1=%d", (int)mHist._getSince1HAcc(), (int)mHist._getLast1HAccPtr()); line_print(buf);
      sprintf(buf, "SHA3=%d LHAP3=%d", (int)mHist._getSince3HAcc(), (int)mHist._getLast3HAccPtr()); line_print(buf);
      
      for(int i=0; i<=WS_ALR_LAST_IDX; i++) sprintf(buf, "A%d: %d", i, (int)alarm_cnt[i]); line_print(buf);      
      }
      break;
    case WS_UI_SET: 
      updateScreenTime(true);  
      break;
    default: break;
  }
}

void hiLightDigit(uint16_t color) {
  uint8_t d=(editmode>0 && editmode<3)?editmode-1:editmode;
  Tft.drawRectangle(FONT_SPACE*WS_CHAR_TIME_SET_SZ*d, 40, FONT_SPACE*WS_CHAR_TIME_SET_SZ, FONT_Y*WS_CHAR_TIME_SET_SZ, color);
}

void updateScreenTime(bool reset) {
  uint8_t sz=0;
  if(uilev==WS_UI_MAIN) {
    sz=WS_CHAR_TIME_SZ;
    dispTimeout((millis()-last_millis_temp)/1000, reset, 0, 160, 2);
  } else if(uilev==WS_UI_SET) {
      if(!editmode) sz=WS_CHAR_TIME_SET_SZ;   
  }
  if(sz) {
    DateTime now = RTC.now();
    printTime(now, reset, 0, 40, sz, true, false);   
  }
}

char *printTemp(int16_t disptemp) {
  char s='+';
  if(disptemp<0) {
    s='-';
    disptemp=-disptemp;
  } else if(disptemp==0) s=' ';
  sprintf(buf, "%c%d.%d", s, disptemp/10, disptemp%10);
  return buf;
} 


char *printVcc(int16_t vcc) {
  sprintf(buf, "%d.%02.2d", vcc/100, vcc%100);   
  return buf;
}

char *printVal(uint8_t type, int16_t val) {
  if(type==TH_HIST_VAL_T) return printTemp(val);
  return printVcc(val);
}
    
static byte p_date[3]={-1,-1,-1};
static byte p_time[3]={-1,-1,-1};
static byte p_to[3]={-1,-1,-1};
static byte p_days=-1;

void dispTimeout(unsigned long ts, bool reset, int x, int y, int sz) {
  byte tmp[3];
  
  if(reset) p_days=-1;
   
  tmp[2]=ts%60; tmp[1]=(ts/60)%60; tmp[0]=(ts/3600)%24;
  uint8_t days=ts/3600/24;  
  if(days>0 && days!=p_days) {
    sprintf(buf, "> %d days", days);
    Tft.drawString(buf,x,y,sz, GREEN, BLACK, true);
    p_days=days;
  }
  else {
     disp_dig(reset, 3, tmp, p_to, x, y, sz, GREEN, ':', 0);
  }     
}

void printTime(const DateTime& pDT, bool reset, int x, int y, int sz, bool blinkd, bool printdate){
  byte tmp[3];  
  tmp[0]=pDT.hour(); tmp[1]=pDT.minute(); tmp[2]=pDT.second();
  disp_dig(reset, 2, tmp, p_time, x, y, sz, YELLOW, (!blinkd || p_time[2]%2) ? ':' : ' ', tmp[2]!=p_time[2]);
  p_time[2]=tmp[2]; // store sec
  if(printdate) {
    tmp[0]=pDT.day(); tmp[1]=pDT.month(); tmp[2]=pDT.year()-2000;
    disp_dig(reset, 3, tmp, p_date, x+6*sz*FONT_SPACE, y, sz, YELLOW, '/', false);
  }
}

/*
void printTime(const DateTime& pDT, bool reset, int x, int y, int sz, bool blinkd, bool printdate){
  byte tmp[6];  
  //tmp[0]=pDT.hour(); tmp[1]=pDT.minute(); tmp[2]=pDT.second();
  pDT.get(tmp);
  disp_dig(reset, 2, tmp, p_time, x, y, sz, YELLOW, (!blinkd || p_time[2]%2) ? ':' : ' ', tmp[2]!=p_time[2]);
  p_time[2]=tmp[2]; // store sec
  if(printdate) {
    //tmp[0]=pDT.day(); tmp[1]=pDT.month(); tmp[2]=pDT.year()-2000;
    disp_dig(reset, 3, tmp+3, p_date, x+6*sz*FONT_SPACE, y, sz, YELLOW, '/', false);
  }
}
*/
void timeUp(int dig, int sz) {
  int ig=dig/2;
  int id=(dig+1)%2; 
  byte val=p_time[ig];
  byte maxv[3]={24, 60, 60};
  if(id) { val+=10; if(val>maxv[ig]) val=val%10; }  
  else { val=(val/10)*10+((val%10)+1)%10; if(val>maxv[ig]) val=(val/10)*10;} 
  byte tmp[3];
  memcpy(tmp, p_time, 3);
  tmp[ig]=val;
  disp_dig(false, 2, tmp, p_time, 0, 40, sz, YELLOW, ':', true);
}

void timeStore() {
  DateTime set=RTC.now();
  set.setTime(p_time[0], p_time[1], 0);
  RTC.adjust(set); 
}

/* 
redraw: always redraw
ngrp: number of digit groups (hh:mm = 2, hh:mm:ss, yy:mm:dd = 3). Always 2 digits in a group
data: ptr to new data. byte[ngrp]
pdata: ptr to old data. byte[ngrp]
x, y, sz - trivial
delim: delimeter symbol. shown before every group except of first
drdrm: always redraw separator (that's for blinking feature); 
*/

void disp_dig(byte redraw, byte ngrp, byte *data, byte *p_data, int x, int y, int sz, int color, char delim, byte drdrm) {
  int posX=x;
  for(byte igrp=0; igrp<ngrp; igrp++) {  
    if(igrp) {
      if(redraw || drdrm) 
        Tft.drawChar(delim, posX, y, sz, color, BLACK, true);  
      posX+=FONT_SPACE*sz;
    }
    for(byte isym=0; isym<2; isym++) {
       byte div10=(isym==0?10:1);
       byte dig=(data[igrp]/div10)%10;
       if(redraw || dig!=(p_data[igrp]/div10)%10)
         Tft.drawChar('0'+dig,posX,y,sz, color, BLACK, true); 
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

uint8_t printHist(uint8_t sid, uint8_t idx) {    
  if(!startIter()) return 0;
  
  uint8_t i=0;
  while(i<idx && mHist.movePrev()) i++;
  if(i<idx) { // wraparound
    mHist.iterBegin();
    mHist.movePrev();
    i=0;
  }  
  line_init(GREEN, 2);
  do {
    TempHistory::wt_msg_hist *h = mHist.getPrev();
    sprintf(buf, "%4d ", (int)i);
    line_printn(buf);    
    line_printn(printTemp(h->getVal(TH_HIST_VAL_T)));
    line_printn(" ");
    line_printn(printVcc(h->getVal(TH_HIST_VAL_V)));
    
    //for(uint8_t j=0; j<2; j++) {line_printn(printVal(j, h->getVal(j))); line_printn(" "); } // increases size
    
    sprintf(buf, " (%d min)", h->mins);
    line_print(buf);
    i++;
    } while(mHist.movePrev() && line_getpos()<230);
  return i;  
}

void chartHist(uint8_t sid, uint8_t scale, uint8_t type) {    
  const uint8_t chart_xstep_denoms[WS_CHART_NLEV]={7, 21, 49, 217};
  uint8_t chart_xstep_denom = chart_xstep_denoms[scale];
  int16_t mint, maxt;
  uint16_t mbefore;
  
  int16_t tdiff=prepChart(&mint, &maxt, type, (uint16_t)CHART_WIDTH*chart_xstep_denom+60);
  if(!tdiff) return;

  mbefore=mHist.getPrevMinsBefore(); // minutes passed after the earliest measurement
  sprintf(buf, "%d mins", (int)mbefore); 
  line_print_at(buf, 160, 0);
 
  int16_t xr;     
  xr=(int32_t)mbefore/chart_xstep_denom;
  if(xr>=CHART_WIDTH) xr=CHART_WIDTH-1;
  
  drawVertDashLine(xr, BLUE);

  DateTime now = RTC.now();
  if(xr>36) { // draw midnight line  
    uint16_t mid = now.hour()*60+now.minute();
    int16_t x1;
    while(mid<mbefore) {
      x1=xr-(int32_t)mid/chart_xstep_denom;
      if(x1>0) drawVertDashLine(x1, YELLOW);
      mid+=1440; // mins in 24h
    }
    DateTime start=DateTime(now.unixtime()-(uint32_t)mHist.getPrevMinsBefore()*60);
    printTime(start, true, 0, 224, 2, false, true);  
  }
 
  uint8_t cnt=0;
  int16_t y0, x0;
  x0=y0=0;
  mHist.iterBegin(); mHist.movePrev();
  do {
    int16_t y1=(int32_t)(maxt-mHist.getPrev()->getVal(type))*CHART_HEIGHT/tdiff;
    int16_t x1=xr-(int32_t)mHist.getPrevMinsBefore()/chart_xstep_denom;
    if(cnt)  {
     if(x1>0 && x0>0) 
       Tft.drawLineThick(x1,CHART_TOP+y1,x0,CHART_TOP+y0, CYAN, 5);
    }    
    x0=x1; y0=y1;
    cnt++;
  } while(mHist.movePrev() && x0>0);     
}

void chartHist60(uint8_t sid) 
{    
  const int DUR_24=24;
  const int DUR_MIN=60;
  const int xstep = CHART_WIDTH/DUR_24;

  { // histogramm scope
  int16_t mint, maxt;
   
  int16_t tdiff=prepChart(&mint, &maxt, TH_HIST_VAL_T, (uint16_t)DUR_24*60+60);  
  if(!tdiff) return;
  
  int y_z=(int32_t)(maxt-0)*CHART_HEIGHT/tdiff; // from top
  uint8_t islot=0;
  int16_t acc=0;
  uint8_t cnt=0;  

  mHist.iterBegin();
  
  do {
    uint8_t is=mHist.movePrev() ? mHist.getPrevMinsBefore()/DUR_MIN : DUR_24;
    /*
    if(is==islot) {
      //acc+=mHist.getPrev()->getVal(TH_HIST_VAL_T);
      //cnt++;      
    } else {
      */
    if(is!=islot) {  
      // draw prev;
      if(cnt) {
        int val=acc/cnt;
        int y0=y_z;
        int h;
        if(val<0) {
          if(maxt<0) { y0=0; h=maxt-val; } else { h=-val; }
        }
        else {
          if(mint>0) { y0=CHART_HEIGHT; h=val-mint;} else {h=val; }
        }
        h=(int32_t)h*CHART_HEIGHT/tdiff;
        if(val>0) y0-=h;     
        int16_t x=CHART_WIDTH-xstep*(islot+1); 
        Tft.fillRectangle(x+1, CHART_TOP+y0, xstep-2, h, WHITE);
      }      
      islot=is;
      acc=0;
      cnt=0;     
    }    
    acc+=mHist.getPrev()->getVal(TH_HIST_VAL_T);
    cnt++;          
  } while(islot<DUR_24); 
  
  }
  { // time labels scope
  DateTime now = RTC.now();  
  printTime(now, true, CHART_WIDTH, 224, 2, false, false);   
  uint16_t mid = now.hour()+1;
  drawVertDashLine(CHART_WIDTH-xstep*mid, YELLOW); // draw midnight line
  mid=mid>12 ? mid-12 : mid+12;
  drawVertDashLine(CHART_WIDTH-xstep*mid, RED); // draw noon line
  }
}


int8_t startIter() {  
  mHist.iterBegin();  
  if(!mHist.movePrev()) {
    Tft.drawString("NO DATA", 40,100, 6, RED, BLACK, false);
    return 0;
  }
  else return 1;
}

int16_t prepChart(int16_t *ptmint, int16_t *ptmaxt, uint8_t type, uint16_t mbefore) {
  if(!startIter()) return 0;  
  int16_t mint, maxt;
  maxt=mint=mHist.getPrev()->getVal(type);
  do {
    int16_t t = mHist.getPrev()->getVal(type);
    if(t>maxt) maxt=t;
    if(t<mint) mint=t;
  } while(mHist.movePrev() && mHist.getPrevMinsBefore()<mbefore);


  line_init(GREEN, 2);
  
  /*
  if(type==TH_HIST_VAL_T) {
    line_printn(printTemp(mint)); line_printn("..."); line_printn(printTemp(maxt)); 
  } else {
    line_printn(printVcc(mint)); line_printn("..."); line_printn(printVcc(maxt)); 
  }
  */
  
  line_printn(printVal(type, mint)); line_printn("..."); line_printn(printVal(type, maxt)); 
  
  if(mint%50) {
    if(mint>0) mint=(mint/50)*50;
     else mint=(mint/50-1)*50;
  }  
  if(maxt%50) {
     if(maxt>0) maxt=(maxt/50+1)*50;
     else maxt=(maxt/50)*50;
  }  
  if(maxt==mint) {mint-=50; maxt+=50;}  
  int16_t tdiff=maxt-mint;
  
  Tft.drawRectangle(0, CHART_TOP, CHART_WIDTH, CHART_HEIGHT, WHITE); 
  int16_t y0;  
  y0=(maxt-mint)>100 ? 50 : 10;  
  for(int16_t ig=mint; ig<=maxt; ig+=y0) { // degree lines
     int16_t yl=CHART_TOP+(int32_t)(maxt-ig)*CHART_HEIGHT/tdiff;
     if(ig>mint && ig<maxt) Tft.drawStraightDashedLine(LCD_HORIZONTAL, 0, yl, CHART_WIDTH, ig==0? BLUE : GREEN, BLACK, 0x0f);
     line_print_at(type==TH_HIST_VAL_T ? printTemp(ig) : printVcc(ig), CHART_WIDTH+1, yl-FONT_Y);
  }
  *ptmaxt=maxt;
  *ptmint=mint;
  return tdiff;
}

void drawVertDashLine(int x, uint16_t color) {
  Tft.drawStraightDashedLine(LCD_VERTICAL, x, CHART_TOP, CHART_HEIGHT, color,BLACK, 0x0f); // 
}


static uint16_t _lp_vpos=0;
static int16_t _lp_hpos=0;
static uint16_t _lp_color=GREEN;
static uint8_t _lp_sz=2;

inline uint16_t line_getpos() { return _lp_vpos; }

void line_init(uint16_t color, uint8_t sz) {
  _lp_color=color;
  _lp_sz=sz;
  _lp_vpos=_lp_hpos=0;
}

void line_print(char* buf) {
  line_printn(buf);
  _lp_hpos=0;
  _lp_vpos+=FONT_Y*_lp_sz;
}

void line_print_at(char* buf, uint16_t x, uint16_t y) {
  _lp_hpos=x; _lp_vpos=y;
  line_printn(buf);
}

void line_printn(char* buf) {
  _lp_hpos = Tft.drawString(buf ,_lp_hpos, _lp_vpos, _lp_sz, _lp_color, BLACK, true);
  //_lp_hpos = Tft.drawString(buf ,_lp_hpos, _lp_vpos, _lp_sz, _lp_color, BLACK, false);
}

/****************** HIST ****************/

uint8_t addHistAcc(struct wt_msg *pmsg) {
  //prev_vcc=last_vcc;
  last_vcc=pmsg->vcc;
  if(DS18_MEAS_FAIL==pmsg->temp) {
    alarms |= WS_ALR_BAD_TEMP; alarm_cnt[WS_ALR_BAD_TEMP_IDX]++;
    //alarm_val=err; 
    return 1;
  }
  msgcnt++;
  //prev_tmp=last_tmp;
  last_tmp=pmsg->temp; 
  last_millis_temp=millis();
  mHist.addAcc(pmsg->temp, pmsg->vcc);
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
