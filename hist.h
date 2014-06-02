
// effective storage is TH_HIST_SZ-1
#define TH_HIST_SZ  96 
//#define TH_HIST_SZ  32
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
    uint8_t getSz() { return head_ptr + (TH_HIST_SZ-1-tail_ptr); }
    unsigned long getLastAccTime() { return acc_prev_time; }
    wt_msg_hist *getData() { return hist; }
    uint8_t getPrev(uint8_t pos) { return pos==0?TH_HIST_SZ-1 : pos-1; }
    uint8_t getNext(uint8_t pos) { return pos==TH_HIST_SZ-1 ? 0 : pos+1; }
    void iterBegin();
    boolean movePrev(); 
    wt_msg_hist *getPrev() { return hist+iter_ptr; }
    uint16_t     getPrevMinsBefore() { return iter_mbefore; }
        
    uint8_t _getHeadPtr() { return head_ptr; }
    uint8_t _getTailPtr() { return tail_ptr; }
    uint8_t _getSinceHAcc() { return since_h_acc; }
    uint8_t _getLastHAccPtr() { return last_h_acc_ptr; }
    
    static unsigned long interval(unsigned long prev);
protected:
    //uint8_t size;
    wt_msg_hist hist[TH_HIST_SZ];
    uint8_t head_ptr; // NEXT record to fill
    uint8_t tail_ptr; // record before oldest one 
    uint8_t since_h_acc;
    uint8_t last_h_acc_ptr;
    wt_msg_acc acc;
    unsigned long acc_prev_time;
    uint8_t iter_ptr;
    uint16_t iter_mbefore;
};
