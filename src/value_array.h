#ifndef $VALUE_ARRAY_H__
#define $VALUE_ARRAY_H__

#include <mruby.h>

static inline void
value_move($value *s1, const $value *s2, size_t n)
{
  if (s1 > s2 && s1 < s2 + n)
  {
    s1 += n;
    s2 += n;
    while (n-- > 0) {
      *--s1 = *--s2;
    }
  }
  else if (s1 != s2) {
    while (n-- > 0) {
      *s1++ = *s2++;
    }
  }
  else {
    /* nothing to do. */
  }
}

#endif /* $VALUE_ARRAY_H__ */
