#include <TFTv2.h>
#include <SPI.h>
#include <NRF24.h>

#include <I2C_SoftwareLibrary.h>

#include "RTClibSS.h"

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
  

//const int BUTTON_1 = PUSH2;     // the number of the pushbutton pin
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
  

//const int BUTTON_1 = PUSH2;     // the number of the pushbutton pin
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

//#define WS_HIST_SZ  32// 32*15=480=8hrs
#define WS_HIST_SZ  96// 96*15=24 hrs
#define HIST_NODATA 0x0FFF
#define WS_ACC_TIME  15 //mins

int16_t last_tmp=HIST_NODATA, prev_tmp=HIST_NODATA;
int16_t last_vcc=0, prev_vcc=-1;
unsigned long last_millis_temp=0;

struct wt_msg_hist {
  uint8_t sid;  // src
  uint8_t mins; // mins since previous put
  int16_t temp;
}; 

struct wt_msg_acc {
  uint8_t cnt;
  uint8_t mins; // mins since previous put
  int32_t temp;
};

wt_msg_hist hist[WS_HIST_SZ];
uint8_t hist_ptr=0; // NEXT record to fill

wt_msg_acc acc={0, 0, 0};
unsigned long acc_prev_time=0;

// ************************ UI

#define WS_NUILEV 6
#define WS_UI_MAIN  0
#define WS_UI_HIST  1
#define WS_UI_CHART 2
#define WS_UI_CHART60 3
/*
#define WS_UI_CHART30 5
#define WS_UI_CHART60 6
#define WS_UI_AVG   7
//#define WS_UI_AVG_2 5
//#define WS_UI_ACC   6
*/
#define WS_UI_STAT 4
#define WS_UI_SET   5

const char *lnames[WS_NUILEV] = {"main", "hist", "chart", "chr60", "stat", "set"};

#define WS_CHAR_UP 128
#define WS_CHAR_DN 129
#define WS_CHAR_TIME_SZ  4
#define WS_CHAR_TIME_SET_SZ  6

const int chart_width=240;
const int chart_height=220;
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
 
  initHist();  
  
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
 
 acc_prev_time=millis();
 mdisp=mui=millis();
 
 updateScreen(true);
}

void loop()
{ 
  char buf[16];

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
     if(!bt1cnt) { bt1cnt=1; dispStat("PRESSED   "); }
     else if(bt1cnt<100) bt1cnt++;
   } 
   else if(bt1cnt) {
     sprintf(buf, "%d  ", (int)bt1cnt);
     Tft.drawString(buf,200,211,2, WHITE, BLACK, true);
     if(bt1cnt<2) {dispStat("DEBOUNCE");} // debounce
     else if(bt1cnt<20) { // short click       
       dispStat("SHORTCLK");
       processShortClick();
     } else { // long click  
         dispStat("LONGCLK.");
         processLongClick();
     }
     bt1cnt=0;
   }
   
   if(digitalRead(BUTTON_2)==LOW) {
     if(!bt2cnt) { bt2cnt=1; dispStat("PRESSED   "); }
     else if(bt2cnt<100) bt2cnt++;
   } else 
   if(bt2cnt) {
     sprintf(buf, "%d  ", (int)bt2cnt);
     Tft.drawString(buf,200,211,2, WHITE, BLACK, true);
     if(bt2cnt<2) {dispStat("DEBOUNCE");} // debounce
     else if(bt2cnt<20) { // short click       
       dispStat("SHORTCLK");
       processShortRightClick();
     } else { // long click  
         dispStat("LONGCLK.");
     }
     bt2cnt=0;
   }
     
   if(ms-mdisp > 500 || mdisp>ms) { // 0.5 sec screen update
     mdisp=ms;   
     if(!(alarms&WS_ALR_TO) && millis()-rf_millis>WS_SENS_TIMEOUT) { //!!!
       alarms |= WS_ALR_TO;
       alarm_val=(millis()-rf_millis)/60000;
       alarm_cnt[WS_ALR_TO_IDX]++;
       //updateScreenData();
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
  char buf[16];
  switch(uilev) {
    case WS_UI_MAIN: {
     uint16_t ent, frc;     
     if((prev_tmp!=last_tmp || refresh) && last_tmp!=HIST_NODATA) {
       int16_t diff = getHistDiff(1);
       Tft.drawString(printTemp(buf, last_tmp, 1), 0, 80,6, GREEN, BLACK, true);
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
  if(uilev==WS_UI_MAIN) {
    printDate(reset, WS_CHAR_TIME_SZ);   
    dispTimeout(millis()-last_millis_temp, reset, 0, 160, 2);
  } else if(uilev==WS_UI_SET) {
      if(editmode) {
        ;
      }
      else printDate(reset, WS_CHAR_TIME_SET_SZ);   
  }
}

void dispTimeout(unsigned long dt, bool reset, int x, int y, int sz) {
  char buf[16];
  static byte p_to[3]={-1,-1,-1};
  static byte p_days=-1;
  byte tmp[3];
  
  if(reset) {
    p_days=-1;
    memset(p_to, -1, 3);
  } 
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

void printDate(bool reset, int sz){
  byte tmp[3];  
  char sbuf[8];
  
  if(reset) {
    memset(p_date, -1, 3);
    memset(p_time, -1, 3);
  }
 
  DateTime now = RTC.now();
  tmp[0]=now.hour(); tmp[1]=now.minute(); tmp[2]=now.second();
  disp_dig(reset, 2, 2, tmp, p_time, 0, 40, sz, YELLOW, p_time[2]%2 ? ':' : ' ', tmp[2]!=p_time[2]);
  p_time[2]=tmp[2];
  tmp[0]=now.day(); tmp[1]=now.month(); tmp[2]=now.year();
  disp_dig(reset, 3, 2, tmp, p_date, 0, 0, 2, RED, '/', false);
 
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
  char sbuf[32];
  sprintf(sbuf, "ERR=%04.4d", err);
  Tft.drawString(sbuf,0,211,2, RED, BLACK, true);
}

void dispStat(char *buf) {
  Tft.drawString(buf,0,211,2, GREEN, BLACK, true);
}


/****************** HIST ****************/

void initHist() {
 for(uint8_t i=0;i<WS_HIST_SZ;i++) hist[i].temp=HIST_NODATA;
}

uint8_t addHistAcc(struct wt_msg *pmsg) {
  uint8_t mins;
  prev_vcc=last_vcc;
  last_vcc=pmsg->vcc;
  if(DS18_MEAS_FAIL==pmsg->temp) {
    alarms |= WS_ALR_BAD_TEMP; alarm_val=err; alarm_cnt[WS_ALR_BAD_TEMP_IDX]++;
    return 1;
  }
  msgcnt++;
  prev_tmp=last_tmp;
  last_tmp=pmsg->temp; 
  acc.cnt++;
  acc.temp+=pmsg->temp;
  mins=interval(acc_prev_time)/60000L;
  last_millis_temp=millis();
  if(mins>=WS_ACC_TIME) { 
    addHist(1, mins, acc.temp/acc.cnt);
    acc_prev_time=millis();
    acc.temp=0;
    acc.cnt=0;
  }
  return 0;
}

void addHist(uint8_t sid, uint8_t mins, int16_t temp) {
  hist[hist_ptr].sid=sid;
  hist[hist_ptr].temp=temp;
  hist[hist_ptr].mins=mins;
  hist_ptr = (hist_ptr+1)%WS_HIST_SZ; // next (and the oldest reading)
}

int16_t getHistDiff(uint8_t sid) {
  uint8_t lst=hist_ptr==0?WS_HIST_SZ-1 : hist_ptr-1;   
  if(hist[lst].temp==HIST_NODATA) return 0;
  return last_tmp-hist[lst].temp;
}

uint8_t getHistSz() {
  uint8_t cnt=0;
  uint8_t lst=hist_ptr==0?WS_HIST_SZ-1 : hist_ptr-1;   
  if(hist[lst].temp==HIST_NODATA) {
    return 0;
  }
  uint8_t prev=lst;
  do {
    cnt++;    
    prev=prev==0?WS_HIST_SZ-1 : prev-1;   
  } while(prev!=lst && hist[prev].temp!=HIST_NODATA);
  return cnt;
}

void printHist(uint8_t sid) {  
  uint8_t lst=hist_ptr==0?WS_HIST_SZ-1 : hist_ptr-1;   
  if(hist[lst].temp==HIST_NODATA) {
    Tft.drawString("NO DATA", 40,100, 6, RED, BLACK, false);
    return;
  }
  uint8_t prev=lst;
  int mbefore=interval(acc_prev_time)/60000L;
  uint8_t vpos=0;
  char buf[16];
  
  do {
    Tft.drawString(printTemp(buf, hist[prev].temp, 0),0,vpos, 1, GREEN, BLACK, false);
    sprintf(buf, "-%d min", mbefore); 
    Tft.drawString(buf,100, vpos, 1, GREEN, BLACK, false);
    mbefore+=hist[prev].mins;
    vpos+=FONT_SPACE+2;
    prev=prev==0?WS_HIST_SZ-1 : prev-1;   
  } while(prev!=lst && hist[prev].temp!=HIST_NODATA && vpos<230);
  
}

int16_t getHistAvg(uint8_t sid, uint16_t from, uint16_t period) { // avearge in from-period .... from
  
  uint8_t lst=hist_ptr==0?WS_HIST_SZ-1 : hist_ptr-1;   
  if(hist[lst].temp==HIST_NODATA) return HIST_NODATA;
  uint16_t mbefore=0;
  uint8_t start=lst;
  if(from>0) { 
    do {
      mbefore+=hist[start].mins;
      start=start==0?WS_HIST_SZ-1 : start-1; 
    } while(start!=lst && hist[start].temp!=HIST_NODATA && mbefore<from);  
    if(start==lst || hist[start].temp==HIST_NODATA) return HIST_NODATA;
  }
  
  mbefore=0;
  from += period;
  uint8_t cnt=0;
  int32_t acc=0;
  do {
    mbefore+=hist[start].mins;
    acc+=hist[start].temp;
    cnt++;
    start=start==0?WS_HIST_SZ-1 : start-1; 
  } while(start!=lst && hist[start].temp!=HIST_NODATA && mbefore<from);

  return acc/cnt;
}

void printAvg(uint8_t sid) {
  /*
  int16_t avg;
  lcd.setCursor(0, 0); lcd.print("30m:");
  for(int i=0; i<3; i++) {
    avg=getHistAvg(sid, i*30, (i+1)*30-1); 
    if(avg == HIST_NODATA) break;
    printTemp(avg);
  }
  lcd.setCursor(0, 1); lcd.print("1 h:");
  for(int i=0; i<3; i++) {
    avg=getHistAvg(sid, i*60, (i+1)*60-1);
    if(avg == HIST_NODATA) break;
    printTemp(avg);
  } 
 */ 
}

/*
void chartHist(uint8_t sid) { // this is not a correct time-chart...just a sequence of measurements
  
  int16_t mint, maxt;
  uint16_t mbefore;
  char buf[16];
  int cnt=0;

  uint8_t lst=hist_ptr==0?WS_HIST_SZ-1 : hist_ptr-1;   
  if(hist[lst].temp==HIST_NODATA) {
    Tft.drawString("NO DATA", 40,100, 6, RED, BLACK, false);
    return;
  }
  maxt=hist[lst].temp;
  mint=hist[lst].temp;
  mbefore=(millis()-acc_prev_time)/60000L; // TEMPORARILY (NEED TO HANDLE WRAPAROUND)!!!!;
   
  uint8_t prev=lst;  
  do {
    if(hist[prev].temp>maxt) maxt=hist[prev].temp;
    if(hist[prev].temp<mint) mint=hist[prev].temp;
    prev=prev==0?WS_HIST_SZ-1 : prev-1; 
    cnt++;
  } while(prev!=lst && hist[prev].temp!=HIST_NODATA);
  
  Tft.drawString(printTemp(buf, mint, 0), 0, 0, 2, GREEN, BLACK, false);
  Tft.drawString(printTemp(buf, maxt, 0), 100, 0, 2, GREEN, BLACK, false);
  
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
  
  const int chart_width=240;
  const int chart_height=220;
  const int chart_top=19;
  const int chart_xstep=2;
  
  Tft.drawRectangle(0, chart_top, chart_width, chart_height, WHITE);
  Tft.drawString(printTemp(buf, maxt, 0), chart_width, chart_top, 2, GREEN, BLACK, false);
  Tft.drawString(printTemp(buf, mint, 0), chart_width, chart_top+chart_height-16, 2, GREEN, BLACK, false);
  
  int y0;
  if(maxt>=0 && mint<=0) { // draw zero line
    y0=(int32_t)(maxt-0)*chart_height/tdiff; // from top
    Tft.drawHorizontalLine(0, chart_top+y0,chart_width,BLUE);
  }
  
  int hpos=cnt*chart_xstep;
  cnt=0;
  y0=(int32_t)(maxt-hist[prev].temp)*chart_height/tdiff;
  prev=lst; 
  do {
    int y=(int32_t)(maxt-hist[prev].temp)*chart_height/tdiff;
    if(cnt) // !!!!!!!
      Tft.drawLine(hpos,chart_top+y0,hpos-chart_xstep,chart_top+y,GREEN);
    y0=y;
    prev=prev==0?WS_HIST_SZ-1 : prev-1; 
    hpos-=chart_xstep;
    cnt++;
  } while(prev!=lst && hist[prev].temp!=HIST_NODATA);   

}
*/
void chartHist(uint8_t sid) { // this is not a correct time-chart...just a sequence of measurements
  
  int16_t mint, maxt;
  uint16_t mbefore;
  char buf[32];
  int cnt=0;

  uint8_t lst=hist_ptr==0?WS_HIST_SZ-1 : hist_ptr-1;   
  if(hist[lst].temp==HIST_NODATA) {
    Tft.drawString("NO DATA", 40,100, 6, RED, BLACK, false);
    return;
  }
  maxt=hist[lst].temp;
  mint=hist[lst].temp;
  mbefore=interval(acc_prev_time)/60000L;
  
  uint8_t prev=lst;  
  do {
    if(hist[prev].temp>maxt) maxt=hist[prev].temp;
    if(hist[prev].temp<mint) mint=hist[prev].temp;
    mbefore+=hist[prev].mins;
    prev=prev==0?WS_HIST_SZ-1 : prev-1; 
    cnt++;
  } while(prev!=lst && hist[prev].temp!=HIST_NODATA);
  
  Tft.drawString(printTemp(buf, mint, 0), 0, 0, 2, GREEN, BLACK, false);
  Tft.drawString(printTemp(buf, maxt, 0), 80, 0, 2, GREEN, BLACK, false);
  sprintf(buf, "%d mins", (int)mbefore); Tft.drawString(buf, 160, 0, 2, GREEN, BLACK, false);
  
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
  
  const int chart_xstep_denom=15;
  const int chart_xstep_nom=2;
  
  Tft.drawRectangle(0, chart_top, chart_width, chart_height, WHITE);
  Tft.drawString(printTemp(buf, maxt, 0), chart_width, chart_top, 2, GREEN, BLACK, false);
  Tft.drawString(printTemp(buf, mint, 0), chart_width, chart_top+chart_height-16, 2, GREEN, BLACK, false);
  
  int y0, x0;
  if(maxt>=0 && mint<=0) { // draw zero line
    y0=(int32_t)(maxt-0)*chart_height/tdiff; // from top
    Tft.drawHorizontalLine(0, chart_top+y0,chart_width,BLUE);
  }
  
  Tft.drawVerticalLine(mbefore*chart_xstep_nom/chart_xstep_denom, chart_top, chart_height,BLUE); // now
  
  //mbefore-=interval(acc_prev_time)/60000L; //!!!!!!!!!!!!!!
  cnt=0;
  x0=y0=0;
  prev=lst; 
  do {
    int y1=(int32_t)(maxt-hist[prev].temp)*chart_height/tdiff;
    int x1=(mbefore)*chart_xstep_nom/chart_xstep_denom;
    if(cnt)  {
      if(cnt<3) {
        sprintf(buf, "%d -> %d", x1, x0); Tft.drawString(buf, 140, FONT_Y*2*cnt, 2, RED, BLACK, false);
      }
      Tft.drawLine(x1,chart_top+y1,x0,chart_top+y0,GREEN);
    }
    y0=y1;
    x0=x1;
    prev=prev==0?WS_HIST_SZ-1 : prev-1; 
    mbefore-=hist[prev].mins;
    cnt++;
  } while(prev!=lst && hist[prev].temp!=HIST_NODATA);   

}

void chartHist60(uint8_t sid) {
  
  const int DUR_24=24;
  const int DUR_MIN=60;
  int16_t alldur=DUR_24*DUR_MIN;
  
  int16_t mint, maxt;
  uint16_t mbefore;
  char buf[32];
  int cnt=0;
 
  uint8_t lst=hist_ptr==0?WS_HIST_SZ-1 : hist_ptr-1;   
  if(hist[lst].temp==HIST_NODATA) {
    Tft.drawString("NO DATA", 40,100, 6, RED, BLACK, false);
    return;
  }
    
  maxt=hist[lst].temp;
  mint=hist[lst].temp;
  mbefore=interval(acc_prev_time)/60000L;
  
  uint8_t prev=lst;  
  do {
    if(hist[prev].temp>maxt) maxt=hist[prev].temp;
    if(hist[prev].temp<mint) mint=hist[prev].temp;
    mbefore+=hist[prev].mins;
    prev=prev==0?WS_HIST_SZ-1 : prev-1; 
    cnt++;
  } while(prev!=lst && hist[prev].temp!=HIST_NODATA);
  
  Tft.drawString(printTemp(buf, mint, 0), 0, 0, 2, GREEN, BLACK, false);
  Tft.drawString(printTemp(buf, maxt, 0), 80, 0, 2, GREEN, BLACK, false);
  sprintf(buf, "%d mins", (int)mbefore); Tft.drawString(buf, 160, 0, 2, GREEN, BLACK, false);
  
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
   
  Tft.drawRectangle(0, chart_top, chart_width, chart_height, WHITE);
  Tft.drawString(printTemp(buf, maxt, 0), chart_width, chart_top, 2, GREEN, BLACK, false);
  Tft.drawString(printTemp(buf, mint, 0), chart_width, chart_top+chart_height-16, 2, GREEN, BLACK, false);
  
  int y_z=0;
  if(maxt>=0 && mint<=0) { // draw zero line
    y_z=(int32_t)(maxt-0)*chart_height/tdiff; // from top
    Tft.drawHorizontalLine(0, chart_top+y_z,chart_width,BLUE);
  }
  
  //const int chart_xstep_denom=15;
  //const int chart_xstep_nom=2;
 
  int xstep = chart_width/DUR_24;
  int x=0;   
  for(int i=0; i<DUR_24; i++) {
    // should be up from zerolinr if positive, down if negative...THIS IS TODO
    int val=i%2 ? (maxt+mint)/2+(maxt-mint)/4 : (maxt+mint)/2-(maxt-mint)/4;
    //int y1=(int32_t)(maxt-val)*chart_height/tdiff;
    int y0=0;
    int h;
    if(val<0) {
      h=(int32_t)(maxt-val)*chart_height/tdiff;
      if(maxt<0) y0=0; else y0=y_z;
    }
    else {
      h=(int32_t)(maxt-val)*chart_height/tdiff;
      if(mint>0) { /*Tft.drawString("*", 0, FONT_Y*2*i, 2, RED, BLACK, false);*/ y0=chart_height;} else y0=y_z;
      y0-=h;
    }
    if(i<2) {
            sprintf(buf, "%d ", val); Tft.drawString(buf, 40, FONT_Y*2*i, 2, RED, BLACK, false);
            sprintf(buf, "%d / %d", y0, h); Tft.drawString(buf, 140, FONT_Y*2*i, 2, RED, BLACK, false);  
  }
    Tft.fillRectangle(x+1, chart_top+y0, xstep-2, h, WHITE);
    x+=xstep;
  }
  
  /*
  uint8_t mbefore=(millis()-acc_prev_time)/60000L; // TEMPORARILY (NEED TO HANDLE WRAPAROUND)!!!!;

  maxt=hist[lst].temp;
  mint=hist[lst].temp;
  //bzero(tmp.cval, sizeof(tmp.cval));
 
  uint8_t prev=lst;  
  uint8_t icol;
  //int16_t alldur=WS_CHART_NCOL*s;
  
  do {
    icol=mbefore/s;
    if(icol>=0 && icol<WS_CHART_NCOL) {
      tmp.cval[icol].t+=hist[prev].temp; tmp.cval[icol].c++;
      if(hist[prev].temp>maxt) maxt=hist[prev].temp;
      if(hist[prev].temp<mint) mint=hist[prev].temp;
    }
    mbefore+=hist[prev].mins;
    prev=prev==0?WS_HIST_SZ-1 : prev-1; 
  } while(prev!=lst && hist[prev].temp!=HIST_NODATA && mbefore<=alldur);  
  
  const uint8_t gridY = 7; 
  int16_t tdiff=maxt-mint;
  int16_t low=mint;
  
  if(tdiff<gridY*4) {
    int16_t adg=(gridY*5-tdiff)/2;
    //maxt+=adg;
    //mint-=adg;
    low=mint-adg;
    tdiff=maxt-mint+2*adg;
  }
  
  uint8_t yg;
  for(icol=0; icol<WS_CHART_NCOL; icol++) {
    if(!tmp.cval[icol].c && icol-1>=0 && icol+1<WS_CHART_NCOL && tmp.cval[icol-1].c && tmp.cval[icol+1].c) { 
      //interpolate
      tmp.cval[icol].c=tmp.cval[icol-1].c+tmp.cval[icol+1].c;
      tmp.cval[icol].t=tmp.cval[icol-1].t+tmp.cval[icol+1].t;
    }
    
    if(tmp.cval[icol].c) {
      if(tdiff==0) yg=4;
      else yg=(uint8_t)((tmp.cval[icol].t/tmp.cval[icol].c-low)*gridY/tdiff);    
      lcd.setCursor(WS_CHART_NCOL-icol-1, 1-yg/4);
      lcd.write((uint8_t)(3-yg%4));
    }
  }
  
  lcd.setCursor(WS_CHART_NCOL, 0); printTemp(maxt);  
  lcd.setCursor(WS_CHART_NCOL, 1); printTemp(mint);
 */
}

/*
void btn1Pressed() {
 bst1 |= WS_BUT_PRES; 
 bt1cnt=0;
 attachInterrupt(BUTTON_1, btn1Released, RISING);
}

void btn1Released() {
 bst1 &= ~WS_BUT_PRES;
 attachInterrupt(BUTTON_1, btn1Pressed, FALLING);
}

void btn2Pressed() {
 bst2 |= WS_BUT_PRES;
 bt2cnt=0;
 attachInterrupt(BUTTON_2, btn2Released, RISING);
}

void btn2Released() {
 bst2 &= ~WS_BUT_PRES;
 attachInterrupt(BUTTON_2, btn2Pressed, FALLING);
}
*/

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
