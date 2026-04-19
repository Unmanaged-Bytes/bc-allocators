# bc-allocators — project context

Memory allocators for the `bc-*` ecosystem: pool (general-purpose),
arena (bump-pointer with reset), slab (fixed-size), a user-facing
context object that threads leak callbacks and stats, plus a
growable typed-array primitive absorbed from bc-containers.


## Invariants (do not break)

- **No comments in `.c` files** — code names itself. Public `.h`
  may carry one-line contracts if the signature is insufficient.
- **No defensive null-checks at function entry.** Return `false`
  on legitimate failure; never assert in production paths.
- **SPDX-License-Identifier: MIT** header on every `.c` and `.h`.
- **Strict C11** with `-Wall -Wextra -Wpedantic -Werror`.
- **Sanitizers (asan/tsan/ubsan/memcheck) stay green** in CI.
- **cppcheck stays clean**; never edit `cppcheck-suppressions.txt`
  to hide real findings.
- **Leak reporting is callback-based** — no direct writes to stderr
  from library code; the context carries a user-provided callback.
