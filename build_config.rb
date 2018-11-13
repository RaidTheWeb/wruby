MRuby::Build.new do |conf|
  toolchain :gcc
  conf.gembox 'default'
  conf.gem :github => 'mattn/mruby-require'
end

MRuby::CrossBuild.new('emscripten') do |conf|
  toolchain :clang
  conf.gembox 'default'
  conf.cc.command = 'emcc'
  conf.cc.flags = %W(-Os)
  conf.linker.command = 'emcc'
  conf.archiver.command = 'emar'
end

# Define wasm build settings
# MRuby::CrossBuild.new('wasm') do |conf|
#   toolchain :clang

#   # C compiler settings
#   conf.cc do |cc|
#     cc.command = 'emcc'
#     cc.flags = [ENV['CFLAGS'] || %w()]
#     cc.include_paths = ["#{root}/include"]
#     cc.option_include_path = '-I%s'
#     cc.option_define = '-D%s'
#     cc.compile_options = "%{flags} -c %{infile} -s WASM=1 -o %{outfile}"
#   end

#   # Archiver settings
#   conf.archiver do |archiver|
#     archiver.command = 'emcc'
#     archiver.archive_options = '%{objs} -s WASM=1 -o %{outfile}'
#   end

#   # file extensions
#   conf.exts do |exts|
#     exts.object = '.bc'
#     exts.executable = '' # '.exe' if Windows
#     exts.library = '.bc'
#   end

#   conf.gembox 'wasm'  
# end