#include "wled.h"

class pwm_control : public Usermod {
private:
  bool enabled = false;
  int8_t pwmPin = -1;
  uint16_t pwmValue = 0;       // 0-255 mapped from PWM
  uint32_t lastPulseTime = 0;
  uint32_t pulseStart = 0;
  uint32_t lastPinState = 0;
  bool pulseActive = false;

  // Configuration: what PWM controls
  // 0=brightness, 1=effect, 2=speed, 3=hue
  uint8_t pwmMode = 0;

  // For debouncing PWM readings
  uint16_t filteredValue = 0;
  static const uint8_t FILTER_SAMPLES = 4;
  uint16_t filterBuffer[FILTER_SAMPLES] = {0};
  uint8_t filterIndex = 0;

  static const char _name[];
  static const char _enabled[];
  static const char _pin[];
  static const char _mode[];

  void updatePWMValue(uint32_t pulseWidth) {
    // Map 1000-2000µs to 0-255
    if (pulseWidth < 1000) pulseWidth = 1000;
    if (pulseWidth > 2000) pulseWidth = 2000;

    uint16_t mapped = ((pulseWidth - 1000) * 255) / 1000;

    // Add to filter buffer
    filterBuffer[filterIndex] = mapped;
    filterIndex = (filterIndex + 1) % FILTER_SAMPLES;

    // Average filter
    uint32_t sum = 0;
    for (uint8_t i = 0; i < FILTER_SAMPLES; i++) {
      sum += filterBuffer[i];
    }
    filteredValue = sum / FILTER_SAMPLES;
    pwmValue = filteredValue;
    lastPulseTime = millis();
  }

public:
  void setup() override {
    if (pwmPin >= 0) {
      pinMode(pwmPin, INPUT);
      enabled = true;
    }
  }

  void loop() override {
    if (!enabled || pwmPin < 0) return;

    // Poll pin state for PWM pulse detection
    uint32_t now = micros();
    uint32_t pinState = digitalRead(pwmPin);

    // Rising edge detected
    if (pinState && !lastPinState) {
      pulseStart = now;
      pulseActive = true;
    }
    // Falling edge detected
    else if (!pinState && lastPinState && pulseActive) {
      uint32_t pulseWidth = now - pulseStart;
      updatePWMValue(pulseWidth);
      pulseActive = false;
    }

    lastPinState = pinState;

    // Apply PWM control based on mode
    // Only update if we received a pulse recently (within last 500ms)
    if (millis() - lastPulseTime < 500) {
      switch (pwmMode) {
        case 0: // Brightness
          bri = pwmValue;
          break;
        case 1: // Effect
          // Map 0-255 to effect index
          {
            uint8_t modeCount = strip.getModeCount();
            if (modeCount > 0) {
              effectCurrent = (pwmValue * modeCount) / 255;
              if (effectCurrent >= modeCount) effectCurrent = modeCount - 1;
            }
          }
          break;
        case 2: // Speed
          effectSpeed = pwmValue;
          break;
        case 3: // Hue
          {
            uint16_t hue = (pwmValue * 65535UL) / 255;
            colorHStoRGB(hue, 255, colPri);
          }
          break;
      }
    }
  }

  void addToJsonInfo(JsonObject& root) override {
    if (!enabled) return;

    JsonObject user = root["u"];
    if (user.isNull()) user = root.createNestedObject("u");

    JsonArray arr = user.createNestedArray(FPSTR(_name));
    arr.add(pwmValue);
    arr.add(F(" ("));
    arr.add((1000 + ((pwmValue * 1000) / 255)));
    arr.add(F("µs)"));
  }

  void addToConfig(JsonObject& root) override {
    JsonObject top = root.createNestedObject(FPSTR(_name));
    top[FPSTR(_enabled)] = enabled;
    top[FPSTR(_pin)] = pwmPin;
    top[FPSTR(_mode)] = pwmMode;
  }

  bool readFromConfig(JsonObject& root) override {
    JsonObject top = root[FPSTR(_name)];
    bool configComplete = !top.isNull();

    configComplete &= getJsonValue(top[FPSTR(_pin)], pwmPin, -1);
    configComplete &= getJsonValue(top[FPSTR(_mode)], pwmMode, 0);
    configComplete &= getJsonValue(top[FPSTR(_enabled)], enabled, false);

    return configComplete;
  }

  void appendConfigData() override {
    oappend(F("addInfo('"));
    oappend(String(FPSTR(_name)).c_str());
    oappend(F(":mode',1,'0=Brightness, 1=Effect, 2=Speed, 3=Hue');"));
  }

  uint16_t getId() override {
    return USERMOD_ID_PWM_INPUT;
  }
};

const char pwm_control::_name[]    PROGMEM = "PWMInput";
const char pwm_control::_enabled[] PROGMEM = "enabled";
const char pwm_control::_pin[]     PROGMEM = "pin";
const char pwm_control::_mode[]    PROGMEM = "mode";

static pwm_control pwm_input;
REGISTER_USERMOD(pwm_input);
