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

#include <mruby.h>
#include <mruby/irep.h>

#import "program.c"
// extern const char _binary_program_mrb_start;
// extern const char _binary_program_mrb_end;
// extern const int _binary_program_mrb_size;
uint8_t* other_module;

#include <stdlib.h>
// returns address of program to be written via js
int reserve_mrb(int size){
	if(size>0)
		other_module=(uint8_t *)malloc(size);// new one
	else
		other_module=(uint8_t *)_binary_program_mrb_start;// old one
	return (int)&other_module;
}


int run_mrb(uint8_t* mrb_program){
  mrb_state *mrb = mrb_open();
  mrb_load_irep(mrb, mrb_program);
  // mrb_load_string(mrb, "puts 'hello world'");
  mrb_close(mrb);
  return (int)mrb_program;
}

int main() {
	run_mrb((uint8_t*)&_binary_program_mrb_start);
}