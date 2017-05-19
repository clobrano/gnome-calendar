project(
  'gnome-calendar', 'c',
  version: '3.25.1',
  license: 'GPL3+',
  default_options: [
    'buildtype=debugoptimized',
    'c_std=gnu99',
    'warning_level=1'
  ],
  meson_version: '>= 0.38.1'
)

gnome_calendar_version = meson.project_version()
version_array = gnome_calendar_version.split('.')
gnome_calendar_major_version = version_array[0].to_int()
gnome_calendar_minor_version = version_array[1].to_int()
gnome_calendar_micro_version = version_array[2].to_int()

gnome_calendar_prefix = get_option('prefix')
gnome_calendar_bindir = join_paths(gnome_calendar_prefix, get_option('bindir'))
gnome_calendar_localedir = join_paths(gnome_calendar_prefix, get_option('localedir'))
gnome_calendar_datadir = join_paths(gnome_calendar_prefix, get_option('datadir'))

gnome_calendar_pkgdatadir = join_paths(gnome_calendar_datadir, meson.project_name())

cc = meson.get_compiler('c')

nl_time_first_weekday_src = '''
  #include <langinfo.h>
  int main() {
    nl_langinfo(_NL_TIME_FIRST_WEEKDAY);
  };
'''

conf = configuration_data()

conf.set('ENABLE_TRACING', get_option('enable-tracing'))

conf.set('ENABLE_NLS', get_option('enable-nls'))
conf.set_quoted('GETTEXT_PACKAGE', meson.project_name())

# package
conf.set_quoted('PACKAGE', meson.project_name())
conf.set_quoted('PACKAGE_BUGREPORT', 'https://bugzilla.gnome.org/enter_bug.cgi?product=gnome-calendar')
conf.set_quoted('PACKAGE_NAME', meson.project_name())
conf.set_quoted('PACKAGE_STRING', '@0@ @1@'.format(meson.project_name(), gnome_calendar_version))
conf.set_quoted('PACKAGE_TARNAME', meson.project_name())
conf.set_quoted('PACKAGE_URL', 'https://wiki.gnome.org/Apps/Calendar')
conf.set_quoted('PACKAGE_VERSION', gnome_calendar_version)
conf.set_quoted('VERSION', gnome_calendar_version)

# headers
conf.set('HAVE_DLFCN_H', cc.has_header('dlfcn.h'))
conf.set('HAVE_INTTYPES_H', cc.has_header('inttypes.h'))
conf.set('HAVE_LANGINFO_H', cc.has_header('langinfo.h'))
conf.set('HAVE__NL_TIME_FIRST_WEEKDAY', cc.compiles(nl_time_first_weekday_src))
conf.set('HAVE_LOCALE_H', cc.has_header('locale.h'))
conf.set('HAVE_MEMORY_H', cc.has_header('memory.h'))
conf.set('HAVE_STDINT_H', cc.has_header('stdint.h'))
conf.set('HAVE_STDLIB_H', cc.has_header('stdlib.h'))
conf.set('HAVE_STRING_H', cc.has_header('string.h'))
conf.set('HAVE_STRINGS_H', cc.has_header('strings.h'))
conf.set('HAVE_SYS_STAT_H', cc.has_header('sys/stat.h'))
conf.set('HAVE_SYS_TYPES_H', cc.has_header('sys/types.h'))
conf.set('HAVE_UNISTD_H', cc.has_header('unistd.h'))

# functions
assert(cc.has_function('strerror'),
       '"strerror" not found')

if host_machine.system().contains('darwin')
  conf.set('HAVE_CFLOCALECOPYCURRENT', cc.has_function('CFLocaleCopyCurrent'))
  conf.set('HAVE_CFPREFERENCESCOPYAPPVALUE', cc.has_function('CFPreferencesCopyAppValue'))
endif

conf.set('HAVE_DCGETTEXT', cc.has_function('dcgettext'))
conf.set('HAVE_FLOOR', cc.has_function('floor'))
conf.set('HAVE_GETTEXT', cc.has_function('gettext'))
conf.set('HAVE_ICONV', cc.has_function('iconv'))
conf.set('HAVE_MEMSET', cc.has_function('memset'))
conf.set('HAVE_NL_LANGINFO', cc.has_function('nl_langinfo'))
conf.set('HAVE_POW', cc.has_function('pow'))
conf.set('HAVE_SETLOCALE', cc.has_function('setlocale'))
conf.set('HAVE_STRSTR', cc.has_function('strstr'))

# Compiler flags
test_cflags = [
  '-Wno-sign-compare'
]
'''
test_cflags = [
  '-fno-strict-aliasing',
  '-Wall',
  '-Warray-bounds',
  '-Wcast-align',
  '-Wdeclaration-after-statement',
  '-Wextra',
  '-Wformat-nonliteral',
  '-Wformat-security',
  '-Wformat=2',
  '-Wimplicit-function-declaration',
  '-Winit-self',
  '-Winline',
  '-Wmissing-declarations',
  '-Wmissing-format-attribute',
  '-Wmissing-include-dirs',
  '-Wmissing-noreturn',
  '-Wmissing-prototypes',
  '-Wnested-externs',
  '-Wno-error=missing-field-initializers',
  '-Wno-error=unused-parameter',
  '-Wno-missing-field-initializers',
  '-Wno-unused-parameter',
  '-Wold-style-definition',
  '-Wpacked',
  '-Wpointer-arith',
  '-Wredundant-decls',
  '-Wreturn-type',
  '-Wshadow',
  '-Wsign-compare',
  '-Wstrict-aliasing',
  '-Wstrict-prototypes',
  '-Wswitch-default',
  '-Wswitch-enum',
  '-Wundef',
  '-Wunused-but-set-variable',
  '-Wwrite-strings'
]
'''

common_flags = [
  '-DHAVE_CONFIG_H',
  '-DPACKAGE_LOCALE_DIR="@0@"'.format(gnome_calendar_localedir),
  '-DPACKAGE_DATA_DIR="@0@"'.format(gnome_calendar_pkgdatadir),
  '-DUI_DATA_DIR="@0@"'.format(join_paths(gnome_calendar_datadir), 'style'),
  '-DEDS_DISABLE_DEPRECATED',
  '-DGOA_API_IS_SUBJECT_TO_CHANGE'
]

debug_flags = []
buildtype = get_option('buildtype')
if buildtype == 'release'
  debug_flags += [
    '-DG_DISABLE_ASSERT',
    '-DG_DISABLE_CHECKS',
    '-DG_DISABLE_CAST_CHECKS'
  ]
elif buildtype == 'debug' or buildtype == 'debugoptimized'
  debug_flags += [
    '-DG_DISABLE_CAST_CHECKS'
  ]
endif

foreach cflag: test_cflags
  if cc.has_argument(cflag)
    common_flags += [cflag]
  endif
endforeach

add_global_arguments(common_flags, language: 'c')

libical_dep = dependency('libical', version: '>= 1.0')
conf.set('HAVE_LIBICAL', libical_dep.found())

assert(cc.has_function('icaltime_days_in_year', dependencies: libical_dep),
       'Error: icaltime_days_in_year() not found in libical!. Upgrade your libical library.')

glib_dep = dependency('glib-2.0', version: '>= 2.43.4')
gtk_dep = dependency('gtk+-3.0', version: '>= 3.21.6')

gnome_calendar_dep = [
  dependency('gmodule-export-2.0'),
  dependency('gio-2.0', version: '>= 2.43.4'),
  glib_dep,
  dependency('goa-1.0', version: '>= 3.2.0'),
  gtk_dep,
  dependency('libecal-1.2', version: '>= 3.13.90'),
  dependency('libedataserver-1.2', version: '>= 3.17.1'),
  dependency('libedataserverui-1.2', version: '>= 3.17.1'),
  libical_dep,
  dependency('libsoup-2.4'),
  dependency('gsettings-desktop-schemas', version: '>= 3.21.2'),
  dependency('threads'),
  cc.find_library('m', required: true)
]

configure_file(
  output : 'config.h',
  configuration : conf
)

gnome = import('gnome')
pkg = import('pkgconfig')

top_inc = include_directories('.')

subdir('src')
subdir('data')

if get_option('enable-gtk-doc')
  subdir('doc')
endif

if get_option('enable-nls')
  i18n = import('i18n')

  subdir('po')
endif
