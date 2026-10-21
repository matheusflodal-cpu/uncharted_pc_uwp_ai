// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// Logical host controls, independent of WinRT numeric enum values.
enum { INPUT_A=1, INPUT_B=2, INPUT_X=4, INPUT_Y=8, INPUT_LB=16, INPUT_RB=32,
       INPUT_LS=64, INPUT_RS=128, INPUT_MENU=256, INPUT_VIEW=512,
       INPUT_UP=1024, INPUT_RIGHT=2048, INPUT_DOWN=4096, INPUT_LEFT=8192 };
typedef struct {
    uint32_t buttons, generation;
    double lx,ly,rx,ry,lt,rt;
    uint64_t timestamp;
} UnchartedInput;
// 1 connected, 0 absent, -1 host API failure. Output is initialized on every call.
int uncharted_uwp_gamepad(UnchartedInput *reading);
int uncharted_uwp_vibration(uint8_t large,uint8_t small);
// Poll real device enumeration before guest startup: 1 ready, 0 timed out,
// -1 host failure. Does not open a guest pad port or fabricate a connection.
int uncharted_uwp_wait_gamepad(unsigned timeout_ms);
#ifdef __cplusplus
}
#endif
