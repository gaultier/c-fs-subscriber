#pragma once

#include <_types/_uint64_t.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef MIN
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#endif

#ifndef MAX
#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#endif

// Check that __COUNTER__ is defined and that __COUNTER__ increases by 1
// every time it is expanded. X + 1 == X + 0 is used in case X is defined to be
// empty. If X is empty the expression becomes (+1 == +0).
#if defined(__COUNTER__) && (__COUNTER__ + 1 == __COUNTER__ + 0)
#define PG_PRIVATE_UNIQUE_ID __COUNTER__
#else
#define PG_PRIVATE_UNIQUE_ID __LINE__
#endif

// Helpers for generating unique variable names
#define PG_PRIVATE_NAME(n) PG_PRIVATE_CONCAT(n, PG_PRIVATE_UNIQUE_ID)
#define PG_PRIVATE_CONCAT(a, b) PG_PRIVATE_CONCAT2(a, b)
#define PG_PRIVATE_CONCAT2(a, b) a##b
#define PG_PAD(n) uint8_t PG_PRIVATE_NAME(_padding)[n]

typedef struct {
  char *data;
  uint64_t len;
} pg_span_t;

typedef struct {
  char *data;
  uint32_t len;
  PG_PAD(4);
} pg_span32_t;

typedef struct pg_allocator_t pg_allocator_t;
struct pg_allocator_t {
  //  void *backing_memory;
  //  uint64_t backing_memory_cap;
  void *(*realloc)(void *old_memory, uint64_t new_size, uint64_t old_size);
  void (*free)(void *memory);
};

void *pg_heap_realloc(void *old_memory, uint64_t new_size, uint64_t old_size) {
  void *res = realloc(old_memory, new_size);
  memset((uint8_t *)res + old_size, 0, new_size - old_size);
  return res;
}

void pg_heap_free(void *memory) { free(memory); }

pg_allocator_t pg_heap_allocator() {
  return (pg_allocator_t){.realloc = pg_heap_realloc, .free = pg_heap_free};
}

void *pg_stack_realloc(void *old_memory, uint64_t new_size, uint64_t old_size) {
  (void)old_memory;
  (void)old_size;

  void *res = alloca(new_size);
  memset(res, 0, new_size);
  return res;
}

void pg_stack_free(void *memory) {
  (void)memory;  // no-op
}

pg_allocator_t pg_stack_allocator() {
  return (pg_allocator_t){.realloc = pg_stack_realloc, .free = pg_stack_free};
}

void *pg_null_realloc(void *old_memory, uint64_t new_size, uint64_t old_size) {
  (void)new_size;
  (void)old_memory;
  (void)old_size;
  __builtin_unreachable();
}

void pg_null_free(void *memory) {
  (void)memory;
  __builtin_unreachable();
}

pg_allocator_t pg_null_allocator() {
  return (pg_allocator_t){.realloc = pg_null_realloc, .free = pg_null_free};
}

// -------------------------- Pool

bool pg_is_power_of_two(uint64_t x) { return (x & (x - 1)) == 0; }

uint64_t pg_align_forward(uint64_t ptr, uint64_t align) {
  uint64_t p = 0, a = 0, modulo = 0;

  assert(pg_is_power_of_two(align));

  p = ptr;
  a = (uintptr_t)align;
  // Same as (p % a) but faster as 'a' is a power of two
  modulo = p & (a - 1);

  if (modulo != 0) {
    // If 'p' address is not aligned, push the address to the
    // next value which is aligned
    p += a - modulo;
  }
  return p;
}

#define PG_DEFAULT_ALIGNEMENT 16

typedef struct pg_pool_free_node_t {
  struct pg_pool_free_node_t *next;
} pg_pool_free_node_t;

typedef struct {
  uint8_t *buf;
  uint64_t buf_len;
  uint64_t chunk_size;
  pg_pool_free_node_t *head;
} pg_pool_t;

void pg_pool_free_all(pg_pool_t *pool) {
  for (uint64_t i = 0; i < pool->buf_len / pool->chunk_size; i++) {
    void *ptr = &pool->buf[i * pool->chunk_size];
    pg_pool_free_node_t *node = (pg_pool_free_node_t *)ptr;
    node->next = pool->head;
    pool->head = node;
  }
}

void *pg_pool_alloc(pg_pool_t *pool) {
  pg_pool_free_node_t *node = pool->head;
  if (node == NULL) return NULL;  // No more space

  pool->head = pool->head->next;

  return memset(node, 0, pool->chunk_size);
}

void pg_pool_free(pg_pool_t *pool, void *ptr) {
  assert(ptr != NULL);
  assert(ptr >= (void *)pool->buf);
  assert((uint8_t *)ptr <
         (uint8_t *)pool->buf + pool->buf_len - sizeof(pg_pool_free_node_t *));

  pg_pool_free_node_t *node = (pg_pool_free_node_t *)ptr;
  node->next = pool->head;
  pool->head = node;
}

void pg_pool_init(pg_pool_t *pool, uint64_t chunk_size,
                  uint64_t max_items_count) {
  // TODO: allow using existing chunk of mem
  // TODO: alignement

  chunk_size = pg_align_forward(chunk_size, PG_DEFAULT_ALIGNEMENT);
  assert(chunk_size >= sizeof(pg_pool_free_node_t));

  pool->chunk_size = chunk_size;
  pool->buf_len = max_items_count * chunk_size;
  pool->buf = calloc(max_items_count, chunk_size);
  assert(pool->buf);
  pool->head = NULL;

  pg_pool_free_all(pool);
}

void pg_pool_destroy(pg_pool_t *pool) { free(pool->buf); }

// --------------------------- Array

typedef struct pg_array_header_t {
  uint64_t len;
  uint64_t capacity;
  pg_allocator_t allocator;
} pg_array_header_t;

#define pg_array_t(Type) Type *

#ifndef PG_ARRAY_GROW_FORMULA
#define PG_ARRAY_GROW_FORMULA(x) (1.5 * (x) + 8)
#endif

#define PG_ARRAY_HEADER(x) ((pg_array_header_t *)(x)-1)
#define pg_array_len(x) (PG_ARRAY_HEADER(x)->len)
#define pg_array_capacity(x) (PG_ARRAY_HEADER(x)->capacity)
#define pg_array_available_space(x) (pg_array_capacity(x) - pg_array_len(x))

#define pg_array_init_reserve(x, cap, my_allocator)                          \
  do {                                                                       \
    void **pg__array_ = (void **)&(x);                                       \
    pg_array_header_t *pg__ah =                                              \
        (pg_array_header_t *)(my_allocator)                                  \
            .realloc(NULL, sizeof(pg_array_header_t) + sizeof(*(x)) * (cap), \
                     0);                                                     \
    pg__ah->len = 0;                                                         \
    pg__ah->capacity = cap;                                                  \
    pg__ah->allocator = my_allocator;                                        \
    *pg__array_ = (void *)(pg__ah + 1);                                      \
  } while (0)

#define pg_array_init(x, my_allocator) pg_array_init_reserve(x, 0, my_allocator)

#define pg_array_free(x)                            \
  do {                                              \
    pg_array_header_t *pg__ah = PG_ARRAY_HEADER(x); \
    pg__ah->allocator.free(pg__ah);                 \
    x = NULL;                                       \
  } while (0)

#define pg_array_grow(x, min_capacity)                                         \
  do {                                                                         \
    uint64_t new_capacity = PG_ARRAY_GROW_FORMULA(pg_array_capacity(x));       \
    if (new_capacity < (min_capacity)) new_capacity = (min_capacity);          \
    const uint64_t old_size =                                                  \
        sizeof(pg_array_header_t) + pg_array_capacity(x) * sizeof(*x);         \
    const uint64_t new_size =                                                  \
        sizeof(pg_array_header_t) + new_capacity * sizeof(*x);                 \
    pg_array_header_t *pg__new_header = PG_ARRAY_HEADER(x)->allocator.realloc( \
        PG_ARRAY_HEADER(x), new_size, old_size);                               \
    pg__new_header->capacity = new_capacity;                                   \
    x = (void *)(pg__new_header + 1);                                          \
  } while (0)

#define pg_array_append(x, item)                                         \
  do {                                                                   \
    if (pg_array_capacity(x) < pg_array_len(x) + 1) pg_array_grow(x, 0); \
    (x)[pg_array_len(x)++] = (item);                                     \
  } while (0)

#define pg_array_pop(x)                  \
  do {                                   \
    assert(PG_ARRAY_HEADER(x)->len > 0); \
    PG_ARRAY_HEADER(x)->len--;           \
  } while (0)
#define pg_array_clear(x)        \
  do {                           \
    PG_ARRAY_HEADER(x)->len = 0; \
  } while (0)

#define pg_array_resize(x, new_count)                         \
  do {                                                        \
    if (PG_ARRAY_HEADER(x)->capacity < (uint64_t)(new_count)) \
      pg_array_grow(x, (uint64_t)(new_count));                \
    PG_ARRAY_HEADER(x)->len = (uint64_t)(new_count);          \
  } while (0)

char pg_char_to_lower(char c) {
  if (c >= 'A' && c <= 'Z') return 'a' + (c - 'A');
  return c;
}

bool pg_char_is_space(char c) {
  if (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v')
    return true;
  return false;
}

bool pg_char_is_digit(char c) {
  if (c >= '0' && c <= '9') return true;
  return false;
}

bool pg_char_is_alpha(char c) {
  if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) return true;
  return false;
}

bool pg_char_is_alphanumeric(char c) {
  return pg_char_is_alpha(c) || pg_char_is_digit(c);
}

bool pg_str_has_prefix(char *haystack0, char *needle0) {
  uint64_t haystack0_len = strlen(haystack0);
  uint64_t needle0_len = strlen(needle0);
  if (needle0_len > haystack0_len) return false;
  return memcmp(haystack0, needle0, needle0_len) == 0;
}

// ------------------ Strings

typedef char *pg_string_t;

// NOTE(bill): If you only need a small string, just use a standard c
// string or change the size from uint64_t to u16, etc.
typedef struct pg_string_header_t {
  pg_allocator_t allocator;
  uint64_t length;
  uint64_t capacity;
} pg_string_header_t;

#define PG_STRING_HEADER(str) ((pg_string_header_t *)(str)-1)

void pg__set_string_length(pg_string_t str, uint64_t len) {
  PG_STRING_HEADER(str)->length = len;
}

void pg__set_string_capacity(pg_string_t str, uint64_t cap) {
  PG_STRING_HEADER(str)->capacity = cap;
}

pg_string_t pg_string_make_reserve(pg_allocator_t a, uint64_t capacity) {
  uint64_t header_size = sizeof(pg_string_header_t);
  void *ptr = a.realloc(NULL, header_size + capacity + 1, 0);

  pg_string_t str;
  pg_string_header_t *header;

  if (ptr == NULL) return NULL;
  memset(ptr, 0, header_size + capacity + 1);

  str = (char *)ptr + header_size;
  header = PG_STRING_HEADER(str);
  header->allocator = a;
  header->length = 0;
  header->capacity = capacity;
  str[capacity] = '\0';

  return str;
}

pg_string_t pg_string_make_length(pg_allocator_t a, void const *init_str,
                                  uint64_t num_bytes) {
  uint64_t header_size = sizeof(pg_string_header_t);
  void *ptr = a.realloc(NULL, header_size + num_bytes + 1, 0);

  pg_string_t str;
  pg_string_header_t *header;

  if (ptr == NULL) return NULL;
  if (!init_str) memset(ptr, 0, header_size + num_bytes + 1);

  str = (char *)ptr + header_size;
  header = PG_STRING_HEADER(str);
  header->allocator = a;
  header->length = num_bytes;
  header->capacity = num_bytes;
  if (num_bytes && init_str) {
    memcpy(str, init_str, num_bytes);
  }
  str[num_bytes] = '\0';

  return str;
}

pg_string_t pg_string_make(pg_allocator_t a, char const *str) {
  uint64_t len = str ? strlen(str) : 0;
  return pg_string_make_length(a, str, len);
}

void pg_string_free(pg_string_t str) {
  if (str) {
    pg_string_header_t *header = PG_STRING_HEADER(str);
    header->allocator.free(header);
  }
}

void pg_string_free_ptr(pg_string_t *str) { pg_string_free(*str); }

uint64_t pg_string_length(pg_string_t const str) {
  return PG_STRING_HEADER(str)->length;
}

pg_string_t pg_string_duplicate(pg_allocator_t a, pg_string_t const str) {
  return pg_string_make_length(a, str, pg_string_length(str));
}

uint64_t pg_string_capacity(pg_string_t const str) {
  return PG_STRING_HEADER(str)->capacity;
}

uint64_t pg_string_available_space(pg_string_t const str) {
  pg_string_header_t *h = PG_STRING_HEADER(str);
  if (h->capacity > h->length) {
    return h->capacity - h->length;
  }
  return 0;
}

void pg_string_clear(pg_string_t str) {
  pg__set_string_length(str, 0);
  str[0] = '\0';
}

pg_string_t pg_string_make_space_for(pg_string_t str, int64_t add_len) {
  int64_t available = pg_string_available_space(str);

  // NOTE(bill): Return if there is enough space left
  if (available >= add_len) {
    return str;
  } else {
    int64_t new_len, old_size, new_size;
    void *ptr, *new_ptr;
    pg_allocator_t a = PG_STRING_HEADER(str)->allocator;
    pg_string_header_t *header;

    new_len = pg_string_length(str) + add_len;
    ptr = PG_STRING_HEADER(str);
    old_size = sizeof(pg_string_header_t) + pg_string_length(str) + 1;
    new_size = sizeof(pg_string_header_t) + new_len + 1;

    new_ptr = PG_STRING_HEADER(str)->allocator.realloc(ptr, new_size, old_size);
    if (new_ptr == NULL) return NULL;

    header = (pg_string_header_t *)new_ptr;
    header->allocator = a;

    str = (pg_string_t)(header + 1);
    pg__set_string_capacity(str, new_len);

    return str;
  }
}
pg_string_t pg_string_append_length(pg_string_t str, void const *other,
                                    uint64_t other_len) {
  if (other_len > 0) {
    uint64_t curr_len = pg_string_length(str);

    str = pg_string_make_space_for(str, other_len);
    if (str == NULL) {
      return NULL;
    }

    memcpy(str + curr_len, other, other_len);
    str[curr_len + other_len] = '\0';
    pg__set_string_length(str, curr_len + other_len);
  }
  return str;
}

pg_string_t pg_string_append(pg_string_t str, pg_string_t const other) {
  return pg_string_append_length(str, other, pg_string_length(other));
}

pg_string_t pg_string_appendc(pg_string_t str, char const *other) {
  return pg_string_append_length(str, other, strlen(other));
}

pg_string_t pg_span_url_encode(pg_allocator_t allocator, pg_span_t src);

pg_string_t pg_string_url_encode(pg_allocator_t allocator, pg_string_t src) {
  pg_span_t span = {.data = src, .len = pg_string_length(src)};
  return pg_span_url_encode(allocator, span);
}

// ---------------- Hashtable

// FNV-1a
uint32_t pg_hash(uint8_t *n, uint64_t len) {
  uint32_t hash = 2166136261u;
  for (uint64_t i = 0; i < len; i++) {
    hash ^= (uint8_t)n[i];
    hash *= 16777619;
  }
  return hash;
}
// ------------------ Span

char pg_span_peek_left(pg_span_t span, bool *found) {
  if (span.len > 0) {
    if (found != NULL) *found = true;
    return span.data[0];
  } else {
    if (found != NULL) *found = false;
    return 0;
  }
}

char pg_span_peek_right(pg_span_t span, bool *found) {
  if (span.len > 0) {
    if (found != NULL) *found = true;
    return span.data[span.len - 1];
  } else {
    if (found != NULL) *found = false;
    return 0;
  }
}

void pg_span_consume_left(pg_span_t *span, uint64_t n) {
  assert(span != NULL);

  if (span->len == 0) return;

  assert(span->data != NULL);
  assert(span->len >= n);

  span->data += n;
  span->len -= n;
}

void pg_span_consume_right(pg_span_t *span, uint64_t n) {
  assert(span != NULL);

  if (span->len == 0) return;

  assert(span->data != NULL);
  assert(span->len >= n);

  span->len -= n;
}

bool pg_span_split_at_first(pg_span_t span, char needle, pg_span_t *left,
                            pg_span_t *right) {
  *left = (pg_span_t){0};
  *right = (pg_span_t){0};

  for (uint64_t i = 0; i < span.len; i++) {
    if (span.data[i] == needle) {
      left->data = span.data;
      left->len = i;
      right->data = span.data + i;
      right->len = span.len - i;
      assert(right->data[0] == needle);

      return true;
    }
  }

  *left = span;
  return false;
}

bool pg_span_split_at_last(pg_span_t span, char needle, pg_span_t *left,
                           pg_span_t *right) {
  *left = (pg_span_t){0};
  *right = (pg_span_t){0};
  for (int64_t i = span.len - 1; i >= 0; i--) {
    if (span.data[i] == needle) {
      left->data = span.data;
      left->len = i;

      right->data = &span.data[i];
      right->len = span.len - i;
      assert(right->data[0] == needle);

      return true;
    }
  }
  *left = span;
  return false;
}

bool pg_span_skip_left_until_inclusive(pg_span_t *span, char needle) {
  pg_span_t left = {0}, right = {0};
  if (!pg_span_split_at_first(*span, needle, &left, &right)) {
    return false;
  }

  *span = right;
  pg_span_consume_left(span, 1);
  return true;
}

void pg_span_trim_left(pg_span_t *span) {
  bool more_chars = false;
  char c = 0;
  while (true) {
    c = pg_span_peek_left(*span, &more_chars);
    if (!more_chars) return;
    if (pg_char_is_space(c))
      pg_span_consume_left(span, 1);
    else
      return;
  }
}

void pg_span_trim_right(pg_span_t *span) {
  bool more_chars = false;
  char c = 0;
  while (true) {
    c = pg_span_peek_right(*span, &more_chars);
    if (!more_chars) return;
    if (pg_char_is_space(c))
      pg_span_consume_right(span, 1);
    else
      return;
  }
}

void pg_span_trim(pg_span_t *span) {
  pg_span_trim_left(span);
  pg_span_trim_right(span);
}

bool pg_span_contains(pg_span_t haystack, pg_span_t needle) {
  if (needle.len > haystack.len) return false;
  return memmem(haystack.data, haystack.len, needle.data, needle.len) != NULL;
}

bool pg_span_ends_with(pg_span_t haystack, pg_span_t needle) {
  if (needle.len > haystack.len) return false;
  return memmem(haystack.data + haystack.len - needle.len, needle.len,
                needle.data, needle.len) != NULL;
}

pg_string_t pg_span_url_encode(pg_allocator_t allocator, pg_span_t src) {
  pg_string_t res = pg_string_make_reserve(allocator, 3 * src.len);

  for (uint64_t i = 0; i < src.len; i++) {
    char buf[4] = {0};
    const uint64_t len =
        snprintf(buf, sizeof(buf), "%%%02X", (uint8_t)src.data[i]);
    res = pg_string_append_length(res, buf, len);
  }

  return res;
}

pg_span_t pg_span_make(pg_string_t s) {
  return (pg_span_t){.data = s, .len = pg_string_length(s)};
}

pg_span_t pg_span_make_c(char *s) {
  return (pg_span_t){.data = s, .len = strlen(s)};
}

bool pg_span_starts_with(pg_span_t haystack, pg_span_t needle) {
  if (needle.len > haystack.len) return false;
  return memmem(haystack.data, needle.len, needle.data, needle.len) != NULL;
}

bool pg_span_eq(pg_span_t a, pg_span_t b) {
  return a.len == b.len && memcmp(a.data, b.data, a.len) == 0;
}

uint64_t pg_span_parse_u64_hex(pg_span_t span, bool *valid) {
  pg_span_trim(&span);

  uint64_t res = 0;
  uint64_t sign = 1;
  if (pg_span_peek_left(span, NULL) == '-') {
    sign = -1;
    pg_span_consume_left(&span, 1);
  } else if (pg_span_peek_left(span, NULL) == '+') {
    pg_span_consume_left(&span, 1);
  }

  if (pg_span_starts_with(span, pg_span_make_c("0x")))
    pg_span_consume_left(&span, 2);

  for (uint64_t i = 0; i < span.len; i++) {
    if (!pg_char_is_alphanumeric(span.data[i])) {
      *valid = false;
      return 0;
    }

    res *= 16;
    uint64_t n = 0;
    char c = pg_char_to_lower(span.data[i]);

    if (c == 'a')
      n = 10;
    else if (c == 'b')
      n = 11;
    else if (c == 'c')
      n = 12;
    else if (c == 'd')
      n = 13;
    else if (c == 'e')
      n = 14;
    else if (c == 'f')
      n = 15;
    else if (pg_char_is_digit(c))
      n = c - '0';
    else {
      *valid = false;
      return 0;
    }
    res += n;
  }

  *valid = true;
  return sign * res;
}

uint64_t pg_span_parse_u64_decimal(pg_span_t span, bool *valid) {
  pg_span_trim(&span);

  uint64_t res = 0;
  uint64_t sign = 1;

  if (pg_span_peek_left(span, NULL) == '-') {
    sign = -1;
    pg_span_consume_left(&span, 1);
  } else if (pg_span_peek_left(span, NULL) == '+') {
    pg_span_consume_left(&span, 1);
  }

  for (uint64_t i = 0; i < span.len; i++) {
    if (!pg_char_is_digit(span.data[i])) {
      *valid = false;
      return 0;
    }

    res *= 10;
    res += span.data[i] - '0';
  }
  *valid = true;
  return sign * res;
}

// ------------- Span u32

char pg_span32_peek(pg_span32_t span) {
  if (span.len > 0)
    return span.data[0];
  else
    return 0;
}

void pg_span32_consume_left(pg_span32_t *span, uint32_t n) {
  assert(span != NULL);

  if (span->len == 0) return;

  assert(span->data != NULL);
  assert(span->len >= n);

  span->data += n;
  span->len -= n;
}

void pg_span32_consume_right(pg_span32_t *span, uint32_t n) {
  assert(span != NULL);

  if (span->len == 0) return;

  assert(span->data != NULL);
  assert(span->len >= n);

  span->len -= n;
}

bool pg_span32_split_left(pg_span32_t span, char needle, pg_span32_t *left,
                          pg_span32_t *right) {
  char *end = memchr(span.data, needle, span.len);
  *left = (pg_span32_t){0};
  *right = (pg_span32_t){0};

  if (end == NULL) {
    *left = span;
    return false;
  }

  left->data = span.data;
  left->len = end - span.data;

  if ((uint32_t)(end - span.data) < span.len - 1) {
    right->data = end;
    right->len = span.len - left->len;
    assert(right->data[0] == needle);
  }
  return true;
}

pg_string_t pg_span32_url_encode(pg_allocator_t allocator, pg_span32_t src) {
  pg_string_t res = pg_string_make_reserve(allocator, 3 * src.len);

  for (uint32_t i = 0; i < src.len; i++) {
    char buf[4] = {0};
    const uint64_t len =
        snprintf(buf, sizeof(buf), "%%%02X", (uint8_t)src.data[i]);
    res = pg_string_append_length(res, buf, len);
  }

  return res;
}

pg_span32_t pg_span32_make(pg_string_t s) {
  return (pg_span32_t){.data = s, .len = pg_string_length(s)};
}

pg_span32_t pg_span32_make_c(char *s) {
  return (pg_span32_t){.data = s, .len = strlen(s)};
}

bool pg_span32_starts_with(pg_span32_t haystack, pg_span32_t needle) {
  if (needle.len > haystack.len) return false;
  return memmem(haystack.data, haystack.len, needle.data, needle.len) == 0;
}

bool pg_span32_eq(pg_span32_t a, pg_span32_t b) {
  return a.len == b.len && memcmp(a.data, b.data, a.len) == 0;
}

uint64_t pg_span32_parse_u64(pg_span32_t span) {
  uint64_t res = 0;

  for (uint64_t i = 0; i < span.len; i++) {
    assert(pg_char_is_digit(span.data[i]));

    res *= 10;
    res += span.data[i] - '0';
  }
  return res;
}

// ------------- File utils

int64_t pg_read_file_fd(pg_allocator_t allocator, int fd,
                        pg_array_t(uint8_t) * buf) {
  struct stat st = {0};
  if (fstat(fd, &st) == -1) {
    return errno;
  }
  const uint64_t read_buffer_size =
      MIN((uint64_t)UINT32_MAX, (uint64_t)st.st_size);
  pg_array_init_reserve(*buf, st.st_size, allocator);
  while (pg_array_len(*buf) < (uint64_t)st.st_size) {
    int64_t ret = read(fd, *buf + pg_array_len(*buf), read_buffer_size);
    if (ret == -1) {
      return errno;
    }
    if (ret == 0) return 0;
    pg_array_resize(*buf, pg_array_len(*buf) + ret);
  }
  return 0;
}

int64_t pg_read_file(pg_allocator_t allocator, char *path,
                     pg_array_t(uint8_t) * buf) {
  int fd = open(path, O_RDONLY);
  if (fd == -1) {
    return errno;
  }
  int ret = pg_read_file_fd(allocator, fd, buf);
  close(fd);
  return ret;
}

// -------------------------- Log

typedef enum {
  PG_LOG_DEBUG,
  PG_LOG_INFO,
  PG_LOG_ERROR,
  PG_LOG_FATAL,
} pg_log_level_t;

typedef struct {
  pg_log_level_t level;
} pg_logger_t;

#define pg_log_debug(logger, fmt, ...)                            \
  do {                                                            \
    if ((logger) != NULL && (logger)->level <= PG_LOG_DEBUG)      \
      fprintf(stderr, "%s[DEBUG] " fmt "%s\n",                    \
              (isatty(2) ? "\x1b[38:5:240m" : ""), ##__VA_ARGS__, \
              (isatty(2) ? "\x1b[0m" : ""));                      \
  } while (0)

#define pg_log_info(logger, fmt, ...)                                        \
  do {                                                                       \
    if ((logger) != NULL && (logger)->level <= PG_LOG_INFO)                  \
      fprintf(stderr, "%s[INFO] " fmt "%s\n", (isatty(2) ? "\x1b[32m" : ""), \
              ##__VA_ARGS__, (isatty(2) ? "\x1b[0m" : ""));                  \
  } while (0)

#define pg_log_error(logger, fmt, ...)                                        \
  do {                                                                        \
    if ((logger) != NULL && (logger)->level <= PG_LOG_ERROR)                  \
      fprintf(stderr, "%s[ERROR] " fmt "%s\n", (isatty(2) ? "\x1b[31m" : ""), \
              ##__VA_ARGS__, (isatty(2) ? "\x1b[0m" : ""));                   \
  } while (0)

#define pg_log_fatal(logger, exit_code, fmt, ...)                 \
  do {                                                            \
    if ((logger) != NULL && (logger)->level <= PG_LOG_FATAL) {    \
      fprintf(stderr, "%s[FATAL] " fmt "%s\n",                    \
              (isatty(2) ? "\x1b[38:5:124m" : ""), ##__VA_ARGS__, \
              (isatty(2) ? "\x1b[0m" : ""));                      \
      exit(exit_code);                                            \
    }                                                             \
  } while (0)

// -------------------------- Ring buffer of bytes

typedef struct {
  uint8_t *data;
  uint64_t len, offset, cap;
  pg_allocator_t allocator;
} pg_ring_t;

void pg_ring_init(pg_allocator_t allocator, pg_ring_t *ring, uint64_t cap) {
  assert(cap > 0);

  ring->len = ring->offset = 0;
  ring->data = allocator.realloc(NULL, cap, 0);
  ring->cap = cap;
  ring->allocator = allocator;
}

uint64_t pg_ring_len(pg_ring_t *ring) { return ring->len; }

uint64_t pg_ring_cap(pg_ring_t *ring) { return ring->cap; }

void pg_ring_destroy(pg_ring_t *ring) { ring->allocator.free(ring->data); }

uint8_t *pg_ring_get_ptr(pg_ring_t *ring, uint64_t i) {
  if (ring->cap == 0) return NULL;

  assert(i < ring->cap);
  const uint64_t index = (i + ring->offset) % ring->cap;
  return &ring->data[index];
}

uint8_t pg_ring_get(pg_ring_t *ring, uint64_t i) {
  return *pg_ring_get_ptr(ring, i);
}

uint8_t *pg_ring_front_ptr(pg_ring_t *ring) {
  assert(ring->offset < ring->cap);
  return &ring->data[ring->offset];
}

uint8_t pg_ring_front(pg_ring_t *ring) { return *pg_ring_front_ptr(ring); }

uint8_t *pg_ring_back_ptr(pg_ring_t *ring) {
  if (ring->cap == 0) return NULL;

  const uint64_t index = (ring->offset + ring->len - 1) % ring->cap;
  return &ring->data[index];
}

uint8_t pg_ring_back(pg_ring_t *ring) { return *pg_ring_back_ptr(ring); }

void pg_ring_push_back(pg_ring_t *ring, uint8_t x) {
  assert(ring->len < ring->cap);

  const uint64_t index = (ring->offset + ring->len) % ring->cap;
  assert(index < ring->cap);
  ring->data[index] = x;
  ring->len += 1;
}

void pg_ring_push_front(pg_ring_t *ring, uint8_t x) {
  assert(ring->len < ring->cap);

  ring->offset = (ring->offset - 1 + ring->cap) % ring->cap;
  assert(ring->offset < ring->cap);
  ring->data[ring->offset] = x;
  ring->len += 1;
}

uint8_t pg_ring_pop_back(pg_ring_t *ring) {
  assert(ring->len > 0);
  ring->len -= 1;
  const uint64_t index = (ring->offset + ring->len) % ring->cap;
  assert(index < ring->cap);
  return ring->data[index];
}

uint8_t pg_ring_pop_front(pg_ring_t *ring) {
  assert(ring->len > 0);
  assert(ring->offset < ring->cap);
  const uint8_t res = ring->data[ring->offset];

  ring->offset = (ring->offset + 1) % ring->cap;
  ring->len -= 1;

  return res;
}

void pg_ring_consume_front(pg_ring_t *ring, uint64_t n) {
  assert(n <= ring->len);
  ring->offset = (ring->offset + n) % ring->cap;
  ring->len -= n;
}

void pg_ring_consume_back(pg_ring_t *ring, uint64_t n) {
  assert(n <= ring->len);
  ring->len -= n;
}

void pg_ring_clear(pg_ring_t *ring) {
  ring->offset = 0;
  ring->len = 0;
}

uint64_t pg_ring_space(pg_ring_t *ring) { return ring->cap - ring->len; }

void pg_ring_push_backv(pg_ring_t *ring, uint8_t *data, uint64_t len) {
  assert(ring->len + len <= ring->cap);

  const uint64_t index = (ring->offset + ring->len) % ring->cap;

  // Fill the tail
  const uint64_t space_tail = ring->cap - index;
  uint64_t to_write_tail_count = len;
  if (to_write_tail_count > space_tail) to_write_tail_count = space_tail;
  assert(index + to_write_tail_count <= ring->cap);
  memcpy(ring->data + index, data, to_write_tail_count);

  // Fill the head
  const uint64_t remain = len - to_write_tail_count;
  memcpy(ring->data, data + to_write_tail_count, remain);
  ring->len += len;
}

// -------------------------- bitarray

typedef struct {
  pg_array_t(uint8_t) data;
  uint64_t max_index;
} pg_bitarray_t;

void pg_bitarray_init(pg_allocator_t allocator, pg_bitarray_t *bitarr,
                      uint64_t max_index) {
  bitarr->max_index = max_index;
  const uint64_t len = 1 + (uint64_t)ceil(((double)max_index) / 8.0);
  pg_array_init_reserve(bitarr->data, len, allocator);
  pg_array_resize(bitarr->data, len);
}

void pg_bitarray_setv(pg_bitarray_t *bitarr, uint8_t *data, uint64_t len) {
  assert(len <= 1 + bitarr->max_index);

  memcpy(bitarr->data, data, len);
}

void pg_bitarray_destroy(pg_bitarray_t *bitarr) { pg_array_free(bitarr->data); }

uint64_t pg_bitarray_len(const pg_bitarray_t *bitarr) {
  return bitarr->max_index + 1;
}

void pg_bitarray_set(pg_bitarray_t *bitarr, uint64_t index) {
  const uint64_t i = index / 8.0;
  assert(i < pg_array_len(bitarr->data));

  bitarr->data[i] |= 1 << (index % 8);
}

bool pg_bitarray_get(const pg_bitarray_t *bitarr, uint64_t index) {
  const uint64_t i = index / 8.0;
  assert(i < pg_array_len(bitarr->data));

  return bitarr->data[i] & (1 << (index % 8));
}

bool pg_bitarray_next(const pg_bitarray_t *bitarr, uint64_t *index,
                      bool *is_set) {
  if (*index >= pg_bitarray_len(bitarr)) return false;

  *is_set = pg_bitarray_get(bitarr, *index);
  *index += 1;
  return true;
}

void pg_bitarray_unset(pg_bitarray_t *bitarr, uint64_t index) {
  const uint64_t i = index / 8.0;
  assert(i < pg_array_len(bitarr->data));

  bitarr->data[i] &= ~(1 << (index % 8));
}

uint64_t pg_bitarray_count_set(pg_bitarray_t *bitarr) {
  uint64_t res = 0;
  for (uint64_t i = 0; i < pg_bitarray_len(bitarr); i++) {
    res += pg_bitarray_get(bitarr, i);
  }
  return res;
}

uint64_t pg_bitarray_count_unset(pg_bitarray_t *bitarr) {
  uint64_t res = 0;
  for (uint64_t i = 0; i < pg_bitarray_len(bitarr); i++) {
    res += !pg_bitarray_get(bitarr, i);
  }
  return res;
}

bool pg_bitarray_is_all_set(pg_bitarray_t *bitarr) {
  for (uint64_t i = 0; i < pg_bitarray_len(bitarr); i++) {
    if (!pg_bitarray_get(bitarr, i)) return false;
  }
  return true;
}

bool pg_bitarray_is_all_unset(pg_bitarray_t *bitarr) {
  for (uint64_t i = 0; i < pg_bitarray_len(bitarr); i++) {
    if (pg_bitarray_get(bitarr, i)) return false;
  }
  return true;
}

void pg_bitarray_set_all(pg_bitarray_t *bitarr) {
  memset(bitarr->data, 0xff, pg_array_len(bitarr->data));
}

void pg_bitarray_unset_all(pg_bitarray_t *bitarr) {
  memset(bitarr->data, 0, pg_array_len(bitarr->data));
}

void pg_bitarray_resize(pg_bitarray_t *bitarr, uint64_t max_index) {
  bitarr->max_index = max_index;
  const uint64_t len = (uint64_t)ceil(((double)max_index) / 8.0);
  pg_array_resize(bitarr->data, len);
}
