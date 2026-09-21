#include "BambuLights.h"
#include <math.h>

//#define DEBUG_COLORS
//#define DEBUG_FADE

const char* BambuLights::stateName(State state) {
    switch (state) {
        case noWiFi:     return "No WiFi";
        case noPrinter:  return "No printer";
        case printer:    return "Idle";
        case printing:   return "Printing";
        case no_lights:  return "Off";
        case white:      return "White";
        case error:      return "Error";
        case warning:    return "Warning";
        case finished:   return "Finished";
    }
    return "Unknown";
}

CompositeConfigItem& BambuLights::getNoWiFiConfig() {
    static BooleanConfigItem colors("colors", true);  // Collapsed
    static ByteConfigItem pattern("pattern", pulse);
    static IntConfigItem  hue("hue", 203);
    static ByteConfigItem value("value", 255);
    static ByteConfigItem saturation("saturation", 255);
    static ByteConfigItem pulse_per_min("pulse_per_min", 10);

    static BaseConfigItem* configSet[] {
        &colors,
        &pattern,
        &hue,
        &value,
        &saturation,
        &pulse_per_min,
        0
    };

    static CompositeConfigItem config("noWiFi", 0, configSet);

    return config;
}

CompositeConfigItem& BambuLights::getNoPrinterConnectedConfig() {
    static BooleanConfigItem colors("colors", true);  // Collapsed
    static ByteConfigItem pattern("pattern", constant);
    static IntConfigItem  hue("hue", 203);
    static ByteConfigItem value("value", 255);
    static ByteConfigItem saturation("saturation", 255);
    static ByteConfigItem pulse_per_min("pulse_per_min", 7);

    static BaseConfigItem* configSet[] {
        &colors,
        &pattern,
        &hue,
        &value,
        &saturation,
        &pulse_per_min,
        0
    };

    static CompositeConfigItem config("noPrinterConnected", 0, configSet);

    return config;
}

CompositeConfigItem& BambuLights::getPrinterConnectedConfig() {
    static BooleanConfigItem colors("colors", true);  // Collapsed
    static ByteConfigItem pattern("pattern", constant);
    static IntConfigItem  hue("hue", 0);
    static ByteConfigItem value("value", 128);
    static ByteConfigItem saturation("saturation", 0);
    static ByteConfigItem pulse_per_min("pulse_per_min", 7);

    static BaseConfigItem* configSet[] {
        &colors,
        &pattern,
        &hue,
        &value,
        &saturation,
        &pulse_per_min,
        0
    };

    static CompositeConfigItem config("printerConnected", 0, configSet);

    return config;
}

CompositeConfigItem& BambuLights::getPrintingConfig() {
    static BooleanConfigItem colors("colors", true);  // Collapsed
    static ByteConfigItem pattern("pattern", constant);
    static IntConfigItem  hue("hue", 0);
    static ByteConfigItem value("value", 255);
    static ByteConfigItem saturation("saturation", 0);
    static ByteConfigItem pulse_per_min("pulse_per_min", 7);

    static BaseConfigItem* configSet[] {
        &colors,
        &pattern,
        &hue,
        &value,
        &saturation,
        &pulse_per_min,
        0
    };

    static CompositeConfigItem config("printing", 0, configSet);

    return config;
}

CompositeConfigItem& BambuLights::getErrorConfig() {
    static BooleanConfigItem colors("colors", true);  // Collapsed
    static ByteConfigItem pattern("pattern", pulse);
    static IntConfigItem  hue("hue", 0);
    static ByteConfigItem value("value", 255);
    static ByteConfigItem saturation("saturation", 255);
    static ByteConfigItem pulse_per_min("pulse_per_min", 7);

    static BaseConfigItem* configSet[] {
        &colors,
        &pattern,
        &hue,
        &value,
        &saturation,
        &pulse_per_min,
        0
    };

    static CompositeConfigItem config("error", 0, configSet);

    return config;
}

CompositeConfigItem& BambuLights::getWarningConfig() {
    static BooleanConfigItem colors("colors", true);  // Collapsed
    static ByteConfigItem pattern("pattern", constant);
    static IntConfigItem  hue("hue", 171);
    static ByteConfigItem value("value", 255);
    static ByteConfigItem saturation("saturation", 255);
    static ByteConfigItem pulse_per_min("pulse_per_min", 7);

    static BaseConfigItem* configSet[] {
        &colors,
        &pattern,
        &hue,
        &value,
        &saturation,
        &pulse_per_min,
        0
    };

    static CompositeConfigItem config("warning", 0, configSet);

    return config;
}

CompositeConfigItem& BambuLights::getFinishedConfig() {
    static BooleanConfigItem colors("colors", true);  // Collapsed
    static ByteConfigItem pattern("pattern", constant);
    static IntConfigItem  hue("hue", 63);
    static ByteConfigItem value("value", 255);
    static ByteConfigItem saturation("saturation", 255);
    static ByteConfigItem pulse_per_min("pulse_per_min", 7);

    static BaseConfigItem* configSet[] {
        &colors,
        &pattern,
        &hue,
        &value,
        &saturation,
        &pulse_per_min,
        &getIdleTimeout(),
        0
    };

    static CompositeConfigItem config("finished", 0, configSet);

    return config;
}

CompositeConfigItem& BambuLights::getAllConfig() {
    static BaseConfigItem* configSet[] {
        &getNoWiFiConfig(),
        &getNoPrinterConnectedConfig(),
        &getPrinterConnectedConfig(),
        &getPrintingConfig(),
        &getErrorConfig(),
        &getWarningConfig(),
        &getFinishedConfig(),
        &getLedType(),
        &getNumLEDs(),
        &getLightMode(),
        &getLightState(),
        &getChamberSync(),
	      0
    };

    static CompositeConfigItem config("leds", 0, configSet);

    return config;
};

BambuLights::BambuLights(int pin) :
    pixels(new NeoPixelBus <NeoGrbFeature, Neo800KbpsMethod>(getNumLEDs(), pin)),
    pin(pin),
    currentState(noWiFi)
{
    setCurrentConfig(getNoWiFiConfig());

    for (int t = 0; t < Tower::NUM_TIERS; t++) {
        lastCondition[t] = Tower::COND_OFF;
        conditionStartMs[t] = 0;
    }
}

void BambuLights::updatePixelCount() {
  if (pixels->PixelCount() != getNumLEDs()) {
    pixels->ClearTo(0);
    show();
    pixels->Dirty();
    show();
    delete pixels;
    pixels = new NeoPixelBus <NeoGrbFeature, Neo800KbpsMethod>(getNumLEDs(), pin);
    begin();
  }
}

void BambuLights::setState(State state) {
  if (currentState != state) {
    currentState = state;

// noWiFi, noPrinter, printer, printing, no_lights, white, warning, error, finished
    bool oldBlack = black;
    bool oldWhite = brightWhite;
    byte oldPattern = *currentPattern;
    black = false;
    brightWhite = false;
    CHSV oldColor = {*currentHue, *currentSaturation, *currentValue};
    if (oldPattern == pulse) {
      oldColor.v = getPulseBrightness();
    }
    // Serial.print("state set to ");Serial.println(state);
    switch (state) {
      case noPrinter:
        setCurrentConfig(getNoPrinterConnectedConfig());
        break;
      case printer:
        setCurrentConfig(getPrinterConnectedConfig());
        break;
      case printing:
        setCurrentConfig(getPrintingConfig());
        break;
      case no_lights:
        black = true;
        break;
      case white:
        brightWhite = true;
        break;
      case warning:
        setCurrentConfig(getWarningConfig());
        break;
      case error:
        setCurrentConfig(getErrorConfig());
        break;
      case finished:
        setCurrentConfig(getFinishedConfig());
        break;
      default:
        setCurrentConfig(getNoWiFiConfig());
        break;
    }

    CHSV newColor = {*currentHue, *currentSaturation, *currentValue};
    if (oldBlack != black) {
      if (black) {
        // Fade from old color to old color at zero brightness
        newColor.h = oldColor.h;
        newColor.s = oldColor.s;
        newColor.v = 0;
      } else {
        // Fade from new color at zero brightness to new color
        oldColor.h = newColor.h;
        oldColor.s = newColor.s;
        oldColor.v = 0;
      }
    }
    if (oldWhite != brightWhite) {
      if (brightWhite) {
        // Fade from old color to old color at zero saturation (aka white) and full brightness
        newColor.h = oldColor.h;
        newColor.s = 0;
        newColor.v = 255;
      } else {
        // Fade from new color at zero saturation (aka white) and full brightness to new color
        oldColor.h = newColor.h;
        oldColor.s = 0;
        oldColor.v = 255;
      }
    }

    crossFade(oldColor, newColor);

    if (*currentPattern == pulse) {
      pulseOffset = millis(); // Always start at brightest level
    }
  }
}

void BambuLights::setCurrentConfig(CompositeConfigItem& config) {
  currentConfig = &config;
  currentPattern = (ByteConfigItem*)config.get("pattern");
  currentHue = (IntConfigItem*)config.get("hue");
  currentValue = (ByteConfigItem*)config.get("value");
  currentSaturation = (ByteConfigItem*)config.get("saturation");
  currentPulsePerMin = (ByteConfigItem*)config.get("pulse_per_min");
}

void BambuLights::begin()  {
	pixels->Begin(); // This initializes the NeoPixel library.
	pixels->Show();
}

void BambuLights::loop() {
  //   enum patterns { dark, constant, rainbow, pulse, breath, num_patterns };
  uint8_t current_pattern = *currentPattern;

  if (black) {
    clear();
  } else if (brightWhite) {
    fill(255, 0, 255);
  } else {
    uint16_t val;
    switch (current_pattern) {
      case pulse:
        val = getPulseBrightness();
        break;
      default:
        val = *currentValue;
        val = val * brightness / 255;
        break;
    }
    fill(*currentHue, *currentSaturation, val);
  }
  show();
}

#ifdef DEBUG_FADE
void printCHSV(const CHSV& color) {
  Serial.print("{h=");Serial.print(color.h);
  Serial.print(",s=");Serial.print(color.s);
  Serial.print(",v=");Serial.print(color.v);
  Serial.print("}");
}
#endif

void BambuLights::crossFade(const CHSV& oldColor, const CHSV& newColor) {
#ifdef DEBUG_FADE
  Serial.print("Blending from ");printCHSV(oldColor);Serial.print(" to ");printCHSV(newColor);Serial.println("");
#endif
  TickType_t xLastWakeTime = xTaskGetTickCount();
  const TickType_t xFrequency = 2 / portTICK_PERIOD_MS; // So we will take approximately 0.75s to do the whole blend

  CHSV blendedColor;
  for (int i=0; i < 255; i++) {
    blendedColor = ::blend(oldColor, newColor, i, SHORTEST_HUES);
#ifdef DEBUG_FADE
    Serial.print("Blended color ");printCHSV(blendedColor);Serial.println("");
#endif
    fill(blendedColor.h, blendedColor.s, blendedColor.v);
    show();
    vTaskDelayUntil(&xLastWakeTime, xFrequency);
  }
}

static byte valueMin = 5;

byte BambuLights::getPulseBrightness() {
  // https://sean.voisen.org/blog/2011/10/breathing-led-with-arduino/
  float delta = (currentValue->value - valueMin) / 2.35040238;  // 2.35040238 = e - 0.36787944

  float pulse_length_millis = (60.0f * 1000) / currentPulsePerMin->value;
  float val = valueMin + (exp(cos(2 * M_PI * (millis() - pulseOffset) / pulse_length_millis)) - 0.36787944f) * delta;
  val = val * currentValue->value / 256;
  val = val * brightness / 255;

  return val;
}

void BambuLights::fill(uint8_t hue, uint8_t sat, uint8_t val) {
  static int oldHue = -1;
  static int oldSat = -1;
  static int oldVal = -1;
  static byte oldCount = -1;
  static byte oldLedType = -1;

  if (hue != oldHue || sat != oldSat || val != oldVal || oldCount != getNumLEDs() || oldLedType != getLedType()) {
    oldHue = hue;
    oldSat = sat;
    oldVal = val;
#ifdef DEBUG_COLORS
    Serial.print("Filling with ");
    Serial.print("{h=");Serial.print(hue);
    Serial.print(",s=");Serial.print(sat);
    Serial.print(",v=");Serial.print(val);
    Serial.print("}");
    Serial.println("");
#endif
    RgbColor color = HsbColor((byte)(hue)/256.0, (byte)(sat)/256.0, val/256.0);
    // color = colorGamma.Correct(color);
    if (getLedType() == 1)  { // RGB not GRB
      uint8_t oldRed = color.R;
      color.R = color.G;
      color.G = oldRed;
    }

    for (uint8_t digit=0; digit < pixels->PixelCount(); digit++) {
      pixels->SetPixelColor(digit, color);
    }
  }
}

void BambuLights::clear() {
  fill(0, 0, 0);
}

// ------------------------------------------------------------- tower mode

void BambuLights::paintSegment(Tower::Tier tier, uint8_t hue, uint8_t sat, uint8_t val) {
  Tower::Segment& seg = Tower::segment(tier);
  paintRange(seg.first_led.value, seg.count.value, hue, sat, val);
}

void BambuLights::paintRange(uint16_t first, uint16_t count,
                             uint8_t hue, uint8_t sat, uint8_t val) {
  const uint16_t total = pixels->PixelCount();

  for (uint16_t i = 0; i < count; i++) {
    const uint16_t idx = first + i;
    if (idx < total) {           // ignore ranges configured past the strip
      setPixelColor(idx, hue, sat, val);
    }
  }
}

// Rainbow needs a different hue per pixel, so it cannot go through the flat
// fill above.
void BambuLights::paintRainbow(uint16_t first, uint16_t count, Tower::Look& lk,
                               uint8_t val, unsigned long now, unsigned long since) {
  const uint16_t total = pixels->PixelCount();

  for (uint16_t i = 0; i < count; i++) {
    const uint16_t idx = first + i;
    if (idx < total) {
      const uint8_t hue = Tower::rainbowHue(lk.rate.value, now, since, i, count);
      setPixelColor(idx, hue, 255, val);
    }
  }
}

void BambuLights::renderTower(const Tower::Condition* conditions) {
  const unsigned long now = millis();

  // Master off, and the "just be a white lamp" mode, both bypass the tiers.
  if (!getLightState()) {
    pixels->ClearTo(RgbColor(0));
    show();
    return;
  }
  if (getLightMode() == 0) {
    for (uint16_t i = 0; i < pixels->PixelCount(); i++) {
      setPixelColor(i, 0, 0, 255 * brightness / 255);
    }
    show();
    return;
  }

  // Segments do not have to cover the whole strip, so start from dark rather
  // than leaving stale pixels lit where a segment was moved or shrunk.
  pixels->ClearTo(RgbColor(0));

  // A finished print can take over the whole tower rather than lighting only
  // its own tier, which is easier to notice from across the room.
  const bool takeover = Tower::getFinishedTakeover()
                     && conditions[Tower::TIER_FINISHED] != Tower::COND_OFF;

  for (int t = 0; t < Tower::NUM_TIERS; t++) {
    const Tower::Condition cond = conditions[t];

    if (cond != lastCondition[t]) {
      lastCondition[t] = cond;
      conditionStartMs[t] = now;   // restart this tier's pattern cleanly
    }

    if (takeover && t != Tower::TIER_FINISHED) {
      continue;                    // the finished tier owns the strip
    }
    if (cond == Tower::COND_OFF) {
      continue;                    // a dark tier is meaningful, leave it dark
    }

    Tower::Look& lk = Tower::look(cond);
    byte val = Tower::patternBrightness(lk.pattern.value, lk.value.value,
                                        lk.rate.value, now, conditionStartMs[t]);
    val = (byte)((uint16_t)val * brightness / 255);

    uint16_t first = Tower::segment((Tower::Tier)t).first_led.value;
    uint16_t count = Tower::segment((Tower::Tier)t).count.value;
    if (takeover) {
      first = 0;
      count = pixels->PixelCount();
    }

    if (lk.pattern.value == Tower::PAT_RAINBOW) {
      paintRainbow(first, count, lk, val, now, conditionStartMs[t]);
    } else {
      paintRange(first, count, (uint8_t)lk.hue.value, lk.saturation.value, val);
    }
  }

  show();
}

void BambuLights::show() {
  pixels->Show();
}

void BambuLights::setPixelColor(uint8_t digit, uint8_t hue, uint8_t sat, uint8_t val) {
    RgbColor color = HsbColor((byte)(hue)/256.0, (byte)(sat)/256.0, val/256.0);
    if (getLedType() == 1)  { // RGB not GRB
      uint8_t oldRed = color.R;
      color.R = color.G;
      color.G = oldRed;
    }
    pixels->SetPixelColor(digit, colorGamma.Correct(color));
}

const String BambuLights::patterns_str[BambuLights::num_patterns] = 
  { "Constant", "Pulse" };
