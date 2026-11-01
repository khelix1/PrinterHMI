#include "demo_storage.h"
#include "esp_heap_caps.h"
#include <stdlib.h>
#include <string.h>
/* Bounded PSRAM store: 192 committed keys, eight transactions, 48 changes each.
 * Values share a 128 KiB budget including staged copies. No real NVS access. */
#define KEYS 192
#define HANDLES 8
#define CHANGES 48
#define BUDGET (128u * 1024u)
typedef struct {
  char space[16], key[16];
  unsigned type;
  size_t size;
  void *data;
} entry_t;
typedef struct {
  nvs_handle_t token;
  bool writable;
  char space[16];
  entry_t pending[CHANGES];
} handle_t;
static struct {
  entry_t entries[KEYS];
  handle_t handles[HANDLES];
  size_t bytes;
  nvs_handle_t serial;
} *store;
static void *allocate(size_t size) {
  return heap_caps_calloc(1, size, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
}
bool demo_storage_init(void) {
  if (!store)
    store = allocate(sizeof(*store));
  return store != NULL;
}
static bool name_ok(const char *name) {
  return name && name[0] && strlen(name) < 16;
}
static handle_t *handle(nvs_handle_t token) {
  if (store)
    for (unsigned i = 0; i < HANDLES; i++)
      if (token && store->handles[i].token == token)
        return &store->handles[i];
  return NULL;
}
static entry_t *find(entry_t *entries, unsigned count, const char *space,
                     const char *key) {
  for (unsigned i = 0; i < count; i++)
    if (!strcmp(entries[i].space, space) && !strcmp(entries[i].key, key))
      return &entries[i];
  return NULL;
}
static void release(entry_t *entry) {
  store->bytes -= entry->size;
  free(entry->data);
  memset(entry, 0, sizeof(*entry));
}
esp_err_t demo_storage_open(const char *space, nvs_open_mode_t mode,
                            nvs_handle_t *out) {
  if (!store || !out || !name_ok(space) ||
      (mode != NVS_READONLY && mode != NVS_READWRITE))
    return ESP_ERR_INVALID_ARG;
  if (mode == NVS_READONLY) {
    bool exists = false;
    for (unsigned i = 0; i < KEYS; i++)
      if (!strcmp(store->entries[i].space, space)) {
        exists = true;
        break;
      }
    if (!exists)
      return ESP_ERR_NVS_NOT_FOUND;
  }
  for (unsigned i = 0; i < HANDLES; i++)
    if (!store->handles[i].token) {
      handle_t *h = &store->handles[i];
      h->writable = mode == NVS_READWRITE;
      strcpy(h->space, space);
      /* Never recycle a token after close; refuse exhaustion rather than wrap.
       */
      if (store->serial == UINT32_MAX)
        return ESP_ERR_NO_MEM;
      h->token = ++store->serial;
      *out = h->token;
      return ESP_OK;
    }
  return ESP_ERR_NO_MEM;
}
void demo_storage_close(nvs_handle_t token) {
  handle_t *h = handle(token);
  if (!h)
    return;
  for (unsigned i = 0; i < CHANGES; i++) {
    release(&h->pending[i]);
  }
  memset(h, 0, sizeof(*h));
}
esp_err_t demo_storage_read(nvs_handle_t token, const char *key, unsigned type,
                            void *out, size_t *size) {
  handle_t *h = handle(token);
  if (!h)
    return ESP_ERR_NVS_INVALID_HANDLE;
  if (!name_ok(key) || !size)
    return ESP_ERR_INVALID_ARG;
  entry_t *e = find(h->pending, CHANGES, h->space, key);
  if (!e)
    e = find(store->entries, KEYS, h->space, key);
  if (!e || !e->type)
    return ESP_ERR_NVS_NOT_FOUND;
  if (e->type != type)
    return ESP_ERR_NVS_TYPE_MISMATCH;
  if (!out) {
    *size = e->size;
    return ESP_OK;
  }
  if (*size < e->size) {
    *size = e->size;
    return ESP_ERR_NVS_INVALID_LENGTH;
  }
  if (e->size) {
    memcpy(out, e->data, e->size);
  }
  *size = e->size;
  return ESP_OK;
}
esp_err_t demo_storage_write(nvs_handle_t token, const char *key, unsigned type,
                             const void *data, size_t size) {
  handle_t *h = handle(token);
  if (!h)
    return ESP_ERR_NVS_INVALID_HANDLE;
  if (!h->writable)
    return ESP_ERR_NVS_READ_ONLY;
  if (!name_ok(key) || size > 8192 || (!data && size))
    return ESP_ERR_INVALID_ARG;
  if (!type) {
    size_t ignored = 0;
    esp_err_t err =
        demo_storage_read(token, key, DEMO_VALUE_BLOB, NULL, &ignored);
    if (err == ESP_ERR_NVS_NOT_FOUND)
      return err;
  }
  entry_t *e = find(h->pending, CHANGES, h->space, key);
  if (!e)
    for (unsigned i = 0; i < CHANGES; i++)
      if (!h->pending[i].key[0]) {
        e = &h->pending[i];
        break;
      }
  if (!e)
    return ESP_ERR_NO_MEM;
  if (size > BUDGET - store->bytes)
    return ESP_ERR_NO_MEM;
  void *copy = size ? allocate(size) : NULL;
  if (size && !copy)
    return ESP_ERR_NO_MEM;
  if (size) {
    memcpy(copy, data, size);
  }
  release(e);
  strcpy(e->space, h->space);
  strcpy(e->key, key);
  e->type = type;
  e->data = copy;
  e->size = size;
  store->bytes += size;
  return ESP_OK;
}
esp_err_t demo_storage_commit(nvs_handle_t token) {
  handle_t *h = handle(token);
  if (!h)
    return ESP_ERR_NVS_INVALID_HANDLE;
  if (!h->writable)
    return ESP_ERR_NVS_READ_ONLY;
  unsigned free_slots = 0, added = 0, removed = 0;
  for (unsigned i = 0; i < KEYS; i++)
    if (!store->entries[i].key[0])
      ++free_slots;
  for (unsigned i = 0; i < CHANGES; i++) {
    entry_t *e = &h->pending[i];
    if (!e->key[0])
      continue;
    entry_t *old = find(store->entries, KEYS, h->space, e->key);
    if (e->type && !old)
      ++added;
    else if (!e->type && old)
      ++removed;
  }
  if (added > free_slots + removed)
    return ESP_ERR_NO_MEM;
  /* Delete first, then transfer ownership; after preflight nothing can fail. */
  for (unsigned i = 0; i < CHANGES; i++) {
    entry_t *e = &h->pending[i];
    if (e->key[0]) {
      entry_t *old = find(store->entries, KEYS, h->space, e->key);
      if (old)
        release(old);
    }
  }
  for (unsigned i = 0; i < CHANGES; i++) {
    entry_t *e = &h->pending[i];
    if (e->key[0] && e->type) {
      for (unsigned j = 0; j < KEYS; j++)
        if (!store->entries[j].key[0]) {
          store->entries[j] = *e;
          memset(e, 0, sizeof(*e));
          break;
        }
    } else
      release(e);
  }
  return ESP_OK;
}
