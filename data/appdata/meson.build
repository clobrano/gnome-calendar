appdata = 'org.gnome.Calendar.appdata.xml'

custom_target(
  appdata,
  input: '@0@.in'.format(appdata),
  output: appdata,
  command: [msgfmt, '--xml', '--template', '@INPUT@', '-d', po_dir, '-o', '@OUTPUT@'],
  install: true,
  install_dir: join_paths(gnome_calendar_datadir, 'appdata')
)
