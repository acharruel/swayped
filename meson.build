project('swayped', 'c')

cc = meson.get_compiler('c')

# sources
src = [
    'src/command.c',
    'src/gesture.c',
    'src/sway/ipc-client.c',
    'src/sway/log.c',
    'src/main.c',
    'src/swipe.c'
    ]

deps = [
    cc.find_library('m'),
    dependency('inih'),
    dependency('json-c'),
    dependency('libinput'),
    dependency('libudev')
    ]

executable('swayped',
    src,
    dependencies: deps,
    install: true)
