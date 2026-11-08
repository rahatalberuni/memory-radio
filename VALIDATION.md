# Hardware validation

Tested on the connected LCDWIKI ES3C28P ESP32-S3 N16R8 with microSD, ILI9341V display, FT6336G touch, and ES8311 audio.

## Automatic narration repair (2026-11-08)

- Built and installed the 1,324,016-byte application; esptool verified its flash hash. The existing latest recording and memory restored, with no automatic playback on startup.
- On-device `WIFI_TEST` passed after buffering decoded speech into 3 KB SD writes.
- Reorganized the existing saved recording over standalone Wi-Fi. Summary completion automatically started a separate speech request without a Play command. The fresh 688,684-byte recap WAV was saved to the same memory folder, followed by `AUDIO_PLAYING 3 688640`.
- The complete 344,320-sample clip reached the playback endpoint with nonzero speaker frames, amplifier enabled, and zero speaker errors. This verifies the digital playback path; audible output was not independently monitored during this repair.
- `test-pause-resume.ps1` passed on the installed firmware: touch controls, frozen position and frame count while paused, resume without rewind, repeated pause, completion, replay, original recording retained, and no new speaker errors.

## Earlier validation

### Narration speed update (2026-11-08)

- Built and installed the 1,326,848-byte application; flash hash verified. The latest user recording and summary restored successfully.
- Expanded `WIFI_TEST` passed: decoded 49,160 bytes identically with the former four-character decoder and the new 1 KB batches, including block boundaries and a padded tail. Measured 158 ms versus 146 ms on the same fixture and SD card. Temporary buffers use PSRAM to stay within the main task's stack limit.
- Two fresh standalone spoken questions downloaded, saved, and received speaker playback acknowledgements. First request: 12,380 ms total, including approximately 3,072 ms of discovery/setup overhead. Second request reused the model cache: 12,947 ms total versus 12,944 ms in speech request/download/conversion, leaving about 3 ms overhead. The questions differ in length, so these totals are not an overall before/after latency comparison. The measured improvement is removal of the repeated approximately three-second discovery check; provider generation and network download still dominate.
- Complete recap text, voice selection, and local Play/Pause/Resume behavior are unchanged by this update.

- Firmware compiled with the installed Arduino ESP32 3.3.12 toolchain and bundled libraries. The 1,323,200-byte application fits the existing partition; esptool verified its flash hash.
- Existing recording, memory metadata, and generated recap survived the update.
- Password-protected setup access point started. The local setup page saved network settings and the Gemini API key to device NVS; credentials were not printed in diagnostics or published.
- On-device streaming speech-parser and source-path checks passed.
- Wi-Fi association and internet access succeeded. Direct HTTPS model discovery and a streamed recording upload produced a structured Gemini memory using gemini-3.5-flash-lite.
- Direct on-device speech generation and download succeeded. The ESP32 decoded/resampled the response and saved a 459,520-byte, 14.36-second PCM recap as a standard WAV on microSD.
- The audio driver acknowledged playback. Nonzero audio frames reached I2S, the speaker amplifier was enabled, and no speaker write errors were observed. The user had also confirmed audible tones and narration during speaker validation.
- After an actual board restart, network settings, key, selected model, memory metadata, original audio, and recap persisted. The device reconnected automatically and replayed the cached recap successfully.
- Separate Play/Pause/Resume touch controls passed a hardware test using an existing saved recap. The playback position and I2S playback-frame counter stayed unchanged during pause; resuming advanced from the held position without rewinding. Repeated pause/resume, normal completion, and replay from the beginning passed, with zero new speaker errors and the original recording retained. The paused layout was inspected from the board's real framebuffer.
- The computer did not perform Gemini processing during these standalone tests. USB was used only to issue diagnostic commands and read status; the ESP32 performed the network requests, JSON handling, SD storage, resampling, and speaker output itself.

Live Gemini availability and quota depend on the configured account and service. Earlier memories remain on the card; the device UI currently opens the latest one. Each original recording is limited to 60 seconds, and each follow-up to 30 seconds. Personal Wi-Fi or a 2.4 GHz hotspot is supported; enterprise credentials and captive-portal browser login are not implemented.
