#include "Tower.h"
// Safe despite BambuLights.h including Tower.h: the include guards break the
// cycle, and only the header chain matters. This lets the tower config own
// the strip-wide settings without moving their accessors.
#include "BambuLights.h"

#include <math.h>
#include <set>

namespace Tower {

const char* PATTERN_NAMES[NUM_PATTERNS] = { "Constant", "Pulse", "Blink", "Fade" };

// Stage ids from MQTTBroker::CURRENT_STAGE_IDS.
static const std::set<int> STAGES_CHANGING = { 4, 22, 24 };   // changing, unloading, loading
static const std::set<int> STAGES_JAM      = { 32, 33, 35 };  // covered nozzle, cutter error, clog
static const std::set<int> STAGES_PAUSED   = { 5, 16, 30 };   // m400, user, user gcode
static const int STAGE_RUNOUT   = 6;
static const int STAGE_AMS_LOST = 26;

// Humidity is reported 1 (dry) to 5 (wet); warn from 4.
static const int HUMIDITY_DAMP = 4;

Look::Look(const char* name, byte pat, int h, byte sat, byte val, byte rateCpm)
  : colors("colors", true),
    pattern("pattern", pat),
    hue("hue", h),
    value("value", val),
    saturation("saturation", sat),
    rate("rate", rateCpm),
    composite(name, 0, set)
{
    set[0] = &colors;
    set[1] = &pattern;
    set[2] = &hue;
    set[3] = &value;
    set[4] = &saturation;
    set[5] = &rate;
    set[6] = 0;
}

Segment::Segment(const char* name, byte first, byte cnt)
  : first_led("first_led", first),
    count("count", cnt),
    composite(name, 0, set)
{
    set[0] = &first_led;
    set[1] = &count;
    set[2] = 0;
}

// ---------------------------------------------------------------- the config

static Look*    looks[NUM_CONDITIONS]  = { 0 };
static Segment* segments[NUM_TIERS]    = { 0 };
// conditions + segments + the strip-wide globals + terminator
static BaseConfigItem* towerSet[NUM_CONDITIONS + NUM_TIERS + 8] = { 0 };
static CompositeConfigItem* towerConfig = 0;

// Hues are 0-255: 0 red, 25 amber, 42 yellow, 85 green, 128 cyan, 170 blue,
// 192 purple. Saturation 0 renders white regardless of hue.
void begin() {
    if (towerConfig) {
        return;
    }

    // Static rather than heap allocated on purpose. These live for the whole
    // run, and putting ~90 small objects on the heap fragmented it enough
    // that mbedTLS could no longer find a contiguous block for the printer
    // connection ("SSL - Memory allocation failed").
    static Look lkFilChanging ("fil_changing",   PAT_PULSE,     25, 255, 255,  20);
    static Look lkFilRunout   ("fil_runout",     PAT_BLINK,      0, 255, 255,  40);
    static Look lkFilJam      ("fil_jam",        PAT_BLINK,      0, 255, 255,  60);
    static Look lkFilAmsLost  ("fil_ams_lost",   PAT_BLINK,    192, 255, 255,  30);
    static Look lkFilDamp     ("fil_damp",       PAT_FADE,     170, 255, 200,  10);

    static Look lkStIdle      ("st_idle",        PAT_CONSTANT,   0,   0,  60,  10);
    static Look lkStPrinting  ("st_printing",    PAT_CONSTANT,  85, 255, 255,  10);
    static Look lkStPaused    ("st_paused",      PAT_PULSE,     25, 255, 255,  20);

    static Look lkFinReady    ("fin_ready",      PAT_PULSE,     85, 255, 255,  12);

    static Look lkSysNoWifi   ("sys_no_wifi",    PAT_BLINK,    170, 255, 255,  30);
    static Look lkSysNoPrinter("sys_no_printer", PAT_BLINK,    128, 255, 255,  20);
    static Look lkSysWarning  ("sys_warning",    PAT_PULSE,     42, 255, 255,  20);
    static Look lkSysError    ("sys_error",      PAT_BLINK,      0, 255, 255,  60);

    // One LED per tier by default. Filament sits last in the chain; whether
    // that is the physical bottom depends on which end your data line enters,
    // and every position is editable on the Tower page anyway.
    static Segment segStatus  ("seg_status",   0, 1);
    static Segment segFinished("seg_finished", 1, 1);
    static Segment segSystem  ("seg_system",   2, 1);
    static Segment segFilament("seg_filament", 3, 1);

    looks[COND_FIL_CHANGING]   = &lkFilChanging;
    looks[COND_FIL_RUNOUT]     = &lkFilRunout;
    looks[COND_FIL_JAM]        = &lkFilJam;
    looks[COND_FIL_AMS_LOST]   = &lkFilAmsLost;
    looks[COND_FIL_DAMP]       = &lkFilDamp;
    looks[COND_ST_IDLE]        = &lkStIdle;
    looks[COND_ST_PRINTING]    = &lkStPrinting;
    looks[COND_ST_PAUSED]      = &lkStPaused;
    looks[COND_FIN_READY]      = &lkFinReady;
    looks[COND_SYS_NO_WIFI]    = &lkSysNoWifi;
    looks[COND_SYS_NO_PRINTER] = &lkSysNoPrinter;
    looks[COND_SYS_WARNING]    = &lkSysWarning;
    looks[COND_SYS_ERROR]      = &lkSysError;

    segments[TIER_STATUS]   = &segStatus;
    segments[TIER_FINISHED] = &segFinished;
    segments[TIER_SYSTEM]   = &segSystem;
    segments[TIER_FILAMENT] = &segFilament;

    int n = 0;

    // Strip-wide settings live here too, so the Tower page is the single
    // place the lights are configured and there is no separate LEDs page.
    towerSet[n++] = &BambuLights::getLightState();
    towerSet[n++] = &BambuLights::getLightMode();
    towerSet[n++] = &BambuLights::getChamberSync();
    towerSet[n++] = &BambuLights::getLedType();
    towerSet[n++] = &BambuLights::getNumLEDs();
    towerSet[n++] = &BambuLights::getIdleTimeout();

    for (int t = 0; t < NUM_TIERS; t++) {
        towerSet[n++] = &segments[t]->composite;
    }
    for (int c = COND_OFF + 1; c < NUM_CONDITIONS; c++) {
        towerSet[n++] = &looks[c]->composite;
    }
    towerSet[n] = 0;

    // Declared last so towerSet is fully populated first.
    static CompositeConfigItem cfg("tower", 0, towerSet);
    towerConfig = &cfg;
}

CompositeConfigItem& getConfig() {
    begin();
    return *towerConfig;
}

Look& look(Condition c) {
    begin();
    // COND_OFF has no Look; callers must check for it first. Fall back to
    // idle rather than dereferencing null if they do not.
    if (c <= COND_OFF || c >= NUM_CONDITIONS || !looks[c]) {
        return *looks[COND_ST_IDLE];
    }
    return *looks[c];
}

Segment& segment(Tier t) {
    begin();
    return *segments[t];
}

// ------------------------------------------------------------------ decision

// Filament problems that have actually stopped the print, as opposed to a
// routine AMS change or an advisory damp reading. These also raise SYSTEM:
// the specific tier says what is wrong, SYSTEM says a human is needed.
static bool blockingFilamentFault(const Facts& f) {
    return f.stage == STAGE_RUNOUT
        || f.stage == STAGE_AMS_LOST
        || STAGES_JAM.count(f.stage) > 0;
}

Condition evaluate(Tier t, const Facts& f) {
    switch (t) {
    case TIER_FILAMENT:
        // Most disruptive first: a runout or jam has stopped the print,
        // a change has not, and damp filament is merely advisory.
        if (f.stage == STAGE_RUNOUT)            return COND_FIL_RUNOUT;
        if (STAGES_JAM.count(f.stage) > 0)      return COND_FIL_JAM;
        if (f.stage == STAGE_AMS_LOST)          return COND_FIL_AMS_LOST;
        if (STAGES_CHANGING.count(f.stage) > 0
            || f.filamentChanging)              return COND_FIL_CHANGING;
        if (f.maxHumidity >= HUMIDITY_DAMP)     return COND_FIL_DAMP;
        return COND_OFF;

    case TIER_STATUS:
        // Never dark: this tier always says what the machine is doing.
        // Note a user pause is not a fault, so it stays out of SYSTEM.
        if (STAGES_PAUSED.count(f.stage) > 0)   return COND_ST_PAUSED;
        if (f.printing)                         return COND_ST_PRINTING;
        return COND_ST_IDLE;

    case TIER_FINISHED:
        return f.finishedPending ? COND_FIN_READY : COND_OFF;

    case TIER_SYSTEM:
        // Connectivity first: without it the other facts are stale.
        if (!f.wifiConnected)                   return COND_SYS_NO_WIFI;
        if (!f.printerConnected)                return COND_SYS_NO_PRINTER;
        // Blocking filament faults raise this tier too, so "something needs
        // me" is readable from one lamp while FILAMENT says which thing.
        if (f.hmsError || f.printError
            || blockingFilamentFault(f))        return COND_SYS_ERROR;
        if (f.hmsWarning)                       return COND_SYS_WARNING;
        return COND_OFF;

    default:
        return COND_OFF;
    }
}

// ------------------------------------------------------------------ patterns

static const byte VALUE_MIN = 5;

byte patternBrightness(byte pattern, byte value, byte rateCpm,
                       unsigned long nowMs, unsigned long offsetMs) {
    if (value == 0) {
        return 0;
    }
    if (pattern == PAT_CONSTANT) {
        return value;
    }
    if (rateCpm == 0) {
        rateCpm = 1;
    }

    const float periodMs = (60.0f * 1000.0f) / rateCpm;
    const float phase    = fmodf((float)(nowMs - offsetMs), periodMs) / periodMs;

    switch (pattern) {
    case PAT_BLINK:
        return (phase < 0.5f) ? value : 0;

    case PAT_FADE: {
        // Triangle: up over the first half, down over the second.
        const float level = (phase < 0.5f) ? (phase * 2.0f)
                                           : ((1.0f - phase) * 2.0f);
        return (byte)(VALUE_MIN + level * (value - VALUE_MIN));
    }

    case PAT_PULSE:
    default: {
        // Exponential breath, as upstream: sean.voisen.org/blog/2011/10/breathing-led-with-arduino
        const float delta = (value - VALUE_MIN) / 2.35040238f;  // e - 0.36787944
        float val = VALUE_MIN
                  + (expf(cosf(2.0f * (float)M_PI * phase)) - 0.36787944f) * delta;
        if (val < 0)   val = 0;
        if (val > 255) val = 255;
        return (byte)val;
    }
    }
}

// --------------------------------------------------------------------- names

uint32_t conditionColor(Condition c) {
    if (c <= COND_OFF || c >= NUM_CONDITIONS) {
        return 0x202020;            // a dark swatch reads as "this tier is off"
    }

    Look& lk = look(c);
    const float h = (lk.hue.value & 0xFF) / 255.0f;
    const float s = lk.saturation.value / 255.0f;
    const float v = lk.value.value / 255.0f;

    const int   i = (int)(h * 6.0f);
    const float f = h * 6.0f - i;
    const float p = v * (1.0f - s);
    const float q = v * (1.0f - f * s);
    const float t = v * (1.0f - (1.0f - f) * s);

    float r = 0, g = 0, b = 0;
    switch (i % 6) {
    case 0: r = v; g = t; b = p; break;
    case 1: r = q; g = v; b = p; break;
    case 2: r = p; g = v; b = t; break;
    case 3: r = p; g = q; b = v; break;
    case 4: r = t; g = p; b = v; break;
    default: r = v; g = p; b = q; break;
    }

    return ((uint32_t)(r * 255.0f) << 16)
         | ((uint32_t)(g * 255.0f) << 8)
         |  (uint32_t)(b * 255.0f);
}

const char* conditionName(Condition c) {
    switch (c) {
    case COND_OFF:            return "Off";
    case COND_FIL_CHANGING:   return "Changing filament";
    case COND_FIL_RUNOUT:     return "Filament runout";
    case COND_FIL_JAM:        return "Filament jam";
    case COND_FIL_AMS_LOST:   return "AMS lost";
    case COND_FIL_DAMP:       return "Filament damp";
    case COND_ST_IDLE:        return "Idle";
    case COND_ST_PRINTING:    return "Printing";
    case COND_ST_PAUSED:      return "Paused";
    case COND_FIN_READY:      return "Ready to collect";
    case COND_SYS_NO_WIFI:    return "No WiFi";
    case COND_SYS_NO_PRINTER: return "No printer";
    case COND_SYS_WARNING:    return "Warning";
    case COND_SYS_ERROR:      return "Error";
    default:                  return "Unknown";
    }
}

const char* tierName(Tier t) {
    switch (t) {
    case TIER_FILAMENT: return "Filament";
    case TIER_STATUS:   return "Status";
    case TIER_FINISHED: return "Finished";
    case TIER_SYSTEM:   return "System";
    default:            return "Unknown";
    }
}

}  // namespace Tower
