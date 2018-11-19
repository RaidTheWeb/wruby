/*
** dump.c - mruby binary dumper (mrbc binary format)
**
** See Copyright Notice in mruby.h
*/

#include <string.h>
#include <limits.h>
#include <mruby/dump.h>
#include <mruby/string.h>
#include <mruby/irep.h>
#include <mruby/numeric.h>
#include <mruby/debug.h>

#define FLAG_BYTEORDER_NATIVE 2
#define FLAG_BYTEORDER_NONATIVE 0

#ifndef WITHOUT_FLOAT
#ifdef USE_FLOAT
#define FLOAT_FMT "%.8e"
#else
#define FLOAT_FMT "%.16e"
#endif
#endif

API size_t get_irep_record_size_1(state *mrb, irep *irep);

#if UINT32_MAX > SIZE_MAX
# error This code cannot be built on your environment.
#endif

API size_t
write_padding(uint8_t *buf)
{
  const size_t align = DUMP_ALIGNMENT;
  size_t pad_len = -(intptr_t)buf & (align-1);
  if (pad_len > 0) {
    memset(buf, 0, pad_len);
  }
  return pad_len;
}

API size_t
get_irep_header_size(state *mrb)
{
  size_t size = 0;

  size += sizeof(uint32_t) * 1;
  size += sizeof(uint16_t) * 3;

  return size;
}

API ptrdiff_t
write_irep_header(state *mrb, irep *irep, uint8_t *buf)
{
  uint8_t *cur = buf;

  cur += uint32_to_bin((uint32_t)get_irep_record_size_1(mrb, irep), cur);  /* record size */
  cur += uint16_to_bin((uint16_t)irep->nlocals, cur);  /* number of local variable */
  cur += uint16_to_bin((uint16_t)irep->nregs, cur);  /* number of register variable */
  cur += uint16_to_bin((uint16_t)irep->rlen, cur);  /* number of child irep */

  return cur - buf;
}


API size_t
get_iseq_block_size(state *mrb, irep *irep)
{
  size_t size = 0;

  size += sizeof(uint32_t); /* ilen */
  size += sizeof(uint32_t); /* max padding */
  size += sizeof(uint32_t) * irep->ilen; /* iseq(n) */

  return size;
}

API ptrdiff_t
write_iseq_block(state *mrb, irep *irep, uint8_t *buf, uint8_t flags)
{
  uint8_t *cur = buf;

  cur += uint32_to_bin(irep->ilen, cur); /* number of opcode */
  cur += write_padding(cur);
  memcpy(cur, irep->iseq, irep->ilen * sizeof(code));
  cur += irep->ilen * sizeof(code);

  return cur - buf;
}


API size_t
get_pool_block_size(state *mrb, irep *irep)
{
  int pool_no;
  size_t size = 0;
  value str;

  size += sizeof(uint32_t); /* plen */
  size += irep->plen * (sizeof(uint8_t) + sizeof(uint16_t)); /* len(n) */

  for (pool_no = 0; pool_no < irep->plen; pool_no++) {
    int ai = gc_arena_save(mrb);

    switch (type(irep->pool[pool_no])) {
    case TT_FIXNUM:
      str = fixnum_to_str(mrb, irep->pool[pool_no], 10);
      {
        int len = RSTRING_LEN(str);
        assert_int_fit(int, len, size_t, SIZE_MAX);
        size += (size_t)len;
      }
      break;

#ifndef WITHOUT_FLOAT
    case TT_FLOAT:
      str = float_to_str(mrb, irep->pool[pool_no], FLOAT_FMT);
      {
        int len = RSTRING_LEN(str);
        assert_int_fit(int, len, size_t, SIZE_MAX);
        size += (size_t)len;
      }
      break;
#endif

    case TT_STRING:
      {
        int len = RSTRING_LEN(irep->pool[pool_no]);
        assert_int_fit(int, len, size_t, SIZE_MAX);
        size += (size_t)len;
      }
      break;

    default:
      break;
    }
    gc_arena_restore(mrb, ai);
  }

  return size;
}

API ptrdiff_t
write_pool_block(state *mrb, irep *irep, uint8_t *buf)
{
  int pool_no;
  uint8_t *cur = buf;
  uint16_t len;
  value str;
  const char *char_ptr;

  cur += uint32_to_bin(irep->plen, cur); /* number of pool */

  for (pool_no = 0; pool_no < irep->plen; pool_no++) {
    int ai = gc_arena_save(mrb);

    switch (type(irep->pool[pool_no])) {
    case TT_FIXNUM:
      cur += uint8_to_bin(IREP_TT_FIXNUM, cur); /* data type */
      str = fixnum_to_str(mrb, irep->pool[pool_no], 10);
      break;

#ifndef WITHOUT_FLOAT
    case TT_FLOAT:
      cur += uint8_to_bin(IREP_TT_FLOAT, cur); /* data type */
      str = float_to_str(mrb, irep->pool[pool_no], FLOAT_FMT);
      break;
#endif

    case TT_STRING:
      cur += uint8_to_bin(IREP_TT_STRING, cur); /* data type */
      str = irep->pool[pool_no];
      break;

    default:
      continue;
    }

    char_ptr = RSTRING_PTR(str);
    {
      int tlen = RSTRING_LEN(str);
      assert_int_fit(int, tlen, uint16_t, UINT16_MAX);
      len = (uint16_t)tlen;
    }

    cur += uint16_to_bin(len, cur); /* data length */
    memcpy(cur, char_ptr, (size_t)len);
    cur += len;

    gc_arena_restore(mrb, ai);
  }

  return cur - buf;
}


API size_t
get_syms_block_size(state *mrb, irep *irep)
{
  size_t size = 0;
  int sym_no;
  int len;

  size += sizeof(uint32_t); /* slen */
  for (sym_no = 0; sym_no < irep->slen; sym_no++) {
    size += sizeof(uint16_t); /* snl(n) */
    if (irep->syms[sym_no] != 0) {
      sym2name_len(mrb, irep->syms[sym_no], &len);
      size += len + 1; /* sn(n) + null char */
    }
  }

  return size;
}

API ptrdiff_t
write_syms_block(state *mrb, irep *irep, uint8_t *buf)
{
  int sym_no;
  uint8_t *cur = buf;
  const char *name;

  cur += uint32_to_bin(irep->slen, cur); /* number of symbol */

  for (sym_no = 0; sym_no < irep->slen; sym_no++) {
    if (irep->syms[sym_no] != 0) {
      int len;

      name = sym2name_len(mrb, irep->syms[sym_no], &len);

      assert_int_fit(int, len, uint16_t, UINT16_MAX);
      cur += uint16_to_bin((uint16_t)len, cur); /* length of symbol name */
      memcpy(cur, name, len); /* symbol name */
      cur += (uint16_t)len;
      *cur++ = '\0';
    }
    else {
      cur += uint16_to_bin(DUMP_NULL_SYM_LEN, cur); /* length of symbol name */
    }
  }

  return cur - buf;
}

API size_t
get_irep_record_size_1(state *mrb, irep *irep)
{
  size_t size = 0;

  size += get_irep_header_size(mrb);
  size += get_iseq_block_size(mrb, irep);
  size += get_pool_block_size(mrb, irep);
  size += get_syms_block_size(mrb, irep);
  return size;
}

API size_t
get_irep_record_size(state *mrb, irep *irep)
{
  size_t size = 0;
  int irep_no;

  size = get_irep_record_size_1(mrb, irep);
  for (irep_no = 0; irep_no < irep->rlen; irep_no++) {
    size += get_irep_record_size(mrb, irep->reps[irep_no]);
  }
  return size;
}

API int
write_irep_record(state *mrb, irep *irep, uint8_t *bin, size_t *irep_record_size, uint8_t flags)
{
  int i;
  uint8_t *src = bin;

  if (irep == NULL) {
    return DUMP_INVALID_IREP;
  }

  *irep_record_size = get_irep_record_size_1(mrb, irep);
  if (*irep_record_size == 0) {
    return DUMP_GENERAL_FAILURE;
  }

  bin += write_irep_header(mrb, irep, bin);
  bin += write_iseq_block(mrb, irep, bin, flags);
  bin += write_pool_block(mrb, irep, bin);
  bin += write_syms_block(mrb, irep, bin);

  for (i = 0; i < irep->rlen; i++) {
    int result;
    size_t rsize;

    result = write_irep_record(mrb, irep->reps[i], bin, &rsize, flags);
    if (result != DUMP_OK) {
      return result;
    }
    bin += rsize;
  }
  *irep_record_size = bin - src;
  return DUMP_OK;
}

API uint32_t
write_footer(state *mrb, uint8_t *bin)
{
  struct rite_binary_footer footer;

  memcpy(footer.section_ident, RITE_BINARY_EOF, sizeof(footer.section_ident));
  uint32_to_bin(sizeof(struct rite_binary_footer), footer.section_size);
  memcpy(bin, &footer, sizeof(struct rite_binary_footer));

  return sizeof(struct rite_binary_footer);
}


API int
write_section_irep_header(state *mrb, size_t section_size, uint8_t *bin)
{
  struct rite_section_irep_header *header = (struct rite_section_irep_header*)bin;

  memcpy(header->section_ident, RITE_SECTION_IREP_IDENT, sizeof(header->section_ident));

  assert_int_fit(size_t, section_size, uint32_t, UINT32_MAX);
  uint32_to_bin((uint32_t)section_size, header->section_size);
  memcpy(header->rite_version, RITE_VM_VER, sizeof(header->rite_version));

  return DUMP_OK;
}

API int
write_section_irep(state *mrb, irep *irep, uint8_t *bin, size_t *len_p, uint8_t flags)
{
  int result;
  size_t rsize = 0;
  uint8_t *cur = bin;

  if (mrb == NULL || bin == NULL) {
    return DUMP_INVALID_ARGUMENT;
  }

  cur += sizeof(struct rite_section_irep_header);

  result = write_irep_record(mrb, irep, cur, &rsize, flags);
  if (result != DUMP_OK) {
    return result;
  }
  *len_p = cur - bin + rsize;
  write_section_irep_header(mrb, *len_p, bin);

  return DUMP_OK;
}

API int
write_section_lineno_header(state *mrb, size_t section_size, uint8_t *bin)
{
  struct rite_section_lineno_header *header = (struct rite_section_lineno_header*)bin;

  memcpy(header->section_ident, RITE_SECTION_LINENO_IDENT, sizeof(header->section_ident));
  uint32_to_bin((uint32_t)section_size, header->section_size);

  return DUMP_OK;
}

API size_t
get_lineno_record_size(state *mrb, irep *irep)
{
  size_t size = 0;

  size += sizeof(uint32_t); /* record size */
  size += sizeof(uint16_t); /* filename size */
  if (irep->filename) {
    size += strlen(irep->filename); /* filename */
  }
  size += sizeof(uint32_t); /* niseq */
  if (irep->lines) {
    size += sizeof(uint16_t) * irep->ilen; /* lineno */
  }

  return size;
}

API size_t
write_lineno_record_1(state *mrb, irep *irep, uint8_t* bin)
{
  uint8_t *cur = bin;
  int iseq_no;
  size_t filename_len;
  ptrdiff_t diff;

  cur += sizeof(uint32_t); /* record size */

  if (irep->filename) {
    filename_len = strlen(irep->filename);
  }
  else {
    filename_len = 0;
  }
  assert_int_fit(size_t, filename_len, uint16_t, UINT16_MAX);
  cur += uint16_to_bin((uint16_t)filename_len, cur); /* filename size */

  if (filename_len) {
    memcpy(cur, irep->filename, filename_len);
    cur += filename_len; /* filename */
  }

  if (irep->lines) {
    assert_int_fit(size_t, irep->ilen, uint32_t, UINT32_MAX);
    cur += uint32_to_bin((uint32_t)(irep->ilen), cur); /* niseq */
    for (iseq_no = 0; iseq_no < irep->ilen; iseq_no++) {
      cur += uint16_to_bin(irep->lines[iseq_no], cur); /* opcode */
    }
  }
  else {
    cur += uint32_to_bin(0, cur); /* niseq */
  }

  diff = cur - bin;
  assert_int_fit(ptrdiff_t, diff, uint32_t, UINT32_MAX);

  uint32_to_bin((uint32_t)diff, bin); /* record size */

  assert_int_fit(ptrdiff_t, diff, size_t, SIZE_MAX);
  return (size_t)diff;
}

API size_t
write_lineno_record(state *mrb, irep *irep, uint8_t* bin)
{
  size_t rlen, size = 0;
  int i;

  rlen = write_lineno_record_1(mrb, irep, bin);
  bin += rlen;
  size += rlen;
  for (i=0; i<irep->rlen; i++) {
    rlen = write_lineno_record(mrb, irep, bin);
    bin += rlen;
    size += rlen;
  }
  return size;
}

API int
write_section_lineno(state *mrb, irep *irep, uint8_t *bin)
{
  size_t section_size = 0;
  size_t rlen = 0; /* size of irep record */
  uint8_t *cur = bin;

  if (mrb == NULL || bin == NULL) {
    return DUMP_INVALID_ARGUMENT;
  }

  cur += sizeof(struct rite_section_lineno_header);
  section_size += sizeof(struct rite_section_lineno_header);

  rlen = write_lineno_record(mrb, irep, cur);
  section_size += rlen;

  write_section_lineno_header(mrb, section_size, bin);

  return DUMP_OK;
}

API size_t
get_debug_record_size(state *mrb, irep *irep)
{
  size_t ret = 0;
  uint16_t f_idx;
  int i;

  ret += sizeof(uint32_t); /* record size */
  ret += sizeof(uint16_t); /* file count */

  for (f_idx = 0; f_idx < irep->debug_info->flen; ++f_idx) {
    irep_debug_info_file const* file = irep->debug_info->files[f_idx];

    ret += sizeof(uint32_t); /* position */
    ret += sizeof(uint16_t); /* filename index */

    /* lines */
    ret += sizeof(uint32_t); /* entry count */
    ret += sizeof(uint8_t); /* line type */
    switch (file->line_type) {
      case debug_line_ary:
        ret += sizeof(uint16_t) * (size_t)(file->line_entry_count);
        break;

      case debug_line_flat_map:
        ret += (sizeof(uint32_t) + sizeof(uint16_t)) * (size_t)(file->line_entry_count);
        break;

      default: assert(0); break;
    }
  }
  for (i=0; i<irep->rlen; i++) {
    ret += get_debug_record_size(mrb, irep->reps[i]);
  }

  return ret;
}

API int
find_filename_index(const sym *ary, int ary_len, sym s)
{
  int i;

  for (i = 0; i < ary_len; ++i) {
    if (ary[i] == s) { return i; }
  }
  return -1;
}

API size_t
get_filename_table_size(state *mrb, irep *irep, sym **fp, uint16_t *lp)
{
  sym *filenames = *fp;
  size_t size = 0;
  irep_debug_info *di = irep->debug_info;
  int i;

  assert(lp);
  for (i = 0; i < di->flen; ++i) {
    irep_debug_info_file *file;
    int filename_len;

    file = di->files[i];
    if (find_filename_index(filenames, *lp, file->filename_sym) == -1) {
      /* register filename */
      *lp += 1;
      *fp = filenames = (sym *)realloc(mrb, filenames, sizeof(sym) * (*lp));
      filenames[*lp - 1] = file->filename_sym;

      /* filename */
      sym2name_len(mrb, file->filename_sym, &filename_len);
      size += sizeof(uint16_t) + (size_t)filename_len;
    }
  }
  for (i=0; i<irep->rlen; i++) {
    size += get_filename_table_size(mrb, irep->reps[i], fp, lp);
  }
  return size;
}

API size_t
write_debug_record_1(state *mrb, irep *irep, uint8_t *bin, sym const* filenames, uint16_t filenames_len)
{
  uint8_t *cur;
  uint16_t f_idx;
  ptrdiff_t ret;

  cur = bin + sizeof(uint32_t); /* skip record size */
  cur += uint16_to_bin(irep->debug_info->flen, cur); /* file count */

  for (f_idx = 0; f_idx < irep->debug_info->flen; ++f_idx) {
    int filename_idx;
    const irep_debug_info_file *file = irep->debug_info->files[f_idx];

    /* position */
    cur += uint32_to_bin(file->start_pos, cur);

    /* filename index */
    filename_idx = find_filename_index(filenames, filenames_len,
                                                  file->filename_sym);
    assert_int_fit(int, filename_idx, uint16_t, UINT16_MAX);
    cur += uint16_to_bin((uint16_t)filename_idx, cur);

    /* lines */
    cur += uint32_to_bin(file->line_entry_count, cur);
    cur += uint8_to_bin(file->line_type, cur);
    switch (file->line_type) {
      case debug_line_ary: {
        uint32_t l;
        for (l = 0; l < file->line_entry_count; ++l) {
          cur += uint16_to_bin(file->lines.ary[l], cur);
        }
      } break;

      case debug_line_flat_map: {
        uint32_t line;
        for (line = 0; line < file->line_entry_count; ++line) {
          cur += uint32_to_bin(file->lines.flat_map[line].start_pos, cur);
          cur += uint16_to_bin(file->lines.flat_map[line].line, cur);
        }
      } break;

      default: assert(0); break;
    }
  }

  ret = cur - bin;
  assert_int_fit(ptrdiff_t, ret, uint32_t, UINT32_MAX);
  uint32_to_bin((uint32_t)ret, bin);

  assert_int_fit(ptrdiff_t, ret, size_t, SIZE_MAX);
  return (size_t)ret;
}

API size_t
write_debug_record(state *mrb, irep *irep, uint8_t *bin, sym const* filenames, uint16_t filenames_len)
{
  size_t size, len;
  int irep_no;

  size = len = write_debug_record_1(mrb, irep, bin, filenames, filenames_len);
  bin += len;
  for (irep_no = 0; irep_no < irep->rlen; irep_no++) {
    len = write_debug_record(mrb, irep->reps[irep_no], bin, filenames, filenames_len);
    bin += len;
    size += len;
  }

  assert(size == get_debug_record_size(mrb, irep));
  return size;
}

API int
write_section_debug(state *mrb, irep *irep, uint8_t *cur, sym const *filenames, uint16_t filenames_len)
{
  size_t section_size = 0;
  const uint8_t *bin = cur;
  struct rite_section_debug_header *header;
  size_t dlen;
  uint16_t i;
  char const *sym; int sym_len;

  if (mrb == NULL || cur == NULL) {
    return DUMP_INVALID_ARGUMENT;
  }

  header = (struct rite_section_debug_header *)bin;
  cur += sizeof(struct rite_section_debug_header);
  section_size += sizeof(struct rite_section_debug_header);

  /* filename table */
  cur += uint16_to_bin(filenames_len, cur);
  section_size += sizeof(uint16_t);
  for (i = 0; i < filenames_len; ++i) {
    sym = sym2name_len(mrb, filenames[i], &sym_len);
    assert(sym);
    cur += uint16_to_bin((uint16_t)sym_len, cur);
    memcpy(cur, sym, sym_len);
    cur += sym_len;
    section_size += sizeof(uint16_t) + sym_len;
  }

  /* debug records */
  dlen = write_debug_record(mrb, irep, cur, filenames, filenames_len);
  section_size += dlen;

  memcpy(header->section_ident, RITE_SECTION_DEBUG_IDENT, sizeof(header->section_ident));
  assert(section_size <= INT32_MAX);
  uint32_to_bin((uint32_t)section_size, header->section_size);

  return DUMP_OK;
}

API void
create_lv_sym_table(state *mrb, const irep *irep, sym **syms, uint32_t *syms_len)
{
  int i;

  if (*syms == NULL) {
    *syms = (sym*)malloc(mrb, sizeof(sym) * 1);
  }

  for (i = 0; i + 1 < irep->nlocals; ++i) {
    sym const name = irep->lv[i].name;
    if (name == 0) continue;
    if (find_filename_index(*syms, *syms_len, name) != -1) continue;

    ++(*syms_len);
    *syms = (sym*)realloc(mrb, *syms, sizeof(sym) * (*syms_len));
    (*syms)[*syms_len - 1] = name;
  }

  for (i = 0; i < irep->rlen; ++i) {
    create_lv_sym_table(mrb, irep->reps[i], syms, syms_len);
  }
}

API int
write_lv_sym_table(state *mrb, uint8_t **start, sym const *syms, uint32_t syms_len)
{
  uint8_t *cur = *start;
  uint32_t i;
  const char *str;
  int str_len;

  cur += uint32_to_bin(syms_len, cur);

  for (i = 0; i < syms_len; ++i) {
    str = sym2name_len(mrb, syms[i], &str_len);
    cur += uint16_to_bin((uint16_t)str_len, cur);
    memcpy(cur, str, str_len);
    cur += str_len;
  }

  *start = cur;

  return DUMP_OK;
}

API int
write_lv_record(state *mrb, const irep *irep, uint8_t **start, sym const *syms, uint32_t syms_len)
{
  uint8_t *cur = *start;
  int i;

  for (i = 0; i + 1 < irep->nlocals; ++i) {
    if (irep->lv[i].name == 0) {
      cur += uint16_to_bin(RITE_LV_NULL_MARK, cur);
      cur += uint16_to_bin(0, cur);
    }
    else {
      int const sym_idx = find_filename_index(syms, syms_len, irep->lv[i].name);
      assert(sym_idx != -1); /* local variable name must be in syms */

      cur += uint16_to_bin(sym_idx, cur);
      cur += uint16_to_bin(irep->lv[i].r, cur);
    }
  }

  for (i = 0; i < irep->rlen; ++i) {
    write_lv_record(mrb, irep->reps[i], &cur, syms, syms_len);
  }

  *start = cur;

  return DUMP_OK;
}

API size_t
get_lv_record_size(state *mrb, irep *irep)
{
  size_t ret = 0;
  int i;

  ret += (sizeof(uint16_t) + sizeof(uint16_t)) * (irep->nlocals - 1);

  for (i = 0; i < irep->rlen; ++i) {
    ret += get_lv_record_size(mrb, irep->reps[i]);
  }

  return ret;
}

API size_t
get_lv_section_size(state *mrb, irep *irep, sym const *syms, uint32_t syms_len)
{
  size_t ret = 0, i;

  ret += sizeof(uint32_t); /* syms_len */
  ret += sizeof(uint16_t) * syms_len; /* symbol name lengths */
  for (i = 0; i < syms_len; ++i) {
    int str_len;
    sym2name_len(mrb, syms[i], &str_len);
    ret += str_len;
  }

  ret += get_lv_record_size(mrb, irep);

  return ret;
}

API int
write_section_lv(state *mrb, irep *irep, uint8_t *start, sym const *syms, uint32_t const syms_len)
{
  uint8_t *cur = start;
  struct rite_section_lv_header *header;
  ptrdiff_t diff;
  int result = DUMP_OK;

  if (mrb == NULL || cur == NULL) {
    return DUMP_INVALID_ARGUMENT;
  }

  header = (struct rite_section_lv_header*)cur;
  cur += sizeof(struct rite_section_lv_header);

  result = write_lv_sym_table(mrb, &cur, syms, syms_len);
  if (result != DUMP_OK) {
    goto lv_section_exit;
  }

  result = write_lv_record(mrb, irep, &cur, syms, syms_len);
  if (result != DUMP_OK) {
    goto lv_section_exit;
  }

  memcpy(header->section_ident, RITE_SECTION_LV_IDENT, sizeof(header->section_ident));

  diff = cur - start;
  assert_int_fit(ptrdiff_t, diff, size_t, SIZE_MAX);
  uint32_to_bin((uint32_t)diff, header->section_size);

lv_section_exit:
  return result;
}

API int
write_rite_binary_header(state *mrb, size_t binary_size, uint8_t *bin, uint8_t flags)
{
  struct rite_binary_header *header = (struct rite_binary_header *)bin;
  uint16_t crc;
  uint32_t offset;

  switch (flags & DUMP_ENDIAN_NAT) {
  endian_big:
  case DUMP_ENDIAN_BIG:
    memcpy(header->binary_ident, RITE_BINARY_IDENT, sizeof(header->binary_ident));
    break;
  endian_little:
  case DUMP_ENDIAN_LIL:
    memcpy(header->binary_ident, RITE_BINARY_IDENT_LIL, sizeof(header->binary_ident));
    break;

  case DUMP_ENDIAN_NAT:
    if (bigendian_p()) goto endian_big;
    goto endian_little;
    break;
  }

  memcpy(header->binary_version, RITE_BINARY_FORMAT_VER, sizeof(header->binary_version));
  memcpy(header->compiler_name, RITE_COMPILER_NAME, sizeof(header->compiler_name));
  memcpy(header->compiler_version, RITE_COMPILER_VERSION, sizeof(header->compiler_version));
  assert(binary_size <= UINT32_MAX);
  uint32_to_bin((uint32_t)binary_size, header->binary_size);

  offset = (uint32_t)((&(header->binary_crc[0]) - bin) + sizeof(uint16_t));
  crc = calc_crc_16_ccitt(bin + offset, binary_size - offset, 0);
  uint16_to_bin(crc, header->binary_crc);

  return DUMP_OK;
}

API bool
is_debug_info_defined(irep *irep)
{
  int i;

  if (!irep->debug_info) return FALSE;
  for (i=0; i<irep->rlen; i++) {
    if (!is_debug_info_defined(irep->reps[i])) return FALSE;
  }
  return TRUE;
}

API bool
is_lv_defined(irep *irep)
{
  int i;

  if (irep->lv) { return TRUE; }

  for (i = 0; i < irep->rlen; ++i) {
    if (is_lv_defined(irep->reps[i])) { return TRUE; }
  }

  return FALSE;
}

API uint8_t
dump_flags(uint8_t flags, uint8_t native)
{
  if (native == FLAG_BYTEORDER_NATIVE) {
    if ((flags & DUMP_ENDIAN_NAT) == 0) {
      return (flags & DUMP_DEBUG_INFO) | DUMP_ENDIAN_NAT;
    }
    return flags;
  }
  if ((flags & DUMP_ENDIAN_NAT) == 0) {
    return (flags & DUMP_DEBUG_INFO) | DUMP_ENDIAN_BIG;
  }
  return flags;
}

API int
dump_irep(state *mrb, irep *irep, uint8_t flags, uint8_t **bin, size_t *bin_size)
{
  int result = DUMP_GENERAL_FAILURE;
  size_t malloc_size;
  size_t section_irep_size;
  size_t section_lineno_size = 0, section_lv_size = 0;
  uint8_t *cur = NULL;
  bool const debug_info_defined = is_debug_info_defined(irep), lv_defined = is_lv_defined(irep);
  sym *lv_syms = NULL; uint32_t lv_syms_len = 0;
  sym *filenames = NULL; uint16_t filenames_len = 0;

  if (mrb == NULL) {
    *bin = NULL;
    return DUMP_GENERAL_FAILURE;
  }

  section_irep_size = sizeof(struct rite_section_irep_header);
  section_irep_size += get_irep_record_size(mrb, irep);

  /* DEBUG section size */
  if (flags & DUMP_DEBUG_INFO) {
    if (debug_info_defined) {
      section_lineno_size += sizeof(struct rite_section_debug_header);
      /* filename table */
      filenames = (sym*)malloc(mrb, sizeof(sym) + 1);

      /* filename table size */
      section_lineno_size += sizeof(uint16_t);
      section_lineno_size += get_filename_table_size(mrb, irep, &filenames, &filenames_len);

      section_lineno_size += get_debug_record_size(mrb, irep);
    }
    else {
      section_lineno_size += sizeof(struct rite_section_lineno_header);
      section_lineno_size += get_lineno_record_size(mrb, irep);
    }
  }

  if (lv_defined) {
    section_lv_size += sizeof(struct rite_section_lv_header);
    create_lv_sym_table(mrb, irep, &lv_syms, &lv_syms_len);
    section_lv_size += get_lv_section_size(mrb, irep, lv_syms, lv_syms_len);
  }

  malloc_size = sizeof(struct rite_binary_header) +
                section_irep_size + section_lineno_size + section_lv_size +
                sizeof(struct rite_binary_footer);
  cur = *bin = (uint8_t*)malloc(mrb, malloc_size);
  cur += sizeof(struct rite_binary_header);

  result = write_section_irep(mrb, irep, cur, &section_irep_size, flags);
  if (result != DUMP_OK) {
    goto error_exit;
  }
  cur += section_irep_size;
  *bin_size = sizeof(struct rite_binary_header) +
              section_irep_size + section_lineno_size + section_lv_size +
              sizeof(struct rite_binary_footer);

  /* write DEBUG section */
  if (flags & DUMP_DEBUG_INFO) {
    if (debug_info_defined) {
      result = write_section_debug(mrb, irep, cur, filenames, filenames_len);
    }
    else {
      result = write_section_lineno(mrb, irep, cur);
    }
    if (result != DUMP_OK) {
      goto error_exit;
    }
    cur += section_lineno_size;
  }

  if (lv_defined) {
    result = write_section_lv(mrb, irep, cur, lv_syms, lv_syms_len);
    if (result != DUMP_OK) {
      goto error_exit;
    }
    cur += section_lv_size;
  }

  write_footer(mrb, cur);
  write_rite_binary_header(mrb, *bin_size, *bin, flags);

error_exit:
  if (result != DUMP_OK) {
    free(mrb, *bin);
    *bin = NULL;
  }
  free(mrb, lv_syms);
  free(mrb, filenames);
  return result;
}

int
dump_irep_(state *mrb, irep *irep, uint8_t flags, uint8_t **bin, size_t *bin_size)
{
  return dump_irep(mrb, irep, dump_flags(flags, FLAG_BYTEORDER_NONATIVE), bin, bin_size);
}

#ifndef DISABLE_STDIO

int
dump_irep_binary(state *mrb, irep *irep, uint8_t flags, FILE* fp)
{
  uint8_t *bin = NULL;
  size_t bin_size = 0;
  int result;

  if (fp == NULL) {
    return DUMP_INVALID_ARGUMENT;
  }

  result = dump_irep(mrb, irep, dump_flags(flags, FLAG_BYTEORDER_NONATIVE), &bin, &bin_size);
  if (result == DUMP_OK) {
    if (fwrite(bin, sizeof(bin[0]), bin_size, fp) != bin_size) {
      result = DUMP_WRITE_FAULT;
    }
  }

  free(mrb, bin);
  return result;
}

API bool
dump_bigendian_p(uint8_t flags)
{
  switch (flags & DUMP_ENDIAN_NAT) {
  case DUMP_ENDIAN_BIG:
    return TRUE;
  case DUMP_ENDIAN_LIL:
    return FALSE;
  default:
  case DUMP_ENDIAN_NAT:
    return bigendian_p();
  }
}

int
dump_irep_cfunc(state *mrb, irep *irep, uint8_t flags, FILE *fp, const char *initname)
{
  uint8_t *bin = NULL;
  size_t bin_size = 0, bin_idx = 0;
  int result;

  if (fp == NULL || initname == NULL || initname[0] == '\0') {
    return DUMP_INVALID_ARGUMENT;
  }
  flags = dump_flags(flags, FLAG_BYTEORDER_NATIVE);
  result = dump_irep(mrb, irep, flags, &bin, &bin_size);
  if (result == DUMP_OK) {
    if (!dump_bigendian_p(flags)) {
      if (fprintf(fp, "/* dumped in little endian order.\n"
                  "   use `mrbc -E` option for big endian CPU. */\n") < 0) {
        free(mrb, bin);
        return DUMP_WRITE_FAULT;
      }
    }
    else {
      if (fprintf(fp, "/* dumped in big endian order.\n"
                  "   use `mrbc -e` option for better performance on little endian CPU. */\n") < 0) {
        free(mrb, bin);
        return DUMP_WRITE_FAULT;
      }
    }
    if (fprintf(fp, "#include <stdint.h>\n") < 0) { /* for uint8_t under at least Darwin */
      free(mrb, bin);
      return DUMP_WRITE_FAULT;
    }
    if (fprintf(fp,
          "extern const uint8_t %s[];\n"
          "const uint8_t\n"
          "#if defined __GNUC__\n"
          "__attribute__((aligned(%u)))\n"
          "#elif defined _MSC_VER\n"
          "__declspec(align(%u))\n"
          "#endif\n"
          "%s[] = {",
          initname,
          (uint16_t)DUMP_ALIGNMENT, (uint16_t)DUMP_ALIGNMENT, initname) < 0) {
      free(mrb, bin);
      return DUMP_WRITE_FAULT;
    }
    while (bin_idx < bin_size) {
      if (bin_idx % 16 == 0) {
        if (fputs("\n", fp) == EOF) {
          free(mrb, bin);
          return DUMP_WRITE_FAULT;
        }
      }
      if (fprintf(fp, "0x%02x,", bin[bin_idx++]) < 0) {
        free(mrb, bin);
        return DUMP_WRITE_FAULT;
      }
    }
    if (fputs("\n};\n", fp) == EOF) {
      free(mrb, bin);
      return DUMP_WRITE_FAULT;
    }
  }

  free(mrb, bin);
  return result;
}

#endif /* DISABLE_STDIO */
