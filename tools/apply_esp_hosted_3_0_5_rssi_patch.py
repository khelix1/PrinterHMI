#!/usr/bin/env python3
from pathlib import Path
import subprocess
import sys

root = Path(__file__).resolve().parents[1]
component = root / "managed_components/espressif__esp_hosted"
patch = root / "tools/patches/esp-hosted-3.0.5-wifi-rssi.patch"

checks = {
    component / "host/features/eh_host_feat_wifi/include/eh_host_feat_wifi_v2_req_ids.inc": [
        "case RPC_ID__Req_WifiStaGetRssi:",
    ],
    component / "host/features/eh_host_feat_wifi/src/eh_host_feat_wifi_v2_req.c": [
        "COMPOSE_REQ_EMPTY(compose_req_wifi_sta_get_rssi,",
        "case RPC_ID__Req_WifiStaGetRssi:   return compose_req_wifi_sta_get_rssi;",
    ],
    component / "host/features/eh_host_feat_wifi/include/eh_host_feat_wifi_v2_resp_ids.inc": [
        "case RPC_ID__Resp_WifiStaGetRssi:",
    ],
    component / "host/features/eh_host_feat_wifi/src/eh_host_feat_wifi_v2_resp_evt.c": [
        "case RPC_ID__Resp_WifiStaGetRssi:",
        "rpc->resp_wifi_sta_get_rssi->rssi;",
    ],
}

states = []

for path, markers in checks.items():
    if not path.is_file():
        raise SystemExit(f"ERROR: ESP-Hosted 3.0.5 file is missing: {path}")

    text = path.read_text()
    states.extend(marker in text for marker in markers)

if all(states):
    print("PASS: ESP-Hosted 3.0.5 RSSI RPC correction already applied")
    raise SystemExit(0)

if any(states):
    raise SystemExit(
        "ERROR: ESP-Hosted RSSI correction is only partially applied"
    )

check = subprocess.run(
    [
        "git",
        "apply",
        "--check",
        f"--directory={component.relative_to(root)}",
        str(patch.relative_to(root)),
    ],
    cwd=root,
)

if check.returncode != 0:
    raise SystemExit("ERROR: ESP-Hosted 3.0.5 RSSI patch no longer applies")

subprocess.run(
    [
        "git",
        "apply",
        f"--directory={component.relative_to(root)}",
        str(patch.relative_to(root)),
    ],
    cwd=root,
    check=True,
)

for path, markers in checks.items():
    text = path.read_text()
    for marker in markers:
        if marker not in text:
            raise SystemExit(
                f"ERROR: patch verification failed for {path}: {marker}"
            )

print("PASS: applied ESP-Hosted 3.0.5 RSSI RPC correction")
