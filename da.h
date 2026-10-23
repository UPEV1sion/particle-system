//
// Created by escha on 28.03.26.
//

#ifndef DA_H_
#define DA_H_

#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <ctype.h>

#define SV_FMT "%.*s"
#define SV_ARG(sv) (int) (sv).len, (sv).s

typedef struct {
    size_t count;
    size_t capacity;
    char *items;
} StringBuilder;

typedef struct {
    const char *s;
    size_t len;
} StringView;

#define DA_GROW_SIZE 2
#define DA_INIT_CAP 64

#define da_reserve(da, expected) \
    do { \
        if((expected) > (da)->capacity) { \
            if((da)->capacity == 0) (da)->capacity = DA_INIT_CAP; \
            while((expected) > (da)->capacity) (da)->capacity *= DA_GROW_SIZE; \
            (da)->items = realloc((da)->items, (da)->capacity * sizeof(*(da)->items)); \
            assert((da)->items && "ERROR: out of mem!\n"); \
        } \
    } while(0)

#define da_append(da, item) \
    do { \
        da_reserve((da), (da)->count + 1); \
        (da)->items[(da)->count++] = (item); \
    } while(0)

#define da_append_many(da, item, sz) \
    do { \
        da_reserve((da), (da)->count + sz); \
        memcpy((da)->items + (da)->count, (item), (sz) * sizeof(*(da)->items)); \
        (da)->count += sz; \
     } while(0) 

#define sb_append(sb, s) \
    do { \
        const size_t len = strlen(s); \
        da_reserve((sb), (sb)->count + len); \
        memcpy((sb)->items + (sb)->count, (s), len); \
        (sb)->count += len; \
    } while(0)

#define da_foreach(type, name, da) for(type *name = (da)->items; name < (da)->items + (da)->count; ++name)

#define da_sort(da, cmp) qsort((da)->items, (da)->count, sizeof(*(da)->items), cmp)

#define da_remove_unordered(da, idx) \
    do { \
        assertmsg(idx < (da)->count, "ERROR: index out of bound!\n"); \
        (da)->items[idx] = (da)->items[--(da)->count];  \
    } while(0)

#define da_free(da) \
    do { \
        free((da)->items); \
    } while(0)

#ifndef DA_DEF
#define DA_DEF
#endif // DA_DEF

DA_DEF StringView sv_from_sb(const StringBuilder *sb);
DA_DEF StringView sv_chop(StringView *sv, int delim);
DA_DEF bool sv_equal(StringView sv1, StringView sv2);
DA_DEF int sv_cmp(StringView sv1, StringView sv2);
DA_DEF bool sv_is_prefix(StringView sv, const char *prefix);
DA_DEF StringView sv_trim(StringView sv);
DA_DEF char* cstr_from_sv(StringView sv);
DA_DEF StringView sv_from_cstr(const char *s);
DA_DEF int read_file(StringBuilder *sb, const char *path);

#ifdef DA_IMPLEMENTATION

DA_DEF StringView sv_from_sb(const StringBuilder *sb)
{
    return (StringView) {
        .s = sb->items,
        .len = sb->count
    };
}

DA_DEF StringView sv_chop(StringView *sv, const int delim)
{
    if(sv->len == 0) return (StringView){NULL, 0};

    size_t i;
    for(i = 0; i < sv->len; ++i) {
        if((unsigned char)sv->s[i] == (unsigned char)delim)
            break;
    }

    const StringView ret = {sv->s, i};

    if(i < sv->len)
    {
        sv->s += i + 1;
        sv->len -= i + 1;
    }
    else
    {
        sv->s += i;
        sv->len = 0;
    }

    return ret;
}

DA_DEF bool sv_equal(const StringView sv1, const StringView sv2)
{
    return sv_cmp(sv1, sv2) == 0;
}

DA_DEF int sv_cmp(const StringView sv1, const StringView sv2)
{
    const size_t min_len = sv1.len < sv2.len ? sv1.len : sv2.len;

    const int cmp = memcmp(sv1.s, sv2.s, min_len);
    if (cmp != 0) return cmp;

    if (sv1.len < sv2.len) return -1;
    if (sv1.len > sv2.len) return 1;
    return 0;
}

DA_DEF bool sv_is_prefix(const StringView sv, const char *prefix)
{
    const size_t len = strlen(prefix);
    if(sv.len < len) return false;

    for(size_t i = 0; i < len; ++i)
    {
        if(sv.s[i] != prefix[i]) return false;
    }

    return true;
}

DA_DEF StringView sv_trim(StringView sv)
{
    while(sv.len > 0 && isspace((unsigned char) *sv.s))
    {
        sv.s++;
        sv.len--;
    }

    while(sv.len > 0 && isspace((unsigned char) sv.s[sv.len - 1]))
    {
        sv.len--;
    }

    return sv;
}

DA_DEF char* cstr_from_sv(const StringView sv)
{
    char *str;
    assert((str = malloc(sv.len + 1)) != NULL && "ERROR: Could not allocate cstr\n");
    memcpy(str, sv.s, sv.len);
    str[sv.len] = 0;
    return str;
}

DA_DEF StringView sv_from_cstr(const char *s)
{
    if (!s) return (StringView) {0};
    return (StringView) {
        .s = s,
        .len = strlen(s)
    };
}

#endif // DA_IMPLEMENTATION

#endif // DA_H_
