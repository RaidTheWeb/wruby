# http://www.blacktm.com/blog/ruby-on-webassembly
# https://github.com/noontage/test-mruby-wasm

cp hello_ruby.rb program.rb
echo "../../bin/mrbc -B_binary_program_mrb_start program.rb # generates program.c "
../../bin/mrbc -B_binary_program_mrb_start program.rb # generates program.c 
echo "emcc -Os -I ../../mruby/include wrapper.c libmruby.a -o program.js"
emcc -I /opt/mruby/include wrapper.c libmruby.a -o program.js -s DEMANGLE_SUPPORT=1 #develop
echo "add -Os --closure 1 for size-optimized release"
emcc -Os -I /opt/mruby/include wrapper.c libmruby.a -o program.min.js --closure 1

time node program.js
# Hello wruby!

# real	0m6.789s
# user	0m7.023s
# sys	0m0.893s

# 0.5 MB ok, but 7 sec to run? unacceptable

# time node program.min.js
# Hello wruby!

# real	0m2.554s

# better, but ...

# time ruby -e "puts 'hi'"
# hi
# real	0m0.070s

# why so slow?
node --prof  program.js
node --prof-process isolate* > program.stats
more program.stats