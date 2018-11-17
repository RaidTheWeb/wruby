/*
** mruby/string.h - String class
**
** See Copyright Notice in mruby.h
*/

#ifndef MRUBY_STRING_H
#define MRUBY_STRING_H

#include "common.h"

/**
 * String class
 */
BEGIN_DECL

extern const char digitmap[];

#define RSTRING_EMBED_LEN_MAX ((int)(sizeof(void*) * 3 - 1))

struct RString {
  OBJECT_HEADER;
  union {
    struct {
      int len;
      union {
        int capa;
        struct shared_string *shared;
        struct RString *fshared;
      } aux;
      char *ptr;
    } heap;
    char ary[RSTRING_EMBED_LEN_MAX + 1];
  } as;
};

#define RSTR_EMBED_P(s) ((s)->flags & STR_EMBED)
#define RSTR_SET_EMBED_FLAG(s) ((s)->flags |= STR_EMBED)
#define RSTR_UNSET_EMBED_FLAG(s) ((s)->flags &= ~(STR_EMBED|STR_EMBED_LEN_MASK))
#define RSTR_SET_EMBED_LEN(s, n) do {\
  size_t tmp_n = (n);\
  s->flags &= ~STR_EMBED_LEN_MASK;\
  s->flags |= (tmp_n) << STR_EMBED_LEN_SHIFT;\
} while (0)
#define RSTR_SET_LEN(s, n) do {\
  if (RSTR_EMBED_P(s)) {\
    RSTR_SET_EMBED_LEN((s),(n));\
  }\
  else {\
    s->as.heap.len = (int)(n);\
  }\
} while (0)
#define RSTR_EMBED_LEN(s)\
  (int)(((s)->flags & STR_EMBED_LEN_MASK) >> STR_EMBED_LEN_SHIFT)
#define RSTR_PTR(s) ((RSTR_EMBED_P(s)) ? (s)->as.ary : (s)->as.heap.ptr)
#define RSTR_LEN(s) ((RSTR_EMBED_P(s)) ? RSTR_EMBED_LEN(s) : (s)->as.heap.len)
#define RSTR_CAPA(s) (RSTR_EMBED_P(s) ? RSTRING_EMBED_LEN_MAX : (s)->as.heap.aux.capa)

#define RSTR_SHARED_P(s) ((s)->flags & STR_SHARED)
#define RSTR_SET_SHARED_FLAG(s) ((s)->flags |= STR_SHARED)
#define RSTR_UNSET_SHARED_FLAG(s) ((s)->flags &= ~STR_SHARED)

#define RSTR_FSHARED_P(s) ((s)->flags & STR_FSHARED)
#define RSTR_SET_FSHARED_FLAG(s) ((s)->flags |= STR_FSHARED)
#define RSTR_UNSET_FSHARED_FLAG(s) ((s)->flags &= ~STR_FSHARED)

#define RSTR_NOFREE_P(s) ((s)->flags & STR_NOFREE)
#define RSTR_SET_NOFREE_FLAG(s) ((s)->flags |= STR_NOFREE)
#define RSTR_UNSET_NOFREE_FLAG(s) ((s)->flags &= ~STR_NOFREE)

#define RSTR_POOL_P(s) ((s)->flags & STR_POOL)
#define RSTR_SET_POOL_FLAG(s) ((s)->flags |= STR_POOL)

/*
 * Returns a pointer from a Ruby string
 */
#define str_ptr(s)       ((struct RString*)(ptr(s)))
#define RSTRING(s)           str_ptr(s)
#define RSTRING_PTR(s)       RSTR_PTR(RSTRING(s))
#define RSTRING_EMBED_LEN(s) RSTR_EMBED_LEN(RSTRING(s))
#define RSTRING_LEN(s)       RSTR_LEN(RSTRING(s))
#define RSTRING_CAPA(s)      RSTR_CAPA(RSTRING(s))
#define RSTRING_END(s)       (RSTRING_PTR(s) + RSTRING_LEN(s))
API int str_strlen(state*, struct RString*);

#define STR_SHARED    1
#define STR_FSHARED   2
#define STR_NOFREE    4
#define STR_POOL      8
#define STR_NO_UTF   16
#define STR_EMBED    32
#define STR_EMBED_LEN_MASK 0x7c0
#define STR_EMBED_LEN_SHIFT 6

void gc_free_str(state*, struct RString*);
API void str_modify(state*, struct RString*);

/*
 * Finds the index of a substring in a string
 */
API int str_index(state*, value, const char*, int, int);
#define str_index_lit(mrb, str, lit, off) str_index(mrb, str, lit, strlen_lit(lit), off);

/*
 * Appends self to other. Returns self as a concatenated string.
 *
 *
 *  Example:
 *
 *     !!!c
 *     int
 *     main(int argc,
 *          char **argv)
 *     {
 *       // Variable declarations.
 *       value str1;
 *       value str2;
 *
 *       state *mrb = open();
 *       if (!mrb)
 *       {
 *          // handle error
 *       }
 *
 *       // Creates new Ruby strings.
 *       str1 = str_new_lit(mrb, "abc");
 *       str2 = str_new_lit(mrb, "def");
 *
 *       // Concatenates str2 to str1.
 *       str_concat(mrb, str1, str2);
 *
 *      // Prints new Concatenated Ruby string.
 *      p(mrb, str1);
 *
 *      close(mrb);
 *      return 0;
 *    }
 *
 *
 *  Result:
 *
 *     => "abcdef"
 *
 * @param [state] mrb The current mruby state.
 * @param [value] self String to concatenate.
 * @param [value] other String to append to self.
 * @return [value] Returns a new String appending other to self.
 */
API void str_concat(state*, value, value);

/*
 * Adds two strings together.
 *
 *
 *  Example:
 *
 *     !!!c
 *     int
 *     main(int argc,
 *          char **argv)
 *     {
 *       // Variable declarations.
 *       value a;
 *       value b;
 *       value c;
 *
 *       state *mrb = open();
 *       if (!mrb)
 *       {
 *          // handle error
 *       }
 *
 *       // Creates two Ruby strings from the passed in C strings.
 *       a = str_new_lit(mrb, "abc");
 *       b = str_new_lit(mrb, "def");
 *
 *       // Prints both C strings.
 *       p(mrb, a);
 *       p(mrb, b);
 *
 *       // Concatenates both Ruby strings.
 *       c = str_plus(mrb, a, b);
 *
 *      // Prints new Concatenated Ruby string.
 *      p(mrb, c);
 *
 *      close(mrb);
 *      return 0;
 *    }
 *
 *
 *  Result:
 *
 *     => "abc"  # First string
 *     => "def"  # Second string
 *     => "abcdef" # First & Second concatenated.
 *
 * @param [state] mrb The current mruby state.
 * @param [value] a First string to concatenate.
 * @param [value] b Second string to concatenate.
 * @return [value] Returns a new String containing a concatenated to b.
 */
API value str_plus(state*, value, value);

/*
 * Converts pointer into a Ruby string.
 *
 * @param [state] mrb The current mruby state.
 * @param [void*] p The pointer to convert to Ruby string.
 * @return [value] Returns a new Ruby String.
 */
API value ptr_to_str(state *, void*);

/*
 * Returns an object as a Ruby string.
 *
 * @param [state] mrb The current mruby state.
 * @param [value] obj An object to return as a Ruby string.
 * @return [value] An object as a Ruby string.
 */
API value obj_as_string(state *mrb, value obj);

/*
 * Resizes the string's length. Returns the amount of characters
 * in the specified by len.
 *
 * Example:
 *
 *     !!!c
 *     int
 *     main(int argc,
 *          char **argv)
 *     {
 *         // Variable declaration.
 *         value str;
 *
 *         state *mrb = open();
 *         if (!mrb)
 *         {
 *            // handle error
 *         }
 *         // Creates a new string.
 *         str = str_new_lit(mrb, "Hello, world!");
 *         // Returns 5 characters of
 *         str_resize(mrb, str, 5);
 *         p(mrb, str);
 *
 *         close(mrb);
 *         return 0;
 *      }
 *
 * Result:
 *
 *     => "Hello"
 *
 * @param [state] mrb The current mruby state.
 * @param [value] str The Ruby string to resize.
 * @param [value] len The length.
 * @return [value] An object as a Ruby string.
 */
API value str_resize(state *mrb, value str, int len);

/*
 * Returns a sub string.
 *
 *  Example:
 *
 *     !!!c
 *     int
 *     main(int argc,
 *     char const **argv)
 *     {
 *       // Variable declarations.
 *       value str1;
 *       value str2;
 *
 *       state *mrb = open();
 *       if (!mrb)
 *       {
 *         // handle error
 *       }
 *       // Creates new string.
 *       str1 = str_new_lit(mrb, "Hello, world!");
 *       // Returns a sub-string within the range of 0..2
 *       str2 = str_substr(mrb, str1, 0, 2);
 *
 *       // Prints sub-string.
 *       p(mrb, str2);
 *
 *       close(mrb);
 *       return 0;
 *     }
 *
 *  Result:
 *
 *     => "He"
 *
 * @param [state] mrb The current mruby state.
 * @param [value] str Ruby string.
 * @param [int] beg The beginning point of the sub-string.
 * @param [int] len The end point of the sub-string.
 * @return [value] An object as a Ruby sub-string.
 */
API value str_substr(state *mrb, value str, int beg, int len);

/*
 * Returns a Ruby string type.
 *
 *
 * @param [state] mrb The current mruby state.
 * @param [value] str Ruby string.
 * @return [value] A Ruby string.
 */
API value string_type(state *mrb, value str);

API value check_string_type(state *mrb, value str);
API value str_new_capa(state *mrb, size_t capa);
API value str_buf_new(state *mrb, size_t capa);

API const char *string_value_cstr(state *mrb, value *ptr);
API const char *string_value_ptr(state *mrb, value str);
/*
 * Returns the length of the Ruby string.
 *
 *
 * @param [state] mrb The current mruby state.
 * @param [value] str Ruby string.
 * @return [int] The length of the passed in Ruby string.
 */
API int string_value_len(state *mrb, value str);

/*
 * Duplicates a string object.
 *
 *
 * @param [state] mrb The current mruby state.
 * @param [value] str Ruby string.
 * @return [value] Duplicated Ruby string.
 */
API value str_dup(state *mrb, value str);

/*
 * Returns a symbol from a passed in Ruby string.
 *
 * @param [state] mrb The current mruby state.
 * @param [value] self Ruby string.
 * @return [value] A symbol.
 */
API value str_intern(state *mrb, value self);

API value str_to_inum(state *mrb, value str, int base, bool badcheck);
API double str_to_dbl(state *mrb, value str, bool badcheck);

/*
 * Returns a converted string type.
 */
API value str_to_str(state *mrb, value str);

/*
 * Returns true if the strings match and false if the strings don't match.
 *
 * @param [state] mrb The current mruby state.
 * @param [value] str1 Ruby string to compare.
 * @param [value] str2 Ruby string to compare.
 * @return [value] boolean value.
 */
API bool str_equal(state *mrb, value str1, value str2);

/*
 * Returns a concated string comprised of a Ruby string and a C string.
 *
 * @param [state] mrb The current mruby state.
 * @param [value] str Ruby string.
 * @param [const char *] ptr A C string.
 * @param [size_t] len length of C string.
 * @return [value] A Ruby string.
 * @see str_cat_cstr
 */
API value str_cat(state *mrb, value str, const char *ptr, size_t len);

/*
 * Returns a concated string comprised of a Ruby string and a C string.
 *
 * @param [state] mrb The current mruby state.
 * @param [value] str Ruby string.
 * @param [const char *] ptr A C string.
 * @return [value] A Ruby string.
 * @see str_cat
 */
API value str_cat_cstr(state *mrb, value str, const char *ptr);
API value str_cat_str(state *mrb, value str, value str2);
#define str_cat_lit(mrb, str, lit) str_cat(mrb, str, lit, strlen_lit(lit))

/*
 * Adds str2 to the end of str1.
 */
API value str_append(state *mrb, value str, value str2);

/*
 * Returns 0 if both Ruby strings are equal. Returns a value < 0 if Ruby str1 is less than Ruby str2. Returns a value > 0 if Ruby str2 is greater than Ruby str1.
 */
API int str_cmp(state *mrb, value str1, value str2);

/*
 * Returns a newly allocated C string from a Ruby string.
 * This is an utility function to pass a Ruby string to C library functions.
 *
 * - Returned string does not contain any NUL characters (but terminator).
 * - It raises an ArgumentError exception if Ruby string contains
 *   NUL characters.
 * - Retured string will be freed automatically on next GC.
 * - Caller can modify returned string without affecting Ruby string
 *   (e.g. it can be used for mkstemp(3)).
 *
 * @param [state *] mrb The current mruby state.
 * @param [value] str Ruby string. Must be an instance of String.
 * @return [char *] A newly allocated C string.
 */
API char *str_to_cstr(state *mrb, value str);

value str_pool(state *mrb, value str);
uint32_t str_hash(state *mrb, value str);
value str_dump(state *mrb, value str);

/*
 * Returns a printable version of str, surrounded by quote marks, with special characters escaped.
 */
value str_inspect(state *mrb, value str);

void noregexp(state *mrb, value self);
void regexp_check(state *mrb, value obj);

/* For backward compatibility */
#define str_cat2(mrb, str, ptr) str_cat_cstr(mrb, str, ptr)
#define str_buf_cat(mrb, str, ptr, len) str_cat(mrb, str, ptr, len)
#define str_buf_append(mrb, str, str2) str_cat_str(mrb, str, str2)

END_DECL

#endif  /* MRUBY_STRING_H */
