#include <Arduino.h>
//#include <WiFi.h>
#include "WiFiMulti.h"
//#include <dataWifi.h>

bool mySmartWifiConfig()
{
  WiFi.mode(WIFI_MODE_STA);
  Serial.println("Turn on smart distribution network");
  WiFi.beginSmartConfig();
  for (size_t i = 0; i < 5; i++)
  {
    Serial.print(".");
    delay(500);
    if (WiFi.smartConfigDone())
    {
      Serial.println("Network distribution successful");
      Serial.printf("SSID:%s", WiFi.SSID().c_str());
      Serial.printf("PSW:%s", WiFi.psk().c_str());
      return 1;
    }
  }
  Serial.println("Unable to automatically configure network!");
  return 0;
}

bool autoConfig()
{  
  WiFi.disconnect(true,true);  
  WiFi.begin();
  for (size_t i = 0; i < 5; i++)
  {
    int wifiStatus = WiFi.status();
    if (wifiStatus == WL_CONNECTED)
    {
      Serial.println("Automatic connection successful!");
      Serial.printf("SSID:%s", WiFi.SSID().c_str());
      Serial.printf("PSW:%s", WiFi.psk().c_str());
      Serial.println();
      return 1;
    }
    else
    {
      delay(500);
      Serial.println("Waiting for automatic network configuration...");
    }
  }
  Serial.println("Unable to automatically configure network!");
  WiFi.disconnect(true,true);    
  return 0;
}