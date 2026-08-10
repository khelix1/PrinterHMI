# Secure Moonraker Setup Guide

## What this provides

Secure Connection is an optional per-printer PrinterHMI setting. It encrypts
Moonraker API and WebSocket traffic with HTTPS/WSS and verifies the printer's
certificate. API-key authentication remains optional and unchanged.

Standard HTTP remains the default. Secure profiles never fall back to HTTP.

## Prerequisites

* A Debian/Armbian Klipper host running Moonraker and Nginx.
* Shell access with `sudo` on that host.
* PrinterHMI firmware that includes Secure Connection support.
* An SD card for transferring the public CA certificate to PrinterHMI.

## 1. Add a local TLS endpoint

The repository includes a guarded Nginx installer:

```bash
sudo bash tools/install_moonraker_tls_nginx.sh --host PRINTER_HOST --export-ca /tmp/moonraker-local-ca.pem
```

Keep the existing port-80 Mainsail site intact. Add a separate Nginx server on
port 443 that proxies Moonraker's `/websocket` and
`/(printer|api|access|machine|server)/` routes to the existing Moonraker
upstream. Use TLS 1.2 or newer and a certificate whose subject alternative name
matches the hostname or IP address configured in the PrinterHMI profile.

For a trusted LAN without public DNS, create a local CA, sign the Nginx server
certificate with it, and retain the CA private key only on the printer host.
Never copy a private key to PrinterHMI or the SD card.

Validate and reload safely:

```bash
sudo nginx -t && sudo systemctl reload nginx
```

## 2. Verify from the printer host

Use the public CA certificate to verify the new endpoint before changing
PrinterHMI:

```bash
curl --cacert /path/to/local-ca.pem https://PRINTER_HOST/server/info
```

The command must succeed. A browser warning or `curl -k` is not validation.

## 3. Export only the public CA

Copy the CA certificate—not its private key—to the PrinterHMI SD card. Use a
clear filename such as `moonraker-local-ca.pem` in the card root. The PEM must
contain `BEGIN CERTIFICATE` and `END CERTIFICATE` markers.

## 4. Configure PrinterHMI

1. Open **Settings → Printers → Edit** for the target printer.
2. Keep the host/IP and TLS port used by Nginx (normally 443).
3. Set **Connection security** to **Secure (HTTPS/WSS)**.
4. Select the CA PEM from the SD card, review its fingerprint, and explicitly
   approve it.
5. Use **Test**, then save the profile.

The API-key field is independent: leave it empty for a trusted Moonraker
client, or supply the same API key used with Standard HTTP.

## Verification and rollback

Verify HTTPS API, WSS live data, file operations, G-code dispatch, printer
switching, restart, OTA and a power cycle. A certificate error must leave the
profile disconnected rather than using HTTP.

To roll back, edit only that profile and select **Standard (HTTP)**. This does
not remove the Nginx TLS endpoint or affect other profiles.

## Troubleshooting

* **Certificate rejected:** the configured host/IP must match a certificate
  subject alternative name; import the issuing CA, not the server private key.
* **Connection refused:** confirm Nginx listens on the selected TLS port and
  local firewall rules permit it.
* **WebSocket fails but Test works:** verify the `/websocket` proxy includes
  Upgrade and Connection headers.
* **API key failure:** TLS is working; correct the Moonraker API key or leave
  it empty only when the Moonraker access policy allows trusted clients.
