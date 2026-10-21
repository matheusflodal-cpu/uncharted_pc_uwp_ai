// SPDX-License-Identifier: GPL-2.0-or-later
// WinRT sampling/vibration adapted from Cosmo/gow3_xbox d0975c9 input.cpp.
#include "input.h"
#include <windows.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Gaming.Input.h>
#include <cstdio>
#include <mutex>
using winrt::Windows::Gaming::Input::Gamepad;
static std::mutex mutex;
static Gamepad selected{nullptr};
static uint32_t generation;
static void apartment() {
    thread_local bool ready=false;
    if(!ready) {
        try { winrt::init_apartment(winrt::apartment_type::multi_threaded); }
        catch(const winrt::hresult_error& error) { if(error.code()!=RPC_E_CHANGED_MODE) throw; }
        ready=true;
    }
}
static Gamepad controller() {
    auto pads=Gamepad::Gamepads();
    if(selected) for(const auto& pad:pads) if(pad==selected) return selected;
    Gamepad next=pads.Size() ? pads.GetAt(0) : nullptr;
    if(next!=selected) { selected=next; ++generation; }
    return selected;
}
extern "C" int uncharted_uwp_gamepad(UnchartedInput* output) {
    if(!output) return -1;
    *output={};
    try {
        apartment(); std::scoped_lock lock(mutex);
        auto pad=controller(); if(!pad) return 0;
        auto reading=pad.GetCurrentReading();
        using B=winrt::Windows::Gaming::Input::GamepadButtons;
        struct Mapping { B host; uint32_t logical; };
        static constexpr Mapping mapping[]={
            {B::A,INPUT_A},{B::B,INPUT_B},{B::X,INPUT_X},{B::Y,INPUT_Y},
            {B::LeftShoulder,INPUT_LB},{B::RightShoulder,INPUT_RB},
            {B::LeftThumbstick,INPUT_LS},{B::RightThumbstick,INPUT_RS},
            {B::Menu,INPUT_MENU},{B::View,INPUT_VIEW},
            {B::DPadUp,INPUT_UP},{B::DPadRight,INPUT_RIGHT},{B::DPadDown,INPUT_DOWN},{B::DPadLeft,INPUT_LEFT}};
        for(const auto& entry:mapping) if((reading.Buttons&entry.host)!=B::None) output->buttons|=entry.logical;
        output->generation=generation;
        output->lx=reading.LeftThumbstickX; output->ly=reading.LeftThumbstickY;
        output->rx=reading.RightThumbstickX; output->ry=reading.RightThumbstickY;
        output->lt=reading.LeftTrigger; output->rt=reading.RightTrigger;
        output->timestamp=reading.Timestamp;
        return 1;
    } catch(const winrt::hresult_error& error) {
        thread_local bool reported=false;
        if(!reported) { std::fprintf(stderr,"Xbox gamepad API: 0x%08x\n",unsigned(error.code())); reported=true; }
        return -1;
    }
}
extern "C" int uncharted_uwp_vibration(uint8_t large,uint8_t small) {
    try {
        apartment(); std::scoped_lock lock(mutex);
        auto pad=controller(); if(!pad) return 0;
        pad.Vibration({large/255.0,small/255.0,0,0});
        return 1;
    } catch(const winrt::hresult_error&) { return -1; }
}
