#!/usr/bin/env bash
set -euo pipefail

# PrinterHMI's optional local Moonraker TLS endpoint.  This script runs on the
# printer host, not on the HMI.  It validates Moonraker before changing nginx.

usage() {
    cat <<'EOF'
Usage: sudo ./install_moonraker_tls_nginx.sh --host HOST \
       [--upstream HOST:PORT] [--listen-port PORT] [--export-ca PATH] [--replace]

--host       LAN hostname or IPv4 address that the HMI will use.
--upstream   Moonraker endpoint; if omitted, a local 7125–7128 endpoint is
             discovered and verified before nginx is changed.
--listen-port  HTTPS port for this Moonraker instance (default: 443).
--export-ca  Copy the public CA PEM to this path after successful verification.
--replace    Replace an existing PrinterHMI TLS nginx site after backing it up.
EOF
}

host=""
upstream=""
export_ca=""
replace=0
listen_port=443
while (($#)); do
    case "$1" in
        --host) host="${2:-}"; shift 2 ;;
        --upstream) upstream="${2:-}"; shift 2 ;;
        --listen-port) listen_port="${2:-}"; shift 2 ;;
        --export-ca) export_ca="${2:-}"; shift 2 ;;
        --replace) replace=1; shift ;;
        -h|--help) usage; exit 0 ;;
        *) usage >&2; exit 2 ;;
    esac
done

die() { echo "ERROR: $*" >&2; exit 1; }
[[ $EUID -eq 0 ]] || die "run with sudo"
[[ "$host" =~ ^[A-Za-z0-9._-]+$ ]] || die "--host must be a hostname or IPv4 address"
[[ -z "$upstream" || "$upstream" =~ ^[A-Za-z0-9._-]+:[0-9]+$ ]] || die "invalid --upstream"
[[ "$listen_port" =~ ^[0-9]+$ && "$listen_port" -ge 1 && "$listen_port" -le 65535 ]] || die "invalid --listen-port"
for command in nginx openssl curl systemctl; do
    command -v "$command" >/dev/null 2>&1 || die "required command not found: $command"
done

verify_upstream() {
    curl --fail --silent --show-error --max-time 8 \
        "http://$1/server/info" >/dev/null
}

if [[ -n "$upstream" ]]; then
    verify_upstream "$upstream" || die "Moonraker did not answer at http://$upstream/server/info"
else
    for candidate in 127.0.0.1:7125 127.0.0.1:7126 127.0.0.1:7127 127.0.0.1:7128; do
        if verify_upstream "$candidate"; then
            upstream="$candidate"
            break
        fi
    done
    [[ -n "$upstream" ]] || die "no local Moonraker endpoint answered on 7125–7128; supply --upstream"
fi
echo "PASS: verified Moonraker upstream http://$upstream/server/info"

site=/etc/nginx/sites-available/printerhmi-moonraker-tls-$listen_port
link=/etc/nginx/sites-enabled/printerhmi-moonraker-tls-$listen_port
tls=/etc/nginx/printerhmi-tls
stamp="$(date +%Y%m%d%H%M%S)"
backup_dir="/var/backups/printerhmi-moonraker-tls-$stamp"
had_site=0
had_link=0
old_link=""
rollback=0

if [[ -e "$site" ]]; then
    (( replace )) || die "$site exists; review it and rerun with --replace"
    had_site=1
fi
if [[ -L "$link" || -e "$link" ]]; then
    had_link=1
    old_link="$(readlink "$link" 2>/dev/null || true)"
fi

rollback_nginx() {
    (( rollback )) || return 0
    echo "WARNING: TLS validation failed; restoring the prior nginx site" >&2
    if (( had_site )); then
        install -d -m 700 "$backup_dir"
        cp -a "$backup_dir/site" "$site" 2>/dev/null || true
    else
        rm -f "$site"
    fi
    if (( had_link )); then
        if [[ -n "$old_link" ]]; then ln -sfn "$old_link" "$link"; fi
    else
        rm -f "$link"
    fi
    nginx -t >/dev/null 2>&1 && systemctl reload nginx || true
}
trap rollback_nginx ERR

if (( had_site )); then
    install -d -m 700 "$backup_dir"
    cp -a "$site" "$backup_dir/site"
fi

install -d -m 700 "$tls"
# A partial key/certificate set is never reused.  Preserve it for recovery,
# then generate a complete matching CA and leaf certificate pair.
if [[ ! -s "$tls/ca.key" || ! -s "$tls/ca.crt" || ! -s "$tls/moonraker.key" || ! -s "$tls/moonraker.crt" ]]; then
    if compgen -G "$tls/*" >/dev/null; then
        install -d -m 700 "$backup_dir/incomplete-tls"
        cp -a "$tls/." "$backup_dir/incomplete-tls/"
    fi
    rm -f "$tls/ca.key" "$tls/ca.crt" "$tls/ca.srl" "$tls/moonraker.key" "$tls/moonraker.crt" "$tls/moonraker.csr" "$tls/moonraker.ext"
    openssl genrsa -out "$tls/ca.key" 4096
    openssl req -x509 -new -nodes -key "$tls/ca.key" -sha256 -days 3650 \
        -subj '/CN=PrinterHMI Local Moonraker CA' -out "$tls/ca.crt"
    openssl genrsa -out "$tls/moonraker.key" 2048
    openssl req -new -key "$tls/moonraker.key" -subj "/CN=$host" -out "$tls/moonraker.csr"
    if [[ "$host" =~ ^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$ ]]; then san="IP:$host"; else san="DNS:$host"; fi
    printf 'basicConstraints=CA:FALSE\nkeyUsage=digitalSignature,keyEncipherment\nextendedKeyUsage=serverAuth\nsubjectAltName=%s\n' "$san" > "$tls/moonraker.ext"
    openssl x509 -req -in "$tls/moonraker.csr" -CA "$tls/ca.crt" -CAkey "$tls/ca.key" -CAcreateserial \
        -out "$tls/moonraker.crt" -days 1825 -sha256 -extfile "$tls/moonraker.ext"
    rm -f "$tls/moonraker.csr" "$tls/moonraker.ext"
fi
chmod 600 "$tls"/*.key
chmod 644 "$tls"/*.crt

cat > "$site" <<EOF
server {
    listen $listen_port ssl;
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
        proxy_set_header Connection "upgrade";
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
rollback=1
nginx -t
systemctl reload nginx
curl --fail --silent --show-error --max-time 10 --cacert "$tls/ca.crt" \
    "https://$host:$listen_port/server/info" >/dev/null
rollback=0

if [[ -n "$export_ca" ]]; then
    install -m 644 "$tls/ca.crt" "$export_ca"
    echo "Public CA exported: $export_ca"
fi
echo "PASS: TLS endpoint https://$host:$listen_port is ready"
echo "Public CA: $tls/ca.crt"

install -m 700 "$(dirname "$0")/renew_moonraker_tls_nginx.sh" /usr/local/sbin/renew_moonraker_tls_nginx.sh
printf 'HOST=%q\nUPSTREAM=%q\n' "$host" "$upstream" > /etc/printerhmi-moonraker-tls.conf
chmod 600 /etc/printerhmi-moonraker-tls.conf
install -m 644 "$(dirname "$0")/printerhmi-moonraker-tls-renew.service" /etc/systemd/system/
install -m 644 "$(dirname "$0")/printerhmi-moonraker-tls-renew.timer" /etc/systemd/system/
systemctl daemon-reload
systemctl enable --now printerhmi-moonraker-tls-renew.timer
echo "PASS: weekly TLS renewal timer enabled"
