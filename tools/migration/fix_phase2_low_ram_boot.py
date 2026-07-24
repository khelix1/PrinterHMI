#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[2]
MAIN = ROOT / "main"


def read(name: str) -> str:
    path = MAIN / name
    if not path.exists():
        raise RuntimeError(f"missing required file: {path}")
    return path.read_text()


def replace_once(text: str, old: str, new: str, label: str) -> str:
    count = text.count(old)
    if count != 1:
        raise RuntimeError(f"expected one {label}, found {count}")
    return text.replace(old, new, 1)


health_h = read("printer_profile_health.h")
health_c = read("printer_profile_health.c")
worker = read("printer_profile_preview_worker_v32.c")
cache = read("printer_preview_cache_v32.c")


if "printer_profile_health_take_next_index" not in health_h:
    health_h += '''

/* Shared round-robin cursor for the existing runtime worker. */
int printer_profile_health_take_next_index(void);
'''


if "int printer_profile_health_take_next_index(void)" not in health_c:
    health_c += '''


int printer_profile_health_take_next_index(void)
{
    uint32_t generation = moonraker_config_generation();

    if (generation != s_generation) {
        printer_profile_health_reset();
    }

    int index = s_next_profile;
    s_next_profile =
        (s_next_profile + 1) % MOONRAKER_CONFIG_MAX_PROFILES;

    return index;
}
'''


worker = replace_once(
    worker,
    '''static const char *TAG = "profile_preview_worker";

static int s_next_profile = 0;
static uint32_t s_generation = 0;
''',
    '''#define TAG "profile_preview_worker"
''',
    "preview-worker static state",
)

worker = replace_once(
    worker,
    '''void printer_profile_preview_worker_v32_reset(void)
{
    s_next_profile = 0;
    s_generation = moonraker_config_generation();
}
''',
    '''void printer_profile_preview_worker_v32_reset(void)
{
    /* Cursor and generation are owned by printer_profile_health. */
}
''',
    "preview-worker reset",
)

worker = replace_once(
    worker,
    '''    uint32_t generation_before = moonraker_config_generation();

    if (generation_before != s_generation) {
        printer_profile_preview_worker_v32_reset();
        generation_before = s_generation;
    }

    int index = s_next_profile;
    s_next_profile =
        (s_next_profile + 1) % MOONRAKER_CONFIG_MAX_PROFILES;
''',
    '''    int index = printer_profile_health_take_next_index();
    uint32_t generation_before = moonraker_config_generation();
''',
    "preview-worker round-robin cursor",
)


cache = cache.replace(
    'static const char *TAG = "printer_preview_cache";\n',
    '#define TAG "printer_preview_cache"\n',
    1,
)


for forbidden in (
    "static int s_next_profile",
    "static uint32_t s_generation",
    'static const char *TAG = "profile_preview_worker"',
):
    if forbidden in worker:
        raise RuntimeError(f"preview worker still owns low-RAM state: {forbidden}")


(MAIN / "printer_profile_health.h").write_text(health_h)
(MAIN / "printer_profile_health.c").write_text(health_c)
(MAIN / "printer_profile_preview_worker_v32.c").write_text(worker)
(MAIN / "printer_preview_cache_v32.c").write_text(cache)

print("PASS: Phase 2 low-RAM polling state consolidated")
print("  - existing health module owns the only cursor/generation state")
print("  - preview worker contributes no writable static state")
print("  - FreeRTOS timer stack and DMA reserve remain unchanged")
print("Next: idf.py fullclean && idf.py build")
