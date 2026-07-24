# Known issues

This list describes current v4.0.0 risks. Resolve release blockers before
calling a build production-ready.

## Release blockers

### Embedded development credentials

Development Wi-Fi credentials are present in tracked source and therefore in
the initial Git history and private remote. They must be rotated, removed from
source, replaced with a provisioning mechanism, and purged from reachable Git
history. A later deletion commit is insufficient.

### Unencrypted local transports

Moonraker HTTP and WebSocket connections currently use unencrypted local
transport. OTA also permits HTTP. Operate only on a trusted network until
authenticated TLS and a firmware trust policy are implemented.

### Development fallback endpoints

The firmware contains local development defaults for Moonraker and OTA,
including an OTA filename from an earlier version. Release builds must not
silently contact a developer-specific endpoint.

### Project license not selected

Component licenses exist, but PrinterHMI itself has no selected project-level
license. The private repository currently grants no redistribution permission.

## Repository hygiene

- Numerous mechanical `.bak_*` source copies are currently tracked.
- Accidental root artifacts and an old cleanup script are currently tracked.
- The dependency lockfile contains an absolute workstation path for the local
  board component.
- Several source filenames retain `_v32` names although the product is v4.0.0.

These items do not necessarily alter the compiled image, but they undermine
review quality, portability and release packaging.

## Runtime and validation

- Brief intermittent splash-frame flashing has been observed during startup.
- Automated host tests and continuous integration are not yet established.
- The build globally suppresses format warnings with `-Wno-format`; this can
  conceal defects and should be removed after warning cleanup.
- OTA integrity relies on the configured server and network path; a complete
  production image-signing policy is not documented or enforced here.
- Factory reset leaves SD-card cache data intact.

## Documentation maintenance

Historical v3.x records intentionally describe obsolete layouts and ownership.
Only documents outside `docs/history/` describe current behavior.
