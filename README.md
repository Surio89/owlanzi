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

That is why anything published is built by `tools/make-installer.ps1` from the
`release` environment, and why that script scans the finished image for those
strings afterwards. If it finds one, it deletes the image and aborts.

## Still open

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
