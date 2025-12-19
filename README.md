# 🎱 Magic Eight Ball: Cardputer Edition

Welcome to your new **Magic Eight Ball**! This handheld device, built on the M5Stack Cardputer (powered by an ESP32 chip), is designed to answer your deepest questions using either the built-in keyboard or voice recording.

This guide will show you how to use your device and, more importantly, how to "hack" it to customize the answers, sounds, and images it displays.

---

## 🕹️ Quick Start Guide

### 1. Powering On

Flip the power switch on the side of the Cardputer. You will be greeted by the **Magic Eight Ball** title screen.

### 2. Asking a Question

There are two ways to consult the oracle:

* **The Keyboard (Text Mode)**: Simply start typing your question. The screen will switch to "Your Question" mode. When finished, press **Enter** or the **[Go] button** (the big button on the front) to get your answer.
* **The Microphone (Voice Mode)**: Press and hold the **[Go] button**. You will see a "Recording..." screen with a live waveform of your voice. Speak your question clearly into the device. The recording lasts for 2 seconds.

### 3. The Revelation

The device will enter a "**Thinking**" state for a few seconds. It uses a special algorithm to turn the "vibe" of your voice or the specific characters you typed into a random seed to pick an answer.

* The answer will appear on the screen.
* Some answers may also play a **custom audio clip** or show a **unique picture**.
* Press **[Go]** to return to the idle screen for your next question.

---

## 🛠️ Hacking the Oracle (No Coding Required!)

You don’t need to be a programmer to change how this device behaves. Most of the "magic" is stored on the **microSD card** inside the device.

### 📂 The SD Card Structure

If you plug the microSD card into your computer, you will see the following files:

* `responses.json`: This is the master list of all possible answers.
* `/audio/`: A folder containing `.wav` sound files.
* `/images/`: A folder containing `.bmp` picture files.

### 📝 Customizing Answers

To change what the Eight Ball says, open `responses.json` in any text editor (like Notepad or TextEdit). It looks like this:

```json
[
  {
    "text": "It is certain",
    "wav": "audio/certain.wav",
    "bitmap": "images/certain.bmp"
  },
  {
    "text": "Reply hazy try again"
  }
]

```

* **To add an answer**: Copy one of the "blocks" and change the text.
* **To remove an answer**: Delete a block.
* **Text**: This is what shows up on the screen (Required).
* **WAV/Bitmap**: These are optional. If you don't want a sound or picture for a specific answer, just leave those lines out.

### 🔊 Adding Your Own Sounds

You can record your own voice or use sound effects!

* **Format**: The files must be **16-bit PCM mono WAV** files at **16kHz** sample rate.
* **Placement**: Put them in the `/audio/` folder and make sure the name in your `.json` file matches exactly.

### 🖼️ Adding Your Own Pictures

* **Format**: Use **24-bit uncompressed BMP** images.
* **Size**: A size of **64x64 pixels** is recommended to fit well with the text.
* **Placement**: Put them in the `/images/` folder and update your `.json` file.

---

## 🧠 Technical Secrets (For the Curious)

* **True Randomness**: When you type a question, the device creates a "hash" of your words and mixes it with the current time. When you speak, it analyzes the **peak amplitude**, **zero-crossings** (how often the wave crosses zero), and **RMS energy** (loudness) of your voice to ensure no two answers are ever truly predictable.
* **The Brain**: The device is an **ESP32-S3**. It has a built-in display, speaker, microphone, and a full physical keyboard.
* **The Software**: Built using the **Arduino** framework and the **M5Stack** libraries.

---

## 🚀 For the Brave: Updating the Firmware

If you want to change the actual code (the "firmware"), you will need a tool called **PlatformIO**.

1. Download the source code repository.
2. Open the `firmware` folder in VS Code with the PlatformIO extension.
3. Connect the Cardputer to your computer via USB-C.
4. Run the **Upload** command to push your new code to the device.

**Happy Hacking!** Whether you're adding inside jokes as responses or recording custom spooky voices, the Magic Eight Ball is now yours to command.