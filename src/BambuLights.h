#ifndef BAMBULIGHTS_H
#define BAMBULIGHTS_H

#include <stdint.h>
#include <ConfigItem.h>
#include <NeoPixelBus.h>
#include <FastLED.h>
#include "Tower.h"

class BambuLights {
public:
  BambuLights(int pin);

  enum Patterns { constant, pulse, num_patterns };
  enum State { noWiFi, noPrinter, printer, printing, no_lights, white, error, warning, finished };

  const static String patterns_str[num_patterns];

  // Human readable name for the Info page.
  static const char* stateName(State state);

  static CompositeConfigItem& getAllConfig();
  static CompositeConfigItem& getNoWiFiConfig();
  static CompositeConfigItem& getNoPrinterConnectedConfig();
  static CompositeConfigItem& getPrinterConnectedConfig();
  static CompositeConfigItem& getPrintingConfig();
  static CompositeConfigItem& getWarningConfig();
  static CompositeConfigItem& getErrorConfig();
  static CompositeConfigItem& getFinishedConfig();
  static ByteConfigItem& getIdleTimeout() { static ByteConfigItem timeout("timeout", 5); return timeout; }
  static ByteConfigItem& getLightMode() { static ByteConfigItem light_mode("light_mode", 1); return light_mode; } /* 0 = white, 1 = reactive */
  static BooleanConfigItem& getLightState() { static BooleanConfigItem light_state("light_state", 1); return light_state; } /* true == on, false == off */
  static BooleanConfigItem& getChamberSync() { static BooleanConfigItem chamber_sync("chamber_sync", 1); return chamber_sync; }
  static ByteConfigItem& getLedType() { static ByteConfigItem led_type("led_type", 0); return led_type; } /* 0 = GRB, 1 = RGB */
  static ByteConfigItem& getNumLEDs() { static ByteConfigItem num_leds("num_leds", 36); return num_leds; }

  void begin();
  void loop();
  void updatePixelCount();

  void setState(State state);
  void setBrightness(byte brightness) { this->brightness = brightness; }

  // Paint the stacked tower: one condition per tier, each rendered into its
  // own segment of the strip. Replaces setState()/loop() when tower mode is
  // on. Call every frame; the patterns are time based.
  void renderTower(const Tower::Condition* conditions);

private:
  bool black = false;
  bool brightWhite = false;
  byte brightness = 255;
  int pin;
  
  NeoPixelBus <NeoGrbFeature, Neo800KbpsMethod> *pixels;
  NeoGamma<NeoGammaTableMethod> colorGamma;

  State currentState;

  // Per tier, so each segment's pattern starts from the moment its own
  // condition appeared rather than from a shared clock.
  Tower::Condition lastCondition[Tower::NUM_TIERS];
  unsigned long conditionStartMs[Tower::NUM_TIERS];

  void paintSegment(Tower::Tier tier, uint8_t hue, uint8_t sat, uint8_t val);

  CompositeConfigItem *currentConfig;
  ByteConfigItem *currentPattern;
  IntConfigItem *currentHue;
  ByteConfigItem *currentValue;
  ByteConfigItem *currentSaturation;
  ByteConfigItem *currentPulsePerMin;
  long pulseOffset = 0;

  void setCurrentConfig(CompositeConfigItem& config);

  // Pattern methods
  byte getPulseBrightness();

  void fill(uint8_t hue, uint8_t val, uint8_t sat);
  void show();
  void clear();
  void setPixelColor(uint8_t digit, uint8_t hue, uint8_t val, uint8_t sat);
  void crossFade(const CHSV& oldColor, const CHSV& newColor);
};

#endif // BAMBULIGHTS_H
