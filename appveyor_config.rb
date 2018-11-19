MRuby::Build.new('debug') do |conf|
  toolchain :visualcpp
  enable_debug

  # include all core GEMs
  conf.gembox 'full-core'
  conf.compilers.each do |c|
    c.defines += %w($GC_STRESS $GC_FIXED_ARENA $METHOD_CACHE)
  end

  build_mrbc_exec
end

MRuby::Build.new('full-debug') do |conf|
  toolchain :visualcpp
  enable_debug

  # include all core GEMs
  conf.gembox 'full-core'
  conf.cc.defines = %w($ENABLE_DEBUG_HOOK)

  conf.enable_test
end

MRuby::Build.new do |conf|
  toolchain :visualcpp

  # include all core GEMs
  conf.gembox 'full-core'
  conf.compilers.each do |c|
    c.defines += %w($GC_FIXED_ARENA)
  end
  conf.enable_bintest
  conf.enable_test
end

MRuby::Build.new('cxx_abi') do |conf|
  toolchain :visualcpp

  conf.gembox 'full-core'
  conf.compilers.each do |c|
    c.defines += %w($GC_FIXED_ARENA)
  end
  conf.enable_bintest
  conf.enable_test

  enable_cxx_abi

  build_mrbc_exec
end
