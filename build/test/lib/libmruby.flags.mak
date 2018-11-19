MRUBY_CFLAGS = -g -std=gnu99 -O3 -Wall -Werror-implicit-function-declaration -Wdeclaration-after-statement -Wwrite-strings -g3 -O0 -DMRB_DEBUG -I"/opt/mruby/include"
MRUBY_LDFLAGS =  -L/opt/mruby/build/test/lib
MRUBY_LDFLAGS_BEFORE_LIBS = 
MRUBY_LIBS = -lmruby -lm -lreadline -lncurses
MRUBY_LIBMRUBY_PATH = /opt/mruby/build/test/lib/libmruby.a
