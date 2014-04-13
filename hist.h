
#define TH_HIST_SZ  96// 96*15=24 hrs
#define TH_NODATA 0x0FFF
#define TH_ACC_TIME  15 //mins

class TempHistory {
public:
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

    TempHistory();
    void init();
    void addAcc(int16_t temp);
    void add(uint8_t sid, uint8_t mins, int16_t temp);
    int16_t getDiff(int16_t val, uint8_t sid);
    uint8_t getSz();
    unsigned long getLastAccTime() { return acc_prev_time; }
    wt_msg_hist *getData() { return hist; }
    uint8_t getHead() { return hist_ptr==0?TH_HIST_SZ-1 : hist_ptr-1; }
    void iterBegin();
    boolean movePrev(); 
    wt_msg_hist *getPrev() { return &hist[iter_ptr]; }
    //uint16_t     getPrevMinsBefore() { return iter_mbefore-hist[iter_ptr].mins; }
    uint16_t     getPrevMinsBefore() { return iter_mbefore; }
        
    static unsigned long interval(unsigned long prev);
protected:
    uint8_t size;
    wt_msg_hist hist[TH_HIST_SZ];
    uint8_t hist_ptr; // NEXT record to fill
    wt_msg_acc acc;
    unsigned long acc_prev_time;
    uint8_t iter_ptr;
    uint8_t iter_firstmov;
    uint16_t iter_mbefore;
};
