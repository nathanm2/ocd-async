project('ocd', 'c')

gio = dependency('gio-2.0')
glib = dependency('glib-2.0')

executable('ocd-sync', files('ocd-sync.c'), dependencies: [gio, glib])
executable('ocd-async', files('ocd-async.c'), dependencies: [gio, glib])
