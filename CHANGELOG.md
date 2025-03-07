# Version 1.0.0 (Mar 2024)
* Initial open-source release.

# Version 1.0.1 (Mar 2024)
* Comment and/or doc changes.  In particular central code example in README had some problems.
* Eliminate ipc_.../CHANGELOG since no VERSION/releases for those repos.

# Version 1.0.2 (Mar 2024)
* Comment and/or doc changes.
* Style refactor (small): Encouraging `static_assert(false)` over `#error`.  Encouraging `if constexpr` over SFINAE.

# Version 2.0.0 (Feb 2025)
* Internally to SHM-jemalloc use vaddr space less aggressively, avoiding eventual fatal error despite not-high SHM RAM use.
  * Direct or indirect (such as via `ipc::transport::struc::Channel`) use of SHM-jemalloc could trigger this problem.
  * SHM-classic (`ipc::shm::classic`) was not affected.
* `ipc::transport::sync_io::Native_socket_stream` and `Blob_stream_mq_sender`: Internal fix to avoid mayhem with user access (`epoll_ctl(DEL)` in particular) of central FDs within these objects, after disconnect/error detected but before object destroyed.
* `ipc::transport::struc::[sync_io::]Channel`: Internal fix to avoid potential crash (or tripped `assert()` if enabled) when issuing messages around the time when a session ends due to the opposing process, e.g., exiting.
  * Issue affected only sessions backed by SHM-jemalloc (`ipc::session::shm::arena_lend::jemalloc`).
  * Sessions backed by SHM-classic (`ipc::session::shm::classic`) and non-zero-copy sessions (`ipc::session`) were not affected.
* Fix `ipc_shm_arena_lend` CMake build-break bug when one specified no (empty) jemalloc API prefix when building jemalloc (dependency). Also nearby:
  * ipc_shm_arena_lend CMake-build knob `JEMALLOC_PREFIX`: rename to `FLOW_IPC_JEMALLOC_PREFIX` to avoid collision with same-named variable elsewhere.
  * ipc_shm_arena_lend CMake-build knobs `JEMALLOC_LIB` and `JEMALLOC_INCLUDE_PATH`: rename to `JEMALLOC_LIBRARIES` and `JEMALLOC_INCLUDEDIR`, respectively, because these names are already used by the default auto-searcher.
  * Internally improve build script when searching for jemalloc install.
* Bug fix in `Bipc_mq_handle` wait-based APIs exception-throwing form.
  * Only direct use of some of these APIs by user (currently unlikely) was affected. Indirect use via other APIs was not affected.
* `ipc::shm::arena_lend_jemalloc::Ipc_arena::construct<T>(bool use_cache, ...)` overload: Remove and replace (with `construct_thread_cached()`) this API due to potential overload ambiguity for some `T`s.
  * `Ipc_arena` public super-class `Shm_pool_collection` also had a `construct()` with potentially ambiguous signature; rename to `construct_maybe_thread_cached()`.
* C++20 mode build and GitHub-CI-pipeline support. C++17 remains the main/minimal mode, but we now ensure user can build/use in C++20 mode also.
* Dependency bump: Boost v1.84 to 1.87.
* Minor dead-code removal from build script(s).
* Flow v2.0.0 has gained a unit test suite similar to Flow-IPC and now houses various common test utilities, so they are removed here, and instead we reuse them from their new location in Flow.
* In test/demo programs using the new (albeit optional) `flow::log::Config::init_component_to_union_idx_mapping()` arg in the now-recommended way.
* Minor test/demo code fixes.
* GitHub CI pipeline: Tweak to get gcc-13 builds to work again (GitHub changed preinstalled packages).
* Comment and/or doc changes.

## Submodule repository `flow`
* Release v2.0.0 (previously: v1.0.2).

## API notes
* New APIs:
  * `ipc::shm::arena_lend::jemalloc::Ipc_arena::construct_thread_cached()`: replaces removed `construct(true, ...)`.
  * `ipc::shm::arena_lend::jemalloc::Shm_pool_collection::construct_maybe_thread_cached()`: replaces removed `construct()`.
  * Error codes: `ipc::transport::struc::shm::error::S_SERIALIZE_FAILED_SESSION_HOSED` and `S_DESERIALIZE_FAILED_SESSION_HOSED`, when working with `ipc::transport::struc` APIs in SHM-jemalloc-backed channel.
* Breaking changes:
  * Removed `ipc::shm::arena_lend::jemalloc::Ipc_arena::construct(bool use_cache)`.
    * For `use_cache = true`, call `construct_thread_cached()` (new).
    * For `use_cache = false`, call `construct(Ctor_args&&... args)`.
  * Renamed `ipc::shm::arena_lend::jemalloc::Shm_pool_collection::construct()` to `construct_maybe_thread_cached()`.
  * `ipc_shm_arena_lend`: Build script: renamed optional cache-settings:
    * `JEMALLOC_PREFIX` to `FLOW_IPC_JEMALLOC_PREFIX`
    * `JEMALLOC_LIB` to `JEMALLOC_LIBRARIES`
    * `JEMALLOC_INCLUDE_PATH` to `JEMALLOC_INCLUDEDIR`
