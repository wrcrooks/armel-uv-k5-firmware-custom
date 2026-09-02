/* Copyright 2023 Dual Tachyon
 * https://github.com/DualTachyon
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 *     Unless required by applicable law or agreed to in writing, software
 *     distributed under the License is distributed on an "AS IS" BASIS,
 *     WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *     See the License for the specific language governing permissions and
 *     limitations under the License.
 */

#ifndef APP_FM_H
#define APP_FM_H

#ifdef ENABLE_FMRADIO

#include "driver/keyboard.h"

#ifndef ENABLE_FMRADIO_MINIMIZED
    #define FM_CHANNEL_UP   0x01
    #define FM_CHANNEL_DOWN 0xFF
#endif

enum {
    FM_SCAN_OFF = 0U,
};

// With ENABLE_FMRADIO_MINIMIZED, gFM_Channels is still loaded from/saved to
// EEPROM by settings.c for layout compatibility, but is otherwise unused -
// the channel-memory/MR-mode/auto-scan feature that reads and writes it in
// the full build is left out to save flash. See app/fm.c/ui/fmradio.c.
extern uint16_t          gFM_Channels[20];
extern bool              gFmRadioMode;
extern uint8_t           gFmRadioCountdown_500ms;
extern volatile uint16_t gFmPlayCountdown_10ms;
extern volatile int8_t   gFM_ScanState;
#ifndef ENABLE_FMRADIO_MINIMIZED
extern bool              gFM_AutoScan;
extern uint8_t           gFM_ChannelPosition;
#endif
// Doubts about          whether this should be signed or not
extern uint16_t          gFM_FrequencyDeviation;
extern bool              gFM_FoundFrequency;
extern uint16_t          gFM_RestoreCountdown_10ms;

#ifndef ENABLE_FMRADIO_MINIMIZED
bool    FM_CheckValidChannel(uint8_t Channel);
// returns first valid channel starting at Channel
uint8_t FM_FindNextChannel(uint8_t Channel, uint8_t Direction);
#endif
int     FM_ConfigureChannelState(void);
void    FM_TurnOff(void);
#ifndef ENABLE_FMRADIO_MINIMIZED
void    FM_EraseChannels(void);
#endif

void    FM_Tune(uint16_t Frequency, int8_t Step, bool bFlag);
void    FM_PlayAndUpdate(void);
int     FM_CheckFrequencyLock(uint16_t Frequency, uint16_t LowerLimit);

void    FM_ProcessKeys(KEY_Code_t Key, bool bKeyPressed, bool bKeyHeld);

void    FM_Play(void);
void    FM_Start(void);

#endif

#endif
