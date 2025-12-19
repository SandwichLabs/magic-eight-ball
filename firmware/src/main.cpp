
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

    // Display will be initialized in Phase 3 with displayIdle()
    printf("Magic Eight Ball initialized\r\n");
}

void loop(void)
{
    M5Cardputer.update();

    // State machine placeholder - will be implemented in Phase 3
    // TODO: Implement IDLE, TEXT_INPUT, VOICE_INPUT, THINKING, SHOWING_ANSWER states

    delay(10);  // Small delay to prevent tight loop
}



