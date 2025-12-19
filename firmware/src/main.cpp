
/*
 * SPDX-FileCopyrightText: 2024 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 *
 * @Hardwares: M5Cardputer
 * @Platform Version: Arduino M5Stack Board Manager v2.0.7
 * @Dependent Library:
 * M5GFX@^0.2.3: https://github.com/m5stack/M5GFX
 * M5Cardputer@^1.0.3: https://github.com/m5stack/M5Cardputer
 */

#include <M5Cardputer.h>
#include <SPI.h>
#include <SD.h>
#include <ArduinoJson.h>

#define SD_SPI_SCK_PIN  (40)
#define SD_SPI_MISO_PIN (39)
#define SD_SPI_MOSI_PIN (14)
#define SD_SPI_CS_PIN   (12)

// Voice input recording: 2 seconds at 16kHz = 32,000 samples
// At 240 samples per chunk = 134 chunks (~64KB buffer)
static constexpr const size_t record_number     = 134;  // Reduced from 512 for voice mode
static constexpr const size_t record_length     = 240;
static constexpr const size_t record_size       = record_number * record_length;
static constexpr const size_t record_samplerate = 16000;

static int16_t prev_y[record_length];
static int16_t prev_h[record_length];
static size_t rec_record_idx  = 2;
static size_t draw_record_idx = 0;
static int16_t* rec_data;

// Magic Eight Ball Response structure
struct Response {
    String text;
    String wav_path;
    String bitmap_path;
};

static std::vector<Response> responses;

// State machine for Magic Eight Ball
enum AppState { IDLE, TEXT_INPUT, VOICE_INPUT, THINKING, SHOWING_ANSWER };
static AppState current_state = IDLE;
static String current_question = "";
static uint8_t current_response_idx = 0;
static unsigned long state_timer = 0;
static bool cursor_visible = true;
static unsigned long last_cursor_blink = 0;
static bool audio_played = false;

// WAV文件头部定义
struct WAVHeader {
    char riff[4]           = {'R', 'I', 'F', 'F'};
    uint32_t fileSize      = 0;
    char wave[4]           = {'W', 'A', 'V', 'E'};
    char fmt[4]            = {'f', 'm', 't', ' '};
    uint32_t fmtSize       = 16;
    uint16_t audioFormat   = 1;
    uint16_t numChannels   = 1;
    uint32_t sampleRate    = record_samplerate;
    uint32_t byteRate      = record_samplerate * sizeof(int16_t);
    uint16_t blockAlign    = sizeof(int16_t);
    uint16_t bitsPerSample = 16;
    char data[4]           = {'d', 'a', 't', 'a'};
    uint32_t dataSize      = 0;
};

// Magic Eight Ball Functions
bool generateDefaultConfig();  // Generate default responses.json if it doesn't exist
bool loadResponsesFromSD();     // Load responses from SD card JSON file

// Randomness generation functions
uint32_t generateSeedFromText(const String& question);
uint32_t generateSeedFromAudio(int16_t* audio_data, size_t num_samples);
uint8_t selectResponse(uint32_t seed);

// Display functions
void displayIdle();
void displayTextInput(const String& question);
void displayVoiceInput(int progress);
void displayThinking();
void displayAnswer(uint8_t idx);

// Helper function for text wrapping
void drawWrappedText(const String& text, int x, int y, int max_width, int line_height);

// Audio playback function
void playResponseAudio(const String& wav_path);

// Generate default responses.json file on SD card
bool generateDefaultConfig() {
    File file = SD.open("/responses.json", FILE_WRITE);
    if (!file) {
        printf("Failed to create responses.json\n");
        return false;
    }

    // Create JSON document with default responses (20 classic + 10 custom)
    JsonDocument doc;
    JsonArray array = doc.to<JsonArray>();

    // Classic positive responses (0-9)
    array.add(JsonObject());
    array[0]["text"] = "It is certain";

    array.add(JsonObject());
    array[1]["text"] = "It is decidedly so";

    array.add(JsonObject());
    array[2]["text"] = "Without a doubt";

    array.add(JsonObject());
    array[3]["text"] = "Yes definitely";

    array.add(JsonObject());
    array[4]["text"] = "You may rely on it";

    array.add(JsonObject());
    array[5]["text"] = "As I see it yes";

    array.add(JsonObject());
    array[6]["text"] = "Most likely";

    array.add(JsonObject());
    array[7]["text"] = "Outlook good";

    array.add(JsonObject());
    array[8]["text"] = "Yes";

    array.add(JsonObject());
    array[9]["text"] = "Signs point to yes";

    // Classic non-committal responses (10-14)
    array.add(JsonObject());
    array[10]["text"] = "Reply hazy try again";

    array.add(JsonObject());
    array[11]["text"] = "Ask again later";

    array.add(JsonObject());
    array[12]["text"] = "Better not tell you now";

    array.add(JsonObject());
    array[13]["text"] = "Cannot predict now";

    array.add(JsonObject());
    array[14]["text"] = "Concentrate and ask again";

    // Classic negative responses (15-19)
    array.add(JsonObject());
    array[15]["text"] = "Don't count on it";

    array.add(JsonObject());
    array[16]["text"] = "My reply is no";

    array.add(JsonObject());
    array[17]["text"] = "My sources say no";

    array.add(JsonObject());
    array[18]["text"] = "Outlook not so good";

    array.add(JsonObject());
    array[19]["text"] = "Very doubtful";

    // Custom creative responses (20-29)
    array.add(JsonObject());
    array[20]["text"] = "The circuits say yes";

    array.add(JsonObject());
    array[21]["text"] = "My ESP32 brain says no";

    array.add(JsonObject());
    array[22]["text"] = "Error 404: Answer not found";

    array.add(JsonObject());
    array[23]["text"] = "Buffering... yes!";

    array.add(JsonObject());
    array[24]["text"] = "Have you tried turning it off and on again?";

    array.add(JsonObject());
    array[25]["text"] = "The SD card has spoken: absolutely";

    array.add(JsonObject());
    array[26]["text"] = "My microphone heard a yes in your voice";

    array.add(JsonObject());
    array[27]["text"] = "The waveform suggests otherwise";

    array.add(JsonObject());
    array[28]["text"] = "Quantum uncertainty says maybe";

    array.add(JsonObject());
    array[29]["text"] = "Stack overflow: ask a simpler question";

    // Write JSON to file
    if (serializeJson(doc, file) == 0) {
        printf("Failed to write JSON to file\n");
        file.close();
        return false;
    }

    file.close();
    printf("Created default responses.json with 30 responses\n");
    return true;
}

// Load responses from SD card JSON file
bool loadResponsesFromSD() {
    File file = SD.open("/responses.json", FILE_READ);
    if (!file) {
        printf("responses.json not found, will generate default\n");
        return false;
    }

    // Parse JSON file
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, file);
    file.close();

    if (error) {
        printf("Failed to parse responses.json: %s\n", error.c_str());
        return false;
    }

    // Check if root is an array
    if (!doc.is<JsonArray>()) {
        printf("responses.json must contain an array\n");
        return false;
    }

    JsonArray array = doc.as<JsonArray>();
    responses.clear();

    // Load each response
    for (JsonObject obj : array) {
        Response r;
        r.text = obj["text"] | "";
        r.wav_path = obj["wav"] | "";
        r.bitmap_path = obj["bitmap"] | "";

        if (r.text.isEmpty()) {
            printf("Skipping response with empty text\n");
            continue;
        }

        responses.push_back(r);
    }

    printf("Loaded %d responses from SD card\n", responses.size());
    return responses.size() > 0;
}

// Generate random seed from text input using DJB2 hash + timestamp
uint32_t generateSeedFromText(const String& question) {
    uint32_t hash = 5381;
    for (size_t i = 0; i < question.length(); i++) {
        hash = ((hash << 5) + hash) + tolower(question[i]);
    }
    hash ^= millis(); // Mix in timestamp
    // Additional mixing for better distribution
    hash ^= (hash >> 16);
    hash *= 0x85ebca6b;
    hash ^= (hash >> 13);
    return hash;
}

// Generate random seed from audio waveform analysis
uint32_t generateSeedFromAudio(int16_t* audio_data, size_t num_samples) {
    uint32_t seed = 0;

    // Peak amplitude
    int16_t peak = 0;
    for (size_t i = 0; i < num_samples; i++) {
        int16_t abs_val = abs(audio_data[i]);
        if (abs_val > peak) peak = abs_val;
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

// Select response index based on seed
uint8_t selectResponse(uint32_t seed) {
    if (responses.size() == 0) return 0;
    return seed % responses.size();
}

// Helper function to draw wrapped text
void drawWrappedText(const String& text, int x, int y, int max_width, int line_height) {
    int cursor_x = x;
    int cursor_y = y;
    String current_word = "";

    for (size_t i = 0; i < text.length(); i++) {
        char c = text[i];

        if (c == ' ' || c == '\n' || i == text.length() - 1) {
            // Add last character if end of string
            if (i == text.length() - 1 && c != ' ' && c != '\n') {
                current_word += c;
            }

            // Check if word fits on current line
            int word_width = M5Cardputer.Display.textWidth(current_word);

            if (cursor_x + word_width > max_width && cursor_x > x) {
                // Move to next line
                cursor_x = x;
                cursor_y += line_height;
            }

            // Draw the word
            M5Cardputer.Display.setCursor(cursor_x, cursor_y);
            M5Cardputer.Display.print(current_word);
            cursor_x += word_width;

            // Add space after word
            if (c == ' ') {
                cursor_x += M5Cardputer.Display.textWidth(" ");
            } else if (c == '\n') {
                cursor_x = x;
                cursor_y += line_height;
            }

            current_word = "";
        } else {
            current_word += c;
        }
    }
}

// Display idle screen with prompt
void displayIdle() {
    M5Cardputer.Display.clear();
    M5Cardputer.Display.setTextDatum(top_left);
    M5Cardputer.Display.setTextSize(1);

    M5Cardputer.Display.setTextColor(WHITE);
    M5Cardputer.Display.drawString("MAGIC EIGHT BALL", 5, 5);

    M5Cardputer.Display.setTextColor(CYAN);
    M5Cardputer.Display.drawString("Type your question", 5, 30);
    M5Cardputer.Display.drawString("Press Enter or [Go]", 5, 45);

    M5Cardputer.Display.setTextColor(YELLOW);
    M5Cardputer.Display.drawString("Press [Go] for voice", 5, 70);
}

// Display text input with blinking cursor
void displayTextInput(const String& question) {
    M5Cardputer.Display.clear();
    M5Cardputer.Display.setTextDatum(top_left);
    M5Cardputer.Display.setTextSize(1);

    M5Cardputer.Display.setTextColor(WHITE);
    M5Cardputer.Display.drawString("Your Question:", 5, 5);

    M5Cardputer.Display.setTextColor(CYAN);
    String display_text = question;
    if (cursor_visible) {
        display_text += "_";
    }
    drawWrappedText(display_text, 5, 25, M5Cardputer.Display.width() - 10, 15);

    M5Cardputer.Display.setTextColor(YELLOW);
    M5Cardputer.Display.drawString("Enter or [Go] to submit", 5, 110);
}

// Display voice recording progress with waveform
void displayVoiceInput(int progress) {
    M5Cardputer.Display.clear();
    M5Cardputer.Display.setTextDatum(top_left);
    M5Cardputer.Display.setTextSize(1);

    M5Cardputer.Display.setTextColor(WHITE);
    M5Cardputer.Display.drawString("Recording...", 5, 5);

    // Progress bar
    int bar_width = (M5Cardputer.Display.width() - 20) * progress / 100;
    M5Cardputer.Display.fillRect(5, 30, bar_width, 10, GREEN);
    M5Cardputer.Display.drawRect(5, 30, M5Cardputer.Display.width() - 10, 10, WHITE);

    // Draw waveform from current buffer
    int y_center = M5Cardputer.Display.height() / 2 + 10;
    for (int x = 0; x < record_length && x < M5Cardputer.Display.width(); x++) {
        size_t idx = draw_record_idx * record_length + x;
        if (idx < record_size) {
            int16_t sample = rec_data[idx];
            int y = y_center + (sample / 2048); // Scale down for display
            M5Cardputer.Display.drawPixel(x, y, CYAN);
        }
    }
}

// Display thinking animation
void displayThinking() {
    M5Cardputer.Display.clear();
    M5Cardputer.Display.setTextDatum(top_left);
    M5Cardputer.Display.setTextSize(1);
    M5Cardputer.Display.setTextColor(MAGENTA);

    // Animate dots based on time
    String dots = "";
    int dot_count = (millis() / 500) % 4;
    for (int i = 0; i < dot_count; i++) {
        dots += ".";
    }

    M5Cardputer.Display.drawString("Thinking" + dots, 5, M5Cardputer.Display.height() / 2 - 10);
}

// Display answer with audio/bitmap indicators
void displayAnswer(uint8_t idx) {
    if (idx >= responses.size()) return;

    M5Cardputer.Display.clear();
    M5Cardputer.Display.setTextDatum(top_left);
    M5Cardputer.Display.setTextSize(1);

    M5Cardputer.Display.setTextColor(GREEN);
    String header = "Answer:";
    if (!responses[idx].wav_path.isEmpty()) {
        header += " [AUDIO]";
    }
    M5Cardputer.Display.drawString(header, 5, 5);

    M5Cardputer.Display.setTextColor(WHITE);
    drawWrappedText(responses[idx].text, 5, 25, M5Cardputer.Display.width() - 10, 15);

    M5Cardputer.Display.setTextColor(YELLOW);
    M5Cardputer.Display.drawString("Press [Go] to continue", 5, 110);
}

// Play response audio from SD card
void playResponseAudio(const String& wav_path) {
    if (wav_path.isEmpty()) {
        printf("No audio file specified\n");
        return;
    }

    File file = SD.open(wav_path.c_str());
    if (!file) {
        printf("Audio file not found: %s\n", wav_path.c_str());
        return;
    }

    // Get file size and allocate buffer
    size_t file_size = file.size();
    if (file_size < 44) {
        printf("Invalid WAV file (too small)\n");
        file.close();
        return;
    }

    // Skip WAV header (44 bytes)
    file.seek(44);
    size_t audio_data_size = file_size - 44;

    // Allocate buffer for audio data
    int16_t* audio_buf = (int16_t*)heap_caps_malloc(audio_data_size, MALLOC_CAP_8BIT);
    if (!audio_buf) {
        printf("Failed to allocate audio buffer (%d bytes)\n", audio_data_size);
        file.close();
        return;
    }

    // Read audio data
    size_t bytes_read = file.read((uint8_t*)audio_buf, audio_data_size);
    file.close();

    if (bytes_read != audio_data_size) {
        printf("Failed to read complete audio file\n");
        free(audio_buf);
        return;
    }

    // Stop microphone and start speaker
    M5Cardputer.Mic.end();
    M5Cardputer.Speaker.begin();
    M5Cardputer.Speaker.setVolume(255);

    // Play audio
    size_t sample_count = audio_data_size / sizeof(int16_t);
    printf("Playing audio: %s (%d samples)\n", wav_path.c_str(), sample_count);
    M5Cardputer.Speaker.playRaw(audio_buf, sample_count, record_samplerate);

    // Wait for playback to complete
    while (M5Cardputer.Speaker.isPlaying()) {
        delay(10);
        M5Cardputer.update(); // Allow button presses during playback
    }

    // Clean up
    M5Cardputer.Speaker.end();
    free(audio_buf);
    printf("Audio playback complete\n");
}

void setup(void)
{
    auto cfg = M5.config();

    M5Cardputer.begin(cfg);
    Serial.begin(115200);
    M5Cardputer.Display.startWrite();
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setTextDatum(top_center);
    M5Cardputer.Display.setTextColor(WHITE);
    M5Cardputer.Display.setFont(&fonts::FreeSansBoldOblique12pt7b);

    // SD Card Initialization
    SPI.begin(SD_SPI_SCK_PIN, SD_SPI_MISO_PIN, SD_SPI_MOSI_PIN, SD_SPI_CS_PIN);

    if (!SD.begin(SD_SPI_CS_PIN, SPI, 25000000)) {
        printf("Card failed, or not present\r\n");
        while (1);
    }
    uint8_t cardType = SD.cardType();
    if (cardType == CARD_NONE) {
        printf("No SD card attached\r\n");
        return;
    }
    printf("SD Card Type: ");
    if (cardType == CARD_MMC) {
        printf("MMC\r\n");
    } else if (cardType == CARD_SD) {
        printf("SDSC\r\n");
    } else if (cardType == CARD_SDHC) {
        printf("SDHC\r\n");
    } else {
        printf("UNKNOWN\r\n");
    }
    uint64_t cardSize = SD.cardSize() / (1024 * 1024);
    printf("SD Card Size: %lluMB\r\n", cardSize);

    // Load Magic Eight Ball responses from JSON config
    if (!loadResponsesFromSD()) {
        printf("Generating default responses.json...\r\n");
        if (generateDefaultConfig()) {
            // Try loading again
            if (!loadResponsesFromSD()) {
                printf("ERROR: Failed to load responses even after generating default\r\n");
                while (1);  // Halt if we can't load responses
            }
        } else {
            printf("ERROR: Failed to generate default config\r\n");
            while (1);  // Halt if we can't generate config
        }
    }

    // Print loaded responses for debugging
    for (size_t i = 0; i < responses.size() && i < 5; i++) {
        printf("  Response %d: %s", i, responses[i].text.c_str());
        if (!responses[i].wav_path.isEmpty()) {
            printf(" [wav: %s]", responses[i].wav_path.c_str());
        }
        if (!responses[i].bitmap_path.isEmpty()) {
            printf(" [bmp: %s]", responses[i].bitmap_path.c_str());
        }
        printf("\r\n");
    }
    if (responses.size() > 5) {
        printf("  ... and %d more responses\r\n", responses.size() - 5);
    }

    rec_data = (typeof(rec_data))heap_caps_malloc(record_size * sizeof(int16_t), MALLOC_CAP_8BIT);
    memset(rec_data, 0, record_size * sizeof(int16_t));
    M5Cardputer.Speaker.setVolume(255);
    M5Cardputer.Speaker.end();
    M5Cardputer.Mic.begin();

    // Show idle screen
    displayIdle();
    printf("Magic Eight Ball initialized\r\n");
}

void loop(void)
{
    M5Cardputer.update();

    // Handle cursor blinking for text input
    if (millis() - last_cursor_blink > 500) {
        cursor_visible = !cursor_visible;
        last_cursor_blink = millis();
        if (current_state == TEXT_INPUT) {
            displayTextInput(current_question);
        }
    }

    switch (current_state) {
        case IDLE: {
            // Check for keyboard input
            if (M5Cardputer.Keyboard.isChange()) {
                if (M5Cardputer.Keyboard.isPressed()) {
                    Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

                    // Check if any printable key was pressed
                    for (auto key : status.word) {
                        if (key >= 0x20 && key <= 0x7E) { // Printable ASCII
                            current_question = "";
                            current_question += (char)key;
                            current_state = TEXT_INPUT;
                            displayTextInput(current_question);
                            break;
                        }
                    }
                }
            }

            // Check for button press to start voice input
            if (M5Cardputer.BtnA.wasPressed()) {
                // Single press - voice input
                current_state = VOICE_INPUT;
                rec_record_idx = 2;
                draw_record_idx = 0;
                M5Cardputer.Mic.begin();
                state_timer = millis();
                displayVoiceInput(0);
            }
            break;
        }

        case TEXT_INPUT: {
            // Handle keyboard input
            if (M5Cardputer.Keyboard.isChange()) {
                if (M5Cardputer.Keyboard.isPressed()) {
                    Keyboard_Class::KeysState status = M5Cardputer.Keyboard.keysState();

                    // Handle special keys
                    if (status.del && current_question.length() > 0) {
                        current_question.remove(current_question.length() - 1);
                        displayTextInput(current_question);
                    } else if (status.enter) {
                        // Submit question
                        if (current_question.length() > 0) {
                            uint32_t seed = generateSeedFromText(current_question);
                            current_response_idx = selectResponse(seed);
                            current_state = THINKING;
                            state_timer = millis();
                            displayThinking();
                        }
                    } else {
                        // Add printable characters
                        for (auto key : status.word) {
                            if (key >= 0x20 && key <= 0x7E) { // Printable ASCII
                                current_question += (char)key;
                                displayTextInput(current_question);
                            }
                        }
                    }
                }
            }

            // Check for button A press (submit)
            if (M5Cardputer.BtnA.wasPressed()) {
                if (current_question.length() > 0) {
                    uint32_t seed = generateSeedFromText(current_question);
                    current_response_idx = selectResponse(seed);
                    current_state = THINKING;
                    state_timer = millis();
                    displayThinking();
                }
            }
            break;
        }

        case VOICE_INPUT: {
            // Record audio for 2 seconds
            if (M5Cardputer.Mic.isEnabled()) {
                if (M5Cardputer.Mic.record(rec_data, record_length, record_samplerate)) {
                    if (rec_record_idx < record_number) {
                        rec_record_idx++;
                        draw_record_idx = rec_record_idx - 2;

                        // Update progress
                        int progress = (rec_record_idx * 100) / record_number;
                        displayVoiceInput(progress);
                    } else {
                        // Recording complete
                        M5Cardputer.Mic.end();

                        // Generate seed from audio
                        uint32_t seed = generateSeedFromAudio(rec_data, record_size);
                        current_response_idx = selectResponse(seed);

                        current_state = THINKING;
                        state_timer = millis();
                        displayThinking();
                    }
                }
            }
            break;
        }

        case THINKING: {
            // Show thinking animation for 2 seconds
            displayThinking(); // Update animation

            if (millis() - state_timer > 2000) {
                current_state = SHOWING_ANSWER;
                state_timer = millis();
                audio_played = false; // Reset audio flag
                displayAnswer(current_response_idx);
            }
            break;
        }

        case SHOWING_ANSWER: {
            // Play audio on first entry to this state
            if (!audio_played) {
                audio_played = true;
                playResponseAudio(responses[current_response_idx].wav_path);
                state_timer = millis(); // Reset timer after audio completes
            }

            // Wait for button press or auto-return after 5 seconds
            if (M5Cardputer.BtnA.wasPressed() || (millis() - state_timer > 5000)) {
                current_state = IDLE;
                current_question = "";
                audio_played = false;
                displayIdle();
            }
            break;
        }
    }

    delay(10);  // Small delay to prevent tight loop
}



