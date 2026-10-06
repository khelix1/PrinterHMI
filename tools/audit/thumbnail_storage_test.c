#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <utime.h>

static int fail_rename;
static int fat_rename;
static int fail_install;
static int fail_write;
static int fail_close;
static int test_rename(const char *a, const char *b)
{
    if (fail_rename) { errno = EIO; return -1; }
    if (fat_rename && access(b, F_OK) == 0) { errno = EEXIST; return -1; }
    if (fail_install && strstr(a, ".tmp")) { fail_install = 0; errno = EIO; return -1; }
    return rename(a, b);
}
static size_t test_fwrite(const void *p, size_t s, size_t n, FILE *f)
{
    return fwrite(p, s, fail_write ? n / 2 : n, f);
}
static int test_fclose(FILE *f)
{
    int result = fclose(f);
    return fail_close ? EOF : result;
}
#define rename test_rename
#define fwrite test_fwrite
#define fclose test_fclose
#include "thumbnail_cache_io.c"
#undef rename
#undef fwrite
#undef fclose

static void path_for(char *out, size_t size, const char *dir, unsigned index)
{
    int n = snprintf(out, size, "%s/%03u.png", dir, index);
    assert(n > 0 && (size_t)n < size);
}
static void inventory(const char *directory, size_t *count, size_t *bytes)
{
    *count = *bytes = 0;
    DIR *dir = opendir(directory); assert(dir);
    struct dirent *entry;
    while ((entry = readdir(dir))) {
        size_t n = strlen(entry->d_name);
        if (n < 5 || strcmp(entry->d_name + n - 4, ".png")) continue;
        char path[384];
        snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
        struct stat st; assert(stat(path, &st) == 0);
        if (S_ISREG(st.st_mode)) { ++*count; *bytes += (size_t)st.st_size; }
    }
    closedir(dir);
}
int main(void)
{
    assert(mkdir(THUMBNAIL_CACHE_ROOT, 0700) == 0);
    const char *thumbs = THUMBNAIL_CACHE_ROOT "/thumbs";
    const char *small = THUMBNAIL_CACHE_ROOT "/thumbs32";
    assert(mkdir(thumbs, 0700) == 0); assert(mkdir(small, 0700) == 0);
    assert(mkdir(THUMBNAIL_CACHE_ROOT "/profile_previews", 0700) == 0);
    unsigned char *data = malloc(THUMBNAIL_CACHE_MAX_PNG_BYTES + 1); assert(data);
    memset(data, 0x42, THUMBNAIL_CACHE_MAX_PNG_BYTES + 1);
    char path[384]; path_for(path, sizeof(path), thumbs, 0);
    assert(thumbnail_cache_write(path, data, 16));
    for (unsigned failure = 0; failure < 3; ++failure) {
        fail_write = failure == 0; fail_close = failure == 1; fail_rename = failure == 2;
        assert(!thumbnail_cache_write(path, data, 32));
        struct stat st; assert(stat(path, &st) == 0 && st.st_size == 16);
        char temporary[400]; snprintf(temporary, sizeof(temporary), "%s.tmp", path);
        assert(access(temporary, F_OK) != 0);
    }
    fail_write = fail_close = fail_rename = 0;
    assert(!thumbnail_cache_write(path, data, THUMBNAIL_CACHE_MAX_PNG_BYTES + 1));
    assert(thumbnail_cache_write(path, data, 32)); /* replace existing cache */
    fat_rename = 1;
    assert(thumbnail_cache_write(path, data, 40));
    fail_install = 1;
    assert(!thumbnail_cache_write(path, data, 48));
    struct stat existing; assert(stat(path, &existing) == 0 && existing.st_size == 40);
    assert(thumbnail_cache_write(path, data, 48));
    const char *profile = THUMBNAIL_CACHE_ROOT "/profile_previews/profile_0.png";
    assert(thumbnail_cache_write(profile, data, 32));
    FILE *note = fopen(THUMBNAIL_CACHE_ROOT "/thumbs/keep.txt", "wb"); assert(note); fclose(note);
    /* Deliberately old mtime ensures FIFO eviction is observable. */
    struct utimbuf old = { 1, 1 }; assert(utime(path, &old) == 0);
    for (unsigned i = 1; i <= 70; ++i) {
        path_for(path, sizeof(path), thumbs, i);
        assert(thumbnail_cache_write(path, data, 16));
    }
    size_t count, bytes; inventory(thumbs, &count, &bytes);
    assert(count == THUMBNAIL_CACHE_MAX_FILES);
    path_for(path, sizeof(path), thumbs, 0); assert(access(path, F_OK) != 0);
    path_for(path, sizeof(path), thumbs, 70); assert(access(path, F_OK) == 0);
    assert(access(profile, F_OK) == 0);
    assert(access(THUMBNAIL_CACHE_ROOT "/thumbs/keep.txt", F_OK) == 0);
    for (unsigned i = 0; i < 40; ++i) {
        path_for(path, sizeof(path), small, i);
        assert(thumbnail_cache_write(path, data, THUMBNAIL_CACHE_MAX_PNG_BYTES));
    }
    inventory(small, &count, &bytes);
    assert(count == 32 && bytes == THUMBNAIL_CACHE_MAX_BYTES);
    path_for(path, sizeof(path), small, 39); assert(access(path, F_OK) == 0);
    /* Nested directories and other stores are outside the retention scope. */
    assert(mkdir(THUMBNAIL_CACHE_ROOT "/thumbs/nested", 0700) == 0);
    char directory[128];
    assert(!retention_directory(THUMBNAIL_CACHE_ROOT "/thumbs/nested/a.png", directory, sizeof(directory)));
    assert(!retention_directory(profile, directory, sizeof(directory)));
    free(data);
    puts("PASS: file/byte retention, newest-write preservation, scoped eviction and failed-write cleanup");
}
