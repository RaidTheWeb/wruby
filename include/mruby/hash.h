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
$BEGIN_DECL

struct RHash {
  $OBJECT_HEADER;
  struct iv_tbl *iv;
  struct seglist *ht;
};

#define $hash_ptr(v)    ((struct RHash*)($ptr(v)))
#define $hash_value(p)  $obj_value((void*)(p))

$API $value $hash_new_capa($state*, $int);
$API $value $ensure_hash_type($state *mrb, $value hash);
$API $value $check_hash_type($state *mrb, $value hash);

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
$API $value $hash_new($state *mrb);

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
$API void $hash_set($state *mrb, $value hash, $value key, $value val);

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
$API $value $hash_get($state *mrb, $value hash, $value key);

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
$API $value $hash_fetch($state *mrb, $value hash, $value key, $value def);

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
$API $value $hash_delete_key($state *mrb, $value hash, $value key);

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
$API $value $hash_keys($state *mrb, $value hash);
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
$API $bool $hash_key_p($state *mrb, $value hash, $value key);

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
$API $bool $hash_empty_p($state *mrb, $value self);

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
$API $value $hash_values($state *mrb, $value hash);

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
$API $value $hash_clear($state *mrb, $value hash);

/*
 * Copies the hash.
 *
 *
 * @param mrb The mruby state reference.
 * @param hash The target hash.
 * @return The copy of the hash
 */
$API $value $hash_dup($state *mrb, $value hash);

/*
 * Merges two hashes. The first hash will be modified by the
 * second hash.
 *
 * @param mrb The mruby state reference.
 * @param hash1 The target hash.
 * @param hash2 Updating hash
 */
$API void $hash_merge($state *mrb, $value hash1, $value hash2);

/* declaration of struct kh_ht */
/* be careful when you touch the internal */
typedef struct {
  $value v;
  $int n;
} $hash_value;

KHASH_DECLARE(ht, $value, $hash_value, TRUE)

/* RHASH_TBL allocates st_table if not available. */
#define RHASH(obj)   ((struct RHash*)($ptr(obj)))
#define RHASH_TBL(h)          (RHASH(h)->ht)
#define RHASH_IFNONE(h)       $iv_get(mrb, (h), $intern_lit(mrb, "ifnone"))
#define RHASH_PROCDEFAULT(h)  RHASH_IFNONE(h)

#define $HASH_DEFAULT      1
#define $HASH_PROC_DEFAULT 2
#define $RHASH_DEFAULT_P(h) (RHASH(h)->flags & $HASH_DEFAULT)
#define $RHASH_PROCDEFAULT_P(h) (RHASH(h)->flags & $HASH_PROC_DEFAULT)

/* GC functions */
void $gc_mark_hash($state*, struct RHash*);
size_t $gc_mark_hash_size($state*, struct RHash*);
void $gc_free_hash($state*, struct RHash*);

$END_DECL

#endif  /* MRUBY_HASH_H */
