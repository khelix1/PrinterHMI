# Bug and security review — feature/profile-setup-dialogs

Reviewed checkpoint `004f6c2`, with the final UI/keyboard pass retained. This
pass covers application source and host regressions. It is not a penetration
test, a dependency CVE inventory, or a claim that the device is secure against
physical access.

## Fixed findings

| Area | Confirmed behavior before this patch | Result |
| --- | --- | --- |
| CA import | Invalid/oversized files replaced or cleared the live CA buffer; NVS failures could leave a new CA active despite reporting failure. | Stage file contents, reject embedded NUL/read errors, and publish to the existing cache only after a successful NVS write and commit. |
| Duplicate endpoints | Endpoint-only HTTP/WS calls selected the first matching profile, allowing a plaintext duplicate to override a TLS profile. | Require TLS if any configured profile for that exact host/port requires it; prefer the active profile among equally secure matches. Missing CA fails closed. |
| URL construction | Endpoint fallback builders did not validate null pointers, port range, host authority delimiters, or path controls. | Validate inputs before formatting or looking up profiles. |
| Backup transport settings | Plain and encrypted backups omitted `secure_transport`, so restoring reset it to false. | Export and validate the flag. Legacy backups retain TLS for endpoints already configured securely on this panel. Explicit boolean settings in new backups are honored. |
| Profile replacement | Restore/replacement skipped API-key validation; fixed-size names were not checked for termination. | Apply the same key validation as profile editing and reject unterminated names before persistence. |
| Print requests | Filenames were interpolated into bounded JSON without escaping or checking truncation. | Encode using cJSON and refuse an oversized encoded body before creating an HTTP client. Quotes, backslashes, controls and UTF-8 round-trip correctly. |
| Crypto size arithmetic | Encryption's public API allowed lengths whose header/tag addition overflows 32-bit `size_t`. Normal UI backups are already capped well below this. | Check subtraction-based bounds before allocation; mirror the guard on decryption. |

Authenticated Moonraker HTTP configurations also disable automatic redirects.
Commands and API keys remain on the chosen endpoint. Reverse proxies must expose
the API directly at the configured host/port rather than depend on redirects.
OTA release redirects remain available for GitHub asset downloads.

## Verification

`python3 tools/audit/security_boundaries_test.py` compiles the actual trust store
and transport security controller with fault-injected NVS, and extracts the
actual profile validators, backup serializer/TLS parser, print request and size
guards. It uses real libcJSON, strict compiler warnings and UndefinedBehaviorSanitizer.
It checks malformed imports, NVS open/write/commit failures, secure duplicates,
missing trust, URL boundaries, restore credential validation, TLS backup round
trips in both modes, legacy migration, JSON filename injection/truncation,
HTTP failures/redirect rejection, and simulated 32-bit encryption-size limits.
The HTTP configuration guard covers all five authenticated request sites.
The emergency-stop transport fixture also checks redirect confinement.

All 77 individual host suites and eight existing source/documentation audits
passed on the patched tree. The targeted security suite and backup regression
were rerun after the final transport/legacy-restore changes. Installer validation
covers refusal without writes, repeat/partial application, unrelated UI work
preservation, and rollback after an injected replacement failure.

The original CA, endpoint selection, profile validation, filename encoding and
encryption-size defects each independently fail a runtime regression assertion
when their old implementation is restored. These tests mock SDK storage and
network effects; they do not perform certificate handshakes, flash writes,
physical SD operations, or cryptographic interoperability tests.

## Deployment boundaries and remaining evidence gaps

- HTTP/WS and local HTTP OTA remain supported for the existing trusted-LAN
  workflow. They do not provide transport confidentiality or authenticated
  firmware delivery. Use a configured TLS connection with its intended CA when
  that protection is required.
- CA certificates remain panel-local and are not copied by configuration
  backups. On another panel, or after changing profile slots, select the
  intended CA again. Restored TLS settings without usable trust fail closed.
  A legacy backup on a fresh panel cannot recover a TLS preference it never
  stored; configure connection security before connecting.
- The checked-in configurations do not enable Secure Boot, flash encryption,
  or encrypted NVS. Encrypted SD backups protect their own payload, not live
  credentials in device storage. Hardware security provisioning requires a
  separate deployment decision and is not changed by this patch.
- CA file preflight still checks PEM framing, not the full certificate chain.
  TLS certificate parsing/peer validation remains the SDK's responsibility.
  This patch does not establish concurrent live CA replacement safety across
  in-flight network requests; stop/reconnect the affected connection when
  changing trust.
- The intermittent backup-to-OTA crash/rollback still has no matching panic
  trace. None of these fixes is asserted to explain or resolve that event.
- ESP-IDF target compilation, actual cryptography, TLS handshakes and device
  execution have not been run in this host environment. Host fixtures cannot
  establish actual flash, certificate-handshake or worker timing behavior.

No eFuses, provisioning settings, version, commits, remotes, or release policy
are changed.
