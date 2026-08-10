# Secure Moonraker Setup

This guide enables the optional, per-profile HTTPS/WSS connection in
PrinterHMI. It is for trusted-LAN Moonraker installations that want encrypted
panel-to-printer traffic. It does not replace Moonraker authentication:
an API key remains optional and works in Standard and Secure modes.

## What you need

- A printer host running Moonraker and Nginx (Debian/Armbian-based systems are
  supported by the supplied installer).
- Shell access with `sudo` on that printer host.
- The printer's stable LAN IPv4 address or hostname.
- The panel's SD card, to carry the exported public CA PEM.

The HMI never receives a private key. Keep `/etc/nginx/printerhmi-tls/*.key`
on the printer host and do not copy it to the SD card.

## Install on the printer host

Copy the repository installer to the printer host, then run it there. Replace
`192.168.1.50` with the address used by PrinterHMI:

```bash
sudo bash ./install_moonraker_tls_nginx.sh   --host 192.168.1.50   --export-ca /tmp/moonraker-local-ca.pem
```

The installer first verifies a local Moonraker endpoint on ports 7125 through
7128. It then creates a local CA and a certificate whose subject alternative
name matches `--host`, writes an Nginx TLS proxy on port 443, reloads Nginx,
and verifies `https://HOST/server/info` using that CA. It exports only the CA
at the requested path after verification succeeds.

For a Moonraker instance on a nonstandard local endpoint, specify it directly:

```bash
sudo bash ./install_moonraker_tls_nginx.sh   --host printer.local --upstream 127.0.0.1:7126   --export-ca /tmp/moonraker-local-ca.pem
```

If an older or incomplete PrinterHMI TLS site exists, inspect it and rerun with
`--replace`. The installer backs up the old site, preserves incomplete TLS
material under `/var/backups/`, and restores the prior Nginx site automatically
if the newly configured HTTPS endpoint cannot be verified.

## Import on PrinterHMI

1. Copy the exported CA to the SD-card root. Any descriptive `.pem` filename
   is fine, for example `sunlu-s9-ca.pem`.
2. On the panel, open **Settings → Printer Profiles → Edit** for that printer.
3. Under **Moonraker Port**, open the security selector and choose **SELECT
   .PEM**. The picker shows only `.pem` files in the SD-card root.
4. Select that printer's CA. Secure HTTPS/WSS is selected and the port becomes
   **443** automatically.
5. Save the profile, then confirm it reconnects and reports live state.

Each profile stores its own CA. Three printers can therefore use separate
certificates, even when two Moonraker instances share one host. Repeating the
same CA for several profiles is also supported.

## Verify and recover

On the host, this verifies the exact public endpoint:

```bash
sudo curl --fail --cacert /etc/nginx/printerhmi-tls/ca.crt   https://YOUR_HOST/server/info
```

If the panel reports offline, verify the host/IP used by the profile matches
the certificate SAN, that port 443 is reachable, and that the selected PEM is
the CA exported by that host. Choose **Standard HTTP** to intentionally return
that profile to its previous unencrypted port and behavior; a failed secure
connection never does this on its own.
