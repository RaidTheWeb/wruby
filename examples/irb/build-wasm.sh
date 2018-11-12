# http://www.blacktm.com/blog/ruby-on-webassembly
# https://github.com/noontage/test-mruby-wasm

MRUBY_HOME=../.. 
MRUBY_HOME=/opt/mruby/

# --emit-symbol-map -s "EXTRA_EXPORTED_RUNTIME_METHODS=['ccall']" --pre-js -s MODULARIZE=1
DEV_CONFIG="-s LINKABLE=1 -g3 -s DEMANGLE_SUPPORT=1"
RELEASE_CONFIG="-Os --closure 1"
# EMCC_DEBUG=1 emcc $DEV_CONFIG -I $MRUBY_HOME/include mirb.c libmruby.a -o wruby.js
emcc $RELEASE_CONFIG -I $MRUBY_HOME/include mirb.c libmruby.a -o wruby.irb.min.js 

# --embed-file MODULE_ADDITIONS.js
cat MODULE_ADDITIONS.js >> wruby.js