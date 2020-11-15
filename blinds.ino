#include <Arduino.h>
#include <arduino_homekit_server.h>
#include "Helper.h"
#include "pins.h"
#include "wifi.h"
#include <EasyButton.h>

#define LOG_D(fmt, ...)   printf_P(PSTR(fmt "\n") , ##__VA_ARGS__);

#define MOTOR_SPEEED 500

enum mode {
  NORMAL,
  CALIBRATE
};

enum calibrationStep {
  NONE,
  INIT,
  UP_KNOWN
};

mode currentMode = NORMAL;
calibrationStep currentCalibrationStep = NONE;

EasyButton mainButton(BUTTON_MAIN);
EasyButton upButton(BUTTON_UP_PIN);
EasyButton downButton(BUTTON_DOWN_PIN);
Helper helper = Helper();

int maxSteps = 0, currentStep = 0, targetStep = 0;
const uint32_t startupTime = millis();
uint32_t lastMovementTime = 0;

void setup() {
  pinMode(LED_PIN, OUTPUT);
  pinMode(BUTTON_MAIN, INPUT);
  pinMode(STEP, OUTPUT);
  pinMode(DIR, OUTPUT);
  pinMode(ENGINE_CONTROLLER, OUTPUT);

  Serial.begin(115200);

  loadConfig();

  if(maxSteps == 0) {
    enableCalibrationMode();
  }

  wifiConnect();
  homekitSetup();

  mainButton.begin();
  upButton.begin();
  downButton.begin();
}

void loop() {
  mainButton.read();
  upButton.read();
  downButton.read();

  handleButtons();
  properLedDisplay();
  handleEngineControllerActivity();

  homekitLoop();
  blindControl();
  delay(10);
}

static uint32_t nextLedMillis = 0;

void properLedDisplay() {
  if(currentMode == CALIBRATE) {

    const uint32_t t = millis();
    if (t > nextLedMillis) {

      nextLedMillis = t + 0.25 * 1000;
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    }

    return;
  }

  digitalWrite(LED_PIN, LOW);
}

void reset() {
  WiFiManager wifiManager;
  helper.resetsettings(wifiManager);
  homekit_storage_reset();

  LOG_D("Stored settings removed");
}

//==============================
// HomeKit setup and loop
//==============================

extern "C" homekit_characteristic_t currentPosition;
extern "C" homekit_characteristic_t targetPosition;
extern "C" homekit_characteristic_t positionState;

homekit_value_t currentPositionGet() {
  return currentPosition.value;
}
homekit_value_t targetPositionGet() {
  return targetPosition.value;
}
homekit_value_t positionStateGet() {
  return positionState.value;
}

void currentPositionSet(homekit_value_t value) {
  currentPosition.value = value;
}
void targetPositionSet(homekit_value_t value) {
  targetPosition.value = value;
}
void positionStateSet(homekit_value_t value) {
  positionState.value = value;
}

extern "C" homekit_server_config_t config;

void handleEngineControllerActivity() {
  if(lastMovementTime != 0 && millis() - lastMovementTime > 100) {
    lastMovementTime = 0;
    digitalWrite(ENGINE_CONTROLLER, LOW);
    saveConfig();

    if(maxSteps != 0) {
      currentPosition.value.int_value = getCurrentPosition();
      homekit_characteristic_notify(&currentPosition, currentPosition.value);
    }
  }
}

int getCurrentPosition() {
  return 100 - (int)((((float)currentStep / (float)maxSteps) + 0.005) * 100);
}

bool loadConfig() {
  LOG_D("Loading config file");

  if (!helper.loadconfig())
    return false;

  JsonVariant json = helper.getconfig();
  json.printTo(Serial);

  currentStep = json["currentStep"];
  maxSteps = json["maxSteps"];
  targetPosition.value.int_value = json["targetPositionValue"];
  currentPosition.value.int_value = getCurrentPosition();

  return true;
}

bool saveConfig() {
  LOG_D("Saving config");

  DynamicJsonBuffer jsonBuffer(500);
  JsonObject& json = jsonBuffer.createObject();

  json["currentStep"] = currentStep;
  json["maxSteps"] = maxSteps;
  json["targetPositionValue"] = targetPosition.value.int_value;

  return helper.saveconfig(json);
}

bool resetConfig() {
  DynamicJsonBuffer jsonBuffer(500);
  JsonObject& json = jsonBuffer.createObject();

  return helper.saveconfig(json);
}

int upStep = 0;

void enableCalibrationMode() {
  LOG_D("Calibrate mode");
  currentMode = CALIBRATE;
  currentCalibrationStep = INIT;
}

void handleButtons() {
  if(millis() - startupTime <= (10 * 1000)) {
      return;
  }

  if(mainButton.pressedFor(10000)) {
    reset();
    digitalWrite(LED_PIN, HIGH);

    wifiConnect();
    homekitSetup();

    digitalWrite(LED_PIN, LOW);
  }

  if(mainButton.pressedFor(5000)) {
    enableCalibrationMode();
  }

  if(currentMode == CALIBRATE) {
    if(upButton.isPressed()) {
      move(false);
    }

    if(downButton.isPressed()) {
      move(true);
    }

    if(mainButton.wasPressed()) {
      if(currentCalibrationStep == INIT) {
        currentCalibrationStep = UP_KNOWN;
        upStep = currentStep;

        digitalWrite(LED_PIN, HIGH);
        delay(500);
        digitalWrite(LED_PIN, LOW);
      } else if(currentCalibrationStep == UP_KNOWN) {
        maxSteps = currentStep - upStep;
        currentStep = maxSteps;

        targetPosition.value.int_value = 0;
        homekit_characteristic_notify(&targetPosition, targetPosition.value);

        currentPosition.value.int_value = 0;
        homekit_characteristic_notify(&currentPosition, currentPosition.value);

        currentMode = NORMAL;

        saveConfig();

        LOG_D("Device calibrated, max steps: %d", maxSteps);
      }
    }
  } else {
    if(upButton.isPressed()) {
      targetPosition.value.int_value = 100;
      homekit_characteristic_notify(&targetPosition, targetPosition.value);
    }

    if(downButton.isPressed()) {
      targetPosition.value.int_value = 0;
      homekit_characteristic_notify(&targetPosition, targetPosition.value);
    }

    if(mainButton.isPressed()) {
      targetPosition.value.int_value = currentPosition.value.int_value;
      homekit_characteristic_notify(&targetPosition, targetPosition.value);
    }
  }
}

void blindControl() {
  if(currentMode != NORMAL) {
    return;
  }

  targetStep = ((100 - (float)targetPosition.value.int_value) / 100) * maxSteps;

  if(targetStep > currentStep) {
    move(true);

    //LOG_D("Opening to %d, current step: %d, target position: %d, current position: %d", targetStep, currentStep, targetPosition.value.int_value, currentPosition.value.int_value);
  } else if(targetStep < currentStep) {
    move(false);

    //LOG_D("Closing to %d, current step: %d, target position: %d, current position: %d", targetStep, currentStep, targetPosition.value.int_value, currentPosition.value.int_value);
  }
}

void homekitSetup() {
  currentPosition.setter = currentPositionSet;
  currentPosition.getter = currentPositionGet;

  targetPosition.setter = targetPositionSet;
  targetPosition.getter = targetPositionGet;

  positionState.setter = positionStateSet;
  positionState.getter = positionStateGet;

  arduino_homekit_setup(&config);
}

static uint32_t nextHeapMillis = 0;

void homekitLoop() {
  arduino_homekit_loop();

  const uint32_t t = millis();
  if (t > nextHeapMillis) {

    nextHeapMillis = t + 5 * 1000;
    LOG_D("Free heap: %d, HomeKit clients: %d",
        ESP.getFreeHeap(), arduino_homekit_connected_clients_count());
    LOG_D("Target position: %d", targetPosition.value.int_value);
  }
}

void move(bool down) {
  digitalWrite(ENGINE_CONTROLLER, HIGH);

  if(!down) {
    currentStep--;

    digitalWrite(DIR, HIGH);

    Serial.print("Up");
  } else {
    currentStep++;

    digitalWrite(DIR, LOW);

    Serial.print("Down");
  }

  digitalWrite(STEP, HIGH);
  delayMicroseconds(MOTOR_SPEEED);
  digitalWrite(STEP, LOW);
  delayMicroseconds(MOTOR_SPEEED);

  digitalWrite(DIR, LOW);

  lastMovementTime = millis();
}
