# http://www.blacktm.com/blog/ruby-on-webassembly
# https://github.com/noontage/test-mruby-wasm

cp hello_ruby.rb program.rb
echo "../../bin/mrbc -B_binary_program_mrb_start program.rb # generates program.c "
../../bin/mrbc -B_binary_program_mrb_start program.rb # generates program.c 
echo "emcc -Os -I ../../mruby/include wrapper.c libmruby.a -o program.js"
emcc -Os -I ../../mruby/include wrapper.c libmruby.a -o program.js --closure 1 -s DEMANGLE_SUPPORT=1 