#!/usr/bin/env bash
set -euo pipefail

usage() { echo "Usage: sudo $0 --host HOST [--upstream HOST:PORT] [--export-ca PATH] [--replace]"; }
host=""; upstream="127.0.0.1:7125"; export_ca=""; replace=0
while (($#)); do
  case "$1" in
    --host) host="${2:-}"; shift 2;;
    --upstream) upstream="${2:-}"; shift 2;;
    --export-ca) export_ca="${2:-}"; shift 2;;
    --replace) replace=1; shift;;
    -h|--help) usage; exit 0;;
    *) usage; exit 2;;
  esac
done
[[ $EUID -eq 0 ]] || { echo "ERROR: run with sudo" >&2; exit 1; }
[[ "$host" =~ ^[A-Za-z0-9._-]+$ ]] || { echo "ERROR: --host must be a hostname or IPv4 address" >&2; exit 1; }
[[ "$upstream" =~ ^[A-Za-z0-9._-]+:[0-9]+$ ]] || { echo "ERROR: invalid --upstream" >&2; exit 1; }
command -v nginx >/dev/null || { echo "ERROR: nginx is required" >&2; exit 1; }
command -v openssl >/dev/null || { echo "ERROR: openssl is required" >&2; exit 1; }
systemctl is-active --quiet moonraker || echo "WARNING: moonraker service not active; validation may fail"

site=/etc/nginx/sites-available/printerhmi-moonraker-tls
link=/etc/nginx/sites-enabled/printerhmi-moonraker-tls
tls=/etc/nginx/printerhmi-tls
if [[ -e $site && $replace -ne 1 ]]; then echo "ERROR: $site exists; use --replace after review" >&2; exit 1; fi
if [[ -e $site ]]; then cp -a "$site" "$site.backup.$(date +%Y%m%d%H%M%S)"; fi
install -d -m 700 "$tls"
if [[ ! -s $tls/ca.key ]]; then
  openssl genrsa -out "$tls/ca.key" 4096
  openssl req -x509 -new -nodes -key "$tls/ca.key" -sha256 -days 3650 -subj '/CN=PrinterHMI Local Moonraker CA' -out "$tls/ca.crt"
fi
if [[ ! -s $tls/moonraker.key ]]; then
  openssl genrsa -out "$tls/moonraker.key" 2048
  openssl req -new -key "$tls/moonraker.key" -subj "/CN=$host" -out "$tls/moonraker.csr"
  if [[ $host =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then san="IP:$host"; else san="DNS:$host"; fi
  printf 'basicConstraints=CA:FALSE\nkeyUsage=digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName=%s\n' "$san" > "$tls/moonraker.ext"
  openssl x509 -req -in "$tls/moonraker.csr" -CA "$tls/ca.crt" -CAkey "$tls/ca.key" -CAcreateserial -out "$tls/moonraker.crt" -days 1825 -sha256 -extfile "$tls/moonraker.ext"
  rm -f "$tls/moonraker.csr" "$tls/moonraker.ext"
fi
chmod 600 "$tls"/*.key; chmod 644 "$tls"/*.crt
cat > "$site" <<EOF
server {
    listen 443 ssl;
    server_name $host;
    ssl_certificate $tls/moonraker.crt;
    ssl_certificate_key $tls/moonraker.key;
    ssl_protocols TLSv1.2 TLSv1.3;
    client_max_body_size 0;
    proxy_request_buffering off;
    location /websocket {
        proxy_pass http://$upstream/websocket;
        proxy_http_version 1.1;
        proxy_set_header Upgrade \$http_upgrade;
        proxy_set_header Connection \$connection_upgrade;
        proxy_set_header Host \$http_host;
        proxy_read_timeout 86400;
    }
    location ~ ^/(printer|api|access|machine|server)/ {
        proxy_pass http://$upstream\$request_uri;
        proxy_http_version 1.1;
        proxy_set_header Upgrade \$http_upgrade;
        proxy_set_header Host \$http_host;
        proxy_set_header X-Scheme \$scheme;
    }
    location / { return 404; }
}
EOF
ln -sfn "$site" "$link"
nginx -t
systemctl reload nginx
if command -v curl >/dev/null; then curl --fail --silent --show-error --cacert "$tls/ca.crt" "https://$host/server/info" >/dev/null; fi
if [[ -n $export_ca ]]; then install -m 644 "$tls/ca.crt" "$export_ca"; fi
echo "PASS: TLS endpoint https://$host is ready"
echo "Public CA: $tls/ca.crt"
