# Memory Radio

A touchscreen oral-history recorder for the LCDWIKI ES3C28P ESP32-S3 N16R8. Gemini organizes your spoken memories into a faithful summary, people, places, a timeline, and three follow-up questions. Tap an entry to see a supporting quote and replay the corresponding original audio. Record answers to enrich the memory. Gemini can narrate the recap and questions through the device speaker.

## Use

1. Keep the board plugged into this Windows computer. Double-click **Connect Gemini.cmd** and enter your Gemini API key in the local password field if prompted. The key is encrypted with Windows DPAPI for this account; it is never stored on the device or microSD.
2. Tap **Record a memory** and tell a short story, up to 60 seconds. Tap **Finish recording**. Your original audio is saved to the microSD immediately, before any API request.
3. Tap **Organize with Gemini**. This sends that recording through the computer to Google's Gemini API. The device then shows the generated summary. Tap the page controls to read more.
4. Tap **People**, **Places**, or **Events**. Tap an entry to read its description and supporting quote; **Hear original moment** plays the corresponding recording excerpt. Timestamps are AI estimates and may need human review.
5. Tap **Questions**, select a Gemini-generated follow-up, and tap **Record my answer**. Record up to 30 seconds. The answer is saved on the card and sent to Gemini to update the existing memory. Questions can also be read aloud.
6. Tap **Narrate** to generate and play a spoken recap. Speech generation uses a separate compatible Gemini TTS model and depends on your account's model access and quota.

Recording and saved playback work without internet. New AI organization, follow-ups, and narration need the computer bridge and internet. When disconnected, the summary's **Play audio** plays a previously saved recap, or the original story if no recap exists. Saved quotes can replay local audio excerpts without the computer. After a restart, the latest recorded memory and any saved summary are restored from the card.

The first version opens the latest memory on the device. Earlier memories stay in their separate folders on the card and computer. The interface uses an ASCII font; original Unicode output and the full transcript are preserved in the computer's JSON archive. Each recording is limited to 60 seconds (30 seconds for a follow-up).

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
  memory.txt         Device-readable restore data
  memory.bak         Previous restore data, when a memory is updated
```

`/memory-radio/latest.txt` identifies the most recent memory. Card audio files are standard WAV and can be opened on a computer. A 60-second recording uses approximately 1.92 MB. Shorter clips are saved at their actual duration. Narration is saved when it arrives at the board, and the same memory's latest recap replaces its previous recap. Original stories and follow-up recordings use separate filenames.

The computer also archives original audio, the full Gemini JSON including transcript, follow-up answers, and generated narration under this project's `memories/` folder. Those recordings are sent to Google only when the user chooses organization or submits a follow-up. Merely recording a new story does not upload it.

## Gemini setup and troubleshooting

`Connect Gemini.cmd` opens the setup window and runs the Windows bridge in a visible console. Keep that console open while using Gemini. Close Arduino Serial Monitor and diagnostic scripts first; only one process can own COM3. The device shows **Gemini connected** after the bridge successfully checks the key's available models and opens the port. Model discovery prefers an available Flash model; narration prefers an available compatible Flash TTS model. Models can be overridden with bridge.ps1's `-Model` and `-VoiceModel` arguments.

To replace the key, run `powershell.exe -NoProfile -ExecutionPolicy Bypass -STA -File .\configure.ps1`, then restart the bridge. The Windows user environment variable `GEMINI_API_KEY` is also supported. Do not paste your key into chat or source code.

Bridge output is in `bridge-output.log` and `bridge-error.log`. Keys and raw audio are not logged. If organization fails, your original WAV remains on the card. Open your recording and retry. If voice generation is unavailable, the text summary and original voice still work. After unplugging the board, restart the bridge. Canceling a request on the device ignores late results; a cloud request already in flight may still finish and be billed.

If a Gemini request fails, double-click **Diagnose Gemini.cmd**. It checks generation access using the saved key in your Windows session, without opening the USB port or uploading a personal recording. The result is saved in `gemini-status.json` (excluded from Git). The updated bridge also records the HTTP code, provider status, model, and sanitized error message for failed API calls. Keys are redacted. A successful model-list request alone does not prove that generation quota or permissions are available. Restart Connect Gemini after updating the bridge so the running process uses the latest code.

SD status appears on every screen. **SD: Saved** indicates the last save completed. **SD: Not available** indicates mounting failed; the card is never formatted automatically. A mounted card with a write failure will remain **SD: Ready** rather than claim the recording was saved. Keep power connected until the recording screen closes and the save completes.

## Development

- MemoryRadio.ino: device UI, microphone, playback, framed USB protocol.
- hardware.h: locally validated ILI9341V / FT6336G / ES8311 drivers reused from PipPet.
- storage.h: WAV files, SD metadata, and restore.
- bridge.ps1: Gemini audio understanding, grounded structured memory, narration, and archives.
- configure.ps1 / start-gemini.ps1 / connect.ps1: local key setup and bridge lifecycle.
- build.ps1 / upload.ps1: existing Arduino ESP32 3.3.12 toolchain; app-only flash at 0x10000.
- inspect.ps1: captures the real framebuffer to build/.
- verify.ps1: hardware integration test with an explicitly labeled test fixture; it creates a short test recording and files under a new /memory-radio folder. It does not call Gemini or claim that fixture is AI output.
- test-transfer.ps1: exercises the numbered USB transfer with a full 60-second payload, lost and duplicate packets, and checksum corruption. It does not modify recordings or call Gemini.
- gemini-errors.ps1 / test-gemini-errors.ps1: provider-error parsing, credential redaction, and diagnostic checks.
- Diagnose Gemini.cmd: local API generation check; no personal recording is submitted.

Recordings use an acknowledged, numbered-chunk upload protocol. The computer verifies the entire recording's checksum before calling Gemini. Missing chunks are retried and duplicate chunks are ignored. The device remains responsive during upload; existing saved recordings can be retried without recording again.

Run scripts with `powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\build.ps1`, substituting the desired filename. This execution-policy setting affects only that process. Rebuilding uses the existing compiler directly because Arduino CLI platform discovery has directory-access errors in this environment. Previous LastTransmission/PipPet/PulseVisualizer projects remain in neighboring folders. Upload replaces only the application and preserves the existing flash partition table and model partition.

References: [board documentation](https://www.lcdwiki.com/2.8inch_ESP32-S3_Display), [Gemini audio understanding](https://ai.google.dev/gemini-api/docs/audio), [structured output](https://ai.google.dev/gemini-api/docs/structured-output), [speech generation](https://ai.google.dev/gemini-api/docs/speech-generation).
