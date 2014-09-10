
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
  head_ptr=0; tail_ptr=TH_HIST_SZ-1;
  acc={0, 0,0, 0 };
  since_h_acc=0;
  last_h_acc_ptr=0;
  acc_prev_time=millis();
}

void TempHistory::addAcc(int16_t temp, int16_t vcc) {
  acc.cnt++;
  acc.temp+=temp;
  acc.vcc+=vcc;
  uint8_t mins=interval(acc_prev_time)/60000L; //time lapsed from previous storage
  if(mins>=TH_ACC_TIME) { 
    add(1, mins, acc.temp/acc.cnt, acc.vcc/acc.cnt);
    acc_prev_time=millis();
    acc.temp=0;
    acc.vcc=0;
    acc.cnt=0;
  }
}

void TempHistory::add(uint8_t sid, uint8_t mins, int16_t temp, int16_t vcc) {
  hist[head_ptr].sid=sid;

  hist[head_ptr].temp=(temp/TH_HIST_DV_T);
  hist[head_ptr].vcc=(vcc/TH_HIST_DV_V);
  hist[head_ptr].mins=mins;
  if(tail_ptr==head_ptr) tail_ptr = (tail_ptr+1)%TH_HIST_SZ;
  head_ptr = (head_ptr+1)%TH_HIST_SZ; // next (and the oldest reading)
  // hour accum here...
  // compress old records to hours...
  since_h_acc+=mins;
  if(since_h_acc>120) { // 2 hours
    int32_t acc_t=0;
    int32_t acc_v=0;
    uint8_t cnt=0;
    uint8_t iptr=last_h_acc_ptr;
    uint16_t mins=0;
    // calculate hour average
    do { 
      mins+=hist[iptr].mins;
      acc_t+=hist[iptr].temp;
      acc_v+=hist[iptr].vcc;
      cnt++;
      iptr = getNext(iptr);
    } while(iptr!=head_ptr && mins<60);
    // store hour average
    hist[last_h_acc_ptr].temp=acc_t/cnt;
    hist[last_h_acc_ptr].vcc=acc_v/cnt;
    hist[last_h_acc_ptr].mins=mins;
    
    last_h_acc_ptr=getNext(last_h_acc_ptr);
    
    // move items down... 
    uint8_t tptr=last_h_acc_ptr;
    mins=0;
    while(iptr!=head_ptr) { 
      mins+=hist[iptr].mins;
      hist[tptr]=hist[iptr];
      iptr = getNext(iptr);
      tptr = getNext(tptr);
    }    
    head_ptr=tptr;
    since_h_acc=mins;    
  }
}

int16_t TempHistory::getDiff(int16_t val, uint8_t sid) {
  uint8_t lst=getPrev(head_ptr);      
  if(lst==tail_ptr) return 0;
  return val-(int16_t)hist[lst].temp*TH_HIST_DV_T;
}

void TempHistory::iterBegin() { 
  iter_ptr = head_ptr; 
  iter_mbefore=interval(acc_prev_time)/60000L; // time lapsed from latest storage
}

boolean TempHistory::movePrev() {
  iter_ptr=getPrev(iter_ptr);   
  if(iter_ptr == tail_ptr) return false;
  iter_mbefore+=hist[iter_ptr].mins; // points to the moment iterated average started to accumulate 
  return true;
} 
