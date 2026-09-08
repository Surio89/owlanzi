# Release 1.0.8 — 2026-09-08

Published on https://owlanzi.com/ with USB and OTA images for TC001 and ESP32dev.
The website success dialog now includes the optional Ko-fi invitation in English
and German. It appears only after successful installation and reconnection; the
existing Continue action remains available without donating. The device web UI
includes a support card and a desktop sidebar link, also in both languages.

Validation:
- Three seed-free ESP32 builds completed; both public targets passed image,
  partition, OTA, checksum and private-seed audits.
- Firmware native regressions, 12 browser-logic tests and all three ELF stack
  checks passed. Website JavaScript tests: 35 passed.
- Python release/analytics/audit suite: 34 passed; after the final verifier fix,
  all 15 deployment tests passed.
- Real-browser success-state checks passed in both languages in the local
  preview before release. The live EN/DE pages load firmware 1.0.8, report ready,
  and expose the expected translated success content and Ko-fi link.
- All 18 deployed files match the release receipt and passed public HTTPS
  verification. USB and OTA images have exact lengths and are served without
  compression or redirects. Firmware binaries contain the Ko-fi UI text.
- Operator configuration, analytics code/link registry, .htaccess, and legal/
  privacy pages are byte-for-byte unchanged.

No connected device was flashed or hardware-tested. Existing clocks receive
this version when their owner chooses the online update. Saved settings use the
unchanged OTA/NVS layout.

Deployment receipt (website repository, ignored build output):
`website-tools/release/.pio/deployed-1.0.8-20260908T075935Z.json`.
Server rollback files:
`firmware/.release-history/1.0.8-20260908T075935Z/before`.

The release checker now follows the current direct-link/opt-out rendering.
SQLite test connections are closed explicitly to avoid Windows file locks
while cleaning up the temporary analytics database.
