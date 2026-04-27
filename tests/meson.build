cmocka_dep = dependency('cmocka')

context_dir = '../src/context'
pool_dir = '../src/pool'
arena_dir = '../src/arena'
slab_dir = '../src/slab'
platform_dir = '../src/platform'

# === Normal tests (link against lib) ===

test_bc_allocators_safe_math = executable('test_bc_allocators_safe_math',
    'test_bc_allocators_safe_math.c',
    dependencies : [cmocka_dep, bc_allocators_dep],
)
test('bc_allocators_safe_math', test_bc_allocators_safe_math)

test_bc_allocators_context = executable('test_bc_allocators_context',
    'test_bc_allocators_context.c',
    include_directories : [bc_allocators_internal_inc],
    dependencies : [cmocka_dep, bc_allocators_dep],
)
test('bc_allocators_context', test_bc_allocators_context)

test_bc_allocators_typed_array = executable('test_bc_allocators_typed_array',
    'test_bc_allocators_typed_array.c',
    dependencies : [cmocka_dep, bc_allocators_dep],
)
test('bc_allocators_typed_array', test_bc_allocators_typed_array)

# === --wrap tests (recompile sources) ===

test_bc_allocators_context_errors = executable('test_bc_allocators_context_errors',
    'test_bc_allocators_context_errors.c',
    context_dir + '/bc_allocators_context.c',
    platform_dir + '/bc_allocators_platform_linux.c',
    include_directories : [bc_allocators_public_inc, bc_allocators_internal_inc],
    dependencies : [cmocka_dep, bc_core_dep],
    link_args : [
        '-Wl,--wrap=bc_allocators_platform_map',
        '-Wl,--wrap=bc_allocators_platform_get_page_size',
        '-Wl,--wrap=bc_allocators_platform_get_cache_line_size',
    ],
)
test('bc_allocators_context_errors', test_bc_allocators_context_errors)

# === Pool allocator tests (cycle 2) ===

test_bc_allocators_pool = executable('test_bc_allocators_pool',
    'test_bc_allocators_pool.c',
    include_directories : [bc_allocators_internal_inc],
    dependencies : [cmocka_dep, bc_allocators_dep],
)
test('bc_allocators_pool', test_bc_allocators_pool)

test_bc_allocators_pool_large = executable('test_bc_allocators_pool_large',
    'test_bc_allocators_pool_large.c',
    include_directories : [bc_allocators_internal_inc],
    dependencies : [cmocka_dep, bc_allocators_dep],
)
test('bc_allocators_pool_large', test_bc_allocators_pool_large)

test_bc_allocators_pool_stats = executable('test_bc_allocators_pool_stats',
    'test_bc_allocators_pool_stats.c',
    include_directories : [bc_allocators_internal_inc],
    dependencies : [cmocka_dep, bc_allocators_dep],
)
test('bc_allocators_pool_stats', test_bc_allocators_pool_stats)

test_bc_allocators_pool_errors = executable('test_bc_allocators_pool_errors',
    'test_bc_allocators_pool_errors.c',
    include_directories : [bc_allocators_internal_inc],
    dependencies : [cmocka_dep, bc_allocators_dep],
)
test('bc_allocators_pool_errors', test_bc_allocators_pool_errors)

test_bc_allocators_pool_wrap = executable('test_bc_allocators_pool_wrap',
    'test_bc_allocators_pool_wrap.c',
    context_dir + '/bc_allocators_context.c',
    pool_dir + '/bc_allocators_pool.c',
    platform_dir + '/bc_allocators_platform_linux.c',
    include_directories : [bc_allocators_public_inc, bc_allocators_internal_inc],
    dependencies : [cmocka_dep, bc_core_dep],
    c_args : ['-fno-inline', '-include', meson.current_source_dir() / 'bc_core_wrap_redirect.h'],
    link_args : [
        '-Wl,--wrap=bc_core_align_up',
        '-Wl,--wrap=bc_core_safe_add',
        '-Wl,--wrap=madvise',
    ],
)
test('bc_allocators_pool_wrap', test_bc_allocators_pool_wrap)

# === Arena allocator tests (cycle 3) ===

test_bc_allocators_arena = executable('test_bc_allocators_arena',
    'test_bc_allocators_arena.c',
    dependencies : [cmocka_dep, bc_allocators_dep],
)
test('bc_allocators_arena', test_bc_allocators_arena)

test_bc_allocators_arena_superpage = executable('test_bc_allocators_arena_superpage',
    'test_bc_allocators_arena_superpage.c',
    include_directories : [bc_allocators_internal_inc],
    dependencies : [cmocka_dep, bc_allocators_dep],
)
test('bc_allocators_arena_superpage', test_bc_allocators_arena_superpage)

test_bc_allocators_arena_coverage = executable('test_bc_allocators_arena_coverage',
    'test_bc_allocators_arena_coverage.c',
    dependencies : [cmocka_dep, bc_allocators_dep],
)
test('bc_allocators_arena_coverage', test_bc_allocators_arena_coverage)

test_bc_allocators_arena_errors = executable('test_bc_allocators_arena_errors',
    'test_bc_allocators_arena_errors.c',
    context_dir + '/bc_allocators_context.c',
    arena_dir + '/bc_allocators_arena.c',
    platform_dir + '/bc_allocators_platform_linux.c',
    include_directories : [bc_allocators_public_inc, bc_allocators_internal_inc],
    dependencies : [cmocka_dep, bc_core_dep],
    c_args : ['-fno-inline', '-include', meson.current_source_dir() / 'bc_core_wrap_redirect.h'],
    link_args : [
        '-Wl,--wrap=bc_allocators_platform_map',
        '-Wl,--wrap=bc_core_align_up',
        '-Wl,--wrap=bc_core_safe_add',
    ],
)
test('bc_allocators_arena_errors', test_bc_allocators_arena_errors)

# === Leak detection integration tests (cycle 3) ===

test_bc_allocators_leak_detection = executable('test_bc_allocators_leak_detection',
    'test_bc_allocators_leak_detection.c',
    dependencies : [cmocka_dep, bc_allocators_dep],
)
test('bc_allocators_leak_detection', test_bc_allocators_leak_detection)

# === Pool small-block free isolated (branch 6) ===

test_bc_allocators_pool_small_free = executable('test_bc_allocators_pool_small_free',
    'test_bc_allocators_pool_small_free.c',
    include_directories : [bc_allocators_internal_inc],
    dependencies : [cmocka_dep, bc_allocators_dep],
)
test('bc_allocators_pool_small_free', test_bc_allocators_pool_small_free)

# === Pool page_class RED tests (header separation removal) ===

test_bc_allocators_pool_page_class = executable('test_bc_allocators_pool_page_class',
    'test_bc_allocators_pool_page_class.c',
    include_directories : [bc_allocators_internal_inc],
    dependencies : [cmocka_dep, bc_allocators_dep],
)
test('bc_allocators_pool_page_class', test_bc_allocators_pool_page_class)

# === Magazine pointer cache tests (branch 5) ===

test_bc_allocators_pool_magazine = executable('test_bc_allocators_pool_magazine',
    'test_bc_allocators_pool_magazine.c',
    include_directories : [bc_allocators_internal_inc],
    dependencies : [cmocka_dep, bc_allocators_dep],
)
test('bc_allocators_pool_magazine', test_bc_allocators_pool_magazine)

# === Alignment tests (module 2 — item 2.1) ===

test_bc_allocators_alignment = executable('test_bc_allocators_alignment',
    'test_bc_allocators_alignment.c',
    dependencies : [cmocka_dep, bc_allocators_dep],
)
test('bc_allocators_alignment', test_bc_allocators_alignment)

# === Slab allocator tests (cycle 4) ===

test_bc_allocators_slab = executable('test_bc_allocators_slab',
    'test_bc_allocators_slab.c',
    include_directories : [bc_allocators_internal_inc],
    dependencies : [cmocka_dep, bc_allocators_dep],
)
test('bc_allocators_slab', test_bc_allocators_slab)

# === Aligned + huge page tests ===

test_bc_allocators_aligned_allocate = executable('test_bc_allocators_aligned_allocate',
    'test_bc_allocators_aligned_allocate.c',
    dependencies : [cmocka_dep, bc_allocators_dep],
)
test('bc_allocators_aligned_allocate', test_bc_allocators_aligned_allocate)

test_bc_allocators_huge_page_allocate = executable('test_bc_allocators_huge_page_allocate',
    'test_bc_allocators_huge_page_allocate.c',
    dependencies : [cmocka_dep, bc_allocators_dep, bc_core_dep],
)
test('bc_allocators_huge_page_allocate', test_bc_allocators_huge_page_allocate)

test_bc_allocators_slab_errors = executable('test_bc_allocators_slab_errors',
    'test_bc_allocators_slab_errors.c',
    slab_dir + '/bc_allocators_slab.c',
    context_dir + '/bc_allocators_context.c',
    pool_dir + '/bc_allocators_pool.c',
    platform_dir + '/bc_allocators_platform_linux.c',
    include_directories : [bc_allocators_public_inc, bc_allocators_internal_inc],
    dependencies : [cmocka_dep, bc_core_dep],
    c_args : ['-fno-inline'],
    link_args : [
        '-Wl,--wrap=bc_allocators_pool_allocate',
    ],
)
test('bc_allocators_slab_errors', test_bc_allocators_slab_errors)
