benchmark('bench_bc_allocators_ops',
    executable('bench_bc_allocators_ops', 'bench_bc_allocators_ops.c',
        include_directories : bc_allocators_public_inc,
        dependencies : [bc_allocators_dep, bc_core_dep],
    ),
    timeout : 120,
)

cc = meson.get_compiler('c')
jemalloc_dep = cc.find_library('jemalloc', required : false)
mimalloc_dep = cc.find_library('mimalloc', required : false)

if jemalloc_dep.found() and mimalloc_dep.found()
    benchmark('bench_bc_allocators_vs_external',
        executable('bench_bc_allocators_vs_external', 'bench_bc_allocators_vs_external.c',
            include_directories : bc_allocators_public_inc,
            dependencies : [bc_allocators_dep, bc_core_dep, jemalloc_dep, mimalloc_dep],
        ),
        timeout : 300,
    )
endif
