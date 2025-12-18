# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

This is a Magic Eight Ball application for the M5Stack M5Cardputer device (ESP32-based handheld). The application accepts questions via keyboard input or voice recording, uses audio waveform analysis for randomness, and displays responses with optional audio playback and bitmap images loaded from SD card.

**Key Features:**
- Type questions on keyboard or record voice questions
- JSON-driven responses from SD card configuration
- Optional WAV audio playback for responses
- Optional bitmap image display for responses
- Audio waveform-based randomness for voice input
- Text hash-based randomness for typed questions

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
- **Required files:** `/responses.json`, `/audio/*.wav` (optional), `/images/*.bmp` (optional)

**Audio Specifications:**
- Sample rate: 16kHz
- Format: 16-bit PCM mono
- Voice recording buffer: 2 seconds (~32,000 samples, 64KB)
- Response audio: Variable length, loaded from SD card

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
The entire application is in `firmware/src/main.cpp` (~400-500 lines). This is intentional for embedded systems - keeping everything in one file simplifies debugging and reduces complexity for resource-constrained devices.

### State Machine Architecture

The application uses a state-based design with 5 states:

1. **IDLE** - Shows welcome screen, waits for input
2. **TEXT_INPUT** - User typing question with live display
3. **VOICE_INPUT** - Recording 2 seconds of audio with waveform
4. **THINKING** - Animated "thinking" display (2 seconds)
5. **SHOWING_ANSWER** - Display response text + audio + bitmap

**State Flow:**
```
IDLE → TEXT_INPUT → THINKING → SHOWING_ANSWER → IDLE
  └→ VOICE_INPUT → THINKING → SHOWING_ANSWER → IDLE
```

### Main Functional Components

**Response Configuration System:**
- `loadResponsesFromSD()` - Parses `/responses.json` using ArduinoJson
- `generateDefaultConfig()` - Creates default config if missing (30 responses: 20 classic + 10 custom)
- `Response` struct - Contains text, wav_path, bitmap_path fields
- Responses stored in `std::vector<Response>` loaded at startup

**Randomness Generation:**
- `generateSeedFromText(question)` - DJB2 hash + timestamp mixing for typed questions
- `generateSeedFromAudio(data, len)` - Peak amplitude + zero-crossings + RMS + LSB entropy for voice
- `selectResponse(seed)` - Maps seed to response index via modulo

**Display Functions:**
- `displayIdle()` - Title and prompt screen
- `displayTextInput(question)` - Live question display with blinking cursor
- `displayVoiceInput(progress)` - Recording progress bar + waveform
- `displayThinking()` - Animated thinking indicator
- `displayAnswer(idx)` - Response text + optional bitmap + audio indicator

**Audio/Media Playback:**
- `playResponseAudio(wav_path)` - Loads and plays WAV from SD card
- `displayBitmap(bitmap_path, x, y)` - Renders BMP image from SD card
- Waveform visualization reused from voice input display

**Input Handling:**
- Keyboard: Uses `Keyboard_Class::KeysState.word` vector for text accumulation
- Voice: Records 2-second audio clip with microphone
- Button A: Triggers response (press) or voice mode (hold)

### JSON Configuration Format

**File: `/responses.json`**
```json
[
  {
    "text": "It is certain",
    "wav": "audio/certain.wav",
    "bitmap": "images/certain.bmp"
  },
  {
    "text": "Reply hazy try again",
    "wav": "audio/hazy.wav"
  },
  {
    "text": "My sources say no"
  }
]
```

**Rules:**
- `text` field is required (the response to display)
- `wav` and `bitmap` are optional (file paths relative to SD root)
- Can contain any number of responses
- Missing files are handled gracefully (skipped, no error)

### Memory Management

- Voice recording buffer: 64KB allocated with `heap_caps_malloc()` for 2-second recording
- Response audio buffer: Dynamically allocated per playback, freed after use
- JSON document: Statically allocated with `StaticJsonDocument` or `DynamicJsonDocument`
- Display buffers: Managed by M5Cardputer/M5GFX library

### User Interface Flow

1. **Startup:** Display shows "MAGIC EIGHT BALL" title with input prompt
2. **Text Input:** User types question → characters accumulate → press BtnA when done
3. **Voice Input:** User holds BtnA → speak question → release when done
4. **Processing:** "Thinking" animation displays for 2 seconds while computing randomness
5. **Response:** Shows text answer with optional bitmap image and plays optional audio
6. **Return:** Auto-returns to idle after 5 seconds or user presses BtnA

### Keyboard Controls

- **Any letter/number key:** Enter TEXT_INPUT mode and start typing
- **Backspace (DEL):** Remove last character in TEXT_INPUT
- **Enter:** Submit question (same as BtnA in TEXT_INPUT)
- **ESC:** Cancel and return to IDLE
- **BtnA (press):** Submit question / return to IDLE from answer
- **BtnA (hold 500ms):** Enter VOICE_INPUT mode

## Development Notes

### Hardware Initialization Order

Critical sequence in `setup()`:
1. M5Cardputer initialization (display, buttons, keyboard)
2. Serial communication (115200 baud for debugging)
3. SD card SPI bus setup
4. SD card mount verification
5. JSON config loading (with fallback to default generation)
6. Memory allocation for audio buffer
7. Initial display (show IDLE screen)

Do not reorder these - SD card must initialize before JSON parsing, and display must configure before any rendering calls.

### Modifying Responses

To add/change responses without rebuilding firmware:
1. Edit `/responses.json` on SD card
2. Add corresponding WAV files to `/audio/` directory (optional)
3. Add corresponding BMP files to `/images/` directory (optional)
4. Reboot device (responses loaded at startup)

### Audio File Requirements

WAV files must be:
- 16-bit PCM mono format
- 16kHz sample rate (recommended, but other rates work)
- Standard RIFF WAV format with 44-byte header
- Reasonable length (< 5 seconds recommended for memory)

### Bitmap File Requirements

BMP files must be:
- 24-bit uncompressed format
- Reasonable size (64x64 recommended for display space)
- Standard BMP format with proper header

### Randomness Algorithm

**Text Input:**
- Uses DJB2 hash of lowercase question text
- Mixes with `millis()` timestamp for temporal variation
- Additional bit mixing for even distribution
- Same question at different times = different responses

**Voice Input:**
- Extracts multiple audio features:
  - Peak amplitude (max absolute value)
  - Zero-crossing count (frequency characteristic)
  - RMS energy (loudness)
  - LSB patterns (high-frequency noise)
- Combines features with XOR and bit shifting
- Mixes with timestamp for uniqueness
- Different vocal characteristics = different responses

### Display Coordinate System

- Origin (0,0) is top-left after rotation
- Text drawn with `top_center` datum (horizontally centered)
- Waveform centered vertically: `(Display.height() >> 1) + y_offset`
- Screen dimensions accessible via `M5Cardputer.Display.width()` and `.height()`

### Example Code Reference

The `docs_examples/example_code/` directory contains official M5Cardputer library examples demonstrating:
- Keyboard text input (`keyboard/inputText/`)
- Display rendering (`display/`)
- Microphone recording (`mic/`, `mic_wav_record/`)
- SD card I/O (`sdcard/`)

These are reference materials, not part of the firmware build.

## Common Development Patterns

### Testing JSON Parsing

Add debug output after loading responses:
```cpp
Serial.printf("Loaded %d responses from SD card\n", responses.size());
for (const auto& r : responses) {
    Serial.printf("  - %s", r.text.c_str());
    if (!r.wav_path.isEmpty()) Serial.printf(" [audio: %s]", r.wav_path.c_str());
    if (!r.bitmap_path.isEmpty()) Serial.printf(" [bitmap: %s]", r.bitmap_path.c_str());
    Serial.println();
}
```

### Testing Randomness Distribution

Log response selections to verify even distribution:
```cpp
// Add counter array
uint32_t response_counts[30] = {0};

// In selectResponse():
uint8_t idx = seed % responses.size();
response_counts[idx]++;
Serial.printf("Response %d selected (total: %d)\n", idx, response_counts[idx]);
```

### Debugging SD Card Issues

Enable verbose SD card output:
```cpp
Serial.println("SD Card Status:");
Serial.printf("  Type: %d\n", SD.cardType());
Serial.printf("  Size: %llu MB\n", SD.cardSize() / (1024 * 1024));
Serial.printf("  Free: %llu MB\n", (SD.totalBytes() - SD.usedBytes()) / (1024 * 1024));
```

### Debugging Display Issues

Enable display after each drawing operation with `M5Cardputer.Display.display()` if changes aren't appearing. The display uses buffered rendering.

### Memory Usage Monitoring

Check available heap to prevent out-of-memory errors:
```cpp
Serial.printf("Free heap: %d bytes\n", ESP.getFreeHeap());
Serial.printf("Largest free block: %d bytes\n", ESP.getMaxAllocHeap());
```

## Dependencies

**Required libraries (platformio.ini):**
```ini
lib_deps =
    m5stack/M5Cardputer@^1.1.1
    bblanchon/ArduinoJson@^7.0.0
```

**M5Cardputer** provides:
- M5GFX display library
- M5Unified hardware abstraction
- Keyboard, microphone, speaker drivers

**ArduinoJson** provides:
- JSON parsing and serialization
- Efficient memory management for embedded systems

## Implementation Reference

See `PLAN.md` for detailed implementation phases and step-by-step guidance.
