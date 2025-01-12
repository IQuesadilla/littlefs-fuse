project('lfs', 'c')

add_global_arguments('-DLFS_MIGRATE', language: 'c')
add_global_arguments('-DLFS_MULTIVERSION', language: 'c')

littlefs_dep = dependency('littlefs')
fuse_dep = dependency('fuse')

executable('lfs',
  ['lfs_fuse.c', 'lfs_fuse_bd.c'],
  dependencies: [fuse_dep, littlefs_dep],
)
