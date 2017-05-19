html_images = [
  join_paths('images', 'gcal-edit-dialog.png'),
  join_paths('images', 'gcal-event-widget.png'),
  join_paths('images', 'gcal-source-dialog.png'),
  join_paths('images', 'gcal-window.png'),
  join_paths('images', 'gcal-window_agendas.png')
]

version_conf = configuration_data()
version_conf.set('VERSION', gnome_calendar_version)

version = 'version.xml'

version_xml = configure_file(
  input: '@0@.in'.format(version),
  output: version,
  configuration: version_conf
)

gtk_prefix = gtk_dep.get_pkgconfig_variable('prefix')
gtk_doc_path = join_paths(gtk_prefix, 'share', 'gtk-doc', 'html')

glib_prefix = glib_dep.get_pkgconfig_variable('prefix')
glib_doc_path = join_paths(glib_prefix, 'share', 'gtk-doc', 'html')
doc_path = join_paths(gnome_calendar_datadir, 'gtk-doc', 'html', meson.project_name())

gnome.gtkdoc(
  meson.project_name(),
  main_sgml: '@0@-docs.sgml'.format(meson.project_name()),
  src_dir: [
    join_paths(meson.source_root(), 'src'),
    join_paths(meson.build_root(), 'src')
  ],
  dependencies: libgcal_dep,
  scan_args: [
    '--rebuild-types'
  ],
  mkdb_args: [
    '--sgml-mode',
    '--output-format=xml'
  ],
  gobject_typesfile: '@0@.types'.format(meson.project_name()),
  fixxref_args: [
    '--extra-dir=@0@'.format(join_paths(gtk_doc_path, 'evolution-data-server')),
    '--extra-dir=@0@'.format(join_paths(glib_doc_path, 'glib')),
    '--extra-dir=@0@'.format(join_paths(glib_doc_path, 'gobject')),
    '--extra-dir=@0@'.format(join_paths(glib_doc_path, 'gio')),
    '--extra-dir=@0@'.format(join_paths(gtk_doc_path, 'gdk')),
    '--extra-dir=@0@'.format(join_paths(gtk_doc_path, 'gtk')),
  ],
  html_assets: html_images,
  install: true,
  install_dir: doc_path
)
