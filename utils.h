#include <Arduino.h>
#include <WiFiUDP.h>
#include <ESP.h>
#include <IPAddress.h>
#include <TimeLib.h>
#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>

#define DAY_MS 86400000l
#define HOUR_MS 3600000l
#define MIN_MS 60000l
#define SEC_MS 1000l
#define NTP_PACKET_SIZE 48

bool ntpTime(char *host, int &tYear, int &tMonth, int &tDay, int &tHour, int &tMin, int &tSec)
{
  unsigned long time_ms = 0;
  bool result = false;
  WiFiUDP *udp = new WiFiUDP();
  udp->begin(321);

  byte packetBuf[NTP_PACKET_SIZE];
  memset(packetBuf, 0, NTP_PACKET_SIZE);
  packetBuf[0] = 0b11100011;
  packetBuf[1] = 0;
  packetBuf[2] = 6;
  packetBuf[3] = 0xEC;
  packetBuf[12] = 49;
  packetBuf[13] = 0x4E;
  packetBuf[14] = 49;
  packetBuf[15] = 52;

  IPAddress timeServerIP;
  WiFi.hostByName(host, timeServerIP);

  udp->beginPacket(timeServerIP, 123);
  udp->write(packetBuf, NTP_PACKET_SIZE);
  udp->endPacket();
  delay(1000);

  int respond = udp->parsePacket();
  if (respond > 0)
  {
    udp->read(packetBuf, NTP_PACKET_SIZE);
    unsigned long highWord = word(packetBuf[40], packetBuf[41]);
    unsigned long lowWord = word(packetBuf[42], packetBuf[43]);
    int32_t secsSince1900 = highWord << 16 | lowWord;
    int32_t time_ms = secsSince1900 - 2208988800UL + 3 * 3600L;
    tDay = day(time_ms);
    tMonth = month(time_ms);
    tYear = year(time_ms);
    tSec = second(time_ms);
    tMin = minute(time_ms);
    tHour = hour(time_ms);
    result = true;
  }
  delete udp;
  return result;
}

bool initWifi(){
    WiFi.setAutoReconnect(false);
    WiFi.setAutoConnect(false);
}

bool connectTo(const char *ssid, const char *psw)
{
    WiFi.disconnect();
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, psw);
    
    ESP.wdtDisable();
    WiFi.waitForConnectResult();
    ESP.wdtEnable(0);
    return WiFi.isConnected();
}

void createAP(const char *ssid, const char *pass)
{
    WiFi.mode(WIFI_AP);
    WiFi.softAP(ssid, pass);
    WiFi.softAPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 0, 0));
}


void upTime(char* uptime)
{
  unsigned long timestamp = millis();
  int days = timestamp / DAY_MS;
  unsigned long leftOfDay = timestamp - (days * DAY_MS);
  int hours = leftOfDay / HOUR_MS;
  unsigned long leftOfHour = leftOfDay - (hours * HOUR_MS);
  int mins = leftOfHour / MIN_MS;

  String d = String(days);
  String h = String(hours);
  String m = String(mins);
  sprintf(uptime, "%s day %s hour %s min", d.c_str(), h.c_str(), m.c_str());
}

void addr2str(DeviceAddress deviceAddress, char out[17])
{
  static char *hex = "0123456789ABCDEF";
  uint8_t i, j;
  for (i = 0, j = 0; i < 8; i++)
  { 
    out[j++] = hex[deviceAddress[i] / 16];
    out[j++] = hex[deviceAddress[i] & 15];
  }
  out[j] = '\0';
}

bool addrcmp(const DeviceAddress one, const DeviceAddress two) {
  for (int i = 0; i < 8; i++){
    if (one[i] != two[i]){
      return false;
    }
  }
  return true;
}