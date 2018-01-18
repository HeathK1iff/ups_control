#include <EEPROM.h>

#define LENGTH_NTP_HOST 30
#define LENGTH_SSID 25
#define LENGTH_SSID_PASS 25
#define MAX_SCHEDULE 5
#define EEPROM_SIZE 256
#define EEPROM_FLAG_SECTIONS 0
#define EEPROM_SETTING_SECTIONS 10
#define DAY_SEC 86400l
#define MIN_SEC 60l
#define HOUR_SEC 3600l

#define DEFAULT_NTP_HOST "2.pool.ntp.org"
#define DEFAULT_STEP_TEMP 1

struct ScheduleTime{
  uint32_t time;
  
  void setTime(int hour, int min){
    time = (hour * HOUR_SEC) + (min * MIN_SEC);
  };
  
  byte getHour(){
    return time / HOUR_SEC;
  };

  byte getMin(){
    return (time - (getHour() * HOUR_SEC)) / MIN_SEC;
  };
};

struct Schedule{
  bool enabled;
  bool state;
  ScheduleTime start;
  ScheduleTime end; 
  void toCharArray(char *out){
    if (state){
      sprintf(out, "%d:%d - %d:%d (on)", start.getHour(), start.getMin(), end.getHour(), end.getMin());
    } else {
      sprintf(out, "%d:%d - %d:%d (off)", start.getHour(), start.getMin(), end.getHour(), end.getMin()); 
    }
  }
};

struct Setting {
  Schedule schedule[MAX_SCHEDULE];
  bool alwaysOn;
  char ssid[LENGTH_SSID];
  char ssidPass[LENGTH_SSID_PASS];
  char ntpHost[LENGTH_NTP_HOST];
  bool autoMode;
};

struct TimeSlot{
  byte iSchedule;
  uint32_t duration;
};

class GlobalSetting {
  private:
    unsigned long tsNeedSave = 0;
    void init() {
      data.ssid[0] = '\0';
      data.ssidPass[0] = '\0';
      data.alwaysOn = true;
      data.autoMode = true;
      strcpy(data.ntpHost, DEFAULT_NTP_HOST);
    }

    void load() {
      EEPROM.begin(EEPROM_SIZE);
      if (EEPROM.read(0) == 1) {
        EEPROM.get(EEPROM_SETTING_SECTIONS, data);
      }
    }
  public:
    Setting data;

    int getActiveSchedule(DateTime now){
      uint32_t utCurrent = now.unixtime();
      DateTime beginDay(now.year(), now.month(), now.day(), 0, 0, 0);
      uint32_t utBeginDay = beginDay.unixtime();
      uint32_t utNextDay = utBeginDay + DAY_SEC;
      
      int iSchedule, iSlot = 0;
      int iMinTimeSlot = -1;
      bool founded;
      
      TimeSlot slots[MAX_SCHEDULE];
      while (iSchedule < MAX_SCHEDULE){
        founded = false;       
        uint32_t startScheduleTime = utBeginDay + data.schedule[iSchedule].start.time;
        uint32_t endScheduleTime = utBeginDay + data.schedule[iSchedule].end.time;

        if (!data.schedule[iSchedule].enabled){
          iSchedule++;
          continue;
        }

        if (data.schedule[iSchedule].start.time > data.schedule[iSchedule].end.time){
          if (((utCurrent >= startScheduleTime)&&(utCurrent <= utNextDay))||
             ((utCurrent >= utBeginDay)&&(utCurrent < endScheduleTime)))
             founded = true;        
        } else {
          if ((utCurrent >= startScheduleTime)&&
             (utCurrent < endScheduleTime))
             founded = true;
        }
       
        if (founded){         
          slots[iSlot].iSchedule = iSchedule;
          slots[iSlot].duration = utCurrent - startScheduleTime; 
          if ((iMinTimeSlot == -1)||(slots[iSlot].duration < slots[iMinTimeSlot].duration))
            iMinTimeSlot = iSlot;
          iSlot++;
        }
        iSchedule++;
      }

      if (iMinTimeSlot >= 0){
        return slots[iMinTimeSlot].iSchedule;
      } else {
        return -1;
      }
    }

    GlobalSetting() {
      init();
      load();
    }

    void maintenance(){
      if ((tsNeedSave != 0)&&(millis() > tsNeedSave)){
        save(true);
      } 
    }
    
    void save(bool force = false) {
      if (force) { 
        EEPROM.begin(EEPROM_SIZE);
        EEPROM.put(EEPROM_SETTING_SECTIONS, data);
        EEPROM.write(0, 1);
        EEPROM.commit();
        tsNeedSave = 0;
      } else {
        tsNeedSave = millis() + (SEC_MS * 10);
      }
    }

    void clear() {
      EEPROM.begin(EEPROM_SIZE);
      for (int i = 0 ; i < EEPROM_SIZE ; i++) {
        EEPROM.write(i, 0);
      }  
      EEPROM.commit();   
      load();
      init();
    }

} global;

