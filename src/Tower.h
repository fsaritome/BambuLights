#ifndef TOWER_H
#define TOWER_H

// Stacked status tower.
//
// The original firmware collapses everything the printer is doing into a
// single state and paints the whole strip with it. A tower instead has
// several independent segments, each answering a different question, so more
// than one can be lit at once and a dark segment is itself information:
//
//   FILAMENT   is the material OK?        dark = nothing wrong with it
//   STATUS     what is the machine doing? never dark
//   FINISHED   is there a part to collect? dark = nothing waiting
//   SYSTEM     is the rig healthy?        dark = healthy
//
// STATUS never reports faults, so it cannot contradict SYSTEM.
//
// Tiers are deliberately not exclusive of each other. Following the usual
// andon convention, SYSTEM means "a human is needed" and lights for anything
// that has stopped the machine -- including a filament runout or jam, which
// also light FILAMENT. You read SYSTEM from across the room and the specific
// tier tells you what to bring. Routine events (an AMS change) and advisory
// ones (damp filament) light only their own tier.
//
// Within a single tier the most severe live condition wins; see evaluate().

#include <stdint.h>
#include <ConfigItem.h>

namespace Tower {

enum Tier {
    TIER_FILAMENT = 0,
    TIER_STATUS,
    TIER_FINISHED,
    TIER_SYSTEM,
    NUM_TIERS
};

// OFF must stay first: it is the "nothing to report" condition and is not
// configurable, it simply leaves the segment dark.
enum Condition {
    COND_OFF = 0,

    COND_FIL_CHANGING,
    COND_FIL_RUNOUT,
    COND_FIL_JAM,
    COND_FIL_AMS_LOST,
    COND_FIL_DAMP,

    COND_ST_IDLE,
    COND_ST_PRINTING,
    COND_ST_PAUSED,

    COND_FIN_READY,

    COND_SYS_NO_WIFI,
    COND_SYS_NO_PRINTER,
    COND_SYS_WARNING,
    COND_SYS_ERROR,

    NUM_CONDITIONS
};

enum Pattern {
    PAT_CONSTANT = 0,
    PAT_PULSE,      // smooth exponential breath, as upstream
    PAT_BLINK,      // hard on/off square wave
    PAT_FADE,       // linear triangle ramp
    PAT_RAINBOW,    // cycles hue over time and across the segment
    NUM_PATTERNS
};

// Everything the tiers are decided from. Filled once per loop in main.cpp so
// evaluate() stays free of Arduino and MQTT dependencies and can be reasoned
// about (and tested) on its own.
struct Facts {
    bool wifiConnected    = false;
    bool printerConnected = false;
    bool printing         = false;  // broker reports an active print
    int  stage            = -1;     // stg_cur, see CURRENT_STAGE_IDS
    bool hmsWarning       = false;
    bool hmsError         = false;
    bool printError       = false;
    bool finishedPending  = false;  // print done, not yet collected
    bool filamentChanging = false;  // ams tray_now != tray_tar
    // Worst AMS humidity level. The scale is inverted from the obvious
    // reading: 5 is driest (grade A in the Bambu app), 1 is wettest (grade E).
    // 0 means not reported.
    int  worstHumidity    = 0;
};

// One configurable appearance: colour, pattern and rate.
struct Look {
    BooleanConfigItem   colors;     // UI: section collapsed or not
    ByteConfigItem      pattern;
    IntConfigItem       hue;
    ByteConfigItem      value;      // brightness
    ByteConfigItem      saturation;
    ByteConfigItem      rate;       // cycles per minute
    BaseConfigItem*     set[7];
    CompositeConfigItem composite;

    Look(const char* name, byte pat, int h, byte sat, byte val, byte rateCpm);
};

// Where a tier lives on the strip.
struct Segment {
    ByteConfigItem      first_led;
    ByteConfigItem      count;
    BaseConfigItem*     set[3];
    CompositeConfigItem composite;

    Segment(const char* name, byte first, byte count);
};

// Builds the config objects. Must run before getConfig() is registered.
void begin();

CompositeConfigItem& getConfig();

// Humidity level at or below which the filament tier reports damp. The scale
// runs 5 (driest, grade A in the Bambu app) down to 1 (wettest, grade E), so
// this is an upper bound on the worst unit. 0 disables the condition.
ByteConfigItem& getDampLevel();

// When the Finished tier lights up, paint the whole strip with it rather than
// just its own segment, so a completed print takes over the tower.
BooleanConfigItem& getFinishedTakeover();

// Hue for the rainbow pattern: cycles with time, and spreads across a segment
// so a multi-LED tier shows a gradient rather than one flat colour.
uint8_t rainbowHue(byte rateCpm, unsigned long nowMs, unsigned long offsetMs,
                   uint16_t pixel, uint16_t span);
Look&    look(Condition c);
Segment& segment(Tier t);

// Most severe live condition for this tier, or COND_OFF.
Condition evaluate(Tier t, const Facts& f);

// Every condition belonging to a tier, for the Test buttons to cycle through.
// Returns how many were written.
int tierConditions(Tier t, Condition* out, int maxOut);

// Brightness for a pattern at a point in time. `value` is the configured
// peak, `rateCpm` cycles per minute, `offsetMs` when the condition started.
byte patternBrightness(byte pattern, byte value, byte rateCpm,
                       unsigned long nowMs, unsigned long offsetMs);

const char* conditionName(Condition c);
const char* tierName(Tier t);

// 0xRRGGBB for this condition's configured colour, so the Info page can show
// the same colour the LED is showing. COND_OFF returns a dark swatch.
uint32_t conditionColor(Condition c);

extern const char* PATTERN_NAMES[NUM_PATTERNS];

}  // namespace Tower

#endif  // TOWER_H
