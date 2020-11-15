#ifndef Helper_h
#define Helper_h

#include "Arduino.h"
#include <ArduinoJson.h>
#include "FS.h"
#include <WiFiClient.h>
#include <WiFiManager.h>
#include <list>

class Helper {
  public:
    Helper();
    boolean loadconfig();
    JsonVariant getconfig();
    boolean saveconfig(JsonVariant json);

    void resetsettings(WiFiManager& wifim);

  private:
    JsonVariant _config;
    String _configfile;
};

#endif
