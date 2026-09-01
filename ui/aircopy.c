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

#ifdef ENABLE_AIRCOPY

#include <string.h>

#include "app/aircopy.h"
#include "driver/st7565.h"
#include "external/printf/printf.h"
#include "misc.h"
#include "radio.h"
#include "ui/aircopy.h"
#include "ui/helper.h"
#include "ui/inputbox.h"

// crc[] (misc.c) is a fixed uint8_t[15] = 120 bits. gErrorsDuringAirCopy grows
// independently of gAirCopyBlockNumber (it increments on every CRC error, with
// no combined cap), so bit_index can exceed 120 - bound it here rather than at
// every call site.
#define CRC_BIT_COUNT 120

static void set_bit(uint8_t* array, int bit_index) {
    if (bit_index < 0 || bit_index >= CRC_BIT_COUNT)
        return;
    array[bit_index / 8] |= (1 << (bit_index % 8));
}

static int get_bit(uint8_t* array, int bit_index) {
    if (bit_index < 0 || bit_index >= CRC_BIT_COUNT)
        return 0;
    return (array[bit_index / 8] >> (bit_index % 8)) & 1;
}

void UI_DisplayAircopy(void)
{
    char String[16] = { 0 };
    char *pPrintStr = { 0 };
    uint16_t percent;

    UI_DisplayClear();

    if (gAircopyState == AIRCOPY_READY) {
        pPrintStr = "AIR COPY(RDY)";
    } else if (gAircopyState == AIRCOPY_TRANSFER) {
        pPrintStr = "AIR COPY";
    } else {
        pPrintStr = "AIR COPY(CMP)";
        gAircopyState = AIRCOPY_READY;
    }

    UI_PrintString(pPrintStr, 2, 127, 0, 8);

    if (gInputBoxIndex == 0) {
        uint32_t frequency = gRxVfo->freq_config_RX.Frequency;
        sprintf(String, "%3u.%05u", frequency / 100000, frequency % 100000);
        // show the remaining 2 small frequency digits
        UI_PrintStringSmallNormal(String + 7, 97, 0, 3);
        String[7] = 0;
        // show the main large frequency digits
        UI_DisplayFrequency(String, 16, 2, false);
    } else {
        const char *ascii = INPUTBOX_GetAscii();
        sprintf(String, "%.3s.%.3s", ascii, ascii + 3);
        UI_DisplayFrequency(String, 16, 2, false);
    }

    memset(String, 0, sizeof(String));

    percent = (gAirCopyBlockNumber * 10000) / 120;

    if (gAirCopyIsSendMode == 0) {
        sprintf(String, "RCV:%02u.%02u%% E:%d", percent / 100, percent % 100, gErrorsDuringAirCopy);
    } else if (gAirCopyIsSendMode == 1) {
        sprintf(String, "SND:%02u.%02u%%", percent / 100, percent % 100);
    }

    // Draw gauge
    if(gAircopyStep != 0)
    {
        UI_PrintString(String, 2, 127, 5, 8);

        gFrameBuffer[4][1] = 0x3c;
        gFrameBuffer[4][2] = 0x42;

        for(uint8_t i = 1; i <= 122; i++)
        {
            gFrameBuffer[4][2 + i] = 0x81;
        }

        gFrameBuffer[4][125] = 0x42;
        gFrameBuffer[4][126] = 0x3c;
    }

    if(gAirCopyBlockNumber + gErrorsDuringAirCopy != 0)
    {
        // Check CRC
        if(gErrorsDuringAirCopy != lErrorsDuringAirCopy)
        {
            set_bit(crc, gAirCopyBlockNumber + gErrorsDuringAirCopy);
            lErrorsDuringAirCopy = gErrorsDuringAirCopy;
        }

        // i is widened to uint16_t and capped at CRC_BIT_COUNT: with an unbounded
        // gErrorsDuringAirCopy, a uint8_t loop variable could wrap and spin forever,
        // and i+4 must stay within gFrameBuffer[4]'s 128-byte row.
        for(uint16_t i = 0; i < (gAirCopyBlockNumber + gErrorsDuringAirCopy) && i < CRC_BIT_COUNT; i++)
        {
            if(get_bit(crc, i) == 0)
            {
                gFrameBuffer[4][i + 4] = 0xbd;
            }
        }
    }

    ST7565_BlitFullScreen();
}

#endif
