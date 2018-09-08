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
    PAD,
    BPM,
    COLOR_CHANGE,
    PAGE_CHANGE,
    ACTIVATE,
    N_BUTTON_TYPES
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
static const u8 _colorChangeButton = 80;
static const u8 _colorChangeColor = WHITE;

// Activate Button
static const u8 _activateButton = 50;
static const u8 _activateButtonColor = ORANGE;

// Tap to BPM Button
static const u8 _BPMButton = 18;
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
static const u8 _padsMap[BUTTON_COUNT] = {
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
u8 _buttonBlinkState = ON;
u8 _currentPage = 0;
u8 _pagesOn[PAD_COUNT] = {OFF};
u8 _currentMode = SEND_EVENTS;
u8 _midiMessageOn = NOTEON;
u8 _midiMessageOff = NOTEOFF;
u8 _midiClockRate = 0;
u32 _timeMs = 0;

//______________________________________________________________________________

void app_surface_event(u8 type, u8 index, u8 value)
{
    switch (type)
    {
        case  TYPEPAD:
        {
            if (value)
            {
                switch(get_button_type(index)) {
                    case PAD:
                        run_pad_button(index);
                        break;
                    case BPM:
                        triggerMidiClock();
                        break;
                    case COLOR_CHANGE:
                        run_color_change();
                        break;
                    case PAGE_CHANGE:
                        run_page_change(index);
                        break;
                    case ACTIVATE:
                        run_activate();
                        break;
                    default:
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

    _timeMs++;
    blinkMs++;

    if (_midiClockRate != 0) {
        sendClockMs++;
        if (sendClockMs >= _midiClockRate / 2) {
            onoff = !onoff;
            if (onoff)
                hal_plot_led(TYPEPAD, _BPMButton, 50,50,50);
            else
                hal_plot_led(TYPEPAD, _BPMButton, 0, 0, 0);
            sendClockMs = 0;
            //hal_send_midi(USBSTANDALONE , MIDITIMINGCLOCK, 0, 0);
        }
    }
    
    if ((_currentMode == SEND_EVENTS) && (++blinkMs >= BLINK_RATE_MS))
    {
        blinkMs = 0;
        u8 channel = 0;
        for (u8 i = 0; i < BUTTON_COUNT; i++) {
            if (_buttons[_currentPage][i] == ON) {
                const u8* color = get_color_array(_currentPage, i);
                channel = get_button_channel(i);
                if (_buttonBlinkState) {
                    hal_plot_led(TYPEPAD, channel, color[0], color[1], color[2]);
                } else {
                    hal_plot_led(TYPEPAD, channel, 0, 0, 0);
                }
            }
        }
        for (u8 i = 0; i < PAD_COUNT; i++) {
            if (_pagesOn[i] == ON && i != _currentPage) {
                const u8* color = _ColorValues[_pageButtonColors[i]];
                if (_buttonBlinkState) {
                    hal_plot_led(TYPEPAD, _pageButtons[i], color[0], color[1], color[2]);
                } else {
                    hal_plot_led(TYPEPAD, _pageButtons[i], 0, 0, 0);
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
    u8 pageColor = _pageButtonColors[_currentPage];
    hal_plot_led(TYPEPAD, _pageButtons[_currentPage], _ColorValues[pageColor][0], _ColorValues[pageColor][1], _ColorValues[pageColor][2]);
    load_current_page();
	_ADC = adc_raw;
}

void load_current_page() {
    switch (_currentMode) {
        case SEND_EVENTS:
            for (u8 i = 0; i < BUTTON_COUNT; i++) {
                if (_buttonActive[_currentPage][i] == ON) {
                    u8 padColor = _buttonColors[_currentPage][i];
                    hal_plot_led(TYPEPAD, _padsMap[i], _ColorValues[padColor][0], _ColorValues[padColor][1], _ColorValues[padColor][2]);
                } else if (_buttonActive[_currentPage][i] == OFF){
                    hal_plot_led(TYPEPAD, _padsMap[i], 0, 0, 0);
                    _buttonActive[_currentPage][i] = OFF;
                } else {
                    _buttonActive[_currentPage][i] = OFF;
                }
            }
            break;
        case SET_COLOR:
            for (u8 i = 0; i < BUTTON_COUNT; i++) {
                if (_buttonActive[_currentPage][i] == ON) {
                    u8 padColor = _buttonColors[_currentPage][i];
                    hal_plot_led(TYPEPAD, _padsMap[i], _ColorValues[padColor][0], _ColorValues[padColor][1], _ColorValues[padColor][2]);
                } else if (_buttonActive[_currentPage][i] == OFF){
                    hal_plot_led(TYPEPAD, _padsMap[i], 0, 0, 0);
                    _buttonActive[_currentPage][i] = OFF;
                } else {
                    _buttonActive[_currentPage][i] = OFF;
                }
            }
            break;
        case SET_ACTIVATE:
            for (u8 i = 0; i < BUTTON_COUNT; i++) {
                if(_buttonActive[_currentPage][i] == ON) {
                    const u8* color;
                    color = get_color_array(_currentPage, i);
                    hal_plot_led(TYPEPAD, _padsMap[i], color[0], color[1], color[2]);
                } else if (_buttonActive[_currentPage][i] == OFF){
                    hal_plot_led(TYPEPAD, _padsMap[i], 0, 0, 0);
                    _buttonActive[_currentPage][i] = OFF;
                } else {
                    _buttonActive[_currentPage][i] = OFF;
                }
            }
            break;
        default:
            break;
    }
}

void load_new_page(u8 channel) {
    u8 newPage = (channel - _pageButtons[0]);
    if ((newPage != _currentPage) && (newPage < PAD_COUNT) && (0 <= newPage)) {
        _pagesOn[_currentPage] = page_has_active_buttons(_currentPage);
        hal_plot_led(TYPEPAD, _pageButtons[_currentPage], 0, 0, 0);
        const u8* color = _ColorValues[_pageButtonColors[newPage]];
        hal_plot_led(TYPEPAD, _pageButtons[newPage], color[0], color[1], color[2]);
        _currentPage = newPage;
    }
    load_current_page();
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
            const u8* color = get_color_array(_currentPage, tempIndex);
            hal_plot_led(TYPEPAD, tempChannel, color[0], color[1], color[2]);
            hal_send_midi(TYPEPAD, NOTEOFF | 0, midiChannel, midiValue);
            _buttons[_currentPage][tempIndex] = OFF;
        }

    }
}

u8 get_button_type(u8 index) {

    if (is_pad_button(index)) {
        return PAD;
    } else if (is_BPM_button(index)) {
        return BPM;
    } else if (is_color_change(index)) {
        return COLOR_CHANGE;
    } else if (is_page_change(index)) {
        return PAGE_CHANGE;
    } else if (is_activate(index)){
        return ACTIVATE;
    } else {
        return N_BUTTON_TYPES;
    }
}

u8 get_button_channel(u8 index) {
    return _padsMap[index];
}

u8 get_button_index(u8 channel) {
    for (u8 index = 0; index < PAD_COUNT; index++) {
        if (_padsMap[index] == channel)
            return index;
    }
    return 101;
}

const u8* get_color_array(u8 currentPage, u8 index) {
    return _ColorValues[_buttonColors[currentPage][index]];
}

u8 is_color_change(u8 channel) {
    return (channel == _colorChangeButton);
}

void run_color_change() {
    if (_currentMode == SET_ACTIVATE)
        run_activate();

    _currentMode = (_currentMode == SET_COLOR) ? SEND_EVENTS : SET_COLOR;
    if (_currentMode == SET_COLOR) {
        const u8* color = _ColorValues[_colorChangeColor];
        hal_plot_led(TYPEPAD, _colorChangeButton, color[0], color[1], color[2]);
    } else {
        hal_plot_led(TYPEPAD, _colorChangeButton, 0, 0, 0);
    }
    load_current_page();
}

u8 is_page_change(u8 channel) {
    for (u8 i = 0; i < PAGE_COUNT; i++) {
        if (channel == _pageButtons[i]) {
            return 1;
        }
    }
    return 0;
}

void run_page_change(u8 channel) {
    load_new_page(channel);
    if (_currentPage < 2) {
        _midiMessageOn = NOTEON;
        _midiMessageOff = NOTEOFF;
    } else {
        _midiMessageOn = CC;
        _midiMessageOff = CC;
    }
}

u8 is_pad_button(u8 channel) {
    for (u8 i = 0; i < BUTTON_COUNT; i++) {
        if (get_button_channel(i) == channel)
            return 1;
    }
    return 0;
}

void run_pad_button(u8 channel) {
    u8 buttonIndex = get_button_index(channel);
    u8 channelIndex = (64 * (_currentPage % 2)) + buttonIndex;
    const u8* color = get_color_array(_currentPage, buttonIndex);
    if (_currentMode == SEND_EVENTS && (_buttonActive[_currentPage][buttonIndex]) == ON) {
        _buttons[_currentPage][buttonIndex] = !_buttons[_currentPage][buttonIndex];
        if (_buttons[_currentPage][buttonIndex] == ON) {
            hal_send_midi(USBSTANDALONE, _midiMessageOn | 0, channelIndex, 127);
            turn_off_button_col(channel);
        } else {
            hal_send_midi(USBSTANDALONE, _midiMessageOff | 0, channelIndex, 0);
            hal_plot_led(TYPEPAD, channel, color[0], color[1], color[2]);
        }
    } else if (_currentMode == SET_COLOR && _buttonActive[_currentPage][buttonIndex] == ON){
        _buttonColors[_currentPage][buttonIndex] += 1;
        _buttonColors[_currentPage][buttonIndex] %= N_COLORS;
        const u8* color = get_color_array(_currentPage, buttonIndex);
        hal_plot_led(TYPEPAD, channel, color[0], color[1], color[2]);
    } else if (_currentMode == SET_ACTIVATE) {
        if (_buttonActive[_currentPage][buttonIndex] == ON) {
            hal_plot_led(TYPEPAD, channel, 0, 0, 0);
            if (_buttons[_currentPage][buttonIndex] == ON)
                hal_send_midi(USBSTANDALONE, _midiMessageOff | 0, channelIndex, 0);
        } else {
            hal_plot_led(TYPEPAD, channel, color[0], color[1], color[2]);
        }
        _buttonActive[_currentPage][buttonIndex] = !_buttonActive[_currentPage][buttonIndex];
    }
}

u8 is_activate(u8 channel) {
    return (channel == _activateButton);
}

void run_activate() {
    if (_currentMode == SET_COLOR)
        run_color_change();
    _currentMode = (_currentMode == SET_ACTIVATE) ? SEND_EVENTS : SET_ACTIVATE;
    if (_currentMode == SET_ACTIVATE) {
        const u8* color = _ColorValues[_activateButtonColor];
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

void triggerMidiClock() {
    static u32 lastTime = 0;
    static u32 timeDeltas[MIDI_CLOCK_TRIGGER_COUNT - 1] = {0};
    static u8 triggerIndex = 0;
    u32 timeSinceLastTrigger = 0;

    if (triggerIndex != 0) {
        timeSinceLastTrigger = _timeMs - lastTime;
        timeDeltas[triggerIndex - 1] = timeSinceLastTrigger;

        if (timeSinceLastTrigger > MIDI_CLOCK_TIMEOUT) {
            triggerIndex = 0;
            hal_plot_led(TYPEPAD, 3 , 0,63,63);
        }

        if (triggerIndex >= MIDI_CLOCK_TRIGGER_COUNT - 1) {
            triggerIndex = 0;
            u32 average = 0;
            hal_plot_led(TYPEPAD, 2 , 63,63,63); //1111111111111111111111111
            for (u8 i = 0; i < MIDI_CLOCK_TRIGGER_COUNT - 1; i++) {
                average += timeDeltas[i];
            }
            average = ((average / (4))); // / 24 * (1/60);
            _midiClockRate = average;
        }
    }
    if (triggerIndex == 1) {
        hal_plot_led(TYPEPAD, 2 , 0,0,0);
        hal_plot_led(TYPEPAD, 3 , 0,0,0);
    }
    lastTime = _timeMs;
    triggerIndex++;
}

u8 page_has_active_buttons(u8 pageIndex) {
    for (u8 button = 0; button < BUTTON_COUNT; button++) {
        if (_buttons[_currentPage][button] == ON)
            return ON;
    }
    return OFF;
}
