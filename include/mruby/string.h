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
MRB_BEGIN_DECL

extern const char _digitmap[];

#define RSTRING_EMBED_LEN_MAX ((_int)(sizeof(void*) * 3 - 1))

struct RString {
  MRB_OBJECT_HEADER;
  union {
    struct {
      _int len;
      union {
        _int capa;
        struct _shared_string *shared;
        struct RString *fshared;
      } aux;
      char *ptr;
    } heap;
    char ary[RSTRING_EMBED_LEN_MAX + 1];
  } as;
};

#define RSTR_EMBED_P(s) ((s)->flags & MRB_STR_EMBED)
#define RSTR_SET_EMBED_FLAG(s) ((s)->flags |= MRB_STR_EMBED)
#define RSTR_UNSET_EMBED_FLAG(s) ((s)->flags &= ~(MRB_STR_EMBED|MRB_STR_EMBED_LEN_MASK))
#define RSTR_SET_EMBED_LEN(s, n) do {\
  size_t tmp_n = (n);\
  s->flags &= ~MRB_STR_EMBED_LEN_MASK;\
  s->flags |= (tmp_n) << MRB_STR_EMBED_LEN_SHIFT;\
} while (0)
#define RSTR_SET_LEN(s, n) do {\
  if (RSTR_EMBED_P(s)) {\
    RSTR_SET_EMBED_LEN((s),(n));\
  }\
  else {\
    s->as.heap.len = (_int)(n);\
  }\
} while (0)
#define RSTR_EMBED_LEN(s)\
  (_int)(((s)->flags & MRB_STR_EMBED_LEN_MASK) >> MRB_STR_EMBED_LEN_SHIFT)
#define RSTR_PTR(s) ((RSTR_EMBED_P(s)) ? (s)->as.ary : (s)->as.heap.ptr)
#define RSTR_LEN(s) ((RSTR_EMBED_P(s)) ? RSTR_EMBED_LEN(s) : (s)->as.heap.len)
#define RSTR_CAPA(s) (RSTR_EMBED_P(s) ? RSTRING_EMBED_LEN_MAX : (s)->as.heap.aux.capa)

#define RSTR_SHARED_P(s) ((s)->flags & MRB_STR_SHARED)
#define RSTR_SET_SHARED_FLAG(s) ((s)->flags |= MRB_STR_SHARED)
#define RSTR_UNSET_SHARED_FLAG(s) ((s)->flags &= ~MRB_STR_SHARED)

#define RSTR_FSHARED_P(s) ((s)->flags & MRB_STR_FSHARED)
#define RSTR_SET_FSHARED_FLAG(s) ((s)->flags |= MRB_STR_FSHARED)
#define RSTR_UNSET_FSHARED_FLAG(s) ((s)->flags &= ~MRB_STR_FSHARED)

#define RSTR_NOFREE_P(s) ((s)->flags & MRB_STR_NOFREE)
#define RSTR_SET_NOFREE_FLAG(s) ((s)->flags |= MRB_STR_NOFREE)
#define RSTR_UNSET_NOFREE_FLAG(s) ((s)->flags &= ~MRB_STR_NOFREE)

#define RSTR_POOL_P(s) ((s)->flags & MRB_STR_POOL)
#define RSTR_SET_POOL_FLAG(s) ((s)->flags |= MRB_STR_POOL)

/*
 * Returns a pointer from a Ruby string
 */
#define _str_ptr(s)       ((struct RString*)(_ptr(s)))
#define RSTRING(s)           _str_ptr(s)
#define RSTRING_PTR(s)       RSTR_PTR(RSTRING(s))
#define RSTRING_EMBED_LEN(s) RSTR_EMBED_LEN(RSTRING(s))
#define RSTRING_LEN(s)       RSTR_LEN(RSTRING(s))
#define RSTRING_CAPA(s)      RSTR_CAPA(RSTRING(s))
#define RSTRING_END(s)       (RSTRING_PTR(s) + RSTRING_LEN(s))
MRB_API _int _str_strlen(_state*, struct RString*);

#define MRB_STR_SHARED    1
#define MRB_STR_FSHARED   2
#define MRB_STR_NOFREE    4
#define MRB_STR_POOL      8
#define MRB_STR_NO_UTF   16
#define MRB_STR_EMBED    32
#define MRB_STR_EMBED_LEN_MASK 0x7c0
#define MRB_STR_EMBED_LEN_SHIFT 6

void _gc_free_str(_state*, struct RString*);
MRB_API void _str_modify(_state*, struct RString*);

/*
 * Finds the index of a substring in a string
 */
MRB_API _int _str_index(_state*, _value, const char*, _int, _int);
#define _str_index_lit(mrb, str, lit, off) _str_index(mrb, str, lit, _strlen_lit(lit), off);

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
 *       _value str1;
 *       _value str2;
 *
 *       _state *mrb = _open();
 *       if (!mrb)
 *       {
 *          // handle error
 *       }
 *
 *       // Creates new Ruby strings.
 *       str1 = _str_new_lit(mrb, "abc");
 *       str2 = _str_new_lit(mrb, "def");
 *
 *       // Concatenates str2 to str1.
 *       _str_concat(mrb, str1, str2);
 *
 *      // Prints new Concatenated Ruby string.
 *      _p(mrb, str1);
 *
 *      _close(mrb);
 *      return 0;
 *    }
 *
 *
 *  Result:
 *
 *     => "abcdef"
 *
 * @param [_state] mrb The current mruby state.
 * @param [_value] self String to concatenate.
 * @param [_value] other String to append to self.
 * @return [_value] Returns a new String appending other to self.
 */
MRB_API void _str_concat(_state*, _value, _value);

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
 *       _value a;
 *       _value b;
 *       _value c;
 *
 *       _state *mrb = _open();
 *       if (!mrb)
 *       {
 *          // handle error
 *       }
 *
 *       // Creates two Ruby strings from the passed in C strings.
 *       a = _str_new_lit(mrb, "abc");
 *       b = _str_new_lit(mrb, "def");
 *
 *       // Prints both C strings.
 *       _p(mrb, a);
 *       _p(mrb, b);
 *
 *       // Concatenates both Ruby strings.
 *       c = _str_plus(mrb, a, b);
 *
 *      // Prints new Concatenated Ruby string.
 *      _p(mrb, c);
 *
 *      _close(mrb);
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
 * @param [_state] mrb The current mruby state.
 * @param [_value] a First string to concatenate.
 * @param [_value] b Second string to concatenate.
 * @return [_value] Returns a new String containing a concatenated to b.
 */
MRB_API _value _str_plus(_state*, _value, _value);

/*
 * Converts pointer into a Ruby string.
 *
 * @param [_state] mrb The current mruby state.
 * @param [void*] p The pointer to convert to Ruby string.
 * @return [_value] Returns a new Ruby String.
 */
MRB_API _value _ptr_to_str(_state *, void*);

/*
 * Returns an object as a Ruby string.
 *
 * @param [_state] mrb The current mruby state.
 * @param [_value] obj An object to return as a Ruby string.
 * @return [_value] An object as a Ruby string.
 */
MRB_API _value _obj_as_string(_state *mrb, _value obj);

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
 *         _value str;
 *
 *         _state *mrb = _open();
 *         if (!mrb)
 *         {
 *            // handle error
 *         }
 *         // Creates a new string.
 *         str = _str_new_lit(mrb, "Hello, world!");
 *         // Returns 5 characters of
 *         _str_resize(mrb, str, 5);
 *         _p(mrb, str);
 *
 *         _close(mrb);
 *         return 0;
 *      }
 *
 * Result:
 *
 *     => "Hello"
 *
 * @param [_state] mrb The current mruby state.
 * @param [_value] str The Ruby string to resize.
 * @param [_value] len The length.
 * @return [_value] An object as a Ruby string.
 */
MRB_API _value _str_resize(_state *mrb, _value str, _int len);

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
 *       _value str1;
 *       _value str2;
 *
 *       _state *mrb = _open();
 *       if (!mrb)
 *       {
 *         // handle error
 *       }
 *       // Creates new string.
 *       str1 = _str_new_lit(mrb, "Hello, world!");
 *       // Returns a sub-string within the range of 0..2
 *       str2 = _str_substr(mrb, str1, 0, 2);
 *
 *       // Prints sub-string.
 *       _p(mrb, str2);
 *
 *       _close(mrb);
 *       return 0;
 *     }
 *
 *  Result:
 *
 *     => "He"
 *
 * @param [_state] mrb The current mruby state.
 * @param [_value] str Ruby string.
 * @param [_int] beg The beginning point of the sub-string.
 * @param [_int] len The end point of the sub-string.
 * @return [_value] An object as a Ruby sub-string.
 */
MRB_API _value _str_substr(_state *mrb, _value str, _int beg, _int len);

/*
 * Returns a Ruby string type.
 *
 *
 * @param [_state] mrb The current mruby state.
 * @param [_value] str Ruby string.
 * @return [_value] A Ruby string.
 */
MRB_API _value _string_type(_state *mrb, _value str);

MRB_API _value _check_string_type(_state *mrb, _value str);
MRB_API _value _str_new_capa(_state *mrb, size_t capa);
MRB_API _value _str_buf_new(_state *mrb, size_t capa);

MRB_API const char *_string_value_cstr(_state *mrb, _value *ptr);
MRB_API const char *_string_value_ptr(_state *mrb, _value str);
/*
 * Returns the length of the Ruby string.
 *
 *
 * @param [_state] mrb The current mruby state.
 * @param [_value] str Ruby string.
 * @return [_int] The length of the passed in Ruby string.
 */
MRB_API _int _string_value_len(_state *mrb, _value str);

/*
 * Duplicates a string object.
 *
 *
 * @param [_state] mrb The current mruby state.
 * @param [_value] str Ruby string.
 * @return [_value] Duplicated Ruby string.
 */
MRB_API _value _str_dup(_state *mrb, _value str);

/*
 * Returns a symbol from a passed in Ruby string.
 *
 * @param [_state] mrb The current mruby state.
 * @param [_value] self Ruby string.
 * @return [_value] A symbol.
 */
MRB_API _value _str_intern(_state *mrb, _value self);

MRB_API _value _str_to_inum(_state *mrb, _value str, _int base, _bool badcheck);
MRB_API double _str_to_dbl(_state *mrb, _value str, _bool badcheck);

/*
 * Returns a converted string type.
 */
MRB_API _value _str_to_str(_state *mrb, _value str);

/*
 * Returns true if the strings match and false if the strings don't match.
 *
 * @param [_state] mrb The current mruby state.
 * @param [_value] str1 Ruby string to compare.
 * @param [_value] str2 Ruby string to compare.
 * @return [_value] boolean value.
 */
MRB_API _bool _str_equal(_state *mrb, _value str1, _value str2);

/*
 * Returns a concated string comprised of a Ruby string and a C string.
 *
 * @param [_state] mrb The current mruby state.
 * @param [_value] str Ruby string.
 * @param [const char *] ptr A C string.
 * @param [size_t] len length of C string.
 * @return [_value] A Ruby string.
 * @see _str_cat_cstr
 */
MRB_API _value _str_cat(_state *mrb, _value str, const char *ptr, size_t len);

/*
 * Returns a concated string comprised of a Ruby string and a C string.
 *
 * @param [_state] mrb The current mruby state.
 * @param [_value] str Ruby string.
 * @param [const char *] ptr A C string.
 * @return [_value] A Ruby string.
 * @see _str_cat
 */
MRB_API _value _str_cat_cstr(_state *mrb, _value str, const char *ptr);
MRB_API _value _str_cat_str(_state *mrb, _value str, _value str2);
#define _str_cat_lit(mrb, str, lit) _str_cat(mrb, str, lit, _strlen_lit(lit))

/*
 * Adds str2 to the end of str1.
 */
MRB_API _value _str_append(_state *mrb, _value str, _value str2);

/*
 * Returns 0 if both Ruby strings are equal. Returns a value < 0 if Ruby str1 is less than Ruby str2. Returns a value > 0 if Ruby str2 is greater than Ruby str1.
 */
MRB_API int _str_cmp(_state *mrb, _value str1, _value str2);

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
 * @param [_state *] mrb The current mruby state.
 * @param [_value] str Ruby string. Must be an instance of String.
 * @return [char *] A newly allocated C string.
 */
MRB_API char *_str_to_cstr(_state *mrb, _value str);

_value _str_pool(_state *mrb, _value str);
uint32_t _str_hash(_state *mrb, _value str);
_value _str_dump(_state *mrb, _value str);

/*
 * Returns a printable version of str, surrounded by quote marks, with special characters escaped.
 */
_value _str_inspect(_state *mrb, _value str);

void _noregexp(_state *mrb, _value self);
void _regexp_check(_state *mrb, _value obj);

/* For backward compatibility */
#define _str_cat2(mrb, str, ptr) _str_cat_cstr(mrb, str, ptr)
#define _str_buf_cat(mrb, str, ptr, len) _str_cat(mrb, str, ptr, len)
#define _str_buf_append(mrb, str, str2) _str_cat_str(mrb, str, str2)

MRB_END_DECL

#endif  /* MRUBY_STRING_H */
