#pragma once
#include <stdint.h>

// ── Feeding schedule entry loaded from Firebase RTDB ─────────────────────────
//
//  The Flutter app writes schedules to:
//    devices/<id>/settings_cache/schedules   (JSON array)
//
//  Each entry looks like:
//    { "hour":8, "minute":0, "portionGrams":60, "enabled":true,
//      "days":[1,2,3,4,5,6,0] }   ← integers: 0=Sun 1=Mon … 6=Sat
//
//  Days are stored here as a bitmask: bit0=Sun, bit1=Mon, ..., bit6=Sat.
//  0x7F means every day.

struct FeedSchedule {
    bool    enabled;
    int     hour;
    int     minute;
    float   portionGrams;
    bool    triggered;  // Prevents double-firing within the same minute
    uint8_t days;       // Bitmask: (1<<0)=Sun (1<<1)=Mon … (1<<6)=Sat
};
