msgfmt = find_program('msgfmt')

po_dir = join_paths(meson.source_root(), 'po')

desktop = 'org.gnome.Calendar.desktop'

custom_target(
  desktop,
  input: '@0@.in'.format(desktop),
  output: desktop,
  command: [msgfmt, '--desktop', '--template', '@INPUT@', '-d', po_dir, '-o', '@OUTPUT@'],
  install: true,
  install_dir: join_paths(gnome_calendar_datadir, 'applications')
)

install_data(
  'org.gnome.Calendar.search-provider.ini',
  install_dir: join_paths(gnome_calendar_datadir, 'gnome-shell', 'search-providers')
)

service_conf = configuration_data()
service_conf.set('bindir', gnome_calendar_bindir)

service = 'org.gnome.Calendar.service'

configure_file(
  input: '@0@.in'.format(service),
  output: service,
  install: true,
  install_dir: join_paths(gnome_calendar_datadir, 'dbus-1', 'services'),
  configuration: service_conf
)

schemas_dir = join_paths(gnome_calendar_datadir, 'glib-2.0', 'schemas')

enums = 'org.gnome.calendar.enums.xml'

custom_target(
  enums,
  input: join_paths(meson.source_root(), 'src', 'gcal-utils.h'),
  output: enums,
  command: [find_program('glib-mkenums'),
            '--comments', '<!-- @comment@ -->',
            '--fhead', '<schemalist>',
            '--vhead', '  <@type@ id="org.gnome.calendar.@EnumName@">',
            '--vprod', '    <value nick="@valuenick@" value="@valuenum@"/>',
            '--vtail', '  </@type@>',
            '--ftail', '</schemalist>',
            '--output', '@OUTPUT@',
            '@INPUT@'],
  install: true,
  install_dir: schemas_dir
)

install_data(
  'org.gnome.calendar.gschema.xml',
  install_dir: schemas_dir
)

meson.add_install_script('meson_post_install.py')

subdir('appdata')
subdir('icons')
