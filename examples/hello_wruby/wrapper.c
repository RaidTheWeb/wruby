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
// extern const char _binary_program_$start;
// extern const char _binary_program_$end;
// extern const int _binary_program_$size;
uint8_t* other_module;

#include <stdlib.h>
// returns address of program to be written via js
int reserve_mrb(int size){
	if(size>0)
		other_module=(uint8_t *)malloc(size);// new one
	else
		other_module=(uint8_t *)_binary_program_$start;// old one
	return (int)&other_module;
}


// include/mruby/common.h:# define $API __declspec(dllexport)
// include/mruby/common.h:# define $API __declspec(dllimport)
// include/mruby/common.h:# define $API extern

// void $vm_const_set($state*, $sym, $value);
// $API $value $const_get($state*, $value, $sym);
// $API void $const_set($state*, $value, $sym, $value);
// $API $bool $const_defined($state*, $value, $sym);


/*
typedef struct $value {
  union {
    $float f;
    void *p;
    $int i;
    $sym sym;
  } value;
  enum $vtype tt;
} $value;
*/

int run_mrb(uint8_t* $program){
  $state *mrb = $open();
  // $API
  $value result=$load_irep(mrb, $program);
  // $load_string(mrb, "puts 'hello world'");
  $close(mrb);
  return (int)result.value.i;
}

int main() {
	run_mrb((uint8_t*)&_binary_program_$start);
}