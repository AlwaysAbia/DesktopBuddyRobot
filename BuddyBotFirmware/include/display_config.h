// Settings for the CURRENT_TIME and MESSAGE_HISTORY modes.
#pragma once

// POSIX TZ string for the clock. NTP gives UTC; this converts it to local time.
// Tbilisi, UTC+4, no daylight saving. (Central Europe would be "CET-1CEST,M3.5.0,M10.5.0/3".)
#define CLOCK_TZ "<+04>-4"
#define NTP_SERVER_1 "pool.ntp.org"
#define NTP_SERVER_2 "time.google.com"

// Screen orientation for the clock face and scrolling text (not the eye).
// Assumes +y is up and +x is right in led_matrix::ledCoords. If the clock
// runs mirrored or text scrolls the wrong way, flip the matching axis (1 / -1).
#define SCREEN_X_SIGN 1
#define SCREEN_Y_SIGN -1   // was 1: the clock ran counterclockwise and 12 o'clock was at the bottom

// DEBUG: 1 = if the message history is empty at boot, store two dummy
// messages (persisted to NVS like real ones) so MESSAGE_HISTORY has something
// to render before Phase 4 exists. Never ship a build with this set.
// To strip it out: delete this define and the #if block in messages.cpp.
#define MESSAGES_DEBUG_SEED 0

// Scrolling text is tilted by this many degrees (counterclockwise) so its rows and
// columns run along the LED grid, which is diagonal on screen. Strokes come out one
// LED wide and crisp; rotate the robot this many degrees clockwise to read it upright.
// 0 = upright text (blurry on this grid).
#define TEXT_ROTATION_DEG -45
