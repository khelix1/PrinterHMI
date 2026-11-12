#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* Preflight recursive JSON parsers before they see untrusted input.
 * Braces in quoted strings do not contribute to nesting. Syntax validation
 * and rejection of trailing data remain the parser's responsibility. */
static inline bool json_document_within_limits(const char *document, size_t size,
                                               size_t max_size, unsigned max_depth)
{
    if (!document || !size || size > max_size || memchr(document, '\0', size)) return false;
    unsigned depth = 0;
    bool quoted = false, escaped = false;
    for (size_t i = 0; i < size; ++i) {
        unsigned char ch = (unsigned char)document[i];
        if (quoted) {
            if (escaped) escaped = false;
            else if (ch == '\\') {
                /* cJSON exposes strings as C strings: an encoded NUL must not
                 * turn a longer key/identifier into a trusted shorter one. */
                if (size - i >= 6 && document[i + 1] == 'u' &&
                    !memcmp(document + i + 2, "0000", 4)) return false;
                escaped = true;
            }
            else if (ch == '"') quoted = false;
        } else if (ch == '"') quoted = true;
        else if (ch == '{' || ch == '[') {
            if (++depth > max_depth) return false;
        } else if (ch == '}' || ch == ']') {
            if (!depth) return false;
            --depth;
        }
    }
    return !quoted && !depth;
}
