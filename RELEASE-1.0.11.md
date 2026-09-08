# Release 1.0.11 — 2026-09-08

Published on https://owlanzi.com/ through SSH host `allinkl`, with matching
TC001 and ESP32dev USB and OTA images. The live installer shows 1.0.11.

- Add Discord support in English and German to the device sidebar and a
  responsive help card. Users can ask questions and help each other, with
  the developer also participating: https://discord.gg/Bhpr3zRfVv.
- Publish the website help section, setup callout and Discord link in all
  ten page footers. Preserve live content and operator configuration, adding
  only the `discord` configuration field. Existing analytics files stay intact.
- Include the previously prepared reconnection improvement: show waiting
  dashes during brief interruptions and delay the normal OFFLINE screen until
  30 seconds after the last successful fetch. Reading freshness and critical
  alarm priority remain strict.

Validation completed:

- Three public ESP32 builds with `NO_LOCAL_SECRETS`; both USB/OTA packages
  passed checksums, partition bounds and three private-seed checks each.
- 259 native firmware checks, 14 JavaScript WebUI checks, 35 website/simulator
  tests and 44 analytics/audit/deployment tests passed.
- All three ELF checks: at most 1072 bytes of application stack frames before
  TLS; network task stack remains 16384 bytes with idle task priority.
- All ten HTML pages and 390 local references verified. The Discord support
  was inspected in the browser before release, including the mobile WebUI and
  language switch. Both public language editions show 1.0.11 and Discord.
- All 21 published files verified over HTTPS, including exact firmware hashes.
  Both OTA manifests have exact content lengths without redirects or compression.
- Ten protected website/analytics files verified unchanged. Existing operator
  configuration remains byte-for-byte intact outside the added Discord field.
- The local `web-installer` image and manifest match the published TC001 build.

Deployment receipt in the website repository:
`website-tools/release/.pio/deployed-1.0.11-20260908T165009Z.json`.
Rollback files on the same host:
`firmware/.release-history/1.0.11-20260908T165009Z/before/`.
Additional local evidence:
`website-tools/.analytics-work/discord-support-1.0.11/`.

TC001 USB SHA-256:
`ea706f742c3617457c4f618c899fe60f8c1eb0098084041de6942f2d10886633`.
TC001 OTA SHA-256:
`e8309cb9c874e5835b17da5ea221568d59c490e956859af6cdb3f44df20dea54`.

No physical-device test, device flash or Git push was performed. Existing
devices receive the new WebUI after installing 1.0.11 through their update UI.
