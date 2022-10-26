#pragma once

#include <_types/_uint64_t.h>
#include <_types/_uint8_t.h>
#include <stdio.h>

#include "../pg/pg.h"

typedef enum {
  BC_KIND_INTEGER,
  BC_KIND_STRING,
  BC_KIND_ARRAY,
  BC_KIND_DICTIONARY,
} bc_kind_t;

const char* bc_value_kind_to_string(int n) {
  switch (n) {
    case BC_KIND_INTEGER:
      return "BC_KIND_INTEGER";
    case BC_KIND_STRING:
      return "BC_KIND_STRING";
    case BC_KIND_ARRAY:
      return "BC_KIND_ARRAY";
    case BC_KIND_DICTIONARY:
      return "BC_KIND_DICTIONARY";
    default:
      __builtin_unreachable();
  }
}

typedef struct bc_value_t bc_value_t;

typedef struct {
  pg_array_t(pg_string_t) keys;
  pg_array_t(bc_value_t) values;
  pg_array_t(uint32_t) hashes;
  pg_allocator_t allocator;
} bc_dictionary_t;

struct bc_value_t {
  bc_kind_t kind;
  union {
    int64_t integer;
    pg_string_t string;
    pg_array_t(bc_value_t) array;
    bc_dictionary_t dictionary;
  } v;
};

void pg_hashtable_init(bc_dictionary_t* hashtable, uint64_t cap,
                       pg_allocator_t allocator) {
  assert(hashtable != NULL);

  hashtable->allocator = allocator;
  pg_array_init_reserve(hashtable->keys, cap, allocator);
  pg_array_init_reserve(hashtable->values, cap, allocator);
  pg_array_init_reserve(hashtable->hashes, cap, allocator);

  assert(hashtable->keys != NULL);
  assert(hashtable->values != NULL);
  assert(hashtable->hashes != NULL);
  assert(pg_array_capacity(hashtable->keys) ==
         pg_array_capacity(hashtable->values));
  assert(pg_array_capacity(hashtable->keys) ==
         pg_array_capacity(hashtable->hashes));
  assert(pg_array_count(hashtable->keys) == pg_array_count(hashtable->values));
  assert(pg_array_count(hashtable->keys) == pg_array_count(hashtable->hashes));
}

bool pg_hashtable_find(bc_dictionary_t* hashtable, pg_string_t key,
                       uint64_t* index) {
  assert(hashtable != NULL);
  assert(hashtable->keys != NULL);
  assert(hashtable->values != NULL);
  assert(hashtable->hashes != NULL);
  assert(pg_array_capacity(hashtable->keys) ==
         pg_array_capacity(hashtable->values));
  assert(pg_array_capacity(hashtable->keys) ==
         pg_array_capacity(hashtable->hashes));
  assert(pg_array_count(hashtable->keys) == pg_array_count(hashtable->values));
  assert(pg_array_count(hashtable->keys) == pg_array_count(hashtable->hashes));

  const uint32_t hash = pg_hash((uint8_t*)key, pg_string_length(key));
  *index = hash % pg_array_capacity(hashtable->keys);

  for (;;) {
    const uint32_t index_hash = hashtable->hashes[*index];
    if (index_hash == 0) break; /* Not found but suitable empty slot */
    if (index_hash == hash &&
        pg_string_length(key) == pg_string_length(hashtable->keys[*index]) &&
        memcmp(key, hashtable->keys[*index], pg_string_length(key)) == 0) {
      /* Found after checking for collision */
      assert(*index < pg_array_capacity(hashtable->keys));
      assert(*index < pg_array_capacity(hashtable->values));
      assert(*index < pg_array_capacity(hashtable->hashes));
      return true;
    }
    /* Keep going to find either an empty slot or a matching hash */
    *index = (*index + 1) % pg_array_capacity(hashtable->keys);
  }
  assert(*index < pg_array_capacity(hashtable->keys));
  assert(*index < pg_array_capacity(hashtable->values));
  assert(*index < pg_array_capacity(hashtable->hashes));
  return false;
}

#define PG_HASHTABLE_LOAD_FACTOR 0.75

void pg_hashtable_upsert(bc_dictionary_t* hashtable, pg_string_t key,
                         bc_value_t* val) {
  assert(hashtable != NULL);
  assert(hashtable->keys != NULL);
  assert(hashtable->values != NULL);
  assert(hashtable->hashes != NULL);
  assert(pg_array_capacity(hashtable->keys) ==
         pg_array_capacity(hashtable->values));
  assert(pg_array_capacity(hashtable->keys) ==
         pg_array_capacity(hashtable->hashes));
  assert(pg_array_count(hashtable->keys) == pg_array_count(hashtable->values));
  assert(pg_array_count(hashtable->keys) == pg_array_count(hashtable->hashes));

  const uint64_t cap = pg_array_capacity(hashtable->keys);
  assert(cap > 0);
  const uint64_t len = pg_array_count(hashtable->keys);
  if ((double)len / cap >= PG_HASHTABLE_LOAD_FACTOR) {
    const uint64_t new_cap = 1.5 * cap;
    pg_array_grow(hashtable->keys, new_cap);
    pg_array_grow(hashtable->values, new_cap);
    pg_array_grow(hashtable->hashes, new_cap);
    assert(pg_array_capacity(hashtable->keys) ==
           pg_array_capacity(hashtable->values));
    assert(pg_array_capacity(hashtable->keys) ==
           pg_array_capacity(hashtable->hashes));
    assert(pg_array_capacity(hashtable->keys) >= new_cap);
    assert(pg_array_capacity(hashtable->values) >= new_cap);
    assert(pg_array_capacity(hashtable->hashes) >= new_cap);
  }
  uint64_t index = -1;
  if (pg_hashtable_find(hashtable, key, &index)) { /* Update */
    hashtable->values[index] = *val;
  } else {
    hashtable->keys[index] = key;
    hashtable->hashes[index] = pg_hash((uint8_t*)key, pg_string_length(key));
    hashtable->values[index] = *val;

    const uint64_t new_len = pg_array_count(hashtable->keys) + 1;
    pg_array_resize(hashtable->keys, new_len);
    pg_array_resize(hashtable->values, new_len);
    pg_array_resize(hashtable->hashes, new_len);
  }
}

void bc_value_destroy(bc_value_t* value);

void pg_hashtable_destroy(bc_dictionary_t* hashtable) {
  assert(hashtable != NULL);
  assert(hashtable->keys != NULL);
  assert(hashtable->values != NULL);
  assert(hashtable->hashes != NULL);
  assert(pg_array_capacity(hashtable->keys) ==
         pg_array_capacity(hashtable->values));
  assert(pg_array_capacity(hashtable->keys) ==
         pg_array_capacity(hashtable->hashes));
  assert(pg_array_count(hashtable->keys) == pg_array_count(hashtable->values));
  assert(pg_array_count(hashtable->keys) == pg_array_count(hashtable->hashes));

  for (uint64_t i = 0; i < pg_array_capacity(hashtable->keys); i++) {
    if (hashtable->hashes[i] != 0) pg_string_free(hashtable->keys[i]);
  }
  pg_array_free(hashtable->keys);

  for (uint64_t i = 0; i < pg_array_capacity(hashtable->values); i++) {
    if (hashtable->hashes[i] != 0) bc_value_destroy(&hashtable->values[i]);
  }
  pg_array_free(hashtable->values);

  pg_array_free(hashtable->hashes);
}

uint64_t pg_hashtable_count(bc_dictionary_t* hashtable) {
  return pg_array_count(hashtable->keys);
}

char bc_peek(pg_string_span_t span) {
  if (span.len > 0)
    return span.data[0];
  else
    return 0;
}

typedef enum {
  BC_PE_NONE,
  BC_PE_EOF,
  BC_PE_UNEXPECTED_CHARACTER,
  BC_PE_INVALID_NUMBER,
  BC_PE_INVALID_STRING_LENGTH,
  BC_PE_DICT_KEY_NOT_STRING,
} bc_parse_error_t;

const char* bc_parse_error_to_string(int e) {
  switch (e) {
    case BC_PE_NONE:
      return "BC_PE_NONE";
    case BC_PE_EOF:
      return "BC_PE_EOF";
    case BC_PE_UNEXPECTED_CHARACTER:
      return "BC_PE_UNEXPECTED_CHARACTER";
    case BC_PE_INVALID_NUMBER:
      return "BC_PE_INVALID_NUMBER";
    case BC_PE_INVALID_STRING_LENGTH:
      return "BC_PE_INVALID_STRING_LENGTH";
    case BC_PE_DICT_KEY_NOT_STRING:
      return "BC_PE_DICT_KEY_NOT_STRING";
    default:
      __builtin_unreachable();
  }
}

bc_parse_error_t bc_consume_char(pg_string_span_t* span, char c) {
  assert(span != NULL);
  assert(span->data != NULL);

  if (span->len == 0) return BC_PE_EOF;
  if (span->data[0] != c) return BC_PE_UNEXPECTED_CHARACTER;
  pg_span_consume(span, 1);

  return BC_PE_NONE;
}

bc_parse_error_t bc_parse_i64(pg_string_span_t* span, int64_t* res) {
  assert(span != NULL);
  assert(span->data != NULL);

  if (span->len == 0) return BC_PE_EOF;

  uint64_t i = 0;
  for (; i < span->len; i++) {
    const char c = span->data[i];
    if ('0' <= c && c <= '9') {
      *res *= 10;
      *res += c - '0';
    } else if (c == '-' && i == 0) {
      continue;
    } else {
      break;
    }
  }
  if (i == 1 && span->data[0] == '-') return BC_PE_INVALID_NUMBER;
  if (i == 0) return BC_PE_INVALID_NUMBER;
  if (span->data[0] == '-') *res *= -1;

  pg_span_consume(span, i);

  return BC_PE_NONE;
}

bc_parse_error_t bc_parse_string(pg_allocator_t allocator,
                                 pg_string_span_t* span, bc_value_t* value) {
  bc_parse_error_t err = BC_PE_NONE;
  int64_t len = 0;
  pg_string_span_t res_span = *span;
  if ((err = bc_parse_i64(&res_span, &len)) != BC_PE_NONE) return err;
  if ((err = bc_consume_char(&res_span, ':')) != BC_PE_NONE) return err;
  if (len <= 0 || (uint64_t)len > res_span.len)
    return BC_PE_INVALID_STRING_LENGTH;

  value->kind = BC_KIND_STRING;
  value->v.string = pg_string_make_length(allocator, res_span.data, len);

  pg_span_consume(&res_span, len);

  *span = res_span;

  return BC_PE_NONE;
}

bc_parse_error_t bc_parse_number(pg_string_span_t* span, bc_value_t* res) {
  bc_parse_error_t err = BC_PE_NONE;
  pg_string_span_t res_span = *span;

  if ((err = bc_consume_char(&res_span, 'i')) != BC_PE_NONE) return err;
  int64_t val = 0;
  if ((err = bc_parse_i64(&res_span, &val)) != BC_PE_NONE) return err;
  if ((err = bc_consume_char(&res_span, 'e')) != BC_PE_NONE) return err;

  res->kind = BC_KIND_INTEGER;
  res->v.integer = val;

  *span = res_span;
  return BC_PE_NONE;
}

void bc_value_destroy(bc_value_t* value) {
  switch (value->kind) {
    case BC_KIND_INTEGER: {
      break;
    }  // No-op
    case BC_KIND_STRING:
      pg_string_free(value->v.string);
      break;
    case BC_KIND_DICTIONARY:
      pg_hashtable_destroy(&value->v.dictionary);
      break;
    case BC_KIND_ARRAY:
      for (uint64_t i = 0; i < pg_array_count(value->v.array); i++)
        bc_value_destroy(&value->v.array[i]);

      pg_array_free(value->v.array);
      break;
    default:
      __builtin_unreachable();
  }
}

bc_parse_error_t bc_parse_value(pg_allocator_t allocator,
                                pg_string_span_t* span, bc_value_t* res);

bc_parse_error_t bc_parse_array(pg_allocator_t allocator,
                                pg_string_span_t* span, bc_value_t* res) {
  bc_parse_error_t err = BC_PE_NONE;
  pg_string_span_t res_span = *span;

  if ((err = bc_consume_char(&res_span, 'l')) != BC_PE_NONE) return err;

  pg_array_t(bc_value_t) values = {0};
  pg_array_init_reserve(values, 8, allocator);

  for (uint64_t i = 0; i < res_span.len; i++) {
    const char c = bc_peek(res_span);
    if (c == 0) {
      err = BC_PE_EOF;
      goto fail;
    }
    if (c == 'e') break;

    bc_value_t value = {0};

    if ((err = bc_parse_value(allocator, &res_span, &value)) != BC_PE_NONE)
      goto fail;

    pg_array_append(values, value);
  }

  if ((err = bc_consume_char(&res_span, 'e')) != BC_PE_NONE) goto fail;

  res->kind = BC_KIND_ARRAY;
  res->v.array = values;

  *span = res_span;

  return BC_PE_NONE;

fail:
  for (uint64_t i = 0; i < pg_array_count(values); i++)
    bc_value_destroy(&values[i]);
  pg_array_free(values);
  return err;
}

bc_parse_error_t bc_parse_dictionary(pg_allocator_t allocator,
                                     pg_string_span_t* span, bc_value_t* res) {
  bc_parse_error_t err = BC_PE_NONE;
  pg_string_span_t res_span = *span;

  if ((err = bc_consume_char(&res_span, 'd')) != BC_PE_NONE) return err;

  bc_dictionary_t dict = {0};
  pg_hashtable_init(&dict, 5, allocator);

  for (uint64_t i = 0; i < res_span.len; i++) {
    const char c = bc_peek(res_span);
    if (c == 0) {
      err = BC_PE_EOF;
      goto fail;
    }
    if (c == 'e') break;

    bc_value_t key = {0};
    if ((err = bc_parse_value(allocator, &res_span, &key)) != BC_PE_NONE)
      goto fail;

    if (key.kind != BC_KIND_STRING) {
      err = BC_PE_DICT_KEY_NOT_STRING;
      goto fail;
    }

    bc_value_t value = {0};
    if ((err = bc_parse_value(allocator, &res_span, &value)) != BC_PE_NONE)
      goto fail;

    pg_hashtable_upsert(&dict, key.v.string, &value);
  }
  if ((err = bc_consume_char(&res_span, 'e')) != BC_PE_NONE) goto fail;

  res->kind = BC_KIND_DICTIONARY;
  res->v.dictionary = dict;

  *span = res_span;

  return BC_PE_NONE;

fail:
  pg_hashtable_destroy(&dict);
  return err;
}

bc_parse_error_t bc_parse_value(pg_allocator_t allocator,
                                pg_string_span_t* span, bc_value_t* res) {
  const char c = bc_peek(*span);
  if (c == 'i')
    return bc_parse_number(span, res);
  else if (c == 'l')
    return bc_parse_array(allocator, span, res);
  else if (c == 'd')
    return bc_parse_dictionary(allocator, span, res);
  else
    return bc_parse_string(allocator, span, res);
}

void bc_value_dump_indent(FILE* f, uint64_t indent) {
  for (uint64_t i = 0; i < indent; i++) fprintf(f, " ");
}

void bc_value_dump(bc_value_t* value, FILE* f, uint64_t indent) {
  switch (value->kind) {
    case BC_KIND_INTEGER:
      fprintf(f, "%lld", value->v.integer);
      break;
    case BC_KIND_STRING: {
      fprintf(f, "\"");
      for (uint64_t i = 0; i < pg_string_length(value->v.string); i++) {
        uint8_t c = (uint8_t)value->v.string[i];
        if (32 <= c && c < 127)
          fprintf(f, "%c", c);
        else
          fprintf(f, "\\u%04x", c);
      }
      fprintf(f, "\"");

      break;
    }
    case BC_KIND_ARRAY:
      fprintf(f, "[\n");
      for (uint64_t i = 0; i < pg_array_count(value->v.array); i++) {
        bc_value_dump_indent(f, indent + 2);
        bc_value_dump(&value->v.array[i], f, indent + 2);
        if (i < pg_array_count(value->v.array) - 1) fprintf(f, ",");
        fprintf(f, "\n");
      }
      bc_value_dump_indent(f, indent);
      fprintf(f, "]");
      break;
    case BC_KIND_DICTIONARY: {
      fprintf(f, "{\n");

      bc_dictionary_t* dict = &value->v.dictionary;

      uint64_t count = 0;
      for (uint64_t i = 0; i < pg_array_capacity(dict->keys); i++) {
        if (dict->hashes[i] == 0) {
          continue;
        }
        count++;

        bc_value_dump_indent(f, indent + 2);
        fprintf(f, "\"%s\": ", dict->keys[i]);
        bc_value_dump(&dict->values[i], f, indent + 2);
        if (count < pg_hashtable_count(dict)) fprintf(f, ",");

        fprintf(f, "\n");
      }

      bc_value_dump_indent(f, indent);
      fprintf(f, "}");
      break;
    }
  }
}
