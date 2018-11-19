/*
** mruby/hash.h - Hash class
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_HASH_H
#define MRUBY_HASH_H

#include "common.h"
#include <mruby/khash.h>

/**
 * Hash class
 */
MRB_BEGIN_DECL

struct RHash {
  MRB_OBJECT_HEADER;
  struct iv_tbl *iv;
  struct seglist *ht;
};

#define _hash_ptr(v)    ((struct RHash*)(_ptr(v)))
#define _hash_value(p)  _obj_value((void*)(p))

MRB_API _value _hash_new_capa(_state*, _int);
MRB_API _value _ensure_hash_type(_state *mrb, _value hash);
MRB_API _value _check_hash_type(_state *mrb, _value hash);

/*
 * Initializes a new hash.
 *
 * Equivalent to:
 *
 *      Hash.new
 *
 * @param mrb The mruby state reference.
 * @return The initialized hash.
 */
MRB_API _value _hash_new(_state *mrb);

/*
 * Sets a keys and values to hashes.
 *
 * Equivalent to:
 *
 *      hash[key] = val
 *
 * @param mrb The mruby state reference.
 * @param hash The target hash.
 * @param key The key to set.
 * @param val The value to set.
 * @return The value.
 */
MRB_API void _hash_set(_state *mrb, _value hash, _value key, _value val);

/*
 * Gets a value from a key. If the key is not found, the default of the
 * hash is used.
 *
 * Equivalent to:
 *
 *     hash[key]
 *
 * @param mrb The mruby state reference.
 * @param hash The target hash.
 * @param key The key to get.
 * @return The found value.
 */
MRB_API _value _hash_get(_state *mrb, _value hash, _value key);

/*
 * Gets a value from a key. If the key is not found, the default parameter is
 * used.
 *
 * Equivalent to:
 *
 *     hash.key?(key) ? hash[key] : def
 *
 * @param mrb The mruby state reference.
 * @param hash The target hash.
 * @param key The key to get.
 * @param def The default value.
 * @return The found value.
 */
MRB_API _value _hash_fetch(_state *mrb, _value hash, _value key, _value def);

/*
 * Deletes hash key and value pair.
 *
 * Equivalent to:
 *
 *     hash.delete(key)
 *
 * @param mrb The mruby state reference.
 * @param hash The target hash.
 * @param key The key to delete.
 * @return The deleted value.
 */
MRB_API _value _hash_delete_key(_state *mrb, _value hash, _value key);

/*
 * Gets an array of keys.
 *
 * Equivalent to:
 *
 *     hash.keys
 *
 * @param mrb The mruby state reference.
 * @param hash The target hash.
 * @return An array with the keys of the hash.
 */
MRB_API _value _hash_keys(_state *mrb, _value hash);
/*
 * Check if the hash has the key.
 *
 * Equivalent to:
 *
 *     hash.key?(key)
 *
 * @param mrb The mruby state reference.
 * @param hash The target hash.
 * @param key The key to check existence.
 * @return True if the hash has the key
 */
MRB_API _bool _hash_key_p(_state *mrb, _value hash, _value key);

/*
 * Check if the hash is empty
 *
 * Equivalent to:
 *
 *     hash.empty?
 *
 * @param mrb The mruby state reference.
 * @param self The target hash.
 * @return True if the hash is empty, false otherwise.
 */
MRB_API _bool _hash_empty_p(_state *mrb, _value self);

/*
 * Gets an array of values.
 *
 * Equivalent to:
 *
 *     hash.values
 *
 * @param mrb The mruby state reference.
 * @param hash The target hash.
 * @return An array with the values of the hash.
 */
MRB_API _value _hash_values(_state *mrb, _value hash);

/*
 * Clears the hash.
 *
 * Equivalent to:
 *
 *     hash.clear
 *
 * @param mrb The mruby state reference.
 * @param hash The target hash.
 * @return The hash
 */
MRB_API _value _hash_clear(_state *mrb, _value hash);

/*
 * Copies the hash.
 *
 *
 * @param mrb The mruby state reference.
 * @param hash The target hash.
 * @return The copy of the hash
 */
MRB_API _value _hash_dup(_state *mrb, _value hash);

/*
 * Merges two hashes. The first hash will be modified by the
 * second hash.
 *
 * @param mrb The mruby state reference.
 * @param hash1 The target hash.
 * @param hash2 Updating hash
 */
MRB_API void _hash_merge(_state *mrb, _value hash1, _value hash2);

/* declaration of struct kh_ht */
/* be careful when you touch the internal */
typedef struct {
  _value v;
  _int n;
} _hash_value;

KHASH_DECLARE(ht, _value, _hash_value, TRUE)

/* RHASH_TBL allocates st_table if not available. */
#define RHASH(obj)   ((struct RHash*)(_ptr(obj)))
#define RHASH_TBL(h)          (RHASH(h)->ht)
#define RHASH_IFNONE(h)       _iv_get(mrb, (h), _intern_lit(mrb, "ifnone"))
#define RHASH_PROCDEFAULT(h)  RHASH_IFNONE(h)

#define MRB_HASH_DEFAULT      1
#define MRB_HASH_PROC_DEFAULT 2
#define MRB_RHASH_DEFAULT_P(h) (RHASH(h)->flags & MRB_HASH_DEFAULT)
#define MRB_RHASH_PROCDEFAULT_P(h) (RHASH(h)->flags & MRB_HASH_PROC_DEFAULT)

/* GC functions */
void _gc_mark_hash(_state*, struct RHash*);
size_t _gc_mark_hash_size(_state*, struct RHash*);
void _gc_free_hash(_state*, struct RHash*);

MRB_END_DECL

#endif  /* MRUBY_HASH_H */
