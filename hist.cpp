
#include <Energia.h>
#include "hist.h"

unsigned long TempHistory::interval(unsigned long prev) {
  unsigned long ms=millis();
  if(ms>prev) return ms-prev;
  return (~prev)+ms;
}  

TempHistory::TempHistory() {
}

void TempHistory::init() {
  hist_ptr=0; 
  acc={0, 0, 0};
  //acc_prev_time=0;
  acc_prev_time=millis();
  for(uint8_t i=0;i<TH_HIST_SZ;i++) hist[i].temp=TH_NODATA;
}

void TempHistory::addAcc(int16_t temp) {
  acc.cnt++;
  acc.temp+=temp;
  uint8_t mins=interval(acc_prev_time)/60000L; //time lapsed from previous storage
  if(mins>=TH_ACC_TIME) { 
    add(1, mins, acc.temp/acc.cnt);
    acc_prev_time=millis();
    acc.temp=0;
    acc.cnt=0;
  }
}

void TempHistory::add(uint8_t sid, uint8_t mins, int16_t temp) {
  hist[hist_ptr].sid=sid;
  hist[hist_ptr].temp=temp;
  hist[hist_ptr].mins=mins;
  hist_ptr = (hist_ptr+1)%TH_HIST_SZ; // next (and the oldest reading)
  // hour accum here...
}

int16_t TempHistory::getDiff(int16_t val, uint8_t sid) {
  uint8_t lst=hist_ptr==0?TH_HIST_SZ-1 : hist_ptr-1;   
  if(hist[lst].temp==TH_NODATA) return 0;
  return val-hist[lst].temp;
}

uint8_t TempHistory::getSz() {
  uint8_t cnt=0;
  uint8_t lst=hist_ptr==0?TH_HIST_SZ-1 : hist_ptr-1;   
  if(hist[lst].temp==TH_NODATA) {
    return 0;
  }
  uint8_t prev=lst;
  do {
    cnt++;    
    prev=prev==0?TH_HIST_SZ-1 : prev-1;   
  } while(prev!=lst && hist[prev].temp!=TH_NODATA);
  return cnt;
}

void TempHistory::iterBegin() { 
  iter_ptr = hist_ptr; 
  iter_firstmov=1; 
  iter_mbefore=interval(acc_prev_time)/60000L; // time lapsed from latest storage
}

boolean TempHistory::movePrev() {
  if(iter_ptr==hist_ptr && !iter_firstmov) return false; 
  iter_firstmov=0; 
  iter_ptr = iter_ptr==0?TH_HIST_SZ-1 : iter_ptr-1; 
  if(hist[iter_ptr].temp==TH_NODATA) return false;
  iter_mbefore+=hist[iter_ptr].mins; // points to the moment iterated average started to accumulate 
  return true;
} 
