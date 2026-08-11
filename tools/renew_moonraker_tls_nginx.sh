#!/usr/bin/env bash
set -euo pipefail

config=/etc/printerhmi-moonraker-tls.conf
[[ $EUID -eq 0 ]] || { echo 'ERROR: run with sudo' >&2; exit 1; }
[[ -r $config ]] || { echo "ERROR: missing $config" >&2; exit 1; }
# shellcheck disable=SC1090
source "$config"
for command in openssl nginx systemctl curl; do command -v "$command" >/dev/null || { echo "ERROR: missing $command" >&2; exit 1; }; done
[[ "$HOST" =~ ^[A-Za-z0-9._-]+$ && "$UPSTREAM" =~ ^[A-Za-z0-9._-]+:[0-9]+$ ]] || { echo 'ERROR: invalid TLS configuration' >&2; exit 1; }
tls=/etc/nginx/printerhmi-tls
openssl x509 -checkend 2592000 -noout -in "$tls/moonraker.crt" && { echo 'PASS: Moonraker TLS certificate is not due for renewal'; exit 0; }
curl --fail --silent --show-error --max-time 8 "http://$UPSTREAM/server/info" >/dev/null
tmp=$(mktemp -d "$tls/.renew.XXXXXX"); trap 'rm -rf "$tmp"' EXIT
openssl genrsa -out "$tmp/moonraker.key" 2048
openssl req -new -key "$tmp/moonraker.key" -subj "/CN=$HOST" -out "$tmp/moonraker.csr"
[[ "$HOST" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]] && san="IP:$HOST" || san="DNS:$HOST"
printf 'basicConstraints=CA:FALSE\nkeyUsage=digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName=%s\n' "$san" > "$tmp/moonraker.ext"
openssl x509 -req -in "$tmp/moonraker.csr" -CA "$tls/ca.crt" -CAkey "$tls/ca.key" -CAcreateserial -out "$tmp/moonraker.crt" -days 1825 -sha256 -extfile "$tmp/moonraker.ext"
install -m 600 "$tmp/moonraker.key" "$tls/moonraker.key.new"; install -m 644 "$tmp/moonraker.crt" "$tls/moonraker.crt.new"
mv "$tls/moonraker.key.new" "$tls/moonraker.key"; mv "$tls/moonraker.crt.new" "$tls/moonraker.crt"
nginx -t && systemctl reload nginx
curl --fail --silent --show-error --max-time 10 --cacert "$tls/ca.crt" "https://$HOST/server/info" >/dev/null
echo 'PASS: Moonraker TLS certificate renewed and verified'
