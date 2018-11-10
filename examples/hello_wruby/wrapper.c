// # ../../bin/mrbc -Bprogram.c program.rb
// ../../bin/mrbc program.rb # generates program.mrb binary module
// ld -r -b binary program.mrb -o program.o # can be linked with wrapper.c
// objdump -x program.o || nm program.o # check ok works
// gcc -fasm-blocks -I /opt/mruby/include program.o wrapper.c -owrapper # test
// emcc -s WASM=1  -Os -I /opt/mruby/include program.o wrapper.c /opt/mruby/build/emscripten/lib/libmruby.a -o program.js  --closure 1



// WITH UNDERSCORE???
#import "program.c"
// extern const char _binary_program_mrb_start;
// extern const char _binary_program_mrb_end;
// extern const int _binary_program_mrb_size;

#include <mruby.h>
#include <mruby/irep.h>

int main() {
  mrb_state *mrb = mrb_open();
  mrb_load_irep(mrb,(uint8_t*) &_binary_program_mrb_start);
  mrb_close(mrb);
  return 0;
}