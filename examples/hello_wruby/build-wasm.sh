# http://www.blacktm.com/blog/ruby-on-webassembly
# https://github.com/noontage/test-mruby-wasm

cp hello_ruby.rb program.rb
../../bin/mrbc -B_binary_program_mrb_start program.rb # generates program.c 
emcc -s WASM=1 -Os -I /opt/mruby/include wrapper.c /opt/mruby/build/emscripten/lib/libmruby.a -o program.js --closure 1 -s DEMANGLE_SUPPORT=1 -Wextra-tokens




../../bin/mrbc program.rb # generates program.mrb binary module
ld -r -b binary program.mrb -o program.o # can be linked with wrapper.c
ld -r -b binary program.mrb -o data.o # can be linked with wrapper.c
objdump -x program.o || nm program.o # check ok works

# name=wrapper.c
# pure=hello_ruby
# clang_options="-DWASM -O1 -dM -Wno-main -I /opt/mruby/include" 
# clang $clang_options --target=wasm32-unknown-unknown-wasm -emit-llvm -c "$name" -o program.bc || exit
# /opt/llvm/bin/llc -march=wasm32 -filetype=obj program.bc -o $pure.o #|| exit
# /opt/llvm/bin/wasm-ld -flavor wasm --no-entry $wasm_ld_options --demangle --allow-undefined $pure.o -o $pure.wasm || exit

# clang -DWASM -O1 -dM -Wno-main --target=wasm32-unknown-unknown-wasm -emit-llvm -c data.o program.c -o program.o
emcc data.o program.c -o program.o
emcc -s WASM=1 -Os -I /opt/mruby/include program.o wrapper.c /opt/mruby/build/emscripten/lib/libmruby.a -o program.js
 --closure 1
# ln -s hello_ruby.html index.html
open http://0.0.0.0:8000 && python3 -m http.server

# emcc -s SIDE_MODULE=1 -I /opt/mruby/include wrapper.c /opt/mruby/build/emscripten/lib/libmruby.a -o hello_ruby.full.wasm  