# Release 1.0.9 — 2026-09-08

Published on https://owlanzi.com/ for TC001 and ESP32dev, including USB and OTA
images. The installer success dialog and device web UI use the selected third
Ko-fi invitation, without owl metaphors:

> **Gefällt dir Owlanzi?**
>
> Schön, dass du Owlanzi nutzt! Wenn du mir eine kleine Freude machen möchtest,
> kannst du mir auf Ko-fi ein Bier oder einen Kaffee spendieren. Das unterstützt
> die Weiterentwicklung und sorgt für ein breites Grinsen beim nächsten
> Programmierabend. 😊

The success label is “Owlanzi wurde erfolgreich installiert!”. The invitation
links to https://ko-fi.com/owlanzi and explicitly remains optional. English copy
is provided throughout. The existing Continue action remains available without
donating, with a separate link to setup.

Validation:

- Three public ESP32 builds completed with local secrets disabled. Both USB/OTA
  packages passed firmware headers, image/partition, SHA-256 and private-seed
  checks.
- Firmware: 182 native checks, 12 JavaScript checks and all three ELF stack
  checks passed. All 149 German translation entries are accounted for.
- Website: 35 JavaScript tests and 35 Python release/analytics/audit tests passed.
- The actual installer renderer passed success/progress/error, translation,
  opt-out and continue-without-donating checks in both languages. The device
  support card was checked in both languages and at a mobile viewport.
- All 17 published files passed exact HTTPS verification. Both live language
  pages report Firmware 1.0.9, ready, and the selected text and Ko-fi destination.
- Operator configuration, analytics files/link registry, .htaccess and legal/
  privacy pages remained byte-for-byte unchanged.

SSH reset and then timed out. The authorized explicit FTPS fallback used
certificate verification, verified backups/staging, live concurrency checks and
atomic renames with the four USB/OTA manifests last. The fallback's successful
path and staging-corruption/concurrent-change refusal paths were checked with a
fake transport before publication.

Concurrent work on daily update checks began after the release build. That work
was preserved in the shared checkout. This release uses the already built and
tested source snapshot, with SHA-256
`b75ae9b777f79306f07cb90f630e67f2d6e876a278b6fe342af6484ca1887329`.
The package was isolated under the website repository's ignored
`website-tools/.analytics-work/support-copy-1.0.9/isolated-release/`; the normal
source equality and release checks ran against that snapshot.

Deployment receipt in the website repository:
`website-tools/release/.pio/deployed-1.0.9-20260908T092138Z.json`.
Server rollback files:
`firmware/.release-history/1.0.9-20260908T092138Z/before`.
The standalone local USB installer was synchronized to the audited 1.0.9 build.

No device was flashed or hardware-tested. Existing clocks receive the updated
support copy after their owner installs the firmware update.
