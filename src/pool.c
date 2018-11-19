/*
** pool.c - memory pool
**
** See Copyright Notice in mruby.h
*/

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <mruby.h>

/* configuration section */
/* allocated memory address should be multiple of POOL_ALIGNMENT */
/* or undef it if alignment does not matter */
#ifndef POOL_ALIGNMENT
#if INTPTR_MAX == INT64_MAX
#define POOL_ALIGNMENT 8
#else
#define POOL_ALIGNMENT 4
#endif
#endif
/* page size of memory pool */
#ifndef POOL_PAGE_SIZE
#define POOL_PAGE_SIZE 16000
#endif
/* end of configuration section */

/* Disable MSVC warning "C4200: nonstandard extension used: zero-sized array
 * in struct/union" when in C++ mode */
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4200)
#endif

struct _pool_page {
  struct _pool_page *next;
  size_t offset;
  size_t len;
  void *last;
  char page[];
};

#ifdef _MSC_VER
#pragma warning(pop)
#endif

struct _pool {
  _state *mrb;
  struct _pool_page *pages;
};

#undef TEST_POOL
#ifdef TEST_POOL

#define _malloc_simple(m,s) malloc(s)
#define _free(m,p) free(p)
#endif

#ifdef POOL_ALIGNMENT
#  define ALIGN_PADDING(x) ((SIZE_MAX - (x) + 1) & (POOL_ALIGNMENT - 1))
#else
#  define ALIGN_PADDING(x) (0)
#endif

MRB_API _pool*
_pool_open(_state *mrb)
{
  _pool *pool = (_pool *)_malloc_simple(mrb, sizeof(_pool));

  if (pool) {
    pool->mrb = mrb;
    pool->pages = NULL;
  }

  return pool;
}

MRB_API void
_pool_close(_pool *pool)
{
  struct _pool_page *page, *tmp;

  if (!pool) return;
  page = pool->pages;
  while (page) {
    tmp = page;
    page = page->next;
    _free(pool->mrb, tmp);
  }
  _free(pool->mrb, pool);
}

static struct _pool_page*
page_alloc(_pool *pool, size_t len)
{
  struct _pool_page *page;

  if (len < POOL_PAGE_SIZE)
    len = POOL_PAGE_SIZE;
  page = (struct _pool_page *)_malloc_simple(pool->mrb, sizeof(struct _pool_page)+len);
  if (page) {
    page->offset = 0;
    page->len = len;
  }

  return page;
}

MRB_API void*
_pool_alloc(_pool *pool, size_t len)
{
  struct _pool_page *page;
  size_t n;

  if (!pool) return NULL;
  len += ALIGN_PADDING(len);
  page = pool->pages;
  while (page) {
    if (page->offset + len <= page->len) {
      n = page->offset;
      page->offset += len;
      page->last = (char*)page->page+n;
      return page->last;
    }
    page = page->next;
  }
  page = page_alloc(pool, len);
  if (!page) return NULL;
  page->offset = len;
  page->next = pool->pages;
  pool->pages = page;

  page->last = (void*)page->page;
  return page->last;
}

MRB_API _bool
_pool_can_realloc(_pool *pool, void *p, size_t len)
{
  struct _pool_page *page;

  if (!pool) return FALSE;
  len += ALIGN_PADDING(len);
  page = pool->pages;
  while (page) {
    if (page->last == p) {
      size_t beg;

      beg = (char*)p - page->page;
      if (beg + len > page->len) return FALSE;
      return TRUE;
    }
    page = page->next;
  }
  return FALSE;
}

MRB_API void*
_pool_realloc(_pool *pool, void *p, size_t oldlen, size_t newlen)
{
  struct _pool_page *page;
  void *np;

  if (!pool) return NULL;
  oldlen += ALIGN_PADDING(oldlen);
  newlen += ALIGN_PADDING(newlen);
  page = pool->pages;
  while (page) {
    if (page->last == p) {
      size_t beg;

      beg = (char*)p - page->page;
      if (beg + oldlen != page->offset) break;
      if (beg + newlen > page->len) {
        page->offset = beg;
        break;
      }
      page->offset = beg + newlen;
      return p;
    }
    page = page->next;
  }
  np = _pool_alloc(pool, newlen);
  if (np == NULL) {
    return NULL;
  }
  memcpy(np, p, oldlen);
  return np;
}

#ifdef TEST_POOL
int
main(void)
{
  int i, len = 250;
  _pool *pool;
  void *p;

  pool = _pool_open(NULL);
  p = _pool_alloc(pool, len);
  for (i=1; i<20; i++) {
    printf("%p (len=%d) %ud\n", p, len, _pool_can_realloc(pool, p, len*2));
    p = _pool_realloc(pool, p, len, len*2);
    len *= 2;
  }
  _pool_close(pool);
  return 0;
}
#endif
