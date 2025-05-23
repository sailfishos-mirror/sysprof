if get_option('gtk') and get_option('libsysprof')
  install_data('sysprof-mime.xml',
    install_dir: join_paths(datadir, 'mime/packages')
  )

  appdata_in = configure_file(
            input: 'org.gnome.Sysprof.appdata.xml.in.in',
           output: 'org.gnome.Sysprof.appdata.xml.in',
    configuration: config_h,
  )
  appdata_file = i18n.merge_file(
            input: appdata_in,
           output: '@0@.appdata.xml'.format(app_id),
           po_dir: podir,
          install: true,
      install_dir: join_paths(datadir, 'metainfo')
  )
  # Validate Appdata
  appstreamcli = find_program('appstreamcli', required: false)
  if appstreamcli.found()
    test(
      'validate-appdata', appstreamcli,
      args: [
        'validate', '--no-net', '--explain', appdata_file
      ],
      depends: appdata_file,
    )
  endif

  desktop_in = configure_file(
            input: 'org.gnome.Sysprof.desktop.in.in',
           output: 'org.gnome.Sysprof.desktop.in',
    configuration: config_h,
  )
  i18n.merge_file(
            input: desktop_in,
           output: '@0@.desktop'.format(app_id),
             type: 'desktop',
           po_dir: podir,
          install: true,
      install_dir: join_paths(datadir, 'applications')
  )

  # D-Bus service file.
  dbusconf = configuration_data()
  dbusconf.set('app_id', app_id)
  dbusconf.set('bindir', join_paths(get_option('prefix'), get_option('bindir')))
  configure_file(
            input: 'org.gnome.Sysprof.service.in',
           output: '@0@.service'.format(app_id),
    configuration: dbusconf,
          install: true,
      install_dir: join_paths(get_option('datadir'), 'dbus-1', 'services'),
  )

  subdir('icons')
endif
