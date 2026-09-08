# Release 1.0.10 — 2026-09-08

Published on https://owlanzi.com/ for TC001 and ESP32dev, with matching USB and
OTA images. Existing devices can install it through System → Install latest
update. No device was flashed as part of this release.

- Daily automatic checks fetch the real OTA manifest with a fixed
  `daily-update-check=1` marker. Only the attempted day is stored locally before
  sending, preventing duplicate markers after reboots or lost responses. Checks
  can be disabled in System settings; installation remains manual. The website
  stores only aggregate counters without device IDs or installed versions.
- A discovered update queues **UPDATE AVAILABLE** / **UPDATE VERFUEGBAR** on the
  matrix for 20 seconds of actual Battery-screen time. The notice yields to all
  other screens immediately; remaining time resumes on Battery. Repeated checks of the same
  version do not rearm it in the same boot. Normal brightness, no sound.
- An amber banner on every device WebUI tab shows the available version, an
  install button and progress/errors. It uses the existing authenticated,
  same-origin OTA handler and blocks duplicate requests while busy.
- The live statistics dashboard now shows approximate daily active devices,
  device-days, a chart and daily CSV. Both live privacy pages explain automatic
  checks and the device setting. No database migration or request IDs were added.

Validation completed:

- Three public ESP32 builds with `NO_LOCAL_SECRETS`; both USB/OTA packages passed
  image/partition/hash checks and the private-seed scan. NVS/OTA layout unchanged.
- 237 native firmware checks; 14 JavaScript WebUI checks; all three ELF stack
  checks: 1072 bytes application frames before TLS, 16384-byte network task.
- 35 website/simulator tests; 40 Python analytics/audit/deployment tests; static
  verification of all ten pages and firmware files.
- Real Chromium UI checks at 1440 and 390 pixels: all four tabs, both languages,
  no overflow, banner visibility, exactly one install POST, disabled busy button,
  cleared banner after updated status, existing settings/navigation behavior.
  Screenshots use a simulated future 1.0.11 release, not a published version.
- All 20 uploaded files verified through FTPS. All 18 public static outputs
  additionally verified byte-for-byte over HTTPS (HTML with the existing opt-out
  transformation); both PHP handlers verified by their HTTP behavior. Both live
  language pages show Firmware 1.0.10 and installer state `ready`.
- Live MariaDB checks for daily markers from both targets, authenticated dashboard
  and daily CSV passed. Synthetic increments removed, real counters preserved.

SSH timed out. The explicitly authorized FTPS fallback used certificate
verification for control and data, verified staging/backups, checks for
concurrent changes, immutable versioned binaries and atomic renames with the
four manifests last. Operator configuration, .htaccess, other analytics files,
link registry and unrelated live content were preserved.

Deployment receipt in the website repository:
`website-tools/release/.pio/deployed-1.0.10-20260908T100254Z.json`.
Rollback files on the same host:
`firmware/.release-history/1.0.10-20260908T100254Z/before/`.
Additional local evidence: `website-tools/.analytics-work/daily-updates-1.0.10/`.

TC001 USB SHA-256:
`0c7381a63991e730c23d7c7e6822aeb6bbbba08b273c6d54d5dd203175d2d2c7`.
TC001 OTA SHA-256:
`2646c4ce56fab84f8035049f3cadb4ae4d308212d4e2ff1e91b195d3286e501c`.

No physical-device test or Git push was performed. The new device notifications
take effect after installing 1.0.10 and detecting a subsequent available update.
