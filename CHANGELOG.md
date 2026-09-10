# 1.0.12 — 2026-09-10

- Include hardware model and installed firmware version with the existing daily
  update check for aggregate statistics. No device identifiers; daily scheduling
  and opt-out remain unchanged.

# 1.0.11 — 2026-09-08

- Add Discord community support in English and German, with a desktop sidebar
  link and a mobile-friendly help card. Users can ask questions, exchange tips
  and help each other, with the developer participating as well.
- Link the same community from the website help section, setup guide and every
  page footer.

- Delay the normal OFFLINE screen until 30 seconds after the last successful
  cloud fetch. Show waiting dashes during short Wi-Fi/cloud interruptions;
  keep strict reading freshness, own-alarm resets and critical alarm priority.
- Explain the temporary reconnection state in the English and German WebUI.

# 1.0.10 — 2026-09-08

- Check for available firmware automatically once per Berlin calendar day,
  after boot, network and clock readiness; never install automatically.
- Mark the ordinary manifest request with `daily-update-check=1` for an
  approximate aggregate daily device count. Persist only the attempted day in
  separate local NVS before sending, preventing duplicate reports after reboot
  or a lost response. No device ID or installed version is transmitted.
- Add a saved daily-check switch and a short explanation in both device UI
  languages. Manual checks remain available without the daily marker.
- Show a short update notice for 20 seconds of actual Battery-screen time,
  once per discovered version per boot. Other screens preempt it immediately;
  remaining time resumes on Battery, with normal brightness and no sound.
- Show an amber update banner on every device WebUI tab, with version,
  installation button and progress/errors using the existing protected OTA API.

# 1.0.9 — 2026-09-08

- Replace the support invitation with the selected friendly Ko-fi wording in
  English and German. Remove owl metaphors from the device support card.

# 1.0.8 — 2026-09-08

- Add an optional Ko-fi support card below the device interface and a desktop
  sidebar link. The friendly virtual-beer invitation is available in English
  and German. Every feature remains free; Ko-fi opens only on a voluntary click.

# 1.0.7 — 2026-09-07

- Refresh the device web interface with a fixed desktop sidebar, a centered
  reading column and the page scrollbar at the far right, including ultrawide
  displays. Keep all settings groups permanently visible in one clear sequence.
- Retain the familiar mobile controls, with responsive spacing, a live matrix
  preview and an accessible tab navigation in German and English.
- Keep save actions available while scrolling. Prevent saving before device
  configuration has loaded, and reveal invalid settings in the relevant tab.
- Add browser coverage for desktop and mobile layouts, always-visible sections,
  navigation, previews, settings changes and validation.

# 1.0.6 — 2026-09-07

- Fix the hardware-confirmed update-check reset: TLS work in the priority-1
  network task starved IDLE0 and triggered the five-second task watchdog.
  Run the worker at idle priority so time slicing lets IDLE0 service its own
  watchdog while TLS calculations continue. Keep both watchdogs enabled.
- Normalize WiFiClientSecure diagnostics: a positive return is a successful
  socket descriptor, not a TLS error.
- Add an explicit hardware regression that checks release metadata and monotonic
  uptime through the device API. Require idle priority in the ESP32 build checks.
- Recover the connected TC001 through its existing manual OTA endpoint, preserving
  settings. The USB readback showed intact 1.0.4 firmware and NVS; intermittent
  serial flash-access failures prevented reliable USB writing.

The 1.0.5 stack/timeout improvements did not fix the reported reset. Its actual
cause was confirmed by the serial task-watchdog report; 1.0.6 supersedes it.

# 1.0.5 — 2026-09-07

- Address restarts during online update checks: move metadata and image buffers
  off the task stack, keep check/install functions separate when compiled, and
  increase the network task stack from 10 KB to 16 KB. Check compiled ESP32
  frames in regression validation; log the stack watermark and reset reason.
- Correct secure-client timeout units (seconds) and explicitly limit the TLS
  handshake to 15 seconds for both update and Owlet connections.
- Handle a missing HTTP response stream without dereferencing a null pointer.
- Keep the last successfully checked release visible during retries and errors.
  Explain unchecked versions and a restart during a check in the WebUI. Expose
  HTTP/TLS diagnostics in update status; checking never installs or reboots.
- Keep the existing NVS and OTA layout. On affected 1.0.4 devices, upload the
  latest `-ota.bin` through the manual firmware field once to receive the fix.

Validation: 181 native assertions, 11 browser tests and a check of the actual
ESP32 stack frames, plus release image and website checks. No physical update or
serial crash log was captured at publication. Subsequent hardware testing
disproved the assumed fix; see the confirmed watchdog correction in 1.0.6.

# 1.0.4 — 2026-09-07

- Add online firmware updates under System in the device web interface.
  One click fetches the latest compatible release from owlanzi.com, installs
  it and restarts. Checking for updates separately is also possible.
- Preserve the existing NVS partition and configuration schema: Wi-Fi, Owlet
  credentials, language, display settings and alarm rules survive an OTA update.
- Verify HTTPS certificates, target, partition layout, image header, exact size
  and SHA-256 before activating the new firmware. Failed or interrupted downloads
  retain the current boot partition. Equal/older releases are not installed.
- Coordinate manual and online updates so only one writer can flash. Require
  the existing web authentication and origin checks, block configuration changes
  during updates, and reject/cancel updates when a critical alarm is active.
- Show progress, errors and reconnection status in German and English. Downloads
  run in the network task with bounded memory/time; cloud polling pauses briefly.
- Repair incorrectly encoded German labels in the device web interface and
  setup page, with a regression check for encoding damage.
- Publish separate application-only OTA binaries and target-specific update
  manifests alongside the merged USB installer images. Deployment verifies both
  formats and switches all manifests after their files are in place.

For an existing 1.0.3 device, upload the 1.0.4 `-ota.bin` once through its
existing firmware upload field. Future releases can then use the online button.
The merged USB image is not suitable for this migration.

Validation: 171 native regression assertions, eight browser tests, generated
browser script syntax checks and both ESP32 release builds. Network, flash and
NVS are simulated in native tests; a physical update/reboot is not yet tested.

# 1.0.3 — 2026-09-07

- Correct sleep decoding throughout the matrix, web status and previews:
  1 awake, 8 light sleep, 15 deep sleep, other values unknown.
- Preserve the cloud measurement timestamp. Require a recent fetch, a recent
  measurement and a measurement from the active session before showing vitals
  or evaluating optional own alarms. Constant new readings remain valid.
- Handle critical oxygen and critical battery properties explicitly. Critical
  oxygen alarms sound; the distinct critical battery notice remains silent.
- Keep critical alarms visible after acknowledgement. New or recurring causes
  sound again. Real alarms interrupt previews and boot messages; old battery
  notices cannot hide connection loss. Retain last known critical alarms with
  an OFFLINE annotation if the connection fails.
- Reset own alarm durations on invalid data, failed fetches, measurement gaps,
  rule changes and disarming. Only measurement time advances durations.
- Protect shared configuration, alarm strings and state with one recursive
  mutex. Network requests use snapshots and reject results after account,
  region or device changes. Web serving and sound no longer block the draw loop.
- Require explicit serial selection when an account contains multiple devices.
  Report and retry APP_ACTIVE failures instead of silently losing live updates.
- Validate configuration types, lengths and ranges. Authenticate every OTA
  stage, reject cross-origin requests and restart only after a completed valid
  upload. Handle cancelled and failed uploads explicitly.
- Use wrap-safe timers, live Wi-Fi state, asynchronous Wi-Fi scans and browser
  expiry of disconnected status/mirror data. Pin release toolchain dependencies.

Validation: 98 native regression assertions against production logic, three
browser outage tests, generated browser script syntax checks, ESP32 builds,
cppcheck and installer binary audits. Network,
flash, NVS, clock and LED hardware are simulated in native tests. Real TC001
flashing and live Owlet timestamp behaviour have not been hardware-tested.
