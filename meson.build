project('L-SMASH-Works', 'cpp',
  default_options : ['buildtype=release', 'b_ndebug=if-release', 'c_std=c++17'],
  meson_version : '>=0.48.0'
)

add_project_arguments('-ffast-math', language : 'cpp')

sources = [
  'src/audiosource.cpp',
  'src/audiosource.h',
  'src/vapoursynth.cpp'
]

vapoursynth_dep = dependency('vapoursynth').partial_dependency(compile_args : true, includes : true)

deps = [
  vapoursynth_dep,
  dependency('libavcodec', version : '>=58.18.0'),
  dependency('libavformat', version : '>=58.12.0')
]

if host_machine.cpu_family().startswith('x86')
  add_project_arguments('-mfpmath=sse', '-msse2', language : 'cpp')
endif

if host_machine.system() == 'windows'
  add_project_arguments('-D__USE_MINGW_ANSI_STDIO', language : 'cpp')
endif

shared_module('bestaudiosource', sources,
  dependencies : deps,
  install : true,
  install_dir : join_paths(vapoursynth_dep.get_pkgconfig_variable('libdir'), 'vapoursynth'),
  gnu_symbol_visibility : 'hidden'
)
