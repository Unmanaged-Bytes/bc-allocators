bc_allocators_lib = static_library('bc-allocators',
    'context/bc_allocators_context.c',
    'pool/bc_allocators_pool.c',
    'arena/bc_allocators_arena.c',
    'slab/bc_allocators_slab.c',
    'aligned/bc_allocators_aligned.c',
    'platform/bc_allocators_platform_linux.c',
    include_directories : [bc_allocators_public_inc, bc_allocators_internal_inc],
    dependencies : [bc_core_dep],
    c_args : lto_c_args,
    link_args : lto_link_args,
    install : true,
)

bc_allocators_dep = declare_dependency(
    link_with : bc_allocators_lib,
    include_directories : bc_allocators_public_inc,
    dependencies : [bc_core_dep],
)
