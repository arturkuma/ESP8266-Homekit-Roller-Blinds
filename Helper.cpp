#include "Arduino.h"
#include "Helper.h"

Helper::Helper() {
  SPIFFS.begin();
  this->_configfile = "/config.json";
}

boolean Helper::loadconfig() {
  File configFile = SPIFFS.open(this->_configfile, "r");
  if (!configFile) {
    Serial.println(F("Failed to open config file"));
    return false;
  }

  size_t size = configFile.size();
  if (size > 1024) {
    Serial.println(F("Config file size is too large"));
    return false;
  }

  // Allocate a buffer to store contents of the file.
  //std::unique_ptr<char[]> buf(new char[size]);

  // We don't use String here because ArduinoJson library requires the input
  // buffer to be mutable. If you don't use ArduinoJson, you may as well
  // use configFile.readString instead.
  //configFile.readBytes(buf.get(), size);

  //No more memory leaks
  DynamicJsonBuffer jsonBuffer(300);

  //Reading from buffer breaks first key
  //this->_config = jsonBuffer.parseObject(buf.get());

  //Reading directly from file DOES NOT cause currentPosition to break
  this->_config = jsonBuffer.parseObject(configFile);

  //Avoid leaving opened files
  configFile.close();

  if (!this->_config.success()) {
    Serial.println("Failed to parse config file");
    return false;
  }
  return true;
}

JsonVariant Helper::getconfig() {
  return this->_config;
}

boolean Helper::saveconfig(JsonVariant json) {
  File configFile = SPIFFS.open(this->_configfile, "w");
  if (!configFile) {
    Serial.println("Failed to open config file for writing");
    return false;
  }

  json.printTo(configFile);
  configFile.flush(); //Making sure it's saved

  Serial.println("Saved JSON to SPIFFS");
  json.printTo(Serial);
  Serial.println();
  return true;
}

void Helper::resetsettings(WiFiManager& wifim) {
  SPIFFS.format();
  wifim.resetSettings();
}
