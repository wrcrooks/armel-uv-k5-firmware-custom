# Makefile Build Flags

This documents every `ENABLE_*` flag in the [Makefile](Makefile). Each flag is
either `0` (disabled) or `1` (enabled) unless noted otherwise, set via
`?=` (so it can be overridden on the `make` command line, e.g.
`make ENABLE_NOAA=1`) and generally either adds a `-D` define to `CFLAGS`
(gating `#ifdef`/`#if defined(...)` blocks in the C source), adds/removes
object files from the build (`OBJS`), or changes compiler/linker behavior
directly.

Flags are grouped exactly as they appear in the Makefile: **Stock Quansheng
Features**, **Custom Mods**, **Contrib Mods**, **F4HWN Mods**, **Debugging**,
and **Compiler/Linker Options**.

`ENABLE_FEAT_F4HWN` is the master switch for the whole "F4HWN" edition layer:
when it's `1`, the build target is named `f4hwn` (vs `firmware`) and the
`ENABLE_FEAT_F4HWN_*` sub-flags below become meaningful; when it's `0`, the
firmware reverts to a plainer "egzumer" feel and those sub-flags have no
effect. Several other flags interact directly with the build system rather
than gating `#ifdef` code: `ENABLE_CLANG=1` forces `ENABLE_LTO` off (GCC's
linker can't consume LLVM bitcode), and `ENABLE_OVERLAY=1` also forces
`ENABLE_LTO` off (the two are mutually exclusive).

---

## Stock Quansheng Features

### `ENABLE_FMRADIO` (default: 1)
Builds the broadcast-FM receiver support (`driver/bk1080.o`, `app/fm.o`, `ui/fmradio.o`) built around the BK1080 chip — a separate FM radio screen/mode reachable from the main menu (F+0), independent of the amateur/PMR transceiver path. Whether it includes channel-memory presets/MR mode/auto-scan/direct frequency entry depends on `ENABLE_FMRADIO_MINIMIZED` below.

### `ENABLE_FMRADIO_MINIMIZED` (default: 1)
Only affects anything when `ENABLE_FMRADIO=1`. Selects between two complete implementations of `app/fm.c`/`ui/fmradio.c`/the FM half of `app/action.c`'s `ACTION_Scan_FM`, both built from the same source (behind `#ifdef ENABLE_FMRADIO_MINIMIZED`/`#else`), not two separate files: with it **on**, FM radio drops the 20-channel-preset memory, MR (memory recall) mode, auto-scan-to-fill-memory, and direct keypad frequency entry, keeping only step-tuning, band cycling, and seek-scan — saving ~1.4KB. With it **off**, FM radio is the original full-featured implementation. `gFM_Channels`/`FM_IsMrMode`/`FM_SelectedChannel` stay in the EEPROM layout either way (`settings.c` loads/saves them unconditionally) so switching this flag doesn't touch EEPROM compatibility, even though the minimized build never acts on that data. Defaults **on** because this repo's current flag combination (`ENABLE_SPECTRUM`, etc.) needs the saved space to fit in 60KB — set to `0` for the full-featured FM radio if you free up room elsewhere (see the flash-cost table below).

### `ENABLE_UART` (default: 1)
Builds the PC serial-programming/control link (`driver/aes.o`, `driver/uart.o`, `driver/crc.o`, `app/uart.o`): a CRC-checked, AES-obfuscated command protocol handling session init (`0x0514`/`0x052F`), EEPROM read (`0x051B`) and write (`0x051D`) — the mechanism CHIRP-like PC software uses to read/flash channel memories — and a reboot-to-bootloader command (`0x05DD`).

### `ENABLE_AIRCOPY` (default: 0)
Builds the over-the-air "Aircopy" feature (`app/aircopy.o`, `ui/aircopy.o`): a dedicated screen/mode that transmits and receives the radio's EEPROM/channel configuration directly between two UV-K5 units via RF (CRC- and obfuscation-protected packets), rather than over USB.

### `ENABLE_NOAA` (default: 0)
Adds the NOAA weather-channel table and an automatic NOAA-alert monitoring mode: dedicated NOAA channel numbers (`NOAA_CHANNEL_FIRST`, `NoaaFrequencyTable`), periodic background scanning/CTCSS(2625Hz)-detection of NOAA frequencies during dual-watch/idle, and auto-switching the active VFO to the detected NOAA channel.

### `ENABLE_VOICE` (default: 0)
Adds spoken voice-prompt audio: a large table of pre-recorded Chinese/English voice clip IDs (`VOICE_ID_*`, e.g. numbers, "LOCK", "SCANNING BEGIN", "LOW VOLTAGE") that get played back through the speaker to announce menu selections, values and status changes instead of (or alongside) beeps.

### `ENABLE_VOX` (default: 1)
Adds Voice-Operated Transmit: detects speech/noise on the mic above a configurable threshold (`VOX0_THRESHOLD`/`VOX1_THRESHOLD` via `BK4819_EnableVox`) and automatically keys the transmitter without a PTT press, with hang-time/resume countdowns (`gVoxResumeCountdown`, `gVoxPauseCountdown`) to release TX after speech stops.

### `ENABLE_ALARM` (default: 0)
Adds a siren/alarm function (triggered as an assignable key action): generates a warbling tone that sweeps 500-1500 Hz, in two modes — `ALARM_MODE_TONE` alternates between transmitting the tone over the air and playing it locally (PA disabled) as a "site alarm," while `ALARM_MODE_SITE` only sounds it locally without transmitting.

### `ENABLE_TX1750` (default: 1)
Adds a 1750 Hz tone-burst transmit function, assignable to the SIDE2 key or the alarm action, used to key open European repeaters that require a 1750 Hz access tone instead of CTCSS/DCS.

### `ENABLE_PWRON_PASSWORD` (default: 0)
Adds a 6-digit numeric password lock screen (`UI_DisplayLock` in `ui/lock.c`) shown at power-on whenever `gEeprom.POWER_ON_PASSWORD` is set, blocking use of the radio until the correct code is entered on the keypad.

### `ENABLE_DTMF_CALLING` (default: 0)
Adds DTMF selective-calling/signaling: decodes received DTMF strings to detect calls addressed to the radio's ANI ID, matches against a stored contact list, supports group vs. private calls with ring/auto-reply responses, and implements remote "kill"/"revive" codes that can remotely disable (`gSetting_KILLED`) or re-enable the radio.

### `ENABLE_FLASHLIGHT` (default: 1)
Builds `app/flashlight.o`, driving the white LED on `GPIOC_PIN_FLASHLIGHT` as an assignable key action. In non-F4HWN builds (or F4HWN with `ENABLE_FEAT_F4HWN_RESCUE_OPS`/`ENABLE_FEAT_F4HWN_FLASHLIGHT_SOS`) it has three modes: steady on, blinking, and an SOS Morse-code blink pattern; otherwise (plain F4HWN) it's a simple on/off toggle.

---

## Custom Mods

### `ENABLE_SPECTRUM` (default: 0)
Compiles in `app/spectrum.c`, a full-screen spectrum-analyzer/bandscope app that sweeps and displays signal strength across a frequency range; it is launched by pressing F+5 (in place of the NOAA-channel shortcut, when `ENABLE_NOAA` is off) via `APP_RunSpectrum()`. Enabling it also disables the F4HWN gauge-bar UI helper (`ST7565_Gauge`) and mute action wherever they conflict with spectrum's own screen usage, and it sets a bit in an EEPROM feature-flags byte.

### `ENABLE_BIG_FREQ` (default: 1)
Splits the on-screen VFO frequency into large main digits (drawn with `UI_DisplayFrequency`) plus two small trailing digits printed separately, both for live frequency entry and normal channel/frequency display, instead of printing the whole frequency string in one uniform font size; as a side effect there's no longer room to show the compander indicator bitmap next to the frequency.

### `ENABLE_SMALL_BOLD` (default: 1)
Adds a second, visually bolder small-font glyph table (`gFontSmallBold`) and makes `UI_PrintStringSmallBold`/`UI_PrintStringSmallBufferBold` use it; without the flag those "bold" print functions silently fall back to the regular small font (`gFontSmall`), so enabling it makes small bold-styled text actually appear bold rather than identical to normal small text.

### `ENABLE_CUSTOM_MENU_LAYOUT` (default: 1)
Replaces the original 3-line inverted-scroll menu list layout with an alternate "experimental" layout in `UI_DisplayMenu` that shows the current item large/bold in the middle with the neighboring items in small text above/below (with wrap-around), and removes the separate current-item indicator bitmap/vertical dotted separator used by the original layout.

### `ENABLE_KEEP_MEM_NAME` (default: 1)
When saving a VFO's settings into a memory channel slot, this stops `SETTINGS_SaveChannel` from blanking the channel's name; without it, any channel save clears the stored name, while with it the name is preserved unless the save is an explicit rename (`Mode >= 3`), in which case the new name from `pVFO->Name` is written.

### `ENABLE_WIDE_RX` (default: 1)
Extends the two "open" receive frequency bands: BAND1 (50MHz) is widened from 50.000–76.000MHz to the BK4819's full low-band limit of 18.000–108.000MHz, and BAND7 (470MHz) is widened from 470.000–600.000MHz up to 1300.000MHz (the chip's upper band limit), and the band-cycling key handler is adjusted to jump straight to 1GHz when crossing from BAND7 into the next band.

### `ENABLE_TX_WHEN_AM` (default: 0)
Removes the check that otherwise forces `VFO_STATE_TX_DISABLE` (blocking transmit) whenever the VFO's modulation mode is not FM; with it enabled, TX is permitted while in AM (or other non-FM) modulation.

### `ENABLE_F_CAL_MENU` (default: 0)
Adds an "F CALI" entry to the settings menu (`MENU_F_CALI`) that lets the user adjust the reference-crystal frequency calibration offset in the range -50 to +50, writing the value via `writeXtalFreqCal()`.

### `ENABLE_CTCSS_TAIL_PHASE_SHIFT` (default: 0)
Changes how `BK4819_PlayCTCSSTail()` signals end-of-transmission on CTCSS: instead of the default QS-style 55Hz tone-burst tail (`BK4819_GenTail(4)`), it uses a 180-degree phase-shift tail (`BK4819_GenTail(2)`), an alternate CTCSS tail-elimination method some repeaters expect.

### `ENABLE_BOOT_BEEPS` (default: 0)
Defined in the Makefile and passed as a `-D` flag, but no `#ifdef ENABLE_BOOT_BEEPS` guard exists anywhere in the current source tree — the flag currently has no effect on compiled behavior.

### `ENABLE_SHOW_CHARGE_LEVEL` (default: 0)
While charging over USB-C, forces periodic display updates and shows a "Charge X.XXV NN%" string (voltage and battery percentage) on the center status line of the main screen, instead of leaving that line free for other status content.

### `ENABLE_REVERSE_BAT_SYMBOL` (default: 0)
Reverses the fill direction of the battery-level icon's bar segments in `UI_DrawBattery`: normally bars fill from the icon's right (terminal) end inward, but with this flag they fill from the left (cap) end outward instead.

### `ENABLE_NO_CODE_SCAN_TIMEOUT` (default: 1)
Removes the ~16-second (32 × 500ms) timeout in `SCANNER_TimeSlice500ms` that would otherwise abort a CTCSS/DCS code scan (marking it found/failed) if no code was detected in time, letting code-scanning run indefinitely; it also prevents the display from being forced back to the main screen while a scan is still in progress.

### `ENABLE_AM_FIX` (default: 1)
Compiles in `am_fix.c`, an adaptive routine that continuously monitors and adjusts the BK4819's AM-mode gain/AGC settings per frequency to reduce AM-demodulator saturation/overload (a known hardware limitation), and adds an "AM FIX" on/off setting plus (with `ENABLE_AM_FIX_SHOW_DATA`) an on-screen debug readout of the adjusted register values.

### `ENABLE_SQUELCH_MORE_SENSITIVE` (default: 1)
In `RADIO_ConfigureSquelchAndOutputPower`, scales the squelch-open thresholds read from EEPROM to be more sensitive — RSSI-open threshold is halved and noise/glitch-open thresholds are doubled — then nudges the corresponding close thresholds apart from the open thresholds if they'd otherwise collide, making the squelch open on weaker signals than the stock calibration.

### `ENABLE_FASTER_CHANNEL_SCAN` (default: 1)
Shortens the pause between scan steps to a fixed 90ms for both frequency scanning (down from the stock 100ms) and memory-channel scanning (down from the stock 200ms), via `gScanPauseDelayIn_10ms = 9` in `app/chFrScanner.c`, making channel scanning noticeably faster at the risk of missing brief signals.

### `ENABLE_RSSI_BAR` (default: 1)
Replaces the small antenna/signal-bar icon in the VFO frequency line with a full-width graphical RSSI bar shown on the center status line while receiving (`DisplayRSSIBar`), instead of the compact multi-bar antenna glyph used otherwise.

### `ENABLE_AUDIO_BAR` (default: 1)
Adds a "MIC" setting and a microphone/audio-level bar graph (`UI_DisplayAudioBar`) drawn on the center status line while transmitting, refreshed roughly every 150ms, giving a live visual indication of TX audio level.

### `ENABLE_COPY_CHAN_TO_VFO` (default: 1)
Changes what the F+1 ("1 A/B") key does while a memory channel is selected: instead of being a no-op, it copies that channel's configuration into the corresponding frequency-mode VFO slot and switches the display to frequency mode on that VFO (guarded against firing during an active scan or when the VFO is closed).

### `ENABLE_REDUCE_LOW_MID_TX_POWER` (default: 0)
Further reduces the calculated TX power register values when the output-power setting is LOW or MID: LOW power is divided by 5 and MID power by 3 (on top of the normal power curve), lowering actual transmit power below the standard LOW/MID levels.

### `ENABLE_BYP_RAW_DEMODULATORS` (default: 0)
Adds two extra selectable demodulation/modulation modes beyond FM/AM/USB — "BYP" (an undocumented BK4819 baseband path, `BK4819_AF_UNKNOWN3`) and "RAW" (raw baseband output, `BK4819_AF_BASEBAND1`) — selectable per VFO/channel via the MODE action, bypassing the chip's built-in demodulators.

### `ENABLE_BLMIN_TMP_OFF` (default: 0)
Adds a "BLMIN TMP OFF" action-menu entry that toggles a tri-state `BACKLIGHT_MIN_STAT` flag; when engaged, `BACKLIGHT_TurnOff()` forces the idle backlight brightness to 0 instead of the configured `BACKLIGHT_MIN` level, letting the user temporarily disable the minimum/idle backlight glow without changing the stored setting.

### `ENABLE_SCAN_RANGES` (default: 1)
Lets the user define a custom frequency scan range by pressing the scanlist-toggle key (`*`) while on a non-memory (frequency) VFO: it sets `gScanRangeStart`/`gScanRangeStop` from the two VFOs' current frequencies (swapping if needed) and subsequent scans restrict themselves to that range instead of sweeping the whole band; switching VFOs (`COMMON_SwitchVFOs`) clears the range, and while a range is active it also suppresses starting DTMF entry via the same key.

---

## Contrib Mods

### `ENABLE_REGA` (default: 0)
Adds Swiss Air Rescue (REGA) test/alarm tone functions (`app/rega.c`, by @markusb): an assignable key action that keys the transmitter and sends a fixed ZVEI tone sequence — `21301` for "Test" or `21414` for "Alarm" — using precomputed BK4819 register values, then returns to receive.

### `ENABLE_EXTRA_UART_CMD` (default: 0)
Adds extra serial commands to the UART protocol (by @reppad): `0x0527` reads live RSSI/noise/glitch indicator values, `0x0529` reads battery voltage/current from the ADC, and (when not building the F4HWN variant) `0x052D` performs AES-challenge authentication to unlock EEPROM write access.

---

## F4HWN Mods

`ENABLE_FEAT_F4HWN` is the master switch for the "F4HWN" edition of the firmware; when off, the build reverts to the plain "egzumer" feel (stock strings, `firmware` target instead of `f4hwn`, simpler flashlight/volume/lock behavior). All the `ENABLE_FEAT_F4HWN_*` sub-flags below only take effect when the code they guard is itself compiled in (most also require the base feature, e.g. `ENABLE_SPECTRUM`, `ENABLE_FLASHLIGHT`), and several are additionally referenced together with `ENABLE_FEAT_F4HWN` via `#if defined(...)` combinations rather than being nested inside a single Makefile block.

### `ENABLE_FEAT_F4HWN` (default: 1)
Master edition switch: sets the build `TARGET` to `f4hwn` (vs `firmware`), defines `ALERT_TOT=10` and a custom `SQL_TONE`, sets the F4HWN author/version/edition strings used in the firmware-pack step and welcome screen, and is checked directly or in combination (`defined(ENABLE_FEAT_F4HWN_X) || ...`) throughout the UI, settings, and radio code to gate the F4HWN-specific behaviors documented below.

### `ENABLE_FEAT_F4HWN_GAME` (default: 0)
Compiles in `app/breakout.c`, a Breakout-style arcade game, and binds it to the F-key "7" shortcut on the main screen (`APP_RunBreakout()` in `app/main.c`); also reserves an EEPROM build-option bit for it in `settings.c`.

### `ENABLE_FEAT_F4HWN_SCREENSHOT` (default: 0)
Compiles `screenshot.c`/`.h`, which periodically diffs the LCD framebuffer (`gStatusLine` + main display) and streams changed blocks out over UART (`getScreenShot()`, called from `app/app.c` on every display update) so a PC tool can capture live screenshots; also adds a UART-cable-connected check (`UART_IsCableConnected`) and a lock (`gUART_LockScreenshot`) that suppresses screenshotting for a short time after a UART/CHIRP command to avoid interference.

### `ENABLE_FEAT_F4HWN_SPECTRUM` (default: 1)
Adds F4HWN-specific enhancements layered on top of the base spectrum analyzer (only meaningful when the separate `ENABLE_SPECTRUM` flag builds `app/spectrum.c` at all): a dedicated EEPROM settings block for the spectrum screen (LoadSettings/SaveSettings at `0x1FF0`), named-in-dB LNA/LNA-short/VGA gain and BPF-bandwidth option labels, a "tail tone found" BK4819 interrupt used to end squelch-open dwell time faster/more accurately than the plain still-RSSI check, showing the matching channel name under the frequency when listening, restoring the selected listen bandwidth instead of always forcing `BK4819_FILTER_BW_WIDE`, and (with `ENABLE_FEAT_F4HWN_RESUME_STATE`) writing/restoring which spectrum mode was active across power cycles.

### `ENABLE_FEAT_F4HWN_RX_TX_TIMER` (default: 1)
Adds a live TX-elapsed / RX-elapsed clock (`MM:SS`) drawn on the status line while transmitting or receiving (toggled on/off via the `SetTmr` setting); a 1-hour countdown (`gRxTimerCountdown_500ms`, reset to 7200 on RX/squelch-open) is decremented in the systick handler and the elapsed value derived from it is what gets printed.

### `ENABLE_FEAT_F4HWN_CHARGING_C` (default: 0)
Adds a USB-C charging icon on the status line: when the radio detects it's on charge (battery current above ~500 mA, tracked in `gChargingWithTypeC` in `helper/battery.c`), the status bar shows `BITMAP_USB_C` instead of the flashlight/mute/other icon slot (`ui/status.c`). The charge detection itself is unconditional; the flag only controls whether the icon is drawn.

### `ENABLE_FEAT_F4HWN_SLEEP` (default: 1)
Adds a true display/power "deep sleep" mode with its own configurable timeout (new `SetOff` menu entry, 0–120 min): after the configured idle period the LCD controller is fully shut down (`ST7565_ShutDown()`), power-save intervals switch to a deeper duty cycle (`BATTERY_SAVE * 200` vs `* 10`), and the next keypress (other than PTT, which is swallowed once) wakes the screen back up and reinitializes the display.

### `ENABLE_FEAT_F4HWN_RESUME_STATE` (default: 1)
Persists which "mode" the radio was in when powered off (`gEeprom.CURRENT_STATE`, 0–5: none, channel scan, frequency-range scan, FM radio, spectrum, or spectrum-in-scan-range) to EEPROM and, at boot, automatically re-enters that mode (restarts `CHFRSCANNER_Start`, `ACTION_FM()`, or `APP_RunSpectrum()` as appropriate) instead of always booting into the plain VFO/channel screen.

### `ENABLE_FEAT_F4HWN_NARROWER` (default: 1)
Adds a "SetNFM" setting (NARROW/NARROWER) that, when enabled, swaps the standard narrow-FM filter bandwidth for an even tighter `BK4819_FILTER_BW_NARROWER` filter on RX, and shows an "N" narrower-bandwidth indicator on the VFO screen.

### `ENABLE_FEAT_F4HWN_INV` (default: 1)
Adds a "SetInv" on/off setting that inverts the LCD display (white-on-black), applied via `ST7565_ContrastAndInv()`/the `ST7565_CMD_INVERSE_DISPLAY` command.

### `ENABLE_FEAT_F4HWN_CTR` (default: 1)
Adds a "SetCtr" numeric (0–15) LCD contrast setting, applied via `ST7565_ContrastAndInv()` writing `21 + gSetting_set_ctr` to the display's contrast/electronic-volume command.

### `ENABLE_FEAT_F4HWN_RESCUE_OPS` (default: 0)
Adds a hidden boot-time key combo (`10 + gEeprom.SET_KEY`, the key itself configurable via a new "SetKey" menu entry, held with PTT unpressed at power-on) that toggles a persistent `gEeprom.MENU_LOCK` flag; when locked, the Menu key and the star/F-function key are disabled (radio is restricted to basic RX/TX on the main screen, shown with an "RO" status-line icon) — useful for handing a preconfigured radio to non-technical users. It also adds two quick-action bindings, "Power High" (force max TX power, bypassing the normal power-level setting) and "Remove Offset" (transmit on the RX frequency, ignoring any configured repeater shift), intended as emergency/first-responder overrides that remain reachable even while the menu is locked. Also implies the plain (non-`ENABLE_FEAT_F4HWN`-editing) flashlight SOS-blink mode via `app/flashlight.c`'s guard.

### `ENABLE_FEAT_F4HWN_FLASHLIGHT_SOS` (default: 1)
Enables the flashlight's existing SOS-blink cycling mode (OFF → ON → BLINK → SOS, `app/flashlight.c`) in the F4HWN "Custom" build even without `ENABLE_FEAT_F4HWN_RESCUE_OPS` enabled; without either of those two flags (or when `ENABLE_FEAT_F4HWN` is off entirely) the flashlight action falls back to a plain on/off toggle with no blink or SOS states.

### `ENABLE_FEAT_F4HWN_VOL` (default: 0)
Adds a "SetVol" menu entry (0–63) that exposes the AF volume-gain register (`gEeprom.VOLUME_GAIN`) directly in the settings menu, with its own dedicated EEPROM write-back (`SETTINGS_WriteCurrentVol()` at `0x1F88`) so it's saved immediately rather than only on a full settings save.

### `ENABLE_FEAT_F4HWN_RESET_CHANNEL` (default: 0)
When doing a full ("ALL") EEPROM reset, pre-populates the first 5 memory channels with a default frequency table (145.000, 145.500, 433.000, 433.200, 433.500 MHz) instead of leaving them at whatever the reset template contains.

### `ENABLE_FEAT_F4HWN_PMR` (default: 0)
Adds a "PMR 446" option to the TX frequency-lock (`F_LOCK`) menu, restricting transmit to 446.00625–446.19375 MHz (the European PMR446 channel block).

### `ENABLE_FEAT_F4HWN_GMRS_FRS_MURS` (default: 0)
Adds a "GMRS/FRS/MURS" option to the `F_LOCK` menu, restricting transmit to the US FRS/GMRS channels (462.5500–462.7250 MHz and 467.5500–467.7250 MHz) plus the 5 discrete MURS frequencies (151.820/151.880/151.940/154.570/154.600 MHz).

### `ENABLE_FEAT_F4HWN_CA` (default: 1)
Adds a "CA HAM" option to the `F_LOCK` menu, restricting transmit to 144–148 MHz and 430–450 MHz (the Canadian amateur-radio band allocation, slightly wider on UHF than the CE/GB European options already in the base firmware).

### `ENABLE_FEAT_F4HWN_DEBUG` (default: 0)
Replaces the RX/TX elapsed-timer readout on the status line (see `ENABLE_FEAT_F4HWN_RX_TX_TIMER`) with a raw numeric field printing a developer scratch variable (`gDebug`); intended for ad-hoc firmware debugging (currently no code actively assigns `gDebug`, only a commented-out example in `app/app.c`).

---

## Debugging

### `ENABLE_AM_FIX_SHOW_DATA` (default: 0)
Adds an on-screen debug overlay (requires `ENABLE_AM_FIX`) that periodically prints the AM-fix gain-table index, applied gain in dB, and smoothed RSSI on the main screen's center line, refreshed at up to 250 ms via `AM_fix_print_data`.

### `ENABLE_AGC_SHOW_DATA` (default: 0)
Adds `UI_MAIN_PrintAGC`, an on-screen debug overlay replacing the normal main-screen center line with live BK4819 AGC register data (signal strength/gain index bits), updated whenever the AM-fix gain table index changes or on each 500 ms tick.

### `ENABLE_UART_RW_BK_REGS` (default: 0)
Adds two serial commands, `0x0601` and `0x0602`, that let a PC directly read or write arbitrary BK4819 RF-chip registers over UART (`CMD_0601_ReadBK4819Reg`/`CMD_0602_WriteBK4819Reg`) — a raw register-level debug/tuning backdoor.

---

## Compiler/Linker Options

### `ENABLE_CLANG` (default: 0)
No `#ifdef` guards in source — purely a toolchain switch. Setting it to 1 changes `CC` from `arm-none-eabi-gcc` to `clang --sysroot=/usr/arm-none-eabi --target=arm-none-eabi`, and forces `ENABLE_LTO` off because GCC's linker can't consume LLVM bitcode.

### `ENABLE_SWD` (default: 0)
Reconfigures GPIO pins PB11/PB14 in `board.c` to act as the SWDIO/SWCLK debug interface instead of driving the ST7565 LCD control line, for attaching a hardware SWD debugger/programmer; also adds `-DENABLE_SWD` to `CFLAGS` and affects the ASM startup via `ASFLAGS`.

### `ENABLE_OVERLAY` (default: 0)
Builds `sram-overlay.o`/`driver/flash.o` and places flash-controller functions (`overlay_FLASH_*`) into RAM (`.sramtext`/`.srambss` linker sections) so they can safely run while manipulating the flash "mask" register — used to implement `overlay_FLASH_RebootToBootloader()`, a reset path that reboots straight into the factory USB bootloader (used in place of `NVIC_SystemReset()` for the UART `0x05DD` command and various menu/low-battery reset paths). Mutually exclusive with LTO (the Makefile forces `ENABLE_LTO := 0` when this is on).

### `ENABLE_LTO` (default: 1)
No `#ifdef` guards in source — a pure compiler/linker option. Enables GCC link-time optimization (`-flto=auto`); when disabled, the build instead adds `-ffunction-sections -fdata-sections` for section-level dead-code elimination. Disabled automatically if `ENABLE_CLANG` or `ENABLE_OVERLAY` is set.

### `ENABLE_EXPERIMENTAL_CLFAGS` (default: 1)
No `#ifdef` guards in source — a pure compiler-flags option. Adds `-funroll-loops -ffat-lto-objects` to `CFLAGS` for potentially better code generation/size at the cost of being a less battle-tested flag combination.

---

## Flash cost of each flag

Measured with [`measure_flash_flags.sh`](measure_flash_flags.sh), which builds
a baseline using the Makefile's own current `?=` defaults (no overrides),
then rebuilds once per flag with just that flag toggled to its opposite
value, diffing `.text + .data` against the baseline. Costs are normalized to
**"bytes added when the flag is ON"** regardless of which direction had to be
tested to measure that — for a flag that defaults to 1, this means measuring
what disabling it *saves*, then reporting the negation.

**Baseline** at time of measurement: `.text` 61,300 + `.data` 52 = 61,352
bytes, against a 61,440-byte (60KiB) `FLASH` region — **88 bytes of
headroom**. That headroom is small enough that most currently-disabled flags
overflow the linker's `FLASH` region when toggled on; those rows are marked
"est." and computed as `overflow bytes + 88 bytes headroom`, which is the
minimum possible cost (the true cost could be somewhat higher — this only
proves a lower bound, since a smaller baseline might reveal the feature costs
more once it has room to fully link in without hitting the region limit
early). Flags already fitting after being toggled are exact, direct
measurements.

**This is not a table of isolable, additive costs.** Flags interact (shared
helper functions, shared menu-table rows, overlapping `#ifdef` branches like
`ENABLE_NOAA`/`ENABLE_SPECTRUM` both hooking the same `F+5` shortcut), so a
flag's cost measured against *this* baseline can differ from its cost against
a different one — see the `ENABLE_NOAA`/`ENABLE_AIRCOPY` figures below versus
the larger numbers quoted earlier in this session against a build that also
had `ENABLE_SPECTRUM`/`ENABLE_FMRADIO` on. Re-run the script after changing
your own baseline configuration if you need numbers accurate to it.

This sweep originally caught **four rows** hitting genuine, pre-existing
compile errors unrelated to flash budget — real latent bugs (undeclared
identifiers, and one unbalanced `#ifdef`/`#else`/`#endif` spanning an
`if`/`else` in a way that left a stray unmatched brace) in code paths that
only compile under specific flag combinations. `ENABLE_VOICE`,
`ENABLE_BIG_FREQ`, and `ENABLE_SCAN_RANGES` have since been fixed and now
measure cleanly — the table below reflects that. `ENABLE_FEAT_F4HWN` (the
non-F4HWN "egzumer" build) is still broken (a `driver/backlight.c` issue
identified independently, out of scope for this pass) and remains N/A.

| Flag | Default | Cost when ON (flash bytes) | Notes |
|---|---|---|---|
| `ENABLE_FMRADIO` | 1 | +2608 | measured directly |
| `ENABLE_FMRADIO_MINIMIZED` | 1 | ~-1540 (est.) | disabling overflowed the baseline by 1452 bytes, i.e. the OFF (full-featured) state is bigger - ON saves an estimated 1540 bytes (overflow + 88 bytes headroom) |
| `ENABLE_UART` | 1 | +1220 | measured directly |
| `ENABLE_AIRCOPY` | 0 | ~+2100 (est.) | enabling overflowed the baseline by 2012 bytes; cost estimated as overflow + 88 bytes headroom |
| `ENABLE_NOAA` | 0 | ~+1796 (est.) | enabling overflowed the baseline by 1708 bytes; cost estimated as overflow + 88 bytes headroom |
| `ENABLE_VOICE` | 0 | ~+1548 (est.) | enabling overflowed the baseline by 1460 bytes; cost estimated as overflow + 88 bytes headroom |
| `ENABLE_VOX` | 1 | +916 | measured directly |
| `ENABLE_ALARM` | 0 | ~+916 (est.) | enabling overflowed the baseline by 828 bytes; cost estimated as overflow + 88 bytes headroom |
| `ENABLE_TX1750` | 0 | ~+252 (est.) | enabling overflowed the baseline by 164 bytes; cost estimated as overflow + 88 bytes headroom |
| `ENABLE_PWRON_PASSWORD` | 0 | ~+652 (est.) | enabling overflowed the baseline by 564 bytes; cost estimated as overflow + 88 bytes headroom |
| `ENABLE_DTMF_CALLING` | 0 | ~+3720 (est.) | enabling overflowed the baseline by 3632 bytes; cost estimated as overflow + 88 bytes headroom |
| `ENABLE_FLASHLIGHT` | 1 | +276 | measured directly |
| `ENABLE_SPECTRUM` | 1 | +6172 | measured directly |
| `ENABLE_BIG_FREQ` | 1 | +104 | measured directly |
| `ENABLE_SMALL_BOLD` | 1 | +580 | measured directly |
| `ENABLE_CUSTOM_MENU_LAYOUT` | 1 | -52 | measured directly |
| `ENABLE_KEEP_MEM_NAME` | 1 | +12 | measured directly |
| `ENABLE_WIDE_RX` | 1 | +72 | measured directly |
| `ENABLE_TX_WHEN_AM` | 0 | -8 | measured directly |
| `ENABLE_F_CAL_MENU` | 0 | ~+196 (est.) | enabling overflowed the baseline by 108 bytes; cost estimated as overflow + 88 bytes headroom |
| `ENABLE_CTCSS_TAIL_PHASE_SHIFT` | 0 | +0 | measured directly |
| `ENABLE_BOOT_BEEPS` | 0 | +0 | measured directly (this flag has no `#ifdef` anywhere in source - see above) |
| `ENABLE_SHOW_CHARGE_LEVEL` | 0 | ~+100 (est.) | enabling overflowed the baseline by 12 bytes; cost estimated as overflow + 88 bytes headroom |
| `ENABLE_REVERSE_BAT_SYMBOL` | 0 | +0 | measured directly |
| `ENABLE_NO_CODE_SCAN_TIMEOUT` | 1 | -20 | measured directly |
| `ENABLE_AM_FIX` | 1 | +652 | measured directly |
| `ENABLE_SQUELCH_MORE_SENSITIVE` | 1 | +92 | measured directly |
| `ENABLE_FASTER_CHANNEL_SCAN` | 1 | +0 | measured directly |
| `ENABLE_RSSI_BAR` | 1 | +508 | measured directly |
| `ENABLE_AUDIO_BAR` | 0 | ~+432 (est.) | enabling overflowed the baseline by 344 bytes; cost estimated as overflow + 88 bytes headroom |
| `ENABLE_COPY_CHAN_TO_VFO` | 1 | +128 | measured directly |
| `ENABLE_REDUCE_LOW_MID_TX_POWER` | 0 | +0 | measured directly |
| `ENABLE_BYP_RAW_DEMODULATORS` | 1 | +32 | measured directly |
| `ENABLE_BLMIN_TMP_OFF` | 0 | +76 | measured directly |
| `ENABLE_SCAN_RANGES` | 1 | +1060 | measured directly |
| `ENABLE_REGA` | 0 | ~+628 (est.) | enabling overflowed the baseline by 540 bytes; cost estimated as overflow + 88 bytes headroom |
| `ENABLE_EXTRA_UART_CMD` | 0 | ~+224 (est.) | enabling overflowed the baseline by 136 bytes; cost estimated as overflow + 88 bytes headroom |
| `ENABLE_FEAT_F4HWN` | 1 | N/A | pre-existing bug, confirmed independently earlier this session: disabling it (the non-F4HWN "egzumer" build) fails to compile - `driver/backlight.c` has several undeclared-identifier errors in that code path |
| `ENABLE_FEAT_F4HWN_GAME` | 0 | ~+1884 (est.) | enabling overflowed the baseline by 1796 bytes; cost estimated as overflow + 88 bytes headroom |
| `ENABLE_FEAT_F4HWN_SCREENSHOT` | 0 | ~+468 (est.) | enabling overflowed the baseline by 380 bytes; cost estimated as overflow + 88 bytes headroom |
| `ENABLE_FEAT_F4HWN_SPECTRUM` | 1 | +604 | measured directly |
| `ENABLE_FEAT_F4HWN_RX_TX_TIMER` | 0 | ~+236 (est.) | enabling overflowed the baseline by 148 bytes; cost estimated as overflow + 88 bytes headroom |
| `ENABLE_FEAT_F4HWN_CHARGING_C` | 0 | +28 | measured directly |
| `ENABLE_FEAT_F4HWN_SLEEP` | 1 | +512 | measured directly |
| `ENABLE_FEAT_F4HWN_RESUME_STATE` | 1 | +384 | measured directly |
| `ENABLE_FEAT_F4HWN_NARROWER` | 1 | +216 | measured directly |
| `ENABLE_FEAT_F4HWN_INV` | 0 | +56 | measured directly |
| `ENABLE_FEAT_F4HWN_CTR` | 0 | +88 | measured directly |
| `ENABLE_FEAT_F4HWN_RESCUE_OPS` | 0 | ~+500 (est.) | enabling overflowed the baseline by 412 bytes; cost estimated as overflow + 88 bytes headroom |
| `ENABLE_FEAT_F4HWN_FLASHLIGHT_SOS` | 1 | +204 | measured directly |
| `ENABLE_FEAT_F4HWN_VOL` | 0 | ~+132 (est.) | enabling overflowed the baseline by 44 bytes; cost estimated as overflow + 88 bytes headroom |
| `ENABLE_FEAT_F4HWN_RESET_CHANNEL` | 0 | +68 | measured directly |
| `ENABLE_FEAT_F4HWN_PMR` | 0 | +32 | measured directly |
| `ENABLE_FEAT_F4HWN_GMRS_FRS_MURS` | 0 | ~+116 (est.) | enabling overflowed the baseline by 28 bytes; cost estimated as overflow + 88 bytes headroom |
| `ENABLE_FEAT_F4HWN_CA` | 0 | +56 | measured directly |
| `ENABLE_FEAT_F4HWN_DEBUG` | 0 | -124 | measured directly |
| `ENABLE_AM_FIX_SHOW_DATA` | 0 | ~+352 (est.) | enabling overflowed the baseline by 264 bytes; cost estimated as overflow + 88 bytes headroom |
| `ENABLE_AGC_SHOW_DATA` | 0 | +40 | measured directly |
| `ENABLE_UART_RW_BK_REGS` | 0 | +72 | measured directly |
| `ENABLE_CLANG` | 0 | N/A | not measurable in this environment: `clang` isn't installed in the Docker build image used for measurement (only `arm-none-eabi-gcc`) |
| `ENABLE_SWD` | 0 | +4 | measured directly |
| `ENABLE_OVERLAY` | 0 | ~+808 (est.) | enabling overflowed the baseline by 720 bytes; cost estimated as overflow + 88 bytes headroom |
| `ENABLE_LTO` | 1 | ~-4236 (est.) | disabling overflowed the baseline by 4148 bytes, i.e. the OFF state is bigger - ON saves an estimated 4236 bytes (overflow + 88 bytes headroom) |
| `ENABLE_EXPERIMENTAL_CLFAGS` | 1 | -24 | measured directly |

A few standouts: `ENABLE_LTO` is by far the single biggest lever on this list
(~4.2KB) and costs nothing feature-wise to keep on. `ENABLE_SPECTRUM`
(+6.2KB) and `ENABLE_DTMF_CALLING` (~+3.7KB est.) are the priciest actual
features. Several flags measure `+0` exactly — `ENABLE_CTCSS_TAIL_PHASE_SHIFT`,
`ENABLE_REVERSE_BAT_SYMBOL`, `ENABLE_FASTER_CHANNEL_SCAN`,
`ENABLE_REDUCE_LOW_MID_TX_POWER` — these swap constants/behavior rather than
adding code, so they're effectively free either way.
