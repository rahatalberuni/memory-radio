# Hardware validation

Tested on the connected LCDWIKI ES3C28P ESP32-S3 N16R8 with microSD, ILI9341V display, FT6336G touch, and ES8311 audio.

- Firmware compiled with the installed Arduino ESP32 3.3.12 toolchain and bundled libraries. The 1,322,256-byte application fits the existing partition; esptool verified its flash hash.
- Existing recording, memory metadata, and generated recap survived the update.
- Password-protected setup access point started. The local setup page saved network settings and the Gemini API key to device NVS; credentials were not printed in diagnostics or published.
- On-device streaming speech-parser and source-path checks passed.
- Wi-Fi association and internet access succeeded. Direct HTTPS model discovery and a streamed recording upload produced a structured Gemini memory using gemini-3.5-flash-lite.
- Direct on-device speech generation and download succeeded. The ESP32 decoded/resampled the response and saved a 459,520-byte, 14.36-second PCM recap as a standard WAV on microSD.
- The audio driver acknowledged playback. Nonzero audio frames reached I2S, the speaker amplifier was enabled, and no speaker write errors were observed. The user had also confirmed audible tones and narration during speaker validation.
- After an actual board restart, network settings, key, selected model, memory metadata, original audio, and recap persisted. The device reconnected automatically and replayed the cached recap successfully.
- The computer did not perform Gemini processing during these standalone tests. USB was used only to issue diagnostic commands and read status; the ESP32 performed the network requests, JSON handling, SD storage, resampling, and speaker output itself.

Live Gemini availability and quota depend on the configured account and service. Earlier memories remain on the card; the device UI currently opens the latest one. Each original recording is limited to 60 seconds, and each follow-up to 30 seconds. Personal Wi-Fi or a 2.4 GHz hotspot is supported; enterprise credentials and captive-portal browser login are not implemented.
