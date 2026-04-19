# Changelog

All notable changes to bc-allocators are documented here.

## [1.0.0]

Initial public release.

### Added

- **Context** (`bc_allocators_context.h`): user-facing allocator
  handle threading leak-warning callbacks and statistics.
- **Pool allocator** (`bc_allocators_pool.h`): general-purpose
  allocate / reallocate / free with a 1 GB default capacity,
  backed by a typed free-list.
- **Arena allocator** (`bc_allocators_arena.h`): bump-pointer arena
  with bulk reset, secure-wipe reset, page-release, and string
  copy helpers.
- **Slab allocator** (`bc_allocators_slab.h`): fixed-size slab
  with O(1) alloc / free for hot object pools.
- **Typed array** (`bc_allocators_typed_array.h`): growable
  typed array absorbed from bc-containers.

### Quality

- Unit tests under `tests/`, built with cmocka.
- Sanitizers (asan / tsan / ubsan / memcheck) pass.
- cppcheck clean on the project sources.
- MIT-licensed, static `.a` published as Debian `.deb` via GitHub
  Releases.

[1.0.0]: https://github.com/Unmanaged-Bytes/bc-allocators/releases/tag/v1.0.0
