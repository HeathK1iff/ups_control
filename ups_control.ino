#define DEFAULT_SSID_PSW "!T@74j22casdW"

#define TIMEOUT_RECONNECT MIN_MS*5
#define TIMEOUT_CHECK_UPS SEC_MS*7
#define RESPOND_BUFFER_SIZE 600

#define DEVICE_NAME "UPS"

#include <HtmlGen.h>
#include <UdpEcho.h>
#include <DS3231.h>
#include <Wire.h> 
#include <ArduinoJson.h>
#include <ESP8266HTTPClient.h>
#include <DallasTemperature.h>
#include "utils.h"
#include "global.h"
#include "ups.h"

#define API_HTML_JSON_ARG "json"
#define API_HTML_INDEX_ARG "index"
#define API_RESPOND_TYPE "text/json"
#define API_RESPONSE_TYPE "response_type"
#define API_RESPONSE_TEXT "response_text"
#define API_ERROR_TYPE "error"
#define API_SUCCESS_TYPE "success"

#define API_METHOD_MAIN "/"
#define API_METHOD_NETWORK "/network.html"
#define API_METHOD_API_HELP "/rpc"
#define API_METHOD_INFO "/rpc/info.json"
#define API_METHOD_SENSORS "/rpc/sensors.json"
#define API_METHOD_SETTING "/rpc/setting.json"
#define API_METHOD_SCHEDULE "/rpc/setting/schedule.json"
#define API_METHOD_RESET "/rpc/setting/reset.do"
#define API_METHOD_RELAY "/rpc/relay.json"
#define API_METHOD_RELAY_ON "/rpc/relay/on.do"
#define API_METHOD_RELAY_OFF "/rpc/relay/off.do"
#define API_METHOD_RELAY_AUTO "/rpc/relay/auto.do"

#define W_IP_VOLTAGE "ip_volt"
#define W_IP_FAULT_VOLTAGE "ip_fault_volt"
#define W_OP_VOLTAGE "op_volt"
#define W_OP_CURRENT "op_current"
#define W_OP_FREQUENCY "op_freq"
#define W_BAT_VOLTAGE "bat_volt"
#define W_TEMP "temp"
#define W_ONLINE "online"
#define W_RELAY "relay"
#define W_AUTO_MODE "auto_mode"
#define W_ACTIVE "active_shedule"
#define W_ON "on"
#define W_OFF "off"
#define W_OK "ok"
#define W_STATE "state"
#define W_FAIL "fail"
#define W_SSID "ssid"
#define W_PASS "pass"
#define W_NTP "ntp"
#define W_ALWAYS_ON "always_on"
#define W_MAC "mac"
#define W_UPTIME "uptime"
#define W_HEAP "heap"
#define W_SDK "sdk"
#define W_BUILD "build"
#define W_HOUR "hour"
#define W_MIN "min"
#define W_START "start"
#define W_END "end"
#define W_ENABLED "enabled"
#define W_STATE "state"
#define W_COUNT "count"
#define W_DATETIME "time"
#define W_TEMP "temp"
#define W_UPS_TEMP "ups_temp"
#define W_DESCRIPTION "desc"
#define SENSOR_VOLT "volt"
#define SENSOR_FREQ "freq"
#define SENSOR_ID "id"
#define SENSOR_TYPE "type"
#define SENSOR_VOLT "volt"
#define SENSOR_VAL "val"
#define SENSORS "sensors"

#define SENSOR_TYPE_TEMP "temp"
#define SENSOR_TYPE_POWER_VOLTAGE "pwr_volt"
#define SENSOR_TYPE_POWER_FREQUENCY "pwr_freq"

#define API_SENSOR_ID "id"

#define E_JSON_PARSE_FAIL "JSON_PARSE_FAIL"
#define E_JSON_ARGUMENT_NOT_FOUND "JSON_ARGUMENT_NOT_FOUND"
#define E_PARAMS_NOT_FOUND "PARAMS_NOT_FOUND"
#define E_INDEX_ARGUMENT_NOT_FOUND "INDEX_ARGUMENT_NOT_FOUND"
#define E_INCORRECT_FORMAT "JSON_INCORRECT_FORMAT"
#define E_SCHEDULE_NOT_FOUND "SCHEDULE_NOT_FOUND"

const int PIN_RELAY = 12;

#define ONE_WIRE_BUS 14
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
RTClib rtc;
DS3231 clock;
ESP8266WebServer server(80);
UdpEcho echoServer(8312);
DeviceAddress tempAddress;
bool hasTempSensor = false;
bool autoMode = true;
int activeSchedule;
char deviceName[50];
char buildTimeFirmware[50];
UPSInfo upsinfo;

unsigned long tsReconnect = 0;
unsigned long tsNtptime = 0;
unsigned long tsUpsCheck = 0;
unsigned long tsRelayUpdate = 0;

void makeResponse(char *response, const char *type, const char *message){
  StaticJsonBuffer<RESPOND_BUFFER_SIZE> *jsonBuffer = new StaticJsonBuffer<RESPOND_BUFFER_SIZE>();
  JsonObject& out = jsonBuffer->createObject();
  out[API_RESPONSE_TYPE] = type;
  out[API_RESPONSE_TEXT] = message;
  out.printTo(response, RESPOND_BUFFER_SIZE);
  delete jsonBuffer;
}

void callAPIInfo(){
  char buf[RESPOND_BUFFER_SIZE];
  char uptime[20];
  upTime(uptime);

  DateTime tm = rtc.now();
  char dTime[25];
  sprintf(dTime, "%d.%d.%d %d:%d:%d", tm.day(), tm.month(),
    tm.year(), tm.hour(), tm.minute(), tm.second());

  StaticJsonBuffer<RESPOND_BUFFER_SIZE>* jsonBuffer = new StaticJsonBuffer<RESPOND_BUFFER_SIZE>();
  JsonObject& out = jsonBuffer->createObject();
  out[API_RESPONSE_TYPE] = API_SUCCESS_TYPE;
  out[API_RESPONSE_TEXT] = W_OK; 
  out[W_DATETIME] = dTime;

  out[W_MAC] = WiFi.macAddress();
  out[W_UPTIME] = uptime;
  out[W_HEAP] = ESP.getFreeHeap();
  out[W_BUILD] = buildTimeFirmware;
  out[W_DESCRIPTION] = "Mustek PowerMust 600 USB";

  if (upsinfo.isConnectedUps()){
    out[W_OP_VOLTAGE] = upsinfo.getOpVoltage();
    out[W_OP_CURRENT] = upsinfo.getOpCurrent();
    out[W_BAT_VOLTAGE] = upsinfo.getBatVoltage();
    out[W_UPS_TEMP] = upsinfo.getTemperature();
    out[W_ONLINE] = !upsinfo.getStatus().option.UtilityFail;
  }

  out.printTo(buf, RESPOND_BUFFER_SIZE);
  server.send(200, API_RESPOND_TYPE, buf);
  delete jsonBuffer;
}

void callAPISetting() {
  char respond[RESPOND_BUFFER_SIZE];
  if (server.args() > 0) {
    if (server.hasArg(API_HTML_JSON_ARG)) {
      String inStr = server.arg(API_HTML_JSON_ARG);
      StaticJsonBuffer<RESPOND_BUFFER_SIZE>* jsonBuffer = new StaticJsonBuffer<RESPOND_BUFFER_SIZE>();
      JsonObject& in = jsonBuffer->parseObject(inStr);
      if (in.success()){
        strcpy(global.data.ntpHost, in[W_NTP]);
        global.data.alwaysOn = in[W_ALWAYS_ON];
        global.data.autoMode = in[W_AUTO_MODE];
        global.save();
        makeResponse(respond, API_SUCCESS_TYPE, W_OK);
      } else {
        makeResponse(respond, API_ERROR_TYPE, E_JSON_PARSE_FAIL);
      }
      delete jsonBuffer;
    } else {
      makeResponse(respond, API_ERROR_TYPE, E_JSON_ARGUMENT_NOT_FOUND);
    }
  } else {
    StaticJsonBuffer<RESPOND_BUFFER_SIZE>* jsonBuffer = new StaticJsonBuffer<RESPOND_BUFFER_SIZE>();
    JsonObject& out = jsonBuffer->createObject(); 
    out[API_RESPONSE_TYPE] = API_SUCCESS_TYPE;
    out[API_RESPONSE_TEXT] = W_OK;
    out[W_NTP] = global.data.ntpHost;
    out[W_ALWAYS_ON] = global.data.alwaysOn;
    out[W_AUTO_MODE] = global.data.autoMode;
    out.printTo(respond, sizeof(respond));
    delete jsonBuffer;
  }
  server.send(200, API_RESPOND_TYPE, respond);  
}

void callAPISchedule() {
  char respond[RESPOND_BUFFER_SIZE];
  byte index = -1;
  byte h, m = 0;
  if (server.args() > 0) {
    if (server.hasArg(API_HTML_INDEX_ARG)) {
      index = server.arg(API_HTML_INDEX_ARG).toInt();

      if ((index >= 0)&&(index < MAX_SCHEDULE)){
        if (server.hasArg(API_HTML_JSON_ARG)) {     
          String inStr = server.arg(API_HTML_JSON_ARG);
          StaticJsonBuffer<RESPOND_BUFFER_SIZE>* jsonBuffer = new StaticJsonBuffer<RESPOND_BUFFER_SIZE>();
          JsonObject& in = jsonBuffer->parseObject(inStr);
          if (in.success()){
            global.data.schedule[index].enabled = in[W_ENABLED];
            global.data.schedule[index].state = in[W_STATE];
            global.data.schedule[index].start.setTime(in[W_START][W_HOUR], in[W_START][W_MIN]);
            global.data.schedule[index].end.setTime(in[W_END][W_HOUR], in[W_END][W_MIN]);           
            global.save();
            makeResponse(respond, API_SUCCESS_TYPE, W_OK);
          } else {
            makeResponse(respond, API_ERROR_TYPE, E_JSON_PARSE_FAIL);
          }
          delete jsonBuffer;
        } else {
            StaticJsonBuffer<RESPOND_BUFFER_SIZE>* jsonBuffer = new StaticJsonBuffer<RESPOND_BUFFER_SIZE>();
            JsonObject& out = jsonBuffer->createObject(); 
            out[API_RESPONSE_TYPE] = API_SUCCESS_TYPE;
            out[API_RESPONSE_TEXT] = W_OK;
            out[W_ENABLED] = global.data.schedule[index].enabled;
            out[W_STATE] = global.data.schedule[index].state;
            JsonObject& tStart = out.createNestedObject(W_START);
            tStart[W_HOUR] = global.data.schedule[index].start.getHour();
            tStart[W_MIN] = global.data.schedule[index].start.getMin();
            JsonObject& tEnd = out.createNestedObject(W_END);
            tEnd[W_HOUR] = global.data.schedule[index].end.getHour();
            tEnd[W_MIN] = global.data.schedule[index].end.getMin();        
            out.printTo(respond, sizeof(respond));
            delete jsonBuffer;
        }
      } else {
        makeResponse(respond, API_ERROR_TYPE, E_SCHEDULE_NOT_FOUND);
      }
    } else {
      makeResponse(respond, API_ERROR_TYPE, E_INDEX_ARGUMENT_NOT_FOUND);
    }
  } else {
    StaticJsonBuffer<RESPOND_BUFFER_SIZE>* jsonBuffer = new StaticJsonBuffer<RESPOND_BUFFER_SIZE>();
    JsonObject& out = jsonBuffer->createObject(); 
    out[API_RESPONSE_TYPE] = API_SUCCESS_TYPE;
    out[API_RESPONSE_TEXT] = W_OK;
    out[W_COUNT] = MAX_SCHEDULE;
    out.printTo(respond, sizeof(respond));
    delete jsonBuffer;
  }
  server.send(200, API_RESPOND_TYPE, respond);  
}

void callAPIReset(){
  char respond[RESPOND_BUFFER_SIZE];
  makeResponse(respond, API_SUCCESS_TYPE, W_OK);
  server.send(200, API_RESPOND_TYPE, respond); 
  global.clear();
  ESP.eraseConfig();
  ESP.restart();
}

void callAPIAutoMode(){
  autoMode = true;
  char respond[RESPOND_BUFFER_SIZE];
  makeResponse(respond, API_SUCCESS_TYPE, W_OK);
  server.send(200, API_RESPOND_TYPE, respond); 
}

void callAPIRelayOn(){
  autoMode = false;
  digitalWrite(PIN_RELAY, HIGH);
  char respond[RESPOND_BUFFER_SIZE];
  makeResponse(respond, API_SUCCESS_TYPE, W_OK);
  server.send(200, API_RESPOND_TYPE, respond); 
}

void callAPIRelayOff(){
  autoMode = false;
  digitalWrite(PIN_RELAY, LOW);
  char respond[RESPOND_BUFFER_SIZE];
  makeResponse(respond, API_SUCCESS_TYPE, W_OK);
  server.send(200, API_RESPOND_TYPE, respond); 
}

void callAPIRelay(){
  char respond[RESPOND_BUFFER_SIZE];
  StaticJsonBuffer<RESPOND_BUFFER_SIZE>* jsonBuffer = new StaticJsonBuffer<RESPOND_BUFFER_SIZE>();
  JsonObject& out = jsonBuffer->createObject(); 
  out[API_RESPONSE_TYPE] = API_SUCCESS_TYPE;
  out[API_RESPONSE_TEXT] = W_OK;
  out[W_RELAY] = (digitalRead(PIN_RELAY) == HIGH);
  out[W_AUTO_MODE] = autoMode;
  out[W_ACTIVE] = activeSchedule;
  out.printTo(respond, sizeof(respond));
  delete jsonBuffer;
  server.send(200, API_RESPOND_TYPE, respond); 
}

void callAPISensor(){
  char respond[RESPOND_BUFFER_SIZE];
  StaticJsonBuffer<RESPOND_BUFFER_SIZE>* jsonBuffer = new StaticJsonBuffer<RESPOND_BUFFER_SIZE>();
  JsonObject& out = jsonBuffer->createObject(); 
  out[API_RESPONSE_TYPE] = API_SUCCESS_TYPE;
  out[API_RESPONSE_TEXT] = W_OK;
  
  JsonArray& array_sensors = out.createNestedArray(SENSORS);
  sensors.requestTemperatures();
  int deviceCount = sensors.getDeviceCount();
   
  for (int i = 0; i < deviceCount; i++) {
    DeviceAddress device_addr;
    char sensorAddr[18]; 
    sensors.getAddress(device_addr, i);
    addr2str(device_addr, sensorAddr);
    String ds(sensorAddr);
    JsonObject& obj = array_sensors.createNestedObject();
    obj[SENSOR_ID] = ds;
    obj[SENSOR_TYPE] = SENSOR_TYPE_TEMP;
    obj[SENSOR_VAL] = sensors.getTempCByIndex(i);
  }

  if (upsinfo.isConnectedUps()){
    JsonObject& volt = array_sensors.createNestedObject();
    volt[SENSOR_ID] = ESP.getChipId();
    volt[SENSOR_TYPE] = SENSOR_TYPE_POWER_VOLTAGE;
    if (!upsinfo.getStatus().option.UtilityFail){
      volt[SENSOR_VAL] = upsinfo.getIpVoltage();
    } else {
      volt[SENSOR_VAL] = 0;
    }
    JsonObject& freq = array_sensors.createNestedObject();
    freq[SENSOR_ID] = ESP.getChipId() + 1;
    freq[SENSOR_TYPE] = SENSOR_TYPE_POWER_FREQUENCY;
    freq[SENSOR_VAL] = upsinfo.getIpFrequency();
  }

  out.printTo(respond, sizeof(respond));
  delete jsonBuffer;
  server.send(200, API_RESPOND_TYPE, respond); 
}

void makeTabs(HtmlPage *page, int pageIndex) {
  page->getHeader()->setTitle("Welcome (UPS)");
  HtmlTag* htmlTag = new HtmlTag("meta");
  htmlTag->getAttributes()->append("name", "viewport");
  htmlTag->getAttributes()->append("content", "width=device-width");
  page->getHeader()->append(htmlTag);

	HtmlStyleGroup* group = page->getHeader()->getStyle()->createGroup(".tabs td");
  group->append("width", "50%");
  group = page->getHeader()->getStyle()->createGroup(".htable");
  group->append("border", "1px solid #cdd0d4");
  group->append("width", "300px");
  group = page->getHeader()->getStyle()->createGroup(".hheader");
  group->append("background-color", "#cdd0d4");
  group->append("width", "300px");
  group = page->getHeader()->getStyle()->createGroup(".button");
  group->append("width", "100px");
  group->append("height", "40px");

	HtmlTable *tabs = new HtmlTable(2);
	tabs->append();
	tabs->getAttributes()->append("class", "tabs");
	tabs->getStyle()->append("width", "300px");
	tabs->getStyle()->append("background-color", "#DAF7A6");
	tabs->getCellStyle(0, pageIndex)->append("background-color", "#33ff6e");
	tabs->setCell(0, 0, new HtmlLink("/", "Overview"));
  tabs->setCell(0, 1, new HtmlLink("/network.html", "Network"));
	page->getHeader()->append(tabs);
}

void pageOverview() { 
  char uptime[20];
  upTime(uptime);

  DateTime tm = rtc.now();

  HtmlPage *htmlPage = new HtmlPage();
  makeTabs(htmlPage, 0);

  HtmlTable *tableOverview = new HtmlTable(2);
	tableOverview->append(10);
	tableOverview->getStyle()->append("width", "300px");
	tableOverview->getCellAttribute(0, 0)->append("colspan", 2);
	tableOverview->getCellAttribute(0, 0)->append("align", "center");
	tableOverview->getStyle()->append("border", "1px solid #cdd0d4");
	tableOverview->getCellStyle(0, 0)->append("background-color", "#cdd0d4");
  tableOverview->setCell(0, 0, "System");
  
  char dTime[25];
  sprintf(dTime, "%d.%d.%d %d:%d:%d", tm.day(), tm.month(),
          tm.year(), tm.hour(), tm.minute(), tm.second());

  tableOverview->setCell(1, 0, "Time:");
  tableOverview->setCell(1, 1, dTime);
  tableOverview->setCell(2, 0, "SSID:");
  if (WiFi.getMode()==WIFI_STA){
    tableOverview->setCell(2, 1, WiFi.SSID().c_str());
  } else {
    tableOverview->setCell(2, 1, deviceName);
  }
    
  tableOverview->setCell(3, 0, "Mode:");
  const char* modes[] = { "NULL", "STA", "AP", "STA+AP" };
  tableOverview->setCell(3, 1, modes[WiFi.getMode()]);
  tableOverview->setCell(4, 0, "Ip:");
  if (WiFi.getMode()==WIFI_STA){
    tableOverview->setCell(4, 1, WiFi.localIP().toString().c_str());  
  } else {
    tableOverview->setCell(4, 1, WiFi.softAPIP().toString().c_str());  
  }
  tableOverview->setCell(5, 0, "Mac:");
  tableOverview->setCell(5, 1, WiFi.macAddress().c_str());
  tableOverview->setCell(6, 0, "Uptime:");
  tableOverview->setCell(6, 1, uptime);
  tableOverview->setCell(7, 0, "Build:");
  tableOverview->setCell(7, 1, buildTimeFirmware);
  tableOverview->setCell(8, 0, "Heap memory:");
  tableOverview->setCell(8, 1, (int)ESP.getFreeHeap());

  tableOverview->setCell(9, 0, "Active:");
  
  char timeSlot[25] = {0}; 
  if (autoMode){
    if (activeSchedule > 0){
      global.data.schedule[activeSchedule].toCharArray(timeSlot);
      tableOverview->setCell(9, 1, timeSlot);
    } else {
      tableOverview->setCell(9, 1, "auto(not defined)");
    }
  } else {
    if (digitalRead(PIN_RELAY) == HIGH){
      tableOverview->setCell(9, 1, "manual(on)");
    } else {
      tableOverview->setCell(9, 1, "manual(off)");
    }
  }

  if (hasTempSensor){
    sensors.requestTemperatures();
    int r = tableOverview->append();
    tableOverview->setCell(r, 0, "Temperature:");
    tableOverview->setCell(r, 1, sensors.getTempC(tempAddress));
  }

  htmlPage->append(tableOverview); 
  HtmlTable *tableSchedule = new HtmlTable(2);
  tableSchedule->append();
  tableSchedule->getStyle()->append("width", "300px");
  tableSchedule->getCellAttribute(0, 0)->append("colspan", 2);
  tableSchedule->getCellAttribute(0, 0)->append("align", "center");
  tableSchedule->getStyle()->append("border", "1px solid #cdd0d4");
  tableSchedule->getCellStyle(0, 0)->append("background-color", "#cdd0d4");
  tableSchedule->setCell(0, 0, "Schedule");
 
  int r = 1;
  for (int i = 0; i < MAX_SCHEDULE; i++){
    if (!global.data.schedule[i].enabled)
      continue;
    global.data.schedule[i].toCharArray(timeSlot); 
    tableSchedule->append();
    tableSchedule->setCell(r, 0, i);
    tableSchedule->setCell(r, 1, timeSlot);
    r++;
  }

  htmlPage->append(tableSchedule);

  if (upsinfo.isConnectedUps()){
    HtmlTable *tableUps = new HtmlTable(2);
    tableUps->append(6);
    tableUps->getStyle()->append("width", "300px");
    tableUps->getCellAttribute(0, 0)->append("colspan", 2);
    tableUps->getCellAttribute(0, 0)->append("align", "center");
    tableUps->getStyle()->append("border", "1px solid #cdd0d4");
    tableUps->getCellStyle(0, 0)->append("background-color", "#cdd0d4");
    tableUps->setCell(0, 0, "UPS");
   
    tableUps->setCell(1, 0, "Voltage:");
    tableUps->setCell(1, 1, upsinfo.getOpVoltage());
    tableUps->setCell(2, 0, "Frequency:");
    tableUps->setCell(2, 1, upsinfo.getIpFrequency());
    tableUps->setCell(3, 0, "Battery Vol:");
    tableUps->setCell(3, 1, upsinfo.getBatVoltage());
    tableUps->setCell(4, 0, "Temperature:");
    tableUps->setCell(4, 1, upsinfo.getTemperature());
    tableUps->setCell(5, 0, "Is Online:");
    if (!upsinfo.getStatus().option.UtilityFail){
      tableUps->setCell(5, 1, "on");
    } else {
      tableUps->setCell(5, 1, "off");
    }
    htmlPage->append(tableUps);
  }
    
  char html_overiview[1900];
  html_overiview[0] = '\0';
  htmlPage->print(html_overiview);
  server.send(200, "text/html", html_overiview);
  delete htmlPage;
}

char* makeAPIUrl(char *buf, char *url){
  if (WiFi.getMode()==WIFI_STA){
    sprintf(buf, "http://%s%s", WiFi.localIP().toString().c_str(), url);
  } else {
    sprintf(buf, "http://%s%s", WiFi.softAPIP().toString().c_str(), url);
  }
  return buf;
}

void pageAPIHelp() {
  HtmlPage *htmlPage = new HtmlPage();
  char buf[50];
  htmlPage->append(new HtmlLink(makeAPIUrl(buf, API_METHOD_INFO)));
  htmlPage->append(new HtmlTag("br"));
  htmlPage->append(new HtmlLink(makeAPIUrl(buf, API_METHOD_SETTING)));
  htmlPage->append(new HtmlTag("br"));
  htmlPage->append(new HtmlLink(makeAPIUrl(buf, API_METHOD_RESET)));
  htmlPage->append(new HtmlTag("br"));
  htmlPage->append(new HtmlLink(makeAPIUrl(buf, API_METHOD_RELAY)));
  htmlPage->append(new HtmlTag("br"));
  htmlPage->append(new HtmlLink(makeAPIUrl(buf, API_METHOD_RELAY_ON)));
  htmlPage->append(new HtmlTag("br"));
  htmlPage->append(new HtmlLink(makeAPIUrl(buf, API_METHOD_RELAY_OFF)));
  htmlPage->append(new HtmlTag("br"));
  htmlPage->append(new HtmlLink(makeAPIUrl(buf, API_METHOD_RELAY_AUTO)));
  htmlPage->append(new HtmlTag("br"));
  htmlPage->append(new HtmlLink(makeAPIUrl(buf, API_METHOD_SENSORS)));
  htmlPage->append(new HtmlTag("br"));
  htmlPage->append(new HtmlLink(makeAPIUrl(buf, API_METHOD_SCHEDULE)));
  
  char html_overiview[1500];
  html_overiview[0] = '\0';
  htmlPage->print(html_overiview);
  server.send(200, "text/html", html_overiview);
  delete htmlPage;
}

void pageNotFound() {
  HtmlPage *htmlPage = new HtmlPage();
  char buf[50];
  HtmlTag *headText = new HtmlTag("h1");
  headText->setText("Page not found. Please see API information in link below");
  htmlPage->append(headText);
  htmlPage->append(new HtmlTag("br"));
  htmlPage->append(new HtmlLink(makeAPIUrl(buf, API_METHOD_API_HELP)));
  
  char html_overiview[1000];
  html_overiview[0] = '\0';
  htmlPage->print(html_overiview);
  server.send(404, "text/html", html_overiview);
  delete htmlPage;
}

void pageNetwork() {
  if (server.args() > 0)
  {
    if ((server.hasArg(W_SSID)) &&
        (server.hasArg(W_PASS)))
    {
      String ssid = server.arg(W_SSID);
      String psw = server.arg(W_PASS);
      server.send(200, "text/html", "The wifi connection will be changed.");

      if (connectTo(ssid.c_str(), psw.c_str()))
      {
        global.data.ssid[0] = '\0';
        global.data.ssidPass[0] = '\0';
        strcpy(global.data.ssid, ssid.c_str());
        strcpy(global.data.ssidPass, psw.c_str());
      } 
      global.save();
      return;
    }
  }

  HtmlPage *page = new HtmlPage();
  makeTabs(page, 1);

  HtmlForm *formNetwork = new HtmlForm("form_network", "/network.html", Get);
  HtmlTable *tableWifi = new HtmlTable(2);
  tableWifi->append(4);
  tableWifi->setCell(0, 1, "Connection");
  tableWifi->getCellStyle(0, 1)->append("background-color", "#cdd0d4");
  tableWifi->getCellAttribute(0, 1)->append("colspan", 2);
  tableWifi->getCellAttribute(0, 1)->append("align", "center");
  tableWifi->setCell(1, 0, "Ssid:");
  tableWifi->getCellAttribute(1, 0)->append("width", "100px");
  tableWifi->getStyle()->append("border", "1px solid #cdd0d4");
  tableWifi->getStyle()->append("width", "300px");
  HtmlText *ssid = new HtmlText();
  ssid->setName(W_SSID);
  ssid->setSize(26);
  ssid->setValue(global.data.ssid);
  tableWifi->setCell(1, 1, ssid);
  tableWifi->setCell(2, 0, "Password:");
  HtmlPassword *ssidPass = new HtmlPassword();
  ssidPass->setName(W_PASS);
  ssidPass->setSize(26);
  ssidPass->setValue(global.data.ssidPass);
  tableWifi->setCell(2, 1, ssidPass);
  tableWifi->getCellAttribute(3, 0)->append("colspan", 2);
  tableWifi->getCellAttribute(3, 0)->append("align", "right");

  HtmlButton *butConnect = new HtmlButton("Connect");
  butConnect->getAttributes()->append("class", "button");

  tableWifi->setCell(3, 0, butConnect);
  formNetwork->append(tableWifi);
 
  page->append(formNetwork);
  char html_network[1900];
  html_network[0] = '\0';
  page->print(html_network);
  server.send(200, "text/html", html_network);
  delete page;
}

void syncronizeNTP_RTC() {
  int y, mm, d, h, m, s;
  if (ntpTime(global.data.ntpHost, y, mm, d, h, m, s))
  {
    clock.setYear(y - 2000);
    clock.setMonth(mm);
    clock.setDate(d);
    clock.setHour(h);
    clock.setMinute(m);
    clock.setSecond(s);
    tsNtptime = millis() + HOUR_MS * 12; 
  }
}

void setup() {
  Serial.begin(2400);
  Serial.swap(); 
  
  delay(50);

  pinMode(PIN_RELAY, OUTPUT);
  digitalWrite(PIN_RELAY, LOW);
 
  sprintf(buildTimeFirmware, "%s %s", __DATE__, __TIME__);
  sprintf(deviceName, "%s_%d", DEVICE_NAME, ESP.getChipId());

  Wire.begin();

  sensors.begin();
  sensors.requestTemperatures();
  int deviceCount = sensors.getDeviceCount();
  for (int i = 0; i < deviceCount; i++) {
    sensors.getAddress(tempAddress, i);
    sensors.setResolution(tempAddress, 11);
    hasTempSensor = true;
    break;    
  }

  WiFi.hostname(deviceName);
  //Web Pages
  server.on(API_METHOD_MAIN, pageOverview);
  server.on(API_METHOD_NETWORK, pageNetwork);
  server.on(API_METHOD_INFO, callAPIInfo);
  server.on(API_METHOD_SETTING, callAPISetting);
  server.on(API_METHOD_SCHEDULE, callAPISchedule);
  server.on(API_METHOD_RESET, callAPIReset);
  server.on(API_METHOD_RELAY, callAPIRelay);
  server.on(API_METHOD_RELAY_ON, callAPIRelayOn);
  server.on(API_METHOD_RELAY_OFF, callAPIRelayOff);
  server.on(API_METHOD_RELAY_AUTO, callAPIAutoMode);
  server.on(API_METHOD_SENSORS, callAPISensor);
  server.on(API_METHOD_API_HELP, pageAPIHelp); 
  server.onNotFound(pageNotFound);
  autoMode = global.data.autoMode;
  //API
  server.begin();
}

void loop() { 
  server.handleClient();
  upsinfo.update();
  ESP.wdtFeed();
  global.maintenance();
  ESP.wdtFeed(); 
  if ((tsReconnect == 0)||(tsReconnect < millis()))
  { 
    bool changedConnection = false;
    if ((!WiFi.isConnected()))
    {
      if (strlen(global.data.ssid) > 0)
        connectTo(global.data.ssid, global.data.ssidPass);
      
      if (!WiFi.isConnected())
        createAP(deviceName, DEFAULT_SSID_PSW);
      
      tsNtptime = 0;
    }

    tsReconnect = millis() + TIMEOUT_RECONNECT;
  }

  if ((autoMode)&&((tsRelayUpdate == 0)||(tsRelayUpdate < millis()))){
    DateTime now = rtc.now();
    activeSchedule = global.getActiveSchedule(now);
    if (activeSchedule >= 0){
      digitalWrite(PIN_RELAY, global.data.schedule[activeSchedule].state);
    } else {
      digitalWrite(PIN_RELAY, global.data.alwaysOn);
    }

    tsRelayUpdate = millis() + MIN_MS; 
    ESP.wdtFeed();
  }

  if (WiFi.isConnected()) {
    if (tsNtptime < millis()) {
      syncronizeNTP_RTC();
      tsNtptime = millis() + HOUR_MS * 12;
    }
  }

  echoServer.maintenance();
}
