#include "thumbnail_cache_io.h"

#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>

#ifndef THUMBNAIL_CACHE_ROOT
#define THUMBNAIL_CACHE_ROOT "/sdcard/hmi"
#endif

/* Match immediate children only; never prune profile_previews or user files. */
static bool retention_directory(const char *path, char *directory, size_t capacity)
{
    const char *roots[] = {
        THUMBNAIL_CACHE_ROOT "/thumbs/",
        THUMBNAIL_CACHE_ROOT "/thumbs32/"
    };
    for (size_t i = 0; i < sizeof(roots) / sizeof(roots[0]); ++i) {
        size_t length = strlen(roots[i]);
        if (strncmp(path, roots[i], length) != 0) continue;
        const char *name = path + length;
        size_t name_length = strlen(name);
        if (strchr(name, '/') || name_length < 5 ||
            strcmp(name + name_length - 4, ".png") != 0) return false;
        if (length > capacity) return false;
        memcpy(directory, roots[i], length - 1);
        directory[length - 1] = '\0';
        return true;
    }
    return false;
}

static bool prune_directory(const char *directory, const char *keep)
{
    /* Bound work per write when an old installation has a large backlog.
     * Subsequent successful writes continue cleanup; never scan at boot.
     */
    for (unsigned pass = 0; pass < THUMBNAIL_CACHE_MAX_FILES; ++pass) {
        DIR *dir = opendir(directory);
        if (!dir) return false;
        size_t count = 0;
        uint64_t bytes = 0;
        char oldest[384] = "";
        time_t oldest_time = 0;
        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            size_t length = strlen(entry->d_name);
            if (length < 5 || strcmp(entry->d_name + length - 4, ".png") != 0)
                continue;
            char path[384];
            int n = snprintf(path, sizeof(path), "%s/%s", directory, entry->d_name);
            if (n < 0 || (size_t)n >= sizeof(path)) continue;
            struct stat info;
            if (stat(path, &info) != 0 || !S_ISREG(info.st_mode)) continue;
            ++count;
            if (info.st_size > 0) bytes += (uint64_t)info.st_size;
            if (strcmp(path, keep) == 0) continue;
            if (!oldest[0] || info.st_mtime < oldest_time ||
                (info.st_mtime == oldest_time && strcmp(path, oldest) < 0)) {
                memcpy(oldest, path, (size_t)n + 1);
                oldest_time = info.st_mtime;
            }
        }
        closedir(dir);
        if (count <= THUMBNAIL_CACHE_MAX_FILES && bytes <= THUMBNAIL_CACHE_MAX_BYTES)
            return true;
        if (!oldest[0] || remove(oldest) != 0) return false;
    }
    return false;
}

/* FatFS can reject replacement with EEXIST. Move the old cache aside before
 * retrying, and restore it on failure; a cache is disposable after power loss.
 */
static bool replace_cache(const char *temporary, const char *path)
{
    if (rename(temporary, path) == 0) return true;
    if (errno != EEXIST) return false;
    char backup[384];
    int n = snprintf(backup, sizeof(backup), "%s.bak", path);
    if (n < 0 || (size_t)n >= sizeof(backup)) return false;
    struct stat info;
    if (stat(path, &info) != 0 || !S_ISREG(info.st_mode)) return false;
    /* A stale backup belongs to an interrupted preceding cache replacement. */
    if (remove(backup) != 0 && errno != ENOENT) return false;
    if (rename(path, backup) != 0) return false;
    if (rename(temporary, path) != 0) {
        (void)rename(backup, path);
        return false;
    }
    (void)remove(backup);
    return true;
}

bool thumbnail_cache_write(const char *path, const uint8_t *data, size_t size)
{
    if (!path || !path[0] || !data || !size || size > THUMBNAIL_CACHE_MAX_PNG_BYTES)
        return false;
    char temporary[384];
    int n = snprintf(temporary, sizeof(temporary), "%s.tmp", path);
    if (n < 0 || (size_t)n >= sizeof(temporary)) return false;
    FILE *file = fopen(temporary, "wb");
    if (!file) return false;
    size_t written = fwrite(data, 1, size, file);
    int result = fclose(file);
    if (written != size || result != 0 || !replace_cache(temporary, path)) {
        remove(temporary);
        return false;
    }
    char directory[128];
    if (retention_directory(path, directory, sizeof(directory))) {
        /* Retention failure must not reject a successfully downloaded image. */
        (void)prune_directory(directory, path);
    }
    return true;
}
