# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a WAV audio recorder/player application for the M5Stack M5Cardputer device (ESP32-based handheld). The application records audio from the built-in microphone, saves it to an SD card as WAV files, and allows browsing/playback of recorded files with real-time waveform visualization.

## Hardware Target

**Device:** M5Stack M5Cardputer (m5stack-stamps3 board)
- ESP32 microcontroller
- Built-in display, microphone, speaker, keyboard
- SD card slot (SPI interface)
- Display orientation: landscape (rotation = 1)

**SD Card Configuration:**
- SPI pins: SCK=40, MISO=39, MOSI=14, CS=12
- SPI speed: 25MHz
- Supported types: SDSC, SDHC, MMC

**Audio Specifications:**
- Sample rate: 16kHz
- Format: 16-bit PCM mono
- Recording buffer: 512 chunks × 240 samples (~30.4 seconds)
- Memory usage: ~245KB heap allocation per recording buffer

## Build Commands

### Build firmware
```bash
cd firmware
pio run
```

### Upload to device
```bash
cd firmware
pio run --target upload
```

### Monitor serial output
```bash
cd firmware
pio run --target monitor
```

### Clean build
```bash
cd firmware
pio run --target clean
```

### Build and upload
```bash
cd firmware
pio run --target upload && pio run --target monitor
```

## Code Architecture

### Single-File Application
The entire application is in `firmware/src/main.cpp` (399 lines). This is intentional for embedded systems - keeping everything in one file simplifies debugging and reduces complexity for resource-constrained devices.

### Main Functional Components

**Audio Recording Pipeline:**
1. `setup()` initializes M5Cardputer, SD card, microphone, and scans for existing WAV files
2. `loop()` monitors BtnA for recording trigger
3. Recording captures 512 chunks of 240 samples each at 16kHz
4. `saveWAVToSD()` writes proper WAV header (44 bytes) + audio data to SD card
5. Files named as `recorded<N>.wav` with incrementing counter (resets on reboot)

**Playback Pipeline:**
1. `scanAndDisplayWAVFiles()` scans SD card root directory for `.wav` files
2. `updateDisplay()` renders file list with selection highlighting
3. Keyboard navigation: `;` (up), `.` (down), ENTER (play), DEL (delete)
4. `playSelectedWAVFile()` loads WAV data (skipping 44-byte header)
5. `playWAV()` plays audio with synchronized waveform display

**Waveform Visualization:**
- Audio samples are right-shifted by 6 bits for display scaling
- Uses `prev_y[]` and `prev_h[]` arrays to track previous frame for efficient updates
- Draws waveform with black erase + white redraw pattern
- Synchronized with audio playback/recording (displays 2 chunks behind for smoothness)

**Memory Management:**
- Audio buffer allocated with `heap_caps_malloc(record_size * sizeof(int16_t), MALLOC_CAP_8BIT)`
- Buffer reused for both recording and playback
- Global state maintained in static variables

### WAV File Format
The `WAVHeader` struct defines standard RIFF WAV format:
- 44-byte header containing RIFF, format, and data chunk descriptors
- Mono channel, 16-bit PCM encoding
- Sample rate: 16kHz (configurable via `record_samplerate`)
- File size and data size calculated dynamically during save

### User Interface Flow
1. **Startup:** Display shows list of existing WAV files (or "No WAV files found")
2. **Recording:** Press BtnA → red recording indicator → waveform display → auto-save to SD
3. **Browsing:** Use `;`/`.` keys to navigate file list (yellow = selected, white = unselected)
4. **Playback:** Press ENTER → play icon + waveform visualization
5. **Deletion:** Press DEL on selected file → immediate removal from SD card

## Development Notes

### When Modifying Audio Parameters
If you change `record_number`, `record_length`, or `record_samplerate`:
- Update `WAVHeader` struct fields accordingly (`sampleRate`, `byteRate`)
- Recalculate memory allocation in `setup()`
- Adjust waveform display shift factor if visualization appears clipped

### Hardware Initialization Order
Critical sequence in `setup()`:
1. M5Cardputer initialization (display, buttons)
2. SD card SPI bus setup
3. SD card mount verification
4. Memory allocation for audio buffer
5. Speaker disable, microphone enable (recording mode default)
6. Initial file scan

Do not reorder these - SD card must initialize before memory allocation, and audio peripherals must configure after M5Cardputer initialization.

### Display Coordinate System
- Origin (0,0) is top-left after rotation
- Text drawn with `top_center` datum (horizontally centered)
- Waveform centered vertically: `(Display.height() >> 1) + y_offset`

### Example Code Reference
The `docs_examples/example_code/` directory contains official M5Cardputer library examples demonstrating individual hardware features (keyboard, display, microphone, etc.). These are reference materials, not part of the firmware build.

## Common Development Patterns

### Testing SD Card Functionality
Add debug output to `setup()` after SD initialization to verify card type and size:
```cpp
printf("SD Card Type: %s\n", cardType == CARD_SD ? "SDSC" : "SDHC");
printf("SD Card Size: %lluMB\n", cardSize);
```

### Adding New Audio Effects
To process audio during recording/playback, modify the recording loop in `loop()` or playback loop in `playWAV()`. Audio data is in `rec_data` as `int16_t` array.

### Debugging Display Issues
Enable display after each drawing operation with `M5Cardputer.Display.display()` if changes aren't appearing. The display uses buffered rendering.
