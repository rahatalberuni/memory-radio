# Memory Radio

A touchscreen oral-history recorder for the LCDWIKI ES3C28P ESP32-S3 N16R8. Gemini organizes your spoken memories into a faithful summary, people, places, a timeline, and three follow-up questions. Tap an entry to see a supporting quote and replay the corresponding original audio. Record answers to enrich the memory. Gemini can narrate the recap and questions through the device speaker.

## Standalone Wi-Fi setup

The ESP32 now connects directly to Gemini over its onboard Wi-Fi. A running computer or USB bridge is not required. Power it from a USB charger, power bank, or another suitable board supply.

1. On first boot, the device shows **Wi-Fi setup**. You can reopen it later by tapping **WiFi** in the header.
2. On a phone or computer, join the **MemoryRadio-xxxx** network shown on the ESP32. Use the setup password displayed on its screen. If the phone says this temporary network has no internet, stay connected while configuring it.
3. Open **http://192.168.4.1**. Enter your regular **2.4 GHz Wi-Fi** name and password and your Gemini API key, then tap **Save and connect**. Password fields never echo saved credentials back to the page.
4. Check that the board shows it is connected, then tap **Done**. The setup access point shuts down. The header shows **WiFi online** when the regular network is connected and an API key is saved. Internet, valid Gemini access, and quota are still needed for cloud requests.

Network settings and the API key are saved in the device's NVS flash storage. They are not written to the SD card or source files. The password-protected setup network is only active while setup is open. Use a personal Wi-Fi network or 2.4 GHz phone hotspot: this setup does not implement enterprise username/password authentication or browser-based campus/hotel captive-portal login. For a hotspot on the same phone used for setup, save its details first, then enable that hotspot if the phone cannot host it while connected to the temporary setup network.

The firmware uses the ESP certificate bundle to verify Google's HTTPS certificate and syncs its clock using NTP. It streams WAV/Base64 uploads and decodes speech downloads through a temporary SD file, avoiding extra multi-megabyte RAM copies. Gemini summaries, follow-ups, and speech run in a background task while the screen remains responsive. Cancellation stops local handling; an API request already submitted may still finish and incur usage.

The standalone workflow has been tested on the physical board: direct Gemini memory generation, direct speech download and playback, saved microSD audio, and automatic Wi-Fi reconnection after restart all passed. See [VALIDATION.md](VALIDATION.md).

## Record and explore

1. Power the board and connect it using the Wi-Fi setup above. The microSD card is required for standalone Gemini processing and speech downloads. The optional Windows USB bridge is described below.
2. Tap **Record a memory** and tell a short story, up to 60 seconds. Tap **Finish recording**. Your original audio is saved to the microSD immediately, before any API request.
3. Tap **Organize with Gemini**. This sends the recording directly to Google's Gemini API over Wi-Fi. The device then shows the generated summary. Tap the page controls to read more.
4. Tap **People**, **Places**, or **Events**. Tap an entry to read its description and supporting quote; **Hear original moment** plays the corresponding recording excerpt. Timestamps are AI estimates and may need human review.
5. Tap **Questions**, select a Gemini-generated follow-up, and tap **Record my answer**. Record up to 30 seconds. The answer is saved on the card and sent to Gemini to update the existing memory. Questions can also be read aloud.
6. Tap **Play AI audio** to hear the generated recap through the ESP32 speaker. A saved recap plays immediately; if no recap exists, the device requests Gemini speech generation, saves the returned WAV, and starts speaker playback. The button changes to **Stop audio** while playing. Speech generation uses a separate compatible Gemini TTS model and depends on your account's model access and quota.

Recording and saved playback work without internet. New AI organization, follow-ups, and narration need internet through Wi-Fi or the optional USB bridge. When disconnected, **Play AI audio** can replay a saved AI recap. If no AI recap is saved, it asks you to connect Gemini. Original recordings have their separate **Play my recording** button. Saved quotes can replay local audio excerpts without the computer. After a restart, the latest recorded memory and any saved summary are restored from the card.

The home screen has a **Test speaker** button that plays three short tones without Gemini or internet. The header displays **Speaker playing** while audio is active. All PCM playback goes through the onboard ES8311 codec and the speaker amplifier (GPIO1, active low). The bridge waits for an **AUDIO_PLAYING** acknowledgement after the ESP32 successfully writes a playback frame to I2S. This verifies the software output path; audible quality and the physical speaker connection still need a listening check. USB or audio-driver errors are shown explicitly rather than reported as successful playback.

The computer bridge also reuses an existing generated recap when it is newer than the current memory JSON, so replay does not require another speech-generation request. `replay-cached-recap.ps1` can transfer that cached recap to the connected device without reading the API key; it saves recap.wav and verifies the I2S playback acknowledgement.

The device opens the latest memory. Earlier memories stay in their separate folders. The interface uses an ASCII font; standalone requests preserve the full original Gemini JSON and transcript in gemini.json on the card. Each recording is limited to 60 seconds (30 seconds for a follow-up).

## microSD storage

The card is mounted over 1-bit SDMMC using CLK38, CMD40, D0 39. Formatting is disabled. Existing files outside `/memory-radio` are not modified.

Each recording creates a unique folder, for example:

```text
/memory-radio/M1234ABCD/
  story.wav          Original recording, 16kHz / 16-bit / mono PCM
  answer1.wav        First follow-up answer
  answer2.wav        Next follow-up answer
  recap.wav          Most recently generated recap for this memory
  question0.wav      A generated spoken follow-up question
  excerpt-0-0.wav    A requested original-audio excerpt
  memory.json        Summary, questions, people, places, events, quotes, timestamps
  gemini.json        Full standalone Gemini response, including transcript
  memory.txt         Device-readable restore data
  memory.bak         Previous restore data, when a memory is updated
```

`/memory-radio/latest.txt` identifies the most recent memory. Card audio files are standard WAV and can be opened on a computer. A 60-second recording uses approximately 1.92 MB. Shorter clips are saved at their actual duration. Narration is saved when it arrives at the board, and the same memory's latest recap replaces its previous recap. Original stories and follow-up recordings use separate filenames.

In optional USB mode, the computer also archives original audio, the full Gemini JSON including transcript, follow-up answers, and generated narration under this project's `memories/` folder. Those recordings are sent to Google only when the user chooses organization or submits a follow-up. Merely recording a new story does not upload it. Standalone mode stores everything on the SD card.

## Optional Windows USB bridge

The original USB bridge remains available when direct Wi-Fi is not configured or connected. Its key setup is separate from the device's Wi-Fi settings. In this mode, the key is saved with Windows DPAPI for the current Windows account. Standalone Wi-Fi uses the key entered into the device setup page instead.

`Connect Gemini.cmd` opens the setup window and runs the Windows bridge in a visible console. Keep that console open while using Gemini. Close Arduino Serial Monitor and diagnostic scripts first; only one process can own COM3. The device shows **Gemini connected** after the bridge successfully checks the key's available models and opens the port. Model discovery prefers an available Flash model; narration prefers an available compatible Flash TTS model. Models can be overridden with bridge.ps1's `-Model` and `-VoiceModel` arguments.

To replace the key, run `powershell.exe -NoProfile -ExecutionPolicy Bypass -STA -File .\configure.ps1`, then restart the bridge. The Windows user environment variable `GEMINI_API_KEY` is also supported. Do not paste your key into chat or source code.

Bridge output is in `bridge-output.log` and `bridge-error.log`. Keys and raw audio are not logged. If organization fails, your original WAV remains on the card. Open your recording and retry. If voice generation is unavailable, the text summary and original voice still work. After unplugging the board, restart the bridge. Canceling a request on the device ignores late results; a cloud request already in flight may still finish and be billed.

If a Gemini request fails, double-click **Diagnose Gemini.cmd**. It checks generation access using the saved key in your Windows session, without opening the USB port or uploading a personal recording. The result is saved in `gemini-status.json` (excluded from Git). The updated bridge also records the HTTP code, provider status, model, and sanitized error message for failed API calls. Keys are redacted. A successful model-list request alone does not prove that generation quota or permissions are available. Restart Connect Gemini after updating the bridge so the running process uses the latest code.

Temporary server errors (HTTP 500, 503, and 504), and network timeouts without an HTTP response, trigger immediate fallback to another compatible model from the API key's advertised models. Up to three attempts share an 85-second deadline; a model is retried when no additional compatible fallback is available. The bridge remembers a working fallback for later requests. Automatic selection prefers an available Flash-Lite model. API-key, permission, request-format, and quota errors (400, 403, 429) are reported immediately. Speech requests fall back only to another supported speech model. If all available attempts fail, the original recording remains saved for a later retry.

Before opening the USB port, Connect Gemini performs a small text-generation check. The console prints **Gemini generation check passed** before showing **Memory Radio connected**. This checks generation access as well as key/model discovery; it does not submit a personal recording. If the check cannot complete, the bridge reports the actual failure before starting a recording upload.

SD status appears on every screen. **SD: Saved** indicates the last save completed. **SD: Not available** indicates mounting failed; the card is never formatted automatically. A mounted card with a write failure will remain **SD: Ready** rather than claim the recording was saved. Keep power connected until the recording screen closes and the save completes.

## Development

- MemoryRadio.ino: device UI, microphone, playback, framed USB protocol.
- hardware.h: locally validated ILI9341V / FT6336G / ES8311 drivers reused from PipPet.
- storage.h: WAV files, SD metadata, and restore.
- wifi_radio.h: local setup page, device credentials, verified HTTPS, streaming upload/download, Gemini JSON parsing, model fallback, and background jobs.
- bridge.ps1: Gemini audio understanding, grounded structured memory, narration, and archives.
- configure.ps1 / start-gemini.ps1 / connect.ps1: local key setup and bridge lifecycle.
- build.ps1 / upload.ps1: existing Arduino ESP32 3.3.12 toolchain; app-only flash at 0x10000.
- inspect.ps1: captures the real framebuffer to build/.
- verify.ps1: hardware integration test with an explicitly labeled test fixture; it creates a short test recording and files under a new /memory-radio folder. It does not call Gemini or claim that fixture is AI output.
- test-transfer.ps1: exercises the numbered USB transfer with a full 60-second payload, lost and duplicate packets, and checksum corruption. It does not modify recordings or call Gemini.
- test-speaker.ps1: checks the home speaker button, amplifier enable, nonzero I2S writes, and incoming response playback acknowledgement. It retains the original recording and saves its test tone separately as speaker-test.wav.
- replay-cached-recap.ps1: sends the current memory's cached narration to the device, saves it on the card, and starts speaker playback.
- gemini-errors.ps1 / test-gemini-errors.ps1: provider-error parsing, credential redaction, and diagnostic checks.
- gemini-client.ps1 / test-gemini-client.ps1: bounded retries and compatible fallback for temporary server errors.
- Diagnose Gemini.cmd: local API generation check; no personal recording is submitted.

Recordings use an acknowledged, numbered-chunk upload protocol. The computer verifies the entire recording's checksum before calling Gemini. Missing chunks are retried and duplicate chunks are ignored. The device remains responsive during upload; existing saved recordings can be retried without recording again.

Run scripts with `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1`, substituting the desired filename. This execution-policy setting affects only that process. Rebuilding uses the existing compiler directly because Arduino CLI platform discovery has directory-access errors in this environment. Previous LastTransmission/PipPet/PulseVisualizer projects remain in neighboring folders. Upload replaces only the application and preserves the existing flash partition table and model partition.

Standalone additions use the bundled Network, WiFi, WebServer, Preferences, and Hash libraries plus ESP-IDF's esp_http_client, certificate bundle, and cJSON. No third-party Arduino library installation is required. Serial diagnostics: WIFI_STATUS prints connection/setup state without credentials; WIFI_SETUP opens setup; WIFI_TEST checks the streaming speech decoder and source-name validation. A backup of the previously working USB-only application and sources is retained locally under build/usb-version/ (not published).

References: [board documentation](https://www.lcdwiki.com/2.8inch_ESP32-S3_Display), [Gemini audio understanding](https://ai.google.dev/gemini-api/docs/audio), [structured output](https://ai.google.dev/gemini-api/docs/structured-output), [speech generation](https://ai.google.dev/gemini-api/docs/speech-generation).
