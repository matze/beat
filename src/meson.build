beat_sources = [
  'main.c',
  'beat-window.c',
]

beat_deps = [
  dependency('gio-2.0', version: '>= 2.48'),
  dependency('gtk+-3.0', version: '>= 3.18'),
  dependency('libcanberra', version: '>= 0.30'),
]

gnome = import('gnome')

beat_sources += gnome.compile_resources(
  'beat-resources',
  join_paths('resources', 'beat.gresource.xml'),
  source_dir: 'resources',
  c_name: 'beat'
)

executable('beat', beat_sources,
  dependencies: beat_deps,
  install: true,
)
