/*
** load.c - mruby binary loader
**
** See Copyright Notice in mruby.h
*/

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <mruby/dump.h>
#include <mruby/irep.h>
#include <mruby/proc.h>
#include <mruby/string.h>
#include <mruby/debug.h>
#include <mruby/error.h>

#if SIZE_MAX < UINT32_MAX
# error size_t must be at least 32 bits wide
#endif

#define FLAG_BYTEORDER_BIG 2
#define FLAG_BYTEORDER_LIL 4
#define FLAG_BYTEORDER_NATIVE 8
#define FLAG_SRC_MALLOC 1
#define FLAG_SRC_STATIC 0

#define SIZE_ERROR_MUL(nmemb, size) ((size_t)(nmemb) > SIZE_MAX / (size))

static size_t
skip_padding(const uint8_t *buf)
{
  const size_t align = MRB_DUMP_ALIGNMENT;
  return -(intptr_t)buf & (align-1);
}

static size_t
offset_crc_body(void)
{
  struct rite_binary_header header;
  return ((uint8_t *)header.binary_crc - (uint8_t *)&header) + sizeof(header.binary_crc);
}

static _irep*
read_irep_record_1(state *mrb, const uint8_t *bin, size_t *len, uint8_t flags)
{
  int i;
  const uint8_t *src = bin;
  ptrdiff_t diff;
  uint16_t tt, pool_data_len, snl;
  int plen;
  int ai = _gc_arena_save(mrb);
  _irep *irep = _add_irep(mrb);

  /* skip record size */
  src += sizeof(uint32_t);

  /* number of local variable */
  irep->nlocals = bin_to_uint16(src);
  src += sizeof(uint16_t);

  /* number of register variable */
  irep->nregs = bin_to_uint16(src);
  src += sizeof(uint16_t);

  /* number of child irep */
  irep->rlen = (size_t)bin_to_uint16(src);
  src += sizeof(uint16_t);

  /* Binary Data Section */
  /* ISEQ BLOCK */
  irep->ilen = (uint16_t)bin_to_uint32(src);
  src += sizeof(uint32_t);
  src += skip_padding(src);

  if (irep->ilen > 0) {
    if (SIZE_ERROR_MUL(irep->ilen, sizeof(_code))) {
      return NULL;
    }
    if ((flags & FLAG_SRC_MALLOC) == 0 &&
        (flags & FLAG_BYTEORDER_NATIVE)) {
      irep->iseq = (_code*)src;
      src += sizeof(_code) * irep->ilen;
      irep->flags |= MRB_ISEQ_NO_FREE;
    }
    else {
      size_t data_len = sizeof(_code) * irep->ilen;
      irep->iseq = (_code *)_malloc(mrb, data_len);
      memcpy(irep->iseq, src, data_len);
      src += data_len;
    }
  }

  /* POOL BLOCK */
  plen = bin_to_uint32(src); /* number of pool */
  src += sizeof(uint32_t);
  if (plen > 0) {
    if (SIZE_ERROR_MUL(plen, sizeof(value))) {
      return NULL;
    }
    irep->pool = (value*)_malloc(mrb, sizeof(value) * plen);

    for (i = 0; i < plen; i++) {
      value s;

      tt = *src++; /* pool TT */
      pool_data_len = bin_to_uint16(src); /* pool data length */
      src += sizeof(uint16_t);
      if (flags & FLAG_SRC_MALLOC) {
        s = _str_new(mrb, (char *)src, pool_data_len);
      }
      else {
        s = _str_new_static(mrb, (char *)src, pool_data_len);
      }
      src += pool_data_len;
      switch (tt) { /* pool data */
      case IREP_TT_FIXNUM: {
        value num = _str_to_inum(mrb, s, 10, FALSE);
#ifdef MRB_WITHOUT_FLOAT
        irep->pool[i] = num;
#else
        irep->pool[i] = _float_p(num)? _float_pool(mrb, _float(num)) : num;
#endif
        }
        break;

#ifndef MRB_WITHOUT_FLOAT
      case IREP_TT_FLOAT:
        irep->pool[i] = _float_pool(mrb, _str_to_dbl(mrb, s, FALSE));
        break;
#endif

      case IREP_TT_STRING:
        irep->pool[i] = _str_pool(mrb, s);
        break;

      default:
        /* should not happen */
        irep->pool[i] = _nil_value();
        break;
      }
      irep->plen++;
      _gc_arena_restore(mrb, ai);
    }
  }

  /* SYMS BLOCK */
  irep->slen = (uint16_t)bin_to_uint32(src);  /* syms length */
  src += sizeof(uint32_t);
  if (irep->slen > 0) {
    if (SIZE_ERROR_MUL(irep->slen, sizeof(_sym))) {
      return NULL;
    }
    irep->syms = (_sym *)_malloc(mrb, sizeof(_sym) * irep->slen);

    for (i = 0; i < irep->slen; i++) {
      snl = bin_to_uint16(src);               /* symbol name length */
      src += sizeof(uint16_t);

      if (snl == MRB_DUMP_NULL_SYM_LEN) {
        irep->syms[i] = 0;
        continue;
      }

      if (flags & FLAG_SRC_MALLOC) {
        irep->syms[i] = _intern(mrb, (char *)src, snl);
      }
      else {
        irep->syms[i] = _intern_static(mrb, (char *)src, snl);
      }
      src += snl + 1;

      _gc_arena_restore(mrb, ai);
    }
  }

  irep->reps = (_irep**)_malloc(mrb, sizeof(_irep*)*irep->rlen);

  diff = src - bin;
  _assert_int_fit(ptrdiff_t, diff, size_t, SIZE_MAX);
  *len = (size_t)diff;

  return irep;
}

static _irep*
read_irep_record(state *mrb, const uint8_t *bin, size_t *len, uint8_t flags)
{
  _irep *irep = read_irep_record_1(mrb, bin, len, flags);
  int i;

  if (irep == NULL) {
    return NULL;
  }

  bin += *len;
  for (i=0; i<irep->rlen; i++) {
    size_t rlen;

    irep->reps[i] = read_irep_record(mrb, bin, &rlen, flags);
    if (irep->reps[i] == NULL) {
      return NULL;
    }
    bin += rlen;
    *len += rlen;
  }
  return irep;
}

static _irep*
read_section_irep(state *mrb, const uint8_t *bin, uint8_t flags)
{
  size_t len;

  bin += sizeof(struct rite_section_irep_header);
  return read_irep_record(mrb, bin, &len, flags);
}

static int
read_lineno_record_1(state *mrb, const uint8_t *bin, _irep *irep, size_t *len)
{
  size_t i, fname_len, niseq;
  char *fname;
  uint16_t *lines;

  *len = 0;
  bin += sizeof(uint32_t); /* record size */
  *len += sizeof(uint32_t);
  fname_len = bin_to_uint16(bin);
  bin += sizeof(uint16_t);
  *len += sizeof(uint16_t);
  fname = (char *)_malloc(mrb, fname_len + 1);
  memcpy(fname, bin, fname_len);
  fname[fname_len] = '\0';
  bin += fname_len;
  *len += fname_len;

  niseq = (size_t)bin_to_uint32(bin);
  bin += sizeof(uint32_t); /* niseq */
  *len += sizeof(uint32_t);

  if (SIZE_ERROR_MUL(niseq, sizeof(uint16_t))) {
    return MRB_DUMP_GENERAL_FAILURE;
  }
  lines = (uint16_t *)_malloc(mrb, niseq * sizeof(uint16_t));
  for (i = 0; i < niseq; i++) {
    lines[i] = bin_to_uint16(bin);
    bin += sizeof(uint16_t); /* niseq */
    *len += sizeof(uint16_t);
  }

  irep->filename = fname;
  irep->lines = lines;
  return MRB_DUMP_OK;
}

static int
read_lineno_record(state *mrb, const uint8_t *bin, _irep *irep, size_t *lenp)
{
  int result = read_lineno_record_1(mrb, bin, irep, lenp);
  int i;

  if (result != MRB_DUMP_OK) return result;
  for (i = 0; i < irep->rlen; i++) {
    size_t len;

    result = read_lineno_record(mrb, bin, irep->reps[i], &len);
    if (result != MRB_DUMP_OK) break;
    bin += len;
    *lenp += len;
  }
  return result;
}

static int
read_section_lineno(state *mrb, const uint8_t *bin, _irep *irep)
{
  size_t len;

  len = 0;
  bin += sizeof(struct rite_section_lineno_header);

  /* Read Binary Data Section */
  return read_lineno_record(mrb, bin, irep, &len);
}

static int
read_debug_record(state *mrb, const uint8_t *start, _irep* irep, size_t *record_len, const _sym *filenames, size_t filenames_len)
{
  const uint8_t *bin = start;
  ptrdiff_t diff;
  size_t record_size;
  uint16_t f_idx;
  int i;

  if (irep->debug_info) { return MRB_DUMP_INVALID_IREP; }

  irep->debug_info = (_irep_debug_info*)_malloc(mrb, sizeof(_irep_debug_info));
  irep->debug_info->pc_count = (uint32_t)irep->ilen;

  record_size = (size_t)bin_to_uint32(bin);
  bin += sizeof(uint32_t);

  irep->debug_info->flen = bin_to_uint16(bin);
  irep->debug_info->files = (_irep_debug_info_file**)_malloc(mrb, sizeof(_irep_debug_info*) * irep->debug_info->flen);
  bin += sizeof(uint16_t);

  for (f_idx = 0; f_idx < irep->debug_info->flen; ++f_idx) {
    _irep_debug_info_file *file;
    uint16_t filename_idx;
    _int len;

    file = (_irep_debug_info_file *)_malloc(mrb, sizeof(*file));
    irep->debug_info->files[f_idx] = file;

    file->start_pos = bin_to_uint32(bin);
    bin += sizeof(uint32_t);

    /* filename */
    filename_idx = bin_to_uint16(bin);
    bin += sizeof(uint16_t);
    _assert(filename_idx < filenames_len);
    file->filename_sym = filenames[filename_idx];
    len = 0;
    file->filename = _sym2name_len(mrb, file->filename_sym, &len);

    file->line_entry_count = bin_to_uint32(bin);
    bin += sizeof(uint32_t);
    file->line_type = (_debug_line_type)bin_to_uint8(bin);
    bin += sizeof(uint8_t);
    switch (file->line_type) {
      case _debug_line_ary: {
        uint32_t l;

        file->lines.ary = (uint16_t *)_malloc(mrb, sizeof(uint16_t) * (size_t)(file->line_entry_count));
        for (l = 0; l < file->line_entry_count; ++l) {
          file->lines.ary[l] = bin_to_uint16(bin);
          bin += sizeof(uint16_t);
        }
      } break;

      case _debug_line_flat_map: {
        uint32_t l;

        file->lines.flat_map = (_irep_debug_info_line*)_malloc(
            mrb, sizeof(_irep_debug_info_line) * (size_t)(file->line_entry_count));
        for (l = 0; l < file->line_entry_count; ++l) {
          file->lines.flat_map[l].start_pos = bin_to_uint32(bin);
          bin += sizeof(uint32_t);
          file->lines.flat_map[l].line = bin_to_uint16(bin);
          bin += sizeof(uint16_t);
        }
      } break;

      default: return MRB_DUMP_GENERAL_FAILURE;
    }
  }

  diff = bin - start;
  _assert_int_fit(ptrdiff_t, diff, size_t, SIZE_MAX);

  if (record_size != (size_t)diff) {
    return MRB_DUMP_GENERAL_FAILURE;
  }

  for (i = 0; i < irep->rlen; i++) {
    size_t len;
    int ret;

    ret = read_debug_record(mrb, bin, irep->reps[i], &len, filenames, filenames_len);
    if (ret != MRB_DUMP_OK) return ret;
    bin += len;
  }

  diff = bin - start;
  _assert_int_fit(ptrdiff_t, diff, size_t, SIZE_MAX);
  *record_len = (size_t)diff;

  return MRB_DUMP_OK;
}

static int
read_section_debug(state *mrb, const uint8_t *start, _irep *irep, uint8_t flags)
{
  const uint8_t *bin;
  ptrdiff_t diff;
  struct rite_section_debug_header *header;
  uint16_t i;
  size_t len = 0;
  int result;
  uint16_t filenames_len;
  _sym *filenames;

  bin = start;
  header = (struct rite_section_debug_header *)bin;
  bin += sizeof(struct rite_section_debug_header);

  filenames_len = bin_to_uint16(bin);
  bin += sizeof(uint16_t);
  filenames = (_sym*)_malloc(mrb, sizeof(_sym) * (size_t)filenames_len);
  for (i = 0; i < filenames_len; ++i) {
    uint16_t f_len = bin_to_uint16(bin);
    bin += sizeof(uint16_t);
    if (flags & FLAG_SRC_MALLOC) {
      filenames[i] = _intern(mrb, (const char *)bin, (size_t)f_len);
    }
    else {
      filenames[i] = _intern_static(mrb, (const char *)bin, (size_t)f_len);
    }
    bin += f_len;
  }

  result = read_debug_record(mrb, bin, irep, &len, filenames, filenames_len);
  if (result != MRB_DUMP_OK) goto debug_exit;

  bin += len;
  diff = bin - start;
  _assert_int_fit(ptrdiff_t, diff, size_t, SIZE_MAX);
  if ((uint32_t)diff != bin_to_uint32(header->section_size)) {
    result = MRB_DUMP_GENERAL_FAILURE;
  }

debug_exit:
  _free(mrb, filenames);
  return result;
}

static int
read_lv_record(state *mrb, const uint8_t *start, _irep *irep, size_t *record_len, _sym const *syms, uint32_t syms_len)
{
  const uint8_t *bin = start;
  ptrdiff_t diff;
  int i;

  irep->lv = (struct _locals*)_malloc(mrb, sizeof(struct _locals) * (irep->nlocals - 1));

  for (i = 0; i + 1< irep->nlocals; ++i) {
    uint16_t const sym_idx = bin_to_uint16(bin);
    bin += sizeof(uint16_t);
    if (sym_idx == RITE_LV_NULL_MARK) {
      irep->lv[i].name = 0;
      irep->lv[i].r = 0;
    }
    else {
      if (sym_idx >= syms_len) {
        return MRB_DUMP_GENERAL_FAILURE;
      }
      irep->lv[i].name = syms[sym_idx];

      irep->lv[i].r = bin_to_uint16(bin);
    }
    bin += sizeof(uint16_t);
  }

  for (i = 0; i < irep->rlen; ++i) {
    size_t len;
    int ret;

    ret = read_lv_record(mrb, bin, irep->reps[i], &len, syms, syms_len);
    if (ret != MRB_DUMP_OK) return ret;
    bin += len;
  }

  diff = bin - start;
  _assert_int_fit(ptrdiff_t, diff, size_t, SIZE_MAX);
  *record_len = (size_t)diff;

  return MRB_DUMP_OK;
}

static int
read_section_lv(state *mrb, const uint8_t *start, _irep *irep, uint8_t flags)
{
  const uint8_t *bin;
  ptrdiff_t diff;
  struct rite_section_lv_header const *header;
  uint32_t i;
  size_t len = 0;
  int result;
  uint32_t syms_len;
  _sym *syms;
  _sym (*intern_func)(state*, const char*, size_t) =
    (flags & FLAG_SRC_MALLOC)? _intern : _intern_static;

  bin = start;
  header = (struct rite_section_lv_header const*)bin;
  bin += sizeof(struct rite_section_lv_header);

  syms_len = bin_to_uint32(bin);
  bin += sizeof(uint32_t);
  syms = (_sym*)_malloc(mrb, sizeof(_sym) * (size_t)syms_len);
  for (i = 0; i < syms_len; ++i) {
    uint16_t const str_len = bin_to_uint16(bin);
    bin += sizeof(uint16_t);

    syms[i] = intern_func(mrb, (const char*)bin, str_len);
    bin += str_len;
  }

  result = read_lv_record(mrb, bin, irep, &len, syms, syms_len);
  if (result != MRB_DUMP_OK) goto lv_exit;

  bin += len;
  diff = bin - start;
  _assert_int_fit(ptrdiff_t, diff, size_t, SIZE_MAX);
  if ((uint32_t)diff != bin_to_uint32(header->section_size)) {
    result = MRB_DUMP_GENERAL_FAILURE;
  }

lv_exit:
  _free(mrb, syms);
  return result;
}

static int
read_binary_header(const uint8_t *bin, size_t *bin_size, uint16_t *crc, uint8_t *flags)
{
  const struct rite_binary_header *header = (const struct rite_binary_header *)bin;

  if (memcmp(header->binary_ident, RITE_BINARY_IDENT, sizeof(header->binary_ident)) == 0) {
    if (bigendian_p())
      *flags |= FLAG_BYTEORDER_NATIVE;
    else
      *flags |= FLAG_BYTEORDER_BIG;
  }
  else if (memcmp(header->binary_ident, RITE_BINARY_IDENT_LIL, sizeof(header->binary_ident)) == 0) {
    if (bigendian_p())
      *flags |= FLAG_BYTEORDER_LIL;
    else
      *flags |= FLAG_BYTEORDER_NATIVE;
  }
  else {
    return MRB_DUMP_INVALID_FILE_HEADER;
  }

  if (crc) {
    *crc = bin_to_uint16(header->binary_crc);
  }
  *bin_size = (size_t)bin_to_uint32(header->binary_size);

  return MRB_DUMP_OK;
}

_irep* corrupted(){
  printf("Corrupted mrb binary\n");
  return NULL;
}

static _irep*
read_irep(state *mrb, const uint8_t *bin, uint8_t flags)
{
  int result;
  _irep *irep = NULL;
  const struct rite_section_header *section_header;
  uint16_t crc;
  size_t bin_size = 0;
  size_t n;

  if ((mrb == NULL) || (bin == NULL)) {
    printf("NO PROGRAM GIVEN");
    return corrupted();
  }

  result = read_binary_header(bin, &bin_size, &crc, &flags);
  if (result != MRB_DUMP_OK) {
    printf("Possibly corrupted mrb binary\n");
    return corrupted();
  }

  n = offset_crc_body();// Cyclical Redundancy Check? Nah our binaries are hackable;)
  if (crc != calc_crc_16_ccitt(bin + n, bin_size - n, 0)) {
    printf("Cyclical Redundancy Check failed for corrupted mrb binary\n");
  //   return NULL;
  }

  bin += sizeof(struct rite_binary_header);
  do {
    section_header = (const struct rite_section_header *)bin;
    if (memcmp(section_header->section_ident, RITE_SECTION_IREP_IDENT, sizeof(section_header->section_ident)) == 0) {
      irep = read_section_irep(mrb, bin, flags);
      if (!irep){
        printf("read_section_irep == NULL");
        return corrupted();
      }
    }
    else if (memcmp(section_header->section_ident, RITE_SECTION_LINENO_IDENT, sizeof(section_header->section_ident)) == 0) {
      if (!irep) return NULL;   /* corrupted data */
      result = read_section_lineno(mrb, bin, irep);
      if (result < MRB_DUMP_OK) {
        printf("read_section_lineno != MRB_DUMP_OK");
        return corrupted();
      }
    }
    else if (memcmp(section_header->section_ident, RITE_SECTION_DEBUG_IDENT, sizeof(section_header->section_ident)) == 0) {
      if (!irep) return NULL;   /* corrupted data */
      result = read_section_debug(mrb, bin, irep, flags);
      if (result < MRB_DUMP_OK) {
        return corrupted();
      }
    }
    else if (memcmp(section_header->section_ident, RITE_SECTION_LV_IDENT, sizeof(section_header->section_ident)) == 0) {
      if (!irep) return corrupted();
      result = read_section_lv(mrb, bin, irep, flags);
      if (result < MRB_DUMP_OK) {
        return corrupted();
      }
    }
    bin += bin_to_uint32(section_header->section_size);
  } while (memcmp(section_header->section_ident, RITE_BINARY_EOF, sizeof(section_header->section_ident)) != 0);

  return irep;
}

_irep*
_read_irep(state *mrb, const uint8_t *bin)
{
#ifdef MRB_USE_ETEXT_EDATA
  uint8_t flags = _ro_data_p((char*)bin) ? FLAG_SRC_STATIC : FLAG_SRC_MALLOC;
#else
  uint8_t flags = FLAG_SRC_STATIC;
#endif

  return read_irep(mrb, bin, flags);
}

void _exc_set(state *mrb, value exc);

static void
irep_error(state *mrb)
{
  _exc_set(mrb, _exc_new_str_lit(mrb, E_SCRIPT_ERROR, "irep load error"));
}

void _codedump_all(state*, struct RProc*);

static value
load_irep(state *mrb, _irep *irep, mrbc_context *c)
{
  struct RProc *proc;

  if (!irep) {
    irep_error(mrb);
    return _nil_value();
  }
  proc = _proc_new(mrb, irep);
  proc->c = NULL;
  _irep_decref(mrb, irep);
  if (c && c->dump_result) _codedump_all(mrb, proc);
  if (c && c->no_exec) return _obj_value(proc);
  return _top_run(mrb, proc, _top_self(mrb), 0);
}

MRB_API value
_load_irep_cxt(state *mrb, const uint8_t *bin, mrbc_context *c)
{
  return load_irep(mrb, _read_irep(mrb, bin), c);
}

MRB_API value
_load_irep(state *mrb, const uint8_t *bin)
{
  return _load_irep_cxt(mrb, bin, NULL);
}

#ifndef MRB_DISABLE_STDIO

_irep*
_read_irep_file(state *mrb, FILE* fp)
{
  _irep *irep = NULL;
  uint8_t *buf;
  const size_t header_size = sizeof(struct rite_binary_header);
  size_t buf_size = 0;
  uint8_t flags = 0;
  int result;

  if ((mrb == NULL) || (fp == NULL)) {
    return NULL;
  }

  buf = (uint8_t*)_malloc(mrb, header_size);
  if (fread(buf, header_size, 1, fp) == 0) {
    goto irep_exit;
  }
  result = read_binary_header(buf, &buf_size, NULL, &flags);
  if (result != MRB_DUMP_OK || buf_size <= header_size) {
    goto irep_exit;
  }

  buf = (uint8_t*)_realloc(mrb, buf, buf_size);
  if (fread(buf+header_size, buf_size-header_size, 1, fp) == 0) {
    goto irep_exit;
  }
  irep = read_irep(mrb, buf, FLAG_SRC_MALLOC);

irep_exit:
  _free(mrb, buf);
  return irep;
}

MRB_API value
_load_irep_file_cxt(state *mrb, FILE* fp, mrbc_context *c)
{
  return load_irep(mrb, _read_irep_file(mrb, fp), c);
}

MRB_API value
_load_irep_file(state *mrb, FILE* fp)
{
  return _load_irep_file_cxt(mrb, fp, NULL);
}
#endif /* MRB_DISABLE_STDIO */
