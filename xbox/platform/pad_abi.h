// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <stdint.h>
// Layouts from src/runtime_pad.c, retained for the guest ABI.
typedef struct { uint16_t x,y; uint8_t id,reserve[3]; } UwpPadTouch;
typedef struct {
    uint32_t buttons;
    uint8_t left_x,left_y,right_x,right_y,l2,r2,analog_padding[2];
    float orientation[4],acceleration[3],angular_velocity[3];
    uint8_t touch_count,touch_reserve[3];
    uint32_t touch_held_time;
    UwpPadTouch touches[2];
    uint8_t connected,pad0[3];
    uint64_t timestamp;
    uint8_t extension[16];
    uint8_t connected_count,reserve[2],unique_length,unique[12];
} UwpPadData;
typedef struct {
    float pixel_density; uint16_t resolution_x,resolution_y;
    uint8_t dead_zone_left,dead_zone_right,connection_type,connected_count;
    uint8_t connected,pad[3]; int32_t device_class; uint8_t reserve[8];
} UwpControllerInfo;
