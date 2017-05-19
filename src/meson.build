sources = [
  'e-cal-data-model-subscriber.h',
  'e-cal-data-model-subscriber.c',
  'e-cal-data-model.h',
  'e-cal-data-model.c',
  'main.c',
  'css-code.h',
  join_paths('views', 'gcal-month-view.c'),
  join_paths('views', 'gcal-month-view.h'),
  join_paths('views', 'gcal-range-tree.c'),
  join_paths('views', 'gcal-range-tree.h'),
  join_paths('views', 'gcal-week-grid.c'),
  join_paths('views', 'gcal-week-grid.h'),
  join_paths('views', 'gcal-week-header.c'),
  join_paths('views', 'gcal-week-header.h'),
  join_paths('views', 'gcal-week-view.c'),
  join_paths('views', 'gcal-week-view.h'),
  join_paths('views', 'gcal-year-view.c'),
  join_paths('views', 'gcal-year-view.h'),
  'gcal-application.c',
  'gcal-application.h',
  'gcal-clock.c',
  'gcal-clock.h',
  'gcal-date-chooser-day.c',
  'gcal-date-chooser-day.h',
  'gcal-date-chooser.c',
  'gcal-date-chooser.h',
  'gcal-date-selector.c',
  'gcal-date-selector.h',
  'gcal-edit-dialog.c',
  'gcal-edit-dialog.h',
  'gcal-event.c',
  'gcal-event.h',
  'gcal-event-widget.c',
  'gcal-event-widget.h',
  'gcal-log.c',
  'gcal-log.h',
  'gcal-manager.c',
  'gcal-manager.h',
  'gcal-multi-choice.c',
  'gcal-multi-choice.h',
  'gcal-quick-add-popover.c',
  'gcal-quick-add-popover.h',
  'gcal-search-view.h',
  'gcal-search-view.c',
  'gcal-shell-search-provider.h',
  'gcal-shell-search-provider.c',
  'gcal-source-dialog.c',
  'gcal-source-dialog.h',
  'gcal-subscriber-view.c',
  'gcal-subscriber-view.h',
  'gcal-subscriber-view-private.h',
  'gcal-time-selector.c',
  'gcal-time-selector.h',
  'gcal-utils.c',
  'gcal-utils.h',
  'gcal-view.c',
  'gcal-view.h',
  'gcal-window.h',
  'gcal-window.c'
]

data_dir = join_paths(meson.source_root(), 'data')

sources += gnome.compile_resources(
  'gcal-resources',
  join_paths(data_dir, 'calendar.gresource.xml'),
  export: true,
  source_dir: data_dir,
  c_name: 'calendar'
)

enum_types = 'gcal-enum-types'

sources += gnome.mkenums(
  enum_types,
  sources: 'gcal-utils.h',
  c_template: '@0@.c.template'.format(enum_types),
  h_template: '@0@.h.template'.format(enum_types)
)

sources += gnome.gdbus_codegen(
  'gcal-shell-search-provider-generated',
  join_paths(data_dir, 'shell-search-provider-dbus-interfaces.xml'),
  interface_prefix: 'org.gnome.',
  namespace: 'Gcal'
)

debug = 'gcal-debug.h'

sources += configure_file(
  input: '@0@.in'.format(debug),
  output: debug,
  configuration: conf
)

ldflags = [
  '-Wl,--export-dynamic'
]

gnome_calendar_ldflags = []

if host_machine.system().contains('linux')
  foreach ldflag: ldflags
    if cc.has_argument(ldflag)
      gnome_calendar_ldflags += ldflag
    endif
  endforeach
endif

gnome_calendar_inc = [
  top_inc,
  include_directories(
    'views'
  )
]

executable(
  meson.project_name(),
  sources,
  include_directories: gnome_calendar_inc,
  dependencies: gnome_calendar_dep,
  c_args: common_flags + debug_flags,
  link_args: gnome_calendar_ldflags,
  install: true,
  install_dir: gnome_calendar_bindir
)

libgcal_lib = shared_library(
  'gcal',
  sources: sources,
  include_directories: gnome_calendar_inc,
  dependencies: gnome_calendar_dep,
  c_args: common_flags + debug_flags,
  link_args: gnome_calendar_ldflags
)

libgcal_inc = include_directories('.')
libgcal_dep = declare_dependency(
  link_with: libgcal_lib,
  include_directories: gnome_calendar_inc,
  dependencies: gnome_calendar_dep
)
