# Magic Eight Ball Implementation Plan

## Overview
Transform the WAV recorder into an interactive Magic Eight Ball that:
- Accepts questions via keyboard input or voice recording
- Uses audio waveform analysis for voice-based randomness
- Loads responses from JSON config file on SD card
- Plays optional WAV audio for each response
- Displays optional bitmap images for each response
- Generates test placeholder files for development

## Requirements Summary
- **Input modes**: Type question + press BtnA (primary), voice recording (optional)
- **Responses**: JSON-driven with text + optional WAV + optional bitmap
- **Audio**: Voice + sound effect played simultaneously with text
- **Randomness**: Text hash for typed questions, audio waveform analysis for voice
- **Config**: `/responses.json` on SD card with structure: `[{"text":"...", "wav":"...", "bitmap":"..."}]`

## Critical Files

**Primary:**
- `/home/zac/projects/sandwich-labs/magic-eight-ball/firmware/src/main.cpp` - Complete rewrite
- `/home/zac/projects/sandwich-labs/magic-eight-ball/firmware/platformio.ini` - Add ArduinoJson dependency

**SD Card Files to Create:**
- `/responses.json` - Response configuration
- `/audio/*.wav` - Optional response audio files
- `/images/*.bmp` - Optional response bitmap images

## Architecture Changes

### Keep from Original
- SD card initialization and SPI setup (lines 74-96)
- Microphone recording capability (modified for 2-second voice mode)
- Display initialization and waveform rendering
- Audio buffer allocation

### Remove from Original
- File browsing UI (scanAndDisplayWAVFiles, updateDisplay for file lists)
- WAVHeader struct and saveWAVToSD function
- Playback of recorded files (playSelectedWAVFile, playWAVFileFromSD)
- File selection keyboard navigation

### Add New
- JSON config parser for responses
- State machine (IDLE, TEXT_INPUT, VOICE_INPUT, THINKING, SHOWING_ANSWER)
- Text input handling with live display
- Randomness generation (text hash + audio analysis)
- Bitmap display capability
- Response audio playback (different from voice recording playback)
- Test placeholder file generation on first boot

## JSON Configuration Format

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
- `text` field is required
- `wav` and `bitmap` are optional
- Array can contain any number of responses (recommend 20-30)
- File paths are relative to SD card root

## Implementation Phases

### Phase 1: Prepare Infrastructure
**Goal:** Set up JSON parsing and SD card structure

1. **Add ArduinoJson to platformio.ini**
   ```ini
   lib_deps =
       m5stack/M5Cardputer@^1.1.1
       bblanchon/ArduinoJson@^7.0.0
   ```

2. **Create Response struct**
   ```cpp
   struct Response {
       String text;
       String wav_path;    // Empty if no audio
       String bitmap_path; // Empty if no bitmap
   };
   std::vector<Response> responses;
   ```

3. **Create loadResponsesFromSD() function**
   - Open `/responses.json`
   - Parse with ArduinoJson
   - Populate `responses` vector
   - Return success/failure

4. **Create generateDefaultConfig() function**
   - If `/responses.json` doesn't exist, create it
   - Include 20 classic + 10 custom responses
   - Initially no wav/bitmap paths (placeholders come later)

5. **Modify setup()**
   - Keep SD card init (lines 74-96)
   - Call `generateDefaultConfig()` if needed
   - Call `loadResponsesFromSD()`
   - Verify responses loaded (print count to serial)

**Testing:** Build, upload, check serial output shows "Loaded N responses"

---

### Phase 2: Remove WAV Recorder Functionality
**Goal:** Clean slate for new code

1. **Remove functions:**
   - `scanAndDisplayWAVFiles()`
   - `playSelectedWAVFile()`
   - `playWAV()`
   - `saveWAVToSD()`
   - `updateDisplay()` (file list version)

2. **Remove globals:**
   - `WAVHeader` struct
   - `selectedFileIndex`, `wavFiles` vector
   - `file_counter`

3. **Keep but modify:**
   - Recording loop (will adapt for voice input)
   - Waveform display code (reuse for voice mode)
   - `rec_data` buffer (reduce size to 64KB for 2-second recording)

4. **Gut loop()**
   - Remove all keyboard navigation (`;`, `.`, DEL, ENTER)
   - Remove recording-then-save logic
   - Create empty state machine skeleton

**Testing:** Should compile, display blank screen

---

### Phase 3: Implement Core State Machine
**Goal:** Basic flow without audio/bitmaps yet

1. **Add state enum and globals**
   ```cpp
   enum AppState { IDLE, TEXT_INPUT, VOICE_INPUT, THINKING, SHOWING_ANSWER };
   AppState current_state = IDLE;
   std::string current_question = "";
   uint8_t current_response_idx = 0;
   unsigned long state_timer = 0;
   ```

2. **Implement randomness functions**
   - `generateSeedFromText(const std::string& q)` - DJB2 hash + timestamp mixing
   - `generateSeedFromAudio(int16_t* data, size_t len)` - peak + zero-crossings + RMS + LSB
   - `selectResponse(uint32_t seed)` - return `seed % responses.size()`

3. **Implement display functions (text-only for now)**
   - `displayIdle()` - Title + prompt
   - `displayTextInput(const std::string& q)` - Question with cursor
   - `displayVoiceInput(int progress)` - Recording progress + waveform
   - `displayThinking()` - Animated "thinking" message
   - `displayAnswer(uint8_t idx)` - Show response text (ignore wav/bitmap initially)

4. **Implement state machine in loop()**
   - IDLE: Show prompt, detect keyboard input or BtnA hold
   - TEXT_INPUT: Accumulate characters, handle backspace/enter
   - VOICE_INPUT: Record 2 seconds of audio with waveform
   - THINKING: Animate for 2 seconds
   - SHOWING_ANSWER: Display response text, auto-return to IDLE

**Testing:** Full text input flow works, responses are text-only

---

### Phase 4: Add Audio Playback
**Goal:** Play WAV files during response display

1. **Create playResponseAudio(const String& wav_path)**
   ```cpp
   void playResponseAudio(const String& wav_path) {
       if (wav_path.isEmpty()) return;

       File file = SD.open(wav_path.c_str());
       if (!file) {
           printf("Audio file not found: %s\n", wav_path.c_str());
           return;
       }

       // Skip WAV header (44 bytes)
       file.seek(44);

       // Read audio data into buffer
       size_t file_size = file.size() - 44;
       int16_t* audio_buf = (int16_t*)malloc(file_size);
       file.read((uint8_t*)audio_buf, file_size);
       file.close();

       // Play audio
       M5Cardputer.Speaker.begin();
       M5Cardputer.Speaker.playRaw(audio_buf, file_size / 2, 16000);

       // Wait for playback to complete
       while (M5Cardputer.Speaker.isPlaying()) {
           delay(10);
       }

       free(audio_buf);
       M5Cardputer.Speaker.end();
   }
   ```

2. **Modify SHOWING_ANSWER state**
   - Start audio playback when entering state
   - Display text simultaneously (don't wait for audio)
   - Keep response visible until audio finishes + 2 seconds

3. **Update displayAnswer()**
   - Call `playResponseAudio(responses[idx].wav_path)` in non-blocking way
   - Or use simple blocking approach initially

**Testing:** Responses with WAV files play audio, responses without WAV are silent

---

### Phase 5: Add Bitmap Display
**Goal:** Show images alongside response text

1. **Create displayBitmap(const String& bitmap_path)**
   ```cpp
   void displayBitmap(const String& bitmap_path, int x, int y) {
       if (bitmap_path.isEmpty()) return;

       File file = SD.open(bitmap_path.c_str());
       if (!file) {
           printf("Bitmap not found: %s\n", bitmap_path.c_str());
           return;
       }

       // Simple BMP parsing (assumes 24-bit uncompressed)
       // Skip BMP header (54 bytes typically)
       file.seek(54);

       // Read pixel data and draw to display
       // (Implementation depends on BMP format, use M5GFX's drawBmp if available)
       M5Cardputer.Display.drawBmpFile(SD, bitmap_path.c_str(), x, y);

       file.close();
   }
   ```

2. **Modify displayAnswer()**
   - Layout: bitmap at top (centered), text below
   - If no bitmap, center text vertically
   - Calculate positions dynamically based on bitmap presence

3. **Update SHOWING_ANSWER state**
   - Draw bitmap first, then text, then start audio

**Testing:** Responses with bitmaps show images, responses without are text-only

---

### Phase 6: Generate Test Placeholders
**Goal:** Create sample audio and bitmap files on first boot

1. **Create generatePlaceholderAudio(const char* filename)**
   ```cpp
   void generatePlaceholderAudio(const char* filename) {
       File file = SD.open(filename, FILE_WRITE);
       if (!file) return;

       // Write simple WAV header (44 bytes)
       WAVHeader header;
       header.sampleRate = 16000;
       header.dataSize = 16000 * 2; // 1 second of audio
       header.fileSize = 36 + header.dataSize;
       file.write((uint8_t*)&header, sizeof(WAVHeader));

       // Generate 1 second of test tone (440Hz sine wave)
       for (int i = 0; i < 16000; i++) {
           float t = i / 16000.0;
           int16_t sample = (int16_t)(sin(2 * PI * 440 * t) * 16000);
           file.write((uint8_t*)&sample, 2);
       }

       file.close();
   }
   ```

2. **Create generatePlaceholderBitmap(const char* filename)**
   ```cpp
   void generatePlaceholderBitmap(const char* filename) {
       // Create simple colored square BMP (64x64 pixels)
       // Use M5GFX sprite to render, then save
       LGFX_Sprite sprite;
       sprite.createSprite(64, 64);
       sprite.fillScreen(random(0xFFFF)); // Random color
       sprite.drawString("?", 32, 32); // Draw question mark

       // Save sprite as BMP to SD card
       File file = SD.open(filename, FILE_WRITE);
       // Write BMP header + pixel data
       // (Simplified - actual BMP writing requires proper header formatting)
       file.close();
   }
   ```

3. **Create setupPlaceholderFiles() function**
   - Check if `/audio/` directory exists, create if not
   - Check if `/images/` directory exists, create if not
   - For each response in JSON without wav: generate placeholder
   - For each response in JSON without bitmap: generate placeholder
   - Update JSON with new paths

4. **Call from setup()**
   - After loading responses, call `setupPlaceholderFiles()`
   - Only runs once (checks if files exist first)

**Testing:** First boot creates audio/image directories and placeholder files

---

### Phase 7: Polish and Edge Cases
**Goal:** Handle errors gracefully, improve UX

1. **Error handling**
   - JSON parse failure: Fall back to hardcoded responses
   - SD card missing: Show error screen, disable features
   - File not found: Skip audio/bitmap, show text only
   - Empty responses array: Generate default config

2. **UI improvements**
   - Text wrapping for long responses
   - Color-coded backgrounds (green/yellow/red for positive/neutral/negative)
   - Smooth animations in THINKING state
   - Cursor blink in TEXT_INPUT

3. **Voice mode refinements**
   - Detect silence (low RMS) and prompt user to speak
   - Show recording waveform during input
   - Reduce buffer allocation to 64KB (2 seconds needed)

4. **Memory optimization**
   - Free audio buffer after playback
   - Reuse rec_data buffer for response audio if possible
   - Check heap usage with `ESP.getFreeHeap()`

**Testing:** Full regression, test all error paths

---

## Data Structures

### Response Configuration
```cpp
struct Response {
    String text;          // Required: "It is certain"
    String wav_path;      // Optional: "audio/certain.wav"
    String bitmap_path;   // Optional: "images/certain.bmp"
};

std::vector<Response> responses; // Loaded from SD card JSON
```

### State Management
```cpp
enum AppState { IDLE, TEXT_INPUT, VOICE_INPUT, THINKING, SHOWING_ANSWER };
AppState current_state = IDLE;
std::string current_question = "";
uint8_t current_response_idx = 0;
unsigned long state_timer = 0;
```

## Screen Layouts

### IDLE
```
┌────────────────────────┐
│  MAGIC EIGHT BALL      │ FreeSansBold24pt, WHITE, centered
│                        │
│  Type your question    │ FreeSans12pt, CYAN
│  and press [A]         │
│                        │
│  Hold [A] for voice    │ FreeSans9pt, YELLOW
└────────────────────────┘
```

### SHOWING_ANSWER (with bitmap)
```
┌────────────────────────┐
│  ┌──────────────┐      │
│  │  [BITMAP]    │      │ 64x64 centered
│  └──────────────┘      │
│                        │
│  It is certain         │ FreeSansBold18pt, centered
│                        │
│  [Audio playing...]    │ Indicator if wav exists
└────────────────────────┘
```

## Default JSON Content

**File: `/responses.json`** (generated on first boot)
```json
[
  {"text": "It is certain"},
  {"text": "It is decidedly so"},
  {"text": "Without a doubt"},
  {"text": "Yes definitely"},
  {"text": "You may rely on it"},
  {"text": "As I see it yes"},
  {"text": "Most likely"},
  {"text": "Outlook good"},
  {"text": "Yes"},
  {"text": "Signs point to yes"},
  {"text": "Reply hazy try again"},
  {"text": "Ask again later"},
  {"text": "Better not tell you now"},
  {"text": "Cannot predict now"},
  {"text": "Concentrate and ask again"},
  {"text": "Don't count on it"},
  {"text": "My reply is no"},
  {"text": "My sources say no"},
  {"text": "Outlook not so good"},
  {"text": "Very doubtful"},
  {"text": "The circuits say yes"},
  {"text": "My ESP32 brain says no"},
  {"text": "Error 404: Answer not found"},
  {"text": "Buffering... yes!"},
  {"text": "Have you tried turning it off and on again?"},
  {"text": "The SD card has spoken: absolutely"},
  {"text": "My microphone heard a yes in your voice"},
  {"text": "The waveform suggests otherwise"},
  {"text": "Quantum uncertainty says maybe"},
  {"text": "Stack overflow: ask a simpler question"}
]
```

## Key Algorithms

### Text-Based Randomness
```cpp
uint32_t generateSeedFromText(const std::string& question) {
    uint32_t hash = 5381;
    for (char c : question) {
        hash = ((hash << 5) + hash) + tolower(c);
    }
    hash ^= millis(); // Mix in timestamp
    // Additional mixing for better distribution
    hash ^= (hash >> 16);
    hash *= 0x85ebca6b;
    hash ^= (hash >> 13);
    return hash;
}
```

### Audio-Based Randomness
```cpp
uint32_t generateSeedFromAudio(int16_t* audio_data, size_t num_samples) {
    uint32_t seed = 0;

    // Peak amplitude
    int16_t peak = 0;
    for (size_t i = 0; i < num_samples; i++) {
        peak = max(peak, abs(audio_data[i]));
    }
    seed ^= (uint32_t)peak;

    // Zero-crossing count
    uint16_t zero_crossings = 0;
    for (size_t i = 1; i < num_samples; i++) {
        if ((audio_data[i-1] < 0 && audio_data[i] >= 0) ||
            (audio_data[i-1] >= 0 && audio_data[i] < 0)) {
            zero_crossings++;
        }
    }
    seed ^= ((uint32_t)zero_crossings << 8);

    // RMS energy
    uint64_t sum_squares = 0;
    for (size_t i = 0; i < num_samples; i++) {
        sum_squares += (int32_t)audio_data[i] * audio_data[i];
    }
    uint32_t rms = sqrt(sum_squares / num_samples);
    seed ^= (rms << 16);

    seed ^= millis(); // Mix in timestamp
    return seed;
}
```

## Testing Checklist

- [ ] JSON parsing loads all responses correctly
- [ ] Missing responses.json triggers default generation
- [ ] Text input accumulates characters correctly
- [ ] Backspace removes last character
- [ ] BtnA triggers response selection
- [ ] Voice recording captures 2 seconds of audio
- [ ] Audio waveform displays during recording
- [ ] Randomness differs for different inputs
- [ ] Same input gives different results (timestamp mixing)
- [ ] Responses with WAV files play audio
- [ ] Responses without WAV are silent (no error)
- [ ] Responses with bitmaps display images
- [ ] Responses without bitmaps show text only
- [ ] Placeholder generation creates files on first boot
- [ ] SD card removal shows error message
- [ ] Long responses wrap text correctly
- [ ] Memory doesn't leak (check with ESP.getFreeHeap())

## Dependencies

**platformio.ini additions:**
```ini
lib_deps =
    m5stack/M5Cardputer@^1.1.1
    bblanchon/ArduinoJson@^7.0.0
```

## Estimated Complexity

- **Lines of code:** ~400-500 (similar to original)
- **New functions:** ~15
- **Modified functions:** 3 (setup, loop, plus reused waveform code)
- **New files needed:** 1 JSON + 30 WAV + 30 BMP (generated)
- **Development time:** 7 phases as outlined above
