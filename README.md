# owlanzi

Custom firmware for the **Ulanzi TC001** pixel clock. It polls the Owlet Smart
Sock cloud directly and shows heart rate, oxygen saturation, sleep state and
sock battery on the 8×32 matrix. No Home Assistant, no broker, no bridge — the
clock fetches the values itself and is configured through a web interface on
your own network.

> **This clock does not replace the base station.** It is an additional
> display, nothing more. It is not a medical device, it is not certified, and
> it can fail — Wi-Fi drops, the cloud goes down, firmware has bugs. The base
> station's alarms remain the ones you rely on.

## Support Owlanzi

Owlanzi is free and open source. If you enjoy Owlanzi, you can
[buy me a virtual beer on Ko-fi](https://ko-fi.com/owlanzi) (coffee works too!).
Support is entirely optional and never unlocks or restricts any features.
The device web interface includes the same invitation in English and German.

## What you need

- a **Ulanzi TC001** (ESP32-WROOM-32D, 4 MB flash)
- an **Owlet account** with a paired Smart Sock
- a 2.4 GHz Wi-Fi network
- a USB-C cable for the first flash

## Flashing

`web-installer/` contains a ready-made page for
[ESP Web Tools](https://esphome.github.io/esp-web-tools/): `index.html`,
`manifest.json` and the merged image. Any browser with Web Serial (Chrome,
Edge) can flash the clock from it — no toolchain required.

To build it yourself, use [PlatformIO](https://platformio.org/):

```
pio run -e ulanzi -t upload
```

**Going back to AWTRIX** is not done through an update: an update replaces
only the program, not the partition table. Use the AWTRIX web flasher over
USB instead.

## Setting it up

On first boot the clock opens an open hotspot called **owlanzi**. Join it and
the setup page opens by itself (otherwise browse to `192.168.4.1`). Two
things are needed: your Wi-Fi and your Owlet account. The clock then restarts
and shows its IP address for 15 seconds — that address serves the full
interface.

There you can set, among other things:

- **colours** for every screen, with a preview that recolours as you drag the
  picker
- **brightness**, separately for normal use, daylight, alarm and preview,
  driven by the light sensor
- **your own alarms** on oxygen and heart rate, each with a threshold and a
  minimum duration
- **poll interval** against the Owlet cloud (5, 10 or 15 seconds)
- **language** of the interface and of the text on the matrix (English or
  German)

Credentials stay on the device. The connection to the Owlet cloud verifies
certificates.

## Updating without losing settings

From 1.0.10, the clock also checks for new firmware once
per calendar day in Europe/Berlin, after the first minute of uptime and once
Wi-Fi and network time are ready. It waits while an alarm or another update is
active. These checks only fetch release metadata; installation remains manual.
Disable **System → Update firmware → Check for updates daily** and save to
turn automatic checks off.

The ordinary metadata request carries `daily-update-check=1` and contributes
to an approximate daily active-device counter on owlanzi.com. There is no extra
analytics request, device identifier or installed-version parameter. Only the
attempted day is kept locally in a separate NVS namespace before the request,
so reboots and lost responses do not duplicate it. A failed automatic attempt
waits until the next day; manual checks remain available. Devices offline,
running older firmware or with checks disabled are missing. The count does not
confirm an active Owlet measurement. Erasing NVS can reset the daily reservation.

When a new version is found, the matrix shows **UPDATE AVAILABLE** (German:
**UPDATE VERFUEGBAR**) for 20 seconds of actual Battery-screen time. It waits
until the sock is charging or off the foot, yields immediately to any other
screen and resumes the remaining time on Battery. The same version is announced
once per boot, without sound or increased brightness. The device web interface
also shows an update banner on all tabs, with the available version and an
installation button. Installation remains a deliberate action and uses the
existing verified OTA procedure below.

From firmware 1.0.4, open the clock's web interface and select **System →
Firmware update → Install latest update** (German: **System → Firmware
aktualisieren → Neuestes Update installieren**). The clock downloads the
latest compatible application directly from owlanzi.com, checks it and
restarts. Wi-Fi, Owlet login, display preferences and alarm rules remain in NVS.
Cloud readings pause during the download and restart. Keep the clock powered.
An unsuccessful download leaves the current firmware active; retry deliberately
after resolving the displayed error. If an alarm is active, wait until it clears
and retry.

For a clock still running 1.0.3, or one that restarts when checking for updates
on 1.0.4 or 1.0.5, download
[the TC001 1.0.6 OTA application](https://owlanzi.com/firmware/owlanzi-tc001-1.0.6-ota.bin)
and upload it once through the existing firmware update field in System.
This enables the online update button while preserving settings. Use the
application file ending in `-ota.bin`; the larger merged USB installer image
also contains boot/partition data and is not an OTA upload file.

OTA requires the existing Owlanzi 4 MB partition layout. Changing that layout
requires a USB installation. Manual OTA installation with preserved settings and
check-only HTTPS requests have been verified on a TC001 with 1.0.6. Full online
image installation and interrupted-flash recovery also have simulated coverage;
they have not yet been exercised end to end on the physical clock.

## How the login works

The Owlet cloud sits on Ayla Networks; signing in is a chain of four steps:

1. Firebase: email + password → `idToken`
2. Owlet SSO: `idToken` → `mini_token`
3. Ayla: `mini_token` + app secret → `access_token`
4. Ayla: set `APP_ACTIVE=1`, then read `properties.json`

Step 4 is not optional. Without `APP_ACTIVE` the cloud serves frozen values —
it only refreshes them while an app is listening.

Endpoints and field names were taken from the open-source Python
implementations [pyowletapi](https://github.com/ryanbdclark/pyowletapi) and
[owlet_monitor](https://github.com/mbevand/owlet_monitor).

## Layout

```
src/            firmware: main loop, display, Owlet client, web interface
tools/          PowerShell helpers: build, upload, verify, screenshot
web-installer/  merged image plus the ESP Web Tools page
assets/         logo and preview image
```

Three PlatformIO environments:

| Environment | Purpose                                                   |
|-------------|-----------------------------------------------------------|
| `esp32dev`  | bare dev board, 4 MB, no matrix attached                   |
| `ulanzi`    | the TC001 itself, 4 MB, with OTA partitions                |
| `release`   | same as `ulanzi` but without the local `secrets_local.h`   |

### Credentials while developing

`src/secrets_local.h` (template: `secrets_local.h.example`) seeds the NVS once
so you don't have to walk through the setup hotspot on every test. The file is
in `.gitignore` — **but its string literals end up inside the compiled
image.** A `.bin` built with it present must not be handed to anyone.

Public releases use the audited build in [DEPLOYMENT.md](DEPLOYMENT.md), with
`NO_LOCAL_SECRETS` and a scan for private seed values. The local USB helper
`tools/make-installer.ps1` also builds the `release` environment and scans its
result. An image containing private seeds must not be published.

## Release 1.0.6

See [CHANGELOG.md](CHANGELOG.md) for the review fixes. Run native regression
tests with `python tests/run_regressions.py` after `pio run -e release` has
installed ArduinoJson. The runner requires Node.js and Visual Studio 2022
Build Tools on Windows. It simulates hardware/transport and never reads
developer credentials or contacts Owlet. It also checks the actual ESP32 ELF
stack frames to keep the update buffers out of the HTTPS call stack.

1.0.6 fixes the hardware-confirmed task-watchdog reset during update checks.
The TLS worker shares idle priority so IDLE0 can service its watchdog. Device
tests with `tests/hardware_update_check.py` verify completed checks and monotonic
uptime; run them only against a device authorized for testing.

Sleep states use 1 (awake), 8 (light sleep), 15 (deep sleep); all other codes
remain unknown. Data freshness now uses the cloud measurement timestamp,
not changes in heart rate or oxygen. After boot or an inactive sock, wait for
a measurement taken five seconds after detecting the active session.
For multiple paired devices, select the serial shown under System settings.

After a successful cloud fetch, brief Wi-Fi or fetch failures show waiting
dashes. **OFFLINE** appears once 30 seconds have passed without another
successful fetch. This grace applies only to the label: failed fetches still
invalidate readings and reset own alarm durations immediately, and a fetch
older than 20 seconds still expires. Critical alarms keep priority and retain
the immediate last-known-alarm annotation when data is unavailable. Before the
first successful fetch or after changing accounts/devices, OFFLINE appears
without this grace period.

Web releases use the audited build/deployment workflow in
[DEPLOYMENT.md](DEPLOYMENT.md).

## Responsive device interface

The four tabs (Status, Display, Alarms and System) share the same controls on
phones and desktops. Above 900 px, the sidebar stays at the left edge of the
window, with content centred in the remaining space at a maximum width of
960 px. The document scrollbar stays at the right edge. Settings are grouped
in a single column of permanently visible cards with static headings. All
controls are shown directly, including the manual firmware upload.
The compact readings remain side by side. The live mirror and save actions
stay visible on desktop. Settings pages keep their save actions visible;
saving becomes available after the device configuration has loaded.
Keyboard navigation follows the horizontal or vertical tab layout, and
direct links such as `/#display` keep working in both languages.

For a local preview without a clock, run `node tools/preview-ui.mjs` and open
`http://127.0.0.1:4173/`. It serves the actual embedded HTML from `src/webui.cpp`
with illustrative data and never contacts a device or Owlet. `/setup` previews
hotspot setup. Reload after editing the source. To run browser interaction
checks, enter `await (await import('/__checks.js')).checkUI()` in the preview
page's browser console. Repeat at phone, tablet and desktop viewport sizes.
The checks exercise language switching, navigation, labels, colour previews,
alarm controls, saving, password visibility and checking for updates. They
also verify ultrawide sidebar placement, centred content, a single column of
always-visible topics, retained edits and revealing invalid fields in other tabs.

## Remaining hardware validation

An honest status, not a wishlist:

- **Sleep state.** The numeric values of the `ss` field are undocumented.
  Awake, light sleep and deep sleep are mapped; everything else deliberately
  falls back to a grey bar rather than faking a sleep depth.
- **Light threshold.** The default came from AWTRIX and has not been measured
  on the TC001 yet.
- **Freshness gate.** That the display really switches to "waiting" when the
  sock comes off the charger has not been walked through with a real sock.

## Licence

[PolyForm Noncommercial License 1.0.0](https://polyformproject.org/licenses/noncommercial/1.0.0/)
— see `LICENSE.md`.

In plain words: build it, change it, pass it on, publish your changes. Just
not for commercial purposes, and keep the notice — say where it came from.
Private use, research, teaching, hobby projects, charities, schools and
public bodies are all explicitly fine. Selling it, or building a product or a
service on it, is not.

Note that this makes the project source-available rather than open source in
the OSI sense: every OSI-approved licence permits commercial use, and this one
deliberately does not.

The licence covers the code in this repository. The Owlet endpoints, field
names and app constants are facts about someone else's service, read off the
open-source projects credited above — they are not mine to license.

## Legal

Owlet and Ulanzi are trademarks of their respective owners. This project is
not affiliated with either. It talks to an interface that is not publicly
documented and can change at any time.
