/******************************************************************************
 
 Copyright (c) 2015, Focusrite Audio Engineering Ltd.
 All rights reserved.
 
 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
 
 * Redistributions of source code must retain the above copyright notice, this
 list of conditions and the following disclaimer.
 
 * Redistributions in binary form must reproduce the above copyright notice,
 this list of conditions and the following disclaimer in the documentation
 and/or other materials provided with the distribution.
 
 * Neither the name of Focusrite Audio Engineering Ltd., nor the names of its
 contributors may be used to endorse or promote products derived from
 this software without specific prior written permission.
 
 THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 
 *****************************************************************************/

//______________________________________________________________________________
//
// Headers
//______________________________________________________________________________

#include "app.h"

#define BUTTON_COUNT 64
#define PAGE_COUNT 4
#define MIDI_CLOCK_TIMEOUT 1500
#define MIDI_CLOCK_TRIGGER_COUNT 5

//______________________________________________________________________________
//
// This is where the fun is!  Add your code to the callbacks below to define how
// your app behaves.
//
// In this example, we either render the raw ADC data as LED rainbows or store
// and recall the pad state from flash.
//______________________________________________________________________________
enum {
    OFF,
    ON
};

enum {
    SET_COLOR,
    SEND_EVENTS,
    SET_ACTIVATE
};

enum {
    RED,
    ORANGE,
    YELLOW,
    GREEN,
    AQUA,
    BLUE,
    PURPLE,
    PINK,
    WHITE,
    N_COLORS
};


static const u8 _red[3] = {63, 0, 0};
static const u8 _orange[3] = {63, 16, 0};
static const u8 _yellow[3] = {63, 63, 0};
static const u8 _green[3] = {0, 63, 0};
static const u8 _aqua[3] = {0, 63, 63};
static const u8 _blue[3] = {0, 0, 63};
static const u8 _purple[3] = {16, 0, 63};
static const u8 _pink[3] = {63, 0, 63};
static const u8 _white[3] = {63, 63, 63};

static const u8* _ColorValues[N_COLORS] = {
    _red,
    _orange,
    _yellow,
    _green,
    _aqua,
    _blue,
    _purple,
    _pink,
    _white
};

// Shift Button
static const u8 _colorChangeKey = 80;
static const u8 _colorChangeColor = WHITE;

// Activate Button
static const u8 _activateButton = 50;
static const u8 _activateButtonColor = ORANGE;

// Tap to BPM Button
static const u8 _BPMButton = 10;
static const u8 _BPMButtonColor = RED;


// Page Buttons
// u8 _pageOne = 91;
// u8 _pageTwo = 92;
// u8 _pageThree = 93;
// u8 _pageFour = 94;

// u8 _pageOneColor = RED;
// u8 _pageTwoColor = YELLOW;
// u8 _pageThreeColor = AQUA;
// u8 _pageFourColor = PURPLE;

u8  _pageButtons[PAGE_COUNT] = {
    91,
    92,
    93,
    94
};

u8 _pageButtonsActive[PAGE_COUNT] = {OFF};

u8 _pageButtonColors[PAGE_COUNT] = {
    RED,
    YELLOW,
    AQUA,
    PURPLE
};

// Pads
static const u8 PadsMap[BUTTON_COUNT] = {
    11, 12, 13, 14, 15, 16, 17, 18,
    21, 22, 23, 24, 25, 26, 27, 28,
    31, 32, 33, 34, 35, 36, 37, 38,
    41, 42, 43, 44, 45, 46, 47, 48,
    51, 52, 53, 54, 55, 56, 57, 58,
    61, 62, 63, 64, 65, 66, 67, 68,
    71, 72, 73, 74, 75, 76, 77, 78,
    81, 82, 83, 84, 85, 86, 87, 88
};

// store ADC frame pointer
static const u16 *_ADC = 0;

// Timer
static const u8 BLINK_RATE_MS = 150;
// buffer to store pad states for flash save

u8 _buttons[PAGE_COUNT][BUTTON_COUNT] = {{0}};
u8 _buttonColors[PAGE_COUNT][BUTTON_COUNT] = {{0}};
u8 _buttonActive[PAGE_COUNT][BUTTON_COUNT] = {{0}};
u8 saveBuffer[PAGE_COUNT * BUTTON_COUNT * 2] = {0};
u8 _buttonBlinkState = ON;
u8 _currentPage = 0;
u8 _currentMode = SEND_EVENTS;
u8 midiMessageOn = NOTEON;
u8 midiMessageOff = NOTEOFF;
u8 midiClockRate = 0;
u32 time_ms = 0;

//______________________________________________________________________________

void app_surface_event(u8 type, u8 index, u8 value)
{
    switch (type)
    {
        case  TYPEPAD:
        {
            if (value)
            {
                if (is_equal_button(index)) {
                    u8 buttonIndex = get_button_index(index);
                    u8 channelIndex = (64 * (_currentPage % 2)) + buttonIndex;
                    u8* color = get_color_array(_currentPage, buttonIndex);
                    if (_currentMode == SEND_EVENTS && (_buttonActive[_currentPage][buttonIndex]) == ON) {
                        _buttons[_currentPage][buttonIndex] = !_buttons[_currentPage][buttonIndex];
                        if (_buttons[_currentPage][buttonIndex] == ON) {
                            hal_send_midi(USBSTANDALONE, midiMessageOn | 0, channelIndex, 127);
                            turn_off_button_col(index);
                        } else {
                            hal_send_midi(USBSTANDALONE, midiMessageOff | 0, channelIndex, 0);
                            hal_plot_led(TYPEPAD, index, color[0], color[1], color[2]);
                        }
                    } else if (_currentMode == SET_COLOR && _buttonActive[_currentPage][buttonIndex] == ON){
                        _buttonColors[_currentPage][buttonIndex] += 1;
                        _buttonColors[_currentPage][buttonIndex] %= N_COLORS;
                        u8* color = get_color_array(_currentPage, buttonIndex);
                        hal_plot_led(TYPEPAD, index, color[0], color[1], color[2]);
                    } else if (_currentMode == SET_ACTIVATE) {
                        if (_buttonActive[_currentPage][buttonIndex] == ON) {
                            hal_plot_led(TYPEPAD, index, 0, 0, 0);
                        } else {
                            hal_plot_led(TYPEPAD, index, color[0], color[1], color[2]);
                        }
                        _buttonActive[_currentPage][buttonIndex] = !_buttonActive[_currentPage][buttonIndex];
                    }
                } else if (is_BPM_button(index)) {
                    triggerMidiClock();
                    //hal_plot_led(TYPEPAD, index, _ColorValues[_BPMButtonColor][0], _ColorValues[_BPMButtonColor][1], _ColorValues[_BPMButtonColor][2]);
                } else if (is_equal_color_change(index)) {
                    run_color_change();
                    break;
                } else if (is_equal_page_change(index)) {
                    run_page_change(index);
                    break;
                } else if (is_equal_activate(index)){
                    run_activate();
                } else {
                    break;
                }
            }
        } 
        break;
            
        case TYPESETUP:
        {
            if (value)
            {
                // save button states to flash (reload them by power cycling the hardware
                save();
                //hal_write_flash(0, _Buttons, BUTTON_COUNT);
            }
        }
        break;
    }
}

//______________________________________________________________________________

void app_midi_event(u8 port, u8 status, u8 d1, u8 d2)
{
    // example - MIDI interface functionality for USB "MIDI" port -> DIN port
    if (port == USBMIDI)
    {
        hal_send_midi(DINMIDI, status, d1, d2);
    }
    
    // // example -MIDI interface functionality for DIN -> USB "MIDI" port port
    if (port == DINMIDI)
    {
        hal_send_midi(USBMIDI, status, d1, d2);
    }
}

//______________________________________________________________________________

void app_sysex_event(u8 port, u8 * data, u16 count)
{
    // example - respond to UDI messages?
}

//______________________________________________________________________________

void app_aftertouch_event(u8 index, u8 value)
{
    if (is_BPM_button(index))
    {
        u8* color = _ColorValues[_BPMButtonColor];
        hal_plot_led(TYPEPAD, index, _ColorValues[_BPMButtonColor][0],
                     _ColorValues[_BPMButtonColor][1], _ColorValues[_BPMButtonColor][2]);
    }
    // example - send poly aftertouch to MIDI ports
    //hal_send_midi(USBMIDI, POLYAFTERTOUCH | 0, index, value);
    //hal_plot_led(TYPEPAD, index, value, 0, 0);
    
}

//______________________________________________________________________________

void app_cable_event(u8 type, u8 value)
{
    // example - light the Setup LED to indicate cable connections
    if (type == MIDI_IN_CABLE)
    {
        hal_plot_led(TYPESETUP, 0, 0, value, 0); // green
    }
    else if (type == MIDI_OUT_CABLE)
    {
        hal_plot_led(TYPESETUP, 0, value, 0, 0); // red
    }
}

//______________________________________________________________________________

void app_timer_event()
{
    static u8 blinkMs = BLINK_RATE_MS;
    static u8 sendClockMs = 0;
    static u8 onoff = 0;

    time_ms++;
    blinkMs++;



    if (midiClockRate != 0) {
        sendClockMs++;
        if (sendClockMs >= midiClockRate * 24) {
            onoff = !onoff;
            if (onoff)
                hal_plot_led(TYPEPAD, _BPMButton, 50,50,50);
            else
                hal_plot_led(TYPEPAD, _BPMButton, 0, 0, 0);
            sendClockMs = 0;
            hal_send_midi(USBSTANDALONE , MIDITIMINGCLOCK, 0, 0);
        }
    }
    
    if ((_currentMode == SEND_EVENTS) && (++blinkMs >= BLINK_RATE_MS))
    {
        blinkMs = 0;
        u8 channel = 0;
        u8* color;
        for (u8 i = 0; i < BUTTON_COUNT; i++) {
            if (_buttons[_currentPage][i] == ON) {
                channel = get_button_channel(i);
                color = get_color_array(_currentPage, i);
                if (_buttonBlinkState) {
                    hal_plot_led(TYPEPAD, channel, color[0], color[1], color[2]);
                } else {
                    hal_plot_led(TYPEPAD, channel, 0, 0, 0);
                }
            }
        }
        _buttonBlinkState = !_buttonBlinkState;
    }
}

//______________________________________________________________________________

void app_init(const u16 *adc_raw)
{
    load();
    // for (u8 i = 0; i < PAGE_COUNT; i++) {
    //     for (u8 j = 0; j < BUTTON_COUNT; j++) {
    //         _buttonColors[i][j] = WHITE;
    //     }
    // }

    u8 pageColor = _pageButtonColors[_currentPage];
    hal_plot_led(TYPEPAD, _pageButtons[_currentPage], _ColorValues[pageColor][0], _ColorValues[pageColor][1], _ColorValues[pageColor][2]);

    load_current_page();
	_ADC = adc_raw;
}

void load_current_page() {
    if (_currentMode == SEND_EVENTS) {
        for (u8 i = 0; i < BUTTON_COUNT; i++) {
            if (_buttonActive[_currentPage][i] == ON) {
                u8 padColor = _buttonColors[_currentPage][i];
                hal_plot_led(TYPEPAD, PadsMap[i], _ColorValues[padColor][0], _ColorValues[padColor][1], _ColorValues[padColor][2]);
            } else if (_buttonActive[_currentPage][i] == OFF){
                hal_plot_led(TYPEPAD, PadsMap[i], 0, 0, 0);
                _buttonActive[_currentPage][i] = OFF;
            } else {
                _buttonActive[_currentPage][i] = OFF;
            }
        }
    } else if (_currentMode == SET_COLOR){
        for (u8 i = 0; i < BUTTON_COUNT; i++) {
            if (_buttonActive[_currentPage][i] == ON) {
                u8 padColor = _buttonColors[_currentPage][i];
                hal_plot_led(TYPEPAD, PadsMap[i], _ColorValues[padColor][0], _ColorValues[padColor][1], _ColorValues[padColor][2]);
            } else if (_buttonActive[_currentPage][i] == OFF){
                hal_plot_led(TYPEPAD, PadsMap[i], 0, 0, 0);
                _buttonActive[_currentPage][i] = OFF;
            } else {
                _buttonActive[_currentPage][i] = OFF;
            }
        }
    } else if (_currentMode == SET_ACTIVATE) {
        u8* color;
        for (u8 i = 0; i < BUTTON_COUNT; i++) {
            if(_buttonActive[_currentPage][i] == ON) {
                color = get_color_array(_currentPage, i);
                hal_plot_led(TYPEPAD, PadsMap[i], color[0], color[1], color[2]);
            } else if (_buttonActive[_currentPage][i] == OFF){
                hal_plot_led(TYPEPAD, PadsMap[i], 0, 0, 0);
                _buttonActive[_currentPage][i] = OFF;
            } else {
                _buttonActive[_currentPage][i] = OFF;
            }
        }
    }
    
}

void turn_off_button_col(u8 channel) {
    u8 col = channel % 10;
    for (u8 i = 1; i < 9; i++) {
        u8 tempChannel = col + (i * 10);
        u8 tempIndex = get_button_index(tempChannel);
        if (tempChannel == channel)
            continue;
        if (_buttons[_currentPage][tempIndex] == ON) {
            u8 midiChannel = (64 * (_currentPage % 2)) + tempIndex;
            u8 midiValue = (_currentPage > 1) ? 20 : 10;
            u8* color = get_color_array(_currentPage, tempIndex);
            hal_plot_led(TYPEPAD, tempChannel, color[0], color[1], color[2]);
            hal_send_midi(TYPEPAD, NOTEOFF | 0, midiChannel, midiValue);
            _buttons[_currentPage][tempIndex] = OFF;
        }

    }
}

u8 get_button_channel(u8 index) {
    return PadsMap[index];
}

u8 get_button_index(u8 channel) {
    for (u8 index = 0; index < PAD_COUNT; index++) {
        if (PadsMap[index] == channel)
            return index;
    }
}

u8* get_color_array(u8 currentPage, u8 index) {
    return _ColorValues[_buttonColors[currentPage][index]];
}

u8 is_equal_color_change(u8 channel) {
    return (channel == _colorChangeKey);
}

void run_color_change() {
    if (_currentMode == SET_ACTIVATE)
        run_activate();

    _currentMode = (_currentMode == SET_COLOR) ? SEND_EVENTS : SET_COLOR;
    if (_currentMode == SET_COLOR) {
        u8* color = _ColorValues[_colorChangeColor];
        hal_plot_led(TYPEPAD, _colorChangeKey, color[0], color[1], color[2]);
    } else {
        hal_plot_led(TYPEPAD, _colorChangeKey, 0, 0, 0);
    }
    load_current_page();
}

u8 is_equal_page_change(u8 channel) {
    for (u8 i = 0; i < PAGE_COUNT; i++) {
        if (channel == _pageButtons[i]) {
            return 1;
        }
    }
    return 0;
}

void run_page_change(u8 channel) {
    hal_plot_led(TYPEPAD, _pageButtons[_currentPage], 0, 0, 0);
    _currentPage = channel - _pageButtons[0];
    u8 pageColor = _pageButtonColors[_currentPage];
    hal_plot_led(TYPEPAD, _pageButtons[_currentPage], _ColorValues[pageColor][0], _ColorValues[pageColor][1], _ColorValues[pageColor][2]);
    if (_currentPage < 2) {
        midiMessageOn = NOTEON;
        midiMessageOff = NOTEOFF;
    } else {
        midiMessageOn = CC;
        midiMessageOff = CC;
    }
    load_current_page();
}

u8 is_equal_button(u8 channel) {
    for (u8 i = 0; i < BUTTON_COUNT; i++) {
        if (get_button_channel(i) == channel)
            return 1;
    }
    return 0;
}

u8 is_equal_activate(u8 channel) {
    return (channel == _activateButton);
}

void run_activate() {
    if (_currentMode == SET_COLOR)
        run_color_change();
    _currentMode = (_currentMode == SET_ACTIVATE) ? SEND_EVENTS : SET_ACTIVATE;
    if (_currentMode == SET_ACTIVATE) {
        u8* color = _ColorValues[_activateButtonColor];
        hal_plot_led(TYPEPAD, _activateButton, color[0], color[1], color[2]);
    } else {
        hal_plot_led(TYPEPAD, _activateButton, 0, 0, 0);
    }
    load_current_page();
}

u8 is_BPM_button(u8 channel) {
    return (channel == _BPMButton) ? 1 : 0;
}

void save() {
    u8 saveBuffer[PAGE_COUNT * BUTTON_COUNT] = {0};
    u8 bufferIter = 0;
    union CBits byteActive;
    union CBits byteColor;
    byteColor.bits.b5 = 0;
    byteColor.bits.b6 = 0;
    byteColor.bits.b7 = 0;
    for (u8 page = 0; page < PAGE_COUNT; page++) {
        for (u8 button = 0; button < BUTTON_COUNT; button++, bufferIter++) {
            byteActive.byte = _buttonActive[page][button];
            byteColor.byte = _buttonColors[page][button];
            byteActive.bits.b1 = byteColor.bits.b0;
            byteActive.bits.b2 = byteColor.bits.b1;
            byteActive.bits.b3 = byteColor.bits.b2;
            byteActive.bits.b4 = byteColor.bits.b3;
            saveBuffer[bufferIter] = byteActive.byte;
        }
    }
    hal_write_flash(0, saveBuffer, PAGE_COUNT * BUTTON_COUNT);
}

void load() {
    u8 loadBuffer[PAGE_COUNT * BUTTON_COUNT] = {0};
    u8 bufferIter = 0;
    union CBits byteParser;
    union CBits byteColor;
    byteColor.bits.b5 = 0;
    byteColor.bits.b6 = 0;
    byteColor.bits.b7 = 0;
    hal_read_flash(0, loadBuffer, PAGE_COUNT * BUTTON_COUNT);
    for (u8 page = 0; page < PAGE_COUNT; page++) {
        for (u8 button = 0; button < BUTTON_COUNT; button++, bufferIter++) {
            byteParser.byte = loadBuffer[bufferIter];
            byteColor.bits.b0 = byteParser.bits.b1;
            byteColor.bits.b1 = byteParser.bits.b2;
            byteColor.bits.b2 = byteParser.bits.b3;
            byteColor.bits.b3 = byteParser.bits.b4;
            _buttonActive[page][button] = byteParser.bits.b0 ? ON : OFF;
            _buttonColors[page][button] = byteColor.byte;
        }
    }
}

void triggerMidiClock(){
    static u32 lastTime = 0;
    static u32 timeDeltas[MIDI_CLOCK_TRIGGER_COUNT - 1] = {0};
    static u8 triggerIndex = 0;
    u32 timeSinceLastTrigger = 0;

    if (triggerIndex == 0) {
        lastTime = time_ms;
        triggerIndex++;
        return;
    }

    timeSinceLastTrigger = time_ms - lastTime;
    timeDeltas[triggerIndex - 1] = timeSinceLastTrigger;

    if (timeSinceLastTrigger > MIDI_CLOCK_TIMEOUT)
        triggerIndex = 0;

    if (triggerIndex >= MIDI_CLOCK_TRIGGER_COUNT - 1) {
        triggerIndex = 0;
        u32 average = 0;
        for (u8 i = 0; i < MIDI_CLOCK_TRIGGER_COUNT - 1; i++) {
            average += timeDeltas[i];
        }
        average = ((average / (MIDI_CLOCK_TRIGGER_COUNT - 1)) * 60) / 24;
        midiClockRate = average;
    }

    lastTime = time_ms;
    triggerIndex++;
}
