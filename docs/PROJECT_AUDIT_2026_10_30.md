# Project audit — 2026-10-30

Scope: current main `681c7eb4205a`, excluding parked viewer and Theme Maker work.
Patch: v7.2.4-dev on `fix/project-audit`. Review and testing do not prove absence
of vulnerabilities. No remote exploitation or code execution was attempted.

## Findings addressed

| Area | Trigger and impact | Result |
| --- | --- | --- |
| Camera connection test | A configured/discovered camera supplies a JPEG larger than the fixed frame buffer. The old per-read guard allowed a following inner-loop write beyond 256 KiB. | AddressSanitizer reproduced a heap-buffer-overflow write in the unchanged source; per-byte bound now enforced. Arbitrary code execution was not demonstrated. |
| Camera connection test | A peer continually supplies bytes without JPEG start/end markers. Per-read timeouts alone do not end a continuously active invalid stream. | Frame acquisition has a five-second budget, checked between reads. One in-flight 3.5-second read may extend that phase. Connection/header/lane acquisition have their own timeouts; five seconds is not an end-to-end guarantee. |
| Camera test correctness | HTTP 404/error bodies containing JPEG bytes could report success; an overlong URL was silently truncated. | Require successful headers and HTTP 200, reject overlong URLs before task creation. |
| WebSocket input | Missing/overlapping fragments could reach JSON parsing with stale or uninitialized buffer regions. A malformed SDK event sequence or peer input could produce incorrect status publication. | Require a complete contiguous message with one length and generation. Retire assembly on reconnect/close/client destruction. This is defensive boundary hardening, not a demonstrated code execution or disclosure exploit. |
| Bed Mesh | JSON exponent overflow, such as a parsed infinite number, or a value unrepresentable for display, could poison float geometry and statistics. | Reject heights before cache mutation; preserve safe coordinates. Unchanged source accepts the malformed-height regression; patched source rejects it. |
| OTA application logs | Saved/custom download URLs may contain passwords or signed query tokens and were printed in full. Log readers could recover them. | Application start/save messages omit URL contents. Stored/requested URLs remain unchanged; SDK or external-server logs were not audited for complete redaction. |
| Stale code | No production/test callers remain for `printer_profile_health_poll_one`; combined worker owns current health probing. | Remove obsolete function, declaration, probe include and misleading header comment. Preserve genuine connection-loss handling and hysteresis. |
| Stale test coverage | About expected modified marking only for Custom; the connection-controls mesh fixture lacked current snapshot/renderer dependencies. | Update fixtures, include Development channel, and rerun both suites successfully. |

## Validation

A complete initial run of all 90 existing host test entry points completed:
88 passed; the two stale fixtures above failed and then passed after repair.
This covers the existing TLS/profile/backup/OTA transport, command restrictions,
network/UI ownership, preview/cache, reconnect, Setup Center, theme/layout and
rendering tests. Review covered network and SD entry points, TLS/auth forwarding,
backup bounds and secret handling, generation fencing, image handling and build
identity. Three new ASan/UBSan suites exercise actual camera input, actual
WebSocket assembly and actual Bed Mesh controller JSON. New tests include
normal/max-size JPEGs, split markers, overflow input, endless garbage, error
responses, allocation/open/lane failures, URL limits, fragment geometry and
reconnection/generation retirement, non-finite heights and safe cache retention.
Existing sanitized tests run with `ASAN_OPTIONS=detect_leaks=0` because this host
does not support LeakSanitizer. Relevant suites are repeated after changes;
unrelated passing suites need not be repeatedly rerun.

No ESP-IDF target build or physical panel runs are available here. Host fixtures
cannot reproduce all RTOS scheduling, real sockets, DMA/JPEG hardware, cold
boot, flash persistence or long-running print/recovery behavior. No exhaustive
fuzzer campaign or current third-party vulnerability-advisory sweep was run.

## Remaining trust and deployment limits

- Plain HTTP/WS remains supported. A network attacker on an unprotected path can
  observe or alter traffic, including API keys. Opt-in per-profile HTTPS/WSS
  verifies configured CA trust and must not silently downgrade; existing
  credential-forwarding/redirect boundary tests pass. This patch does not force
  TLS or change users' configured endpoints.
- OTA accepts configured HTTP URLs, and a project-specific signed-firmware policy
  is not enforced. HTTPS protects the selected server connection but does not
  replace firmware-signature verification. A compromised trusted endpoint or an
  attacker on an allowed HTTP update path remains a firmware supply-chain risk.
- Current configuration disables Secure Boot, flash encryption and NVS
  encryption. A person with sufficient physical flash access may recover stored
  credentials or replace firmware. Encrypted SD backups do not encrypt NVS.
- A compromised trusted printer/camera can still send false data, trigger bounded
  repeated work, or become unavailable. Validation reduces harm to the tablet;
  it cannot make that remote service trustworthy or prevent printer-side misuse
  by a party that already controls the printer.

Signing/key management, secure provisioning, verified OTA policy and optional
transport restrictions are separate planned changes requiring existing-device
migration, recovery and full-stack compatibility tests. Do not enable irreversible
eFuse policy as an incidental code-cleanup step.

## Panel verification

Build the patch with `./tools/build_idf6_hosted3.sh`. Check camera tests with a
normal stream, unavailable camera and a failed URL; close/reopen the profile or
Setup Center during the test. Check actual Camera and Dashboard streaming,
printer switching, WebSocket reconnect and post-reboot capability discovery.
Check Bed Mesh during printing/paused and verify calibrate remains restricted.
Check OTA URL retention/start/cancel with a trusted intended image and verify
application logs omit its full URL. Complete a print/recovery and overnight soak
before stable release approval.

## Repository-wide documentation review

The reviewed GitHub main checkpoint tracks 139 Markdown/RST documentation
files. Outside components, its 128 files comprise:
five root Markdown documents, 122 under docs (61 release notes, 17 historical
records and 44 other guides/notices), and one logo-asset README. Eleven additional local-component files include
upstream READMEs/examples/changelogs/performance notes and two PrinterHMI patch
notes. The new audit/release note bring this patch to 141. This scope is the main repository's
project documentation, not every archived branch, GitHub wiki, external linked
website or documentation downloaded later into managed_components. The tracked
local-component docs were included, with upstream content distinguished from
application instructions. The GitHub CI YAML was checked against
its CI guide; no separate issue/PR-template Markdown files exist in this tree.

All files were inventoried and scanned for obsolete release/branch/toolchain
claims and repository-relative references. Current build, recovery, security,
configuration and update instructions were checked against their owning code,
tracked configuration and scripts. Existing link validation covers active
Markdown/release-page rules. Historical records were classified and preserved;
original release notes, license notices and changelog history were not rewritten
into statements about the current firmware. This is a maintenance/content
review, not legal review of licenses or verification of every external URL.

Corrections include IDF 5.4.4 in CONTRIBUTING, flash commands that bypassed the
accepted PPA wrapper configuration, a fresh-clone directory mismatch, a stale
absolute-lockfile warning, wrong nightly asset names, an eight-versus-ten
navigation mismatch, the Studio default and legacy chooser themes, broken NVS
table continuation and missing key owners, wrong preview-cache paths, TLS
multi-instance/default-port behavior, encrypted backup secret/CA/byte-limit
policy, and obsolete Custom/dev comparison rules in the OTA guide. Root SECURITY
now documents optional HTTPS/WSS, plaintext fallback availability, unsigned OTA
and unencrypted NVS limitations accurately. Engineering reports have explicit
checkpoint-history notices rather than masquerading as the latest release state.
Local LVGL-port patch notes now identify the 9.5 pin and retained display/touch
customizations. Generic component/example READMEs have scope notices; sample
SDK/lockfile deletion instructions must not be applied at the application root.
Published historical release bodies/assets are not edited by this local patch.

No host test PASS or operator acceptance was promoted into an unsupported
hardware, security or performance guarantee. Current unresolved validation limits
remain in the audit and known-issues documents. The guarded updater will deliver
these changes locally; GitHub documentation changes only after the user commits
and pushes the reviewed branch.
