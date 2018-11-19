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
// extern const char _binary_program__start;
// extern const char _binary_program__end;
// extern const int _binary_program__size;
uint8_t* other_module;

#include <stdlib.h>
// returns address of program to be written via js
int reserve_mrb(int size){
	if(size>0)
		other_module=(uint8_t *)malloc(size);// new one
	else
		other_module=(uint8_t *)_binary_program__start;// old one
	return (int)&other_module;
}


// include/mruby/common.h:# define MRB_API __declspec(dllexport)
// include/mruby/common.h:# define MRB_API __declspec(dllimport)
// include/mruby/common.h:# define MRB_API extern

// void _vm_const_set(_state*, _sym, _value);
// MRB_API _value _const_get(_state*, _value, _sym);
// MRB_API void _const_set(_state*, _value, _sym, _value);
// MRB_API _bool _const_defined(_state*, _value, _sym);


/*
typedef struct _value {
  union {
    _float f;
    void *p;
    _int i;
    _sym sym;
  } value;
  enum _vtype tt;
} _value;
*/

int run_mrb(uint8_t* _program){
  _state *mrb = _open();
  // MRB_API 
  _value result=_load_irep(mrb, _program);
  // _load_string(mrb, "puts 'hello world'");
  _close(mrb);
  return (int)result.value.i;
}

int main() {
	run_mrb((uint8_t*)&_binary_program__start);
}