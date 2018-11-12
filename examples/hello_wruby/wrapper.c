/*
mrbc -Bprogram.c program.rb # generates binary module
mrbc program.rb # generates program.mrb binary module
ld -r -b binary program.mrb -o program.o # can be linked with wrapper.c
objdump -x program.o || nm program.o # check ok works
gcc -fasm-blocks -I /opt/mruby/include program.o wrapper.c -owrapper # test
emcc  -I /opt/mruby/include wrapper.c libmruby.a -o program.js -s LINKABLE=1 # debug
emcc -s WASM=1  -Os -I /opt/mruby/include program.o wrapper.c libmruby.a -o program.min.js  --closure 1 #optimized
wasm-dis program.wasm> program.wast
*/


// WITH UNDERSCORE???
#import "program.c"
// extern const char _binary_program_mrb_start;
// extern const char _binary_program_mrb_end;
// extern const int _binary_program_mrb_size;

#include <mruby.h>
#include <mruby/irep.h>


void load_module(uint8_t* mrb_program){
  mrb_state *mrb = mrb_open();
  mrb_load_irep(mrb, mrb_program);
  mrb_close(mrb);
}

int main() {
	load_module((uint8_t*)&_binary_program_mrb_start);
}


// int main(int mrb_program) {
// 	load_module((uint8_t*)mrb_program||(uint8_t*)&_binary_program_mrb_start);
// }
