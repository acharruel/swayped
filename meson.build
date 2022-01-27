project('swayped', 'c')

# sources
src = [
    'src/gesture.c',
    'src/main.c'
    ]

deps = [
    dependency('inih'),
    dependency('libinput'),
    dependency('libudev')
    ]

executable('swayped',
    src,
    dependencies: deps)
