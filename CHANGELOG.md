# Version 1.0.0 (Mar 2024)
* Initial open-source release.

# Version 1.0.1 (Mar 2024)
* Comment and/or doc changes. In particular central code example in README had some problems.
* Eliminate ipc_.../CHANGELOG since no VERSION/releases for those repos.

# Version 1.0.2 (Mar 2024)
* Comment and/or doc changes.
* Style refactor (small): Encouraging `static_assert(false)` over `#error`. Encouraging `if constexpr` over SFINAE.

# Version 2.0.0 (Feb 2025)
* Internally to SHM-jemalloc use vaddr space less aggressively, avoiding eventual fatal error despite not-high SHM RAM use.
  * Direct or indirect (such as via `ipc::transport::struc::Channel`) use of SHM-jemalloc could trigger this problem.
  * SHM-classic (`ipc::shm::classic`) was not affected.
* `ipc::transport::sync_io::Native_socket_stream` and `Blob_stream_mq_sender`: Internal fix to avoid mayhem with user access (`epoll_ctl(DEL)` in particular) of central FDs within these objects, after disconnect/error detected but before object destroyed.
* `ipc::transport::struc::[sync_io::]Channel`: Internal fix to avoid potential crash (or tripped `assert()` if enabled) when issuing messages around the time when a session ends due to the opposing process, e.g., exiting.
  * Issue affected only sessions backed by SHM-jemalloc (`ipc::session::shm::arena_lend::jemalloc`).
  * Sessions backed by SHM-classic (`ipc::session::shm::classic`) and non-zero-copy sessions (`ipc::session`) were not affected.
* Fix `ipc_shm_arena_lend` CMake build-break bug when one specified no (empty) jemalloc API prefix when building jemalloc (dependency). Also nearby:
  * `ipc_shm_arena_lend` CMake-build knob `JEMALLOC_PREFIX`: Rename to `FLOW_IPC_JEMALLOC_PREFIX` to avoid collision with same-named variable elsewhere.
  * `ipc_shm_arena_lend` CMake-build knobs `JEMALLOC_LIB` and `JEMALLOC_INCLUDE_PATH`: Rename to `JEMALLOC_LIBRARIES` and `JEMALLOC_INCLUDEDIR`, respectively, because these names are already used by the default auto-searcher.
  * Internally improve build script when searching for jemalloc install.
* Bug fix in `Bipc_mq_handle` wait-based APIs exception-throwing form.
  * Only direct use of some of these APIs by user (currently unlikely) was affected. Indirect use via other APIs was not affected.
* `ipc::shm::arena_lend::jemalloc::Ipc_arena::construct<T>(bool use_cache, ...)` overload: Remove and replace (with `construct_thread_cached()`) this API due to potential overload ambiguity for some `T`s.
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
  * `ipc::shm::arena_lend::jemalloc::Ipc_arena::construct_thread_cached()`: Replaces removed `construct(true, ...)`.
  * `ipc::shm::arena_lend::jemalloc::Shm_pool_collection::construct_maybe_thread_cached()`: Replaces removed `construct()`.
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

# Version 3.0.0 (Nov 2026)
## Highlights
* SHM-jemalloc performance deep-dive:
  * Load-test of large multi-threaded application, generate flame-graphs, optimize hot spots; repeat.
  * Before optimization: collapse under heavy SHM-jemalloc use; several prominent flame-graph hot spots.
  * After optimization: performs well; no SHM-jemalloc hot spots remain.
    * No change to the essential APIs.
* Transport performance deep-dive:
  * Profile the same application across the IPC-transport layers, most notably `struc::Channel` and `Native_socket_stream`.
  * Resulting optimizations improved overall application throughput by another ~10-20%.
  * For direct users of the low-level transports: Add batch-receive, getting many messages per system call (`recvmmsg()`).
  * Bonus: Heap-backed `struc::Channel` messages with large leaves (list, blob, string) now transmit without problems. They're internally auto-split/reassembled when needed.
* Zero-copy-enabled Cap'n Proto RPC (new module `ipc::transport::struc::shm::rpc`):
  * Use capnp-RPC as usual.
  * Underneath, messages live in SHM and are never copied, so payload size no longer matters.
* Stats: Counters, gauges, histograms, and high-water marks at every layer:
  * Covers low-level transports, `struc::Channel`, serialization, sessions, SHM.
    * SHM-jemalloc: Includes curated selection of jemalloc's own stats.
  * These build on `flow::util::stat`, a new general stats API in Flow (a dependency of Flow-IPC).
    * You can use this for your own stats too.
* Hardening and stability: Numerous fixes, notably around session opening and session end, covered by a greatly expanded test suite.
  * SHM-classic major bug fix: `Pool_arena::construct<T>()` could take a few bytes too few for certain object sizes.
  * The new tests join the existing sanitizer runs in the CI pipeline (ASAN/LSAN, TSAN, UBSAN).

## Upgrade notes
* Compatibility: Both processes engaging in IPC must use this release (or later). Internal protocols changed, as described below.
  * A mismatch is handled cleanly: session opening (or a direct `Native_socket_stream` connect) fails; a `struc::Channel` set up outside a session reports a protocol-version error.
* Breaking API changes mostly avoid commonly used APIs.
  * Notable exception: `ipc::transport::struc::[sync_io::]Channel::send()`, `[a]sync_request()` now take `Msg_out*` instead of `const Msg_out&`: please use `ch.send(&msg)`, not `ch.send(msg)`.
  * See "API notes" below for the full list.

## All changes
### Across modules
* Stats: Each module adds stat-set `struct`s and access to recorded stats.
  * Each stat-set is a snapshot. Every `struct` member (counter/histogram, gauge, high-water mark) is documented in the Reference.
  * Print one via `flow::util::stat`: `cout << flow::util::stat::print(channel.stats())`.
  * Reset (zero counters and histograms, etc.) anytime: `channel.stats_reset()`.
  * Where there is a lot of detail (notably SHM-jemalloc), print a whole bundle in one shot: `session.session_shm()->info_dump(&dump); cout << dump`.
  * Guided Manual: The API overview has a Stats section, with a table of where to find what.
* Fix (bug class): The formally allowed `Logger* logger_ptr = nullptr` API arg value could cause a crash.
* Internal worker threads:
  * Use shorter names with the most useful information up-front, as Linux truncates the OS-name (as seen in `gdb` et al).
  * Reset core-affinity at thread start, rather than inheriting it from the user's thread.

### `ipc::util`
* Add `Native_handle` facilities: `close()`, `is_open()`, `dup()` (close-on-exec); RAII/auto-closing owner `Own_native_handle` with `duped_native_handle()`, `disowned_native_handle()`; `swap()`.
  * `constexpr` + `noexcept` added all over.
* Add `Shared_name::ct_from_int()` (efficient integer => name).
* Add `NULL_PROCESS_CREDENTIALS`; as-if-default-constructed `Process_credentials` prints as `null`.
* Add `Info_dump_format`: Common knobs (verbose, multi-line) for info-dump/stats output elsewhere in Flow-IPC.
* Remove `Use_counted_object`. Move `Default_init_allocator` to Flow (`flow::util::Default_init_allocator`).
* Fix `ipc::util::process_running()`:
  * The POSIX `EPERM` outcome (process exists, but signaling it is not permitted) was treated as impossible (assertion trip; or yields "not running"). Now: yields "running."
  * A non-positive PID now yields "not running" (before: essentially undefined behavior).
* Fix `set_resource_permissions()`: A concurrent fork/exec could leak its temporary FD into the child (`O_CLOEXEC`).
* Minor fixes (none affect Flow-IPC's own use of these APIs):
  * Fix `Native_handle` move-assignment: Source was not nullified if it held the same raw handle value as the target.
  * Fix `Shared_name::sanitize()`, `sanitized()`: With a non-default locale, characters outside `[A-Za-z0-9_]` could pass.
  * Fix `Shared_name` `/=` operators when the argument overlaps the target (`s /= s`, `s /= s.str()`, etc.).
  * Fix `istream >> Shared_name`: On failure of the underlying string-read it loaded an empty name; now leaves it as-is.
  * Fix `Process_credentials::process_invoked_as()`: Non-throwing form did not clear `*err_code` on success.

### `ipc::transport` (unstructured)
* `[sync_io::]Native_socket_stream`:
  * Performance: Internally use connected-datagram (`SOCK_SEQPACKET`) sockets instead of stream (`SOCK_STREAM`) sockets: OS maintains message boundaries => much less per-message work happens in user-land.
    * API contracts are unchanged.
  * Add batch-receive: `async_receive_{blob|native_handle}_batch()` receive up to N in-messages in one call, with one native batch-read (`recvmmsg()`). See also `S_RCV_*_BATCH_SZ_RECOMMENDATION`.
  * Add stats: `..._{send|receive}_stats()`, `..._stats_reset()`. (See `Blob_{snd|rcv}_stats` below.)
  * Fix/upgrade the `send_native_handle()` contract: You may close handle (FD), as soon as the call returns.
    * Previously this was unsafe if the socket happened to be in would-block state: the deferred send could then transmit a closed (or reused) descriptor.
    * Internally we `Native_handle::dup()` if deferral required.
  * Add `Native_socket_stream_cfg`: Compile-time constants governing `Native_socket_stream` (socket type, batch-receive mode, max message size, stats logging).
    * The stream-socket code path, and an emulation of batch-receive via one-message-receive OS-calls, are available but dormant (not selected by the shipped configuration).
      * A future release may make these selectable, perhaps via `Native_socket_stream` template parameters per-object.
  * Fix: An assertion could trip when sending graceful-close (`async_end_sending()`), while the outgoing direction was in would-block state.
  * Fix: A received native handle could leak, if an error occurred on the stream very close to it (e.g., the accompanying blob was too big for the user's target buffer).
  * Fix: Retrying `sync_io::Native_socket_stream::sync_connect()` after a failed attempt could trip an assertion.
  * `remote_peer_process_credentials()` may now be called concurrently with one send-op and/or one receive-op.
  * Add `remote_peer_process_credentials()` mutator: Sets what the accessor returns, for socket-streams created more manually (as `ipc::session` does internally).
  * Send operations always log a WARNING upon encountering a socket-hosing error (previously: no WARNING if no native handle was being sent).
  * Reject protocol-violating messages from the opposing peer more stringently.
* All sender/receiver implementations (`Native_socket_stream`, `Blob_stream_mq_*`, and anything built on them):
  * Performance: Each object's timer thread now starts only upon first use of `auto_ping()` or `idle_timer_run()`. Many objects never use those, so no thread is started.
  * Fix: A very-low-probability use-after-free in the internal timer facility.
* `Channel` (and its aliases and subclasses, such as `Socket_stream_channel`):
  * Add batch-receive and stats APIs, forwarding to the underlying pipes.
  * Fix `async_end_sending()` (`sync_io` form, 2-pipe channels only): A would-block on a pipe was reported as synchronous success. At worst one could close the channel before trailing messages were flushed. `struc::Channel::async_end_sending()` is affected indirectly.
  * Minor fixes (none affect Flow-IPC's own use of these APIs):
    * Fix `blob_snd()`, `hndl_snd()` (etc.): The advertised check for an uninitialized pipe was missing (undefined behavior).
    * Fix 2-arg `init_blob_pipe()`, `init_native_handle_pipe()`: The advertised check for a duplicate call was missing (silently overwrote).
    * Fix move-ctor, move-assignment: Moved-from object was not as-if-default-constructed (it remained `initialized()`, with its pipes in NULL state).
    * Fix `remote_peer_process_credentials()`: Did not clear `*err_code` if the channel had no socket pipe.
* `[sync_io::]Blob_stream_mq_sender`, `Blob_stream_mq_receiver`:
  * Add batch-receive `async_receive_blob_batch()` (emulated via one-message MQ receives; implements extended concept `Blob_receiver`).
  * Add stats: `blob_{send|receive}_stats()`, `..._stats_reset()`.
  * Minor fix: Ctors (non-throwing form) did not clear `*err_code` on success.
* Concepts `Blob_sender`, `Blob_receiver`, `Native_handle_sender`, `Native_handle_receiver` (and `sync_io::` counterparts) gain batch-receive and stats APIs, plus the `send_native_handle()` contract fix/upgrade. Hence the concrete implementations (see `Native_socket_stream`, `Blob_stream_mq_*` above) follow these contracts.
* Add transport stat `struct`s `ipc::transport::stat::Blob_snd_stats`, `Blob_rcv_stats`: Detailed per-direction counters (including a histogram), gauges, high-water marks (re. messages, bytes, handles, auto-pings, etc.).
  * Obtain via the `*_stats()` accessors above; reset via `*_stats_reset()`.
* Add (make public) `sync_io::Async_adapter_{sender|receiver}`: Adapt a `sync_io::` core sender/receiver into its async-I/O counterpart, as Flow-IPC does for `Native_socket_stream` and `Blob_stream_mq_*`. Useful for implementing a custom transport (advanced).
* `Posix_mq_handle`, `Bipc_mq_handle` (`Persistent_mq_handle` concept):
  * Minor fix: `Posix_mq_handle` ctors: certain unlikely errors (such as running out of FDs) could leave a newly created MQ's name in the file system, or (rarer) leak its handle.
  * Minor fixes (none affect Flow-IPC's own use of these APIs):
    * Fix `timed_send()`, `timed_receive()` (both classes): If woken up repeatedly without success, could report timeout before the specified time elapsed.
    * Fix `Posix_mq_handle::interrupt_*()`: Could fail to take effect (wrong array length given to `epoll_wait()`).
    * Fix `Posix_mq_handle` move-assignment: Leaked the MQ handle and 2 `epoll` FDs of the target.
    * Fix `Posix_mq_handle` ctors: Non-throwing form did not clear `*err_code` on success.
    * Fix `swap()` of `Bipc_mq_handle`s: Bad state if one was mid-`interrupt_*()`.
* `[sync_io::]Native_socket_stream_acceptor`: Minor fixes (none affect Flow-IPC's own use of these APIs):
  * Fix ctor: Non-throwing form did not clear `*err_code` on success.
  * Fix `sync_io::` form's `async_accept()` when called before `start_accept_ops()` (undefined behavior): Now logs and returns `false`.
  * Fix `sync_io::` form's ctor: Logged a WARNING on success.
* `Protocol_negotiator::compute_negotiated_proto_ver()`: Fixes (none affect Flow-IPC's own use of this API):
  * Fix: If the opposing side speaks an older protocol that this side is configured to be backward-compatible with, this side nevertheless chose to speak the newer one (now: the older one).
    * Flow-IPC itself currently does not exercise backward-compatibility.
  * Minor fix: Non-throwing form did not clear `*err_code` on success.
* `ipc::transport::asio_local_stream_socket` (low-level Unix-domain-socket utilities beneath `Native_socket_stream`; also usable directly):
  * Add connected-datagram (`SOCK_SEQPACKET`) support alongside existing stream (`SOCK_STREAM`) support: Protocol is now a template parameter, particularly of `nb_{read|write}_some_with_native_handle()`.
  * Add class `Msg_batch_in`: Batch-receive (via native `recvmmsg()`) of datagrams, each with an optional native handle (FD). It keeps its OS-call data structures from read to read, so repeated batch-reads cost ~nothing to set up. (Cf.: naively one would set up the full array before each read.)
  * `nb_{read|write}_some_with_native_handle()` accept buffer sequences (scatter/gather), not just one buffer.
  * Remove Boost.Asio extensions `async_write_with_native_handle()`, `async_read_with_target_func()`, `async_read_interruptible()`.
  * Remove `release_native_peer_socket()` (use `util::Native_handle::close()`).

### `ipc::transport` (structured)
* `struc::[sync_io::]Channel`:
  * Performance improvement:
    Major optimization of how we encode/transmit/decode a structured message (`Msg_out`) given to a send API (`send()`, `[a]sync_request()`).
    * Profiling a large, multi-threaded server application under load showed this path as a substantial cost in processor cycles, significantly reduced by this change.
    * Associated breaking change in APIs `send()`, `[a]sync_request()`: They now take `Msg_out*` instead of `const Msg_out&`. A send sets certain internal metadata.
      * Your message contents are not modified, same as before.
  * Performance improvements (internal):
    * Receive lower-layer messages/blobs in batches, automatically, when the underlying pipe recommends it; in practice for `Native_socket_stream`s.
    * Routing of in-messages (by message type; by request/response) uses quicker data structures. Less heap-allocating when registering a request via `[a]sync_request()`.
    * With a 2-pipe underlying `transport::Channel`, the smaller-max-message-size pipe can receive into a smaller target blob (reduces allocation).
    * Heap-backed messages: The first (often, only) serialization segment, allocated and zero-filled for each message, now defaults to ~8KiB instead of the pipe's max message size (~64KiB with `Native_socket_stream`); further segments grow as needed. Also, allocate-and-zero-fill effectively uses `calloc()` instead of `malloc()` + `memset()` (~25% faster in our tests).
  * Optional performance tuning:
    Add knobs to set the size of a message's first (often, only) serialization segment. A decent estimate can help performance under load:
    * Per-`Channel` default: Optional `segment1_sz` arg to the tag ctors (`Serialize_via_heap`, `Serialize_via_session_shm`, `Serialize_via_app_shm`) and to `heap_fixed_builder_config()`.
    * Per-message: New `create_msg()` overload taking a message-size estimate.
  * (`struc::Channel` only) Concurrency: All methods are now formally safe for mutually concurrent execution on the same object (previously only certain accessors and `create_msg()`).
    * `sync_request()`s execute fully serially, even if a blocking-wait is necessary; during such a wait any other (non-blocking) method executes immediately.
    * If such a concurrent `send()` or `async_request()` hoses the channel, the waiting `sync_request()` wakes and emits new `error::Code::S_SYNC_OP_INTERRUPTED_BY_CONCURRENT_NB_ERROR`.
  * For heap-backed messages: A capnp leaf (list, blob, string) too big for 1 low-level message no longer yields error (`S_INTERNAL_ERROR_SERIALIZE_LEAF_TOO_BIG`) on the send attempt. Instead it is automatically split across low-level messages and reassembled on receipt.
  * Add stats: `stats()`, `stats_reset()`, and histogram-configuring methods. More info on stats below.
  * (`struc::Channel` only) `owned_channel()`, `owned_channel_mutable()` now return a proxy that holds a lock for the duration of the expression, so access such as `.owned_channel()->blob_send_stats()` is thread-safe.
  * Add `remote_peer_process_credentials()`: Available for any `Channel`, whether or not it includes a `Native_socket_stream`. The opposing process's information is exchanged internally ahead of any user message.
  * `sync_request()`: While waiting, periodically (`S_SYNC_OP_PEER_PROCESS_LIVENESS_CHECK_PERIOD`, 500 ms) check whether the opposing process is still running, and stop waiting if not. Protects against a hang in case the IPC-transport does not signal the opposing process's death.
    * Add `remote_peer_process_liveness_check()`: If the opposing process is no longer running, hoses the channel with `transport::error::Code::S_PEER_PROCESS_NO_LONGER_EXISTS`.
  * On `Channel` becoming hosed, forget all registered handlers (including those from `expect_msg*()`, `[a]sync_request()`), which also releases their captures.
  * Fix: If a handle-bearing `Msg_out` was destroyed while its send was deferred by would-block, a closed (or reused) descriptor could be transmitted. Rare: requires both a full kernel out-buffer and unlucky timing.
  * Fix (2-pipe `Channel`s): Graceful-close arriving on one in-pipe immediately hosed the `Channel` (as if it were an ungraceful error), so messages still in flight on the other pipe could be silently dropped. Now delivery continues until both in-pipes close.
  * Fix `Channel::start()`, `sync_io::Channel::start_and_poll()` given very early low-level errors: With 2 pipes, setup proceeded to pipe 2 after an error on pipe 1; and an error sending protocol negotiation could go unreported (the on-error handler did not fire).
  * Hardening: Against a buggy opposing Flow-IPC or corrupt data: avoid leaking received native handles; check bounds, alignment, and size of serialized data; fail more loudly on protocol violations during channel setup; and properly report `error::Code::S_STRUCT_CHANNEL_INTERNAL_PROTOCOL_MISMATCH_GOT_NO_HNDL` as an error.
  * Minor fixes (none affect Flow-IPC's own use of these APIs):
    * Fix send APIs during log-in phase: Certain improper inputs returned `false`, as promised, but further use of the `Channel` was undefined behavior.
    * Fix `send()`, `[a]sync_request()` after `async_end_sending()`: Throwing form threw, instead of returning `false` as documented.
    * Fix `undo_expect_msgs()`, `undo_expect_responses()`: Could undo a 1-off expectation (against documented contract).
    * Fix `[un]set_[remote_]unexpected_response_handler()` on a hosed `Channel`: Now always return `false` and no-op.
* `struc::Msg_in`, `Msg_out`:
  * Breaking change: Replace `Msg_in::native_handle_or_null() const` with `emit_native_handle_or_null()`, which gives ownership to the caller. If it is not called, `~Msg_in()` closes the native handle. This also fixes a (possible) leak: a received handle that the user ignored was never closed.
  * Add `Msg_in::serialization_size()`.
  * Breaking change (tiny): `Msg_out` move-assignment to itself is now undefined behavior (it was a no-op).
  * Fix `Msg_out::store_native_handle_or_null()`: Source was not nullified if it held the same raw handle value as the target.
* Add stats `struct`s (counters, gauges, histograms):
  * `struc::stat::Channel_stats`, `struc::sync_io::stat::Channel_stats`: Per-`Channel`; obtain via `Channel::stats()`.
  * `struc::stat::Serializer_stats`: Serialization allocations and sizes, including big leaves; process-wide for heap-backed messages via `Heap_serializer_global_stats` (see also `heap_serializer_info_dump()`).
* capnp-serialization layer, direct use (`Heap_fixed_builder`, `Heap_reader`, and the `Struct_builder`, `Struct_reader` concepts):
  * Add `ostreamable_capnp_full()`, `ostreamable_capnp_brief()`: Print any capnp `Reader` to an `ostream`.
  * Remove frame-postfix capability (frame-prefix remains). Nothing in Flow-IPC used it.
  * Minor fix (does not affect Flow-IPC's own use of this API): `Heap_fixed_builder::emit_serialization()`: non-throwing form did not clear `*err_code` on success.

### `ipc::session`
* Concepts and main APIs are unchanged. This is mostly internal stability work. See "API notes" for the few items that may need attention on upgrade.
* Note: The items below apply equally to SHM-enabled variants (e.g.: `ipc::session::[shm::...::]Session_server`).
* Session setup:
  * Add safety checks:
    * `Client_session::sync_connect()` checks the OS-reported identity of the opposing process (PID against CNS (PID file); UID, GID, executable path against `Server_app`), as `Session_server` has always checked the client. On mismatch it emits new `error::Code::S_CLIENT_MASTER_LOG_IN_SERVER_APP_INCONSISTENT_CREDS`.
    * `Session_server` ctor checks own executable path against `Server_app::m_exec_path`, in addition to UID, GID. On mismatch it emits new `error::Code::S_SERVER_APP_EXEC_PATH_INCONSISTENT`.
  * `Session_server` ctor: An `App::m_name` that is empty or contains the `Shared_name` separator (underscore) now emits `error::Code::S_INVALID_ARGUMENT`.
  * Fix `Client_session::sync_connect()`: If the opposing process went down at just the wrong moment, behavior could be undefined.
  * Log-in: Add an internal timeout in case of buggy opposing-process behavior.
* `Session::open_channel()` (on-demand channel opening; cf. init-channels ready at session start):
  * Fix: If both sides called `open_channel()` at about the same time, an internal cross-process deadlock made each call block for 60 seconds, then fail with a timeout error. Now both complete normally.
  * Fix `Client_session::open_channel()` (MQ-enabled `Client_session` only): If the opposing user closed the new `Channel` immediately in its passive-open handler, behavior was undefined (could trip an assertion). Now it emits a non-fatal system error, leaving the session intact.
  * Fix `Server_session::open_channel()` on an already-hosed session: It emitted the non-fatal resources-unavailable error, instead of returning `false` as documented.
  * Fix (session-server side; socket-stream-enabled `Session` only): If the session's internal socket went down (e.g., the opposing process died) just as a `Channel` was being created, it could trip an assertion. This could occur in `async_accept()` opening init-channels, `Server_session::open_channel()`, or the opposing `Client_session::open_channel()`. Now that channel-opening fails normally, and the session-hosing is reported as usual.
  * Hardening: A bad `open_channel()` response from a buggy opposing Flow-IPC (or corrupt data) now yields the non-fatal `error::Code::S_SESSION_OPEN_CHANNEL_REMOTE_PEER_REJECTED_PASSIVE_OPEN` error, instead of tripping an assertion or being taken as success.
* `Session_server`:
  * Fix `~Session_server()`: Could crash, if session-open(s) were in progress at that exact moment. Now all accept activity is stopped, and any on-done handler has finished, before destruction proceeds.
  * `async_accept(&S)`: First makes `S` as-if default-constructed, synchronously. It was formally allowed to target a session already in PEER state, but that case was not designed for properly.
  * Fix `async_accept()`: On certain failures it modified the out-args, though documented to leave them untouched.
  * Fix throwing ctor form: The resources of failed log-ins (memory, an internal thread) were not released until `Session_server` destruction.
  * Add lifetime tenets to the docs (`Session_server` and Manual): What to destroy in which order; which departures work, though not recommended.
* Session end, liveness:
  * Fix (SHM-jemalloc-enabled `Session`): Depending on timing, the on-error handler could fire with `error::Code::S_SESSION_FINISHED` upon entering the `Session` dtor, though the user was the one ending the session (formally allowed but unhelpful).
  * Increase the idle timeout that detects an unresponsive opposing process from 5s to 15s, to be more forgiving in very slow or overloaded environments. (This is not based on field observations.) Auto-ping period remains 2s.
    * Both values (timeout, auto-ping period) are now public constants in new `Session_cfg`.
* Fix native handle leaks during `Channel` creation, on unlikely errors involving an MQ pipe (if so configured): 1 on the server side, 1 on the client side.
* Fix `Native_socket_stream::remote_peer_process_credentials()` on a `Channel` opened via `ipc::session`: It returned values for the calling process, not the opposing one, due to the internal use of `socketpair()`.
* Add stats: `Session::info_collector()` gives access to the (internal) session master channel's stats (structured and transport layers).
  * These can confirm that this background traffic (initial setup, channel opening, liveness pings) is negligible; and show how many channels are being opened over time.
* Add `Session::remote_peer_process_credentials()` (UID, GID, PID).
* Add `Session_server::mq_msg_size_limit()`, and a mutator of the same name, to override the max message size of session-created MQ pipes.
  * The default is unchanged. For SHM-enabled `Session`s it is small (512 bytes), enough for the SHM handles sent by a SHM-backed `struc::Channel`.
    * However, this is ill-suited if you prefer to use a heap-backed `struc::Channel`, or lower-level transmission of blobs larger than 512 bytes.
    * If so, recommendation: Configure session for `Native_socket_stream` pipes instead of MQ pipes.
      * If staying with MQ pipes, use the mutator to set this higher. (8KiB is typically the maximum for POSIX MQs; no such cap for bipc MQs.)
  * Code generated for MQ-enabled `Session_server<...>` only.
* For generic programming: Add `S_IS_SRV_ELSE_CLI`, `Sync_io_obj`, `Async_io_obj` to `Session`s, and the latter 2 to `Session_server`.
* Minor fix: `ensure_resource_owner_is_app()` logged a misleading WARNING on mismatch, implying it was unable to check. The emitted error was correct.
* Minor fixes (none affect Flow-IPC's own use of these APIs):
  * Fix `sync_io::Session_server_adapter::async_accept()` before `start_ops()`: Undefined behavior, instead of returning `false` as documented.
  * Fix `sync_io::Server_session_adapter::init_handlers()`: Erratic if called before successful `async_accept()`, or after a hosed-error was emitted.
  * Fix `get_logger()`, `get_log_component()` on a moved-from `Session`: Could crash.
  * Fix `Structured_msg_reader_config` of non-SHM `Session`s: Aliased the builder config by mistake, a compile error if used.
  * Fix `Session_server::async_accept()`: A missing lambda `mutable` caused a compile error if `n_init_channels_by_srv_req_func` or `mdt_load_func` had a non-`const` call operator.

### `ipc::transport::struc::shm::rpc` (new)
Zero-copy-enabled capnp-RPC.
* Background: [capnp-RPC](https://capnproto.org/rpc.html), a Remote Procedure Call framework, sits atop capnp serialization: You define `interface`s in a schema; one process implements them; another calls their methods, receiving promises for the results. In Flow-IPC context, it is an alternative to `struc::Channel` messaging. Its conversations consist of capnp messages.
  * Vanilla capnp-RPC copies each capnp message, including large ones, into and out of a socket.
* This Flow-IPC module allows ~identical use of capnp-RPC, but with zero-copy underneath: The capnp messages are stored in SHM and never copied.
  * Performance: Unlike with vanilla capnp-RPC, message size no longer matters. For small messages expect performance similar to vanilla capnp-RPC.
    * Planned: Adaptive hybrid heap/SHM-backed serializer. Small messages would be heap-backed (copied, which is cheap at that size), larger ones SHM-backed, getting the better performance at each size. It would apply to `struc::Channel` as well as capnp-RPC.
* See the new Manual chapter, "Zero-copy capnp-RPC." Example application `test/suite/rpc_demo` replicates capnp's [Calculator sample](https://github.com/capnproto/capnproto/tree/v2/c++/samples).
  * For perf comparisons, or as a fallback, zero-copy can be disabled at run time, invoking vanilla capnp-RPC underneath. For example: `Ez_rpc_client` ctor arg `sans_shm_transport`, `Ez_rpc_server` ctor arg `transport_method_func`. `rpc_demo --no-zc` exercises this. The lower-level APIs below offer the equivalent.
* Key APIs (each has a vanilla capnp-RPC counterpart, so use code stays similar; see Manual chapter for full mapping):
  * `Session_vat_network`: The core. It implements `capnp::VatNetwork`, replacing `capnp::TwoPartyVatNetwork`, acting similarly to it except with messages stored in SHM, never copied.
    * All other core capnp-RPC facilities remain in use, including `RpcSystem` and the KJ event loop.
  * `Ez_rpc_client`, `Ez_rpc_server`: Counterparts of `capnp::EzRpcClient`, `EzRpcServer`. They set up everything, including the event loop, in a few lines.
    * Client connects once.
    * Server automatically serves any incoming such connections, including concurrent and overlapping ones.
    * For finer event-loop control -- as with vanilla capnp-RPC -- use the `RpcSystem`-level approach (next bullet).
  * `Client_context`, `Context_server` (yielding `Server_context`s): Counterparts of connecting or accepting a socket and wrapping it in a `TwoPartyVatNetwork`, for when you maintain the KJ event loop yourself. They establish the Flow-IPC session and supply the `VatNetwork` (`.vat_network()`); you create and drive `capnp::RpcSystem` over it as usual (`capnp::makeRpc{Client|Server}()`).
* Addressing: As with all Flow-IPC sessions, the server is identified via `ipc::session::App`, a simple abstraction, as opposed to a socket address as in vanilla capnp-RPC. This brings safety checks: Only the specified client applications may connect, and each side verifies the other's identity (including executable path, UID, GID).
* Works with SHM-classic and SHM-jemalloc: Provide the suitable session type as template parameters to `Ez_rpc_{client|server}`, `Context_server`, etc.
  * `ipc::session::shm::{classic|arena_lend::jemalloc}::rpc` contains the suitable aliases.
* Access to the rest of Flow-IPC: `Client_context`, `Server_context` provide `.session()`, and through it SHM arenas (for placing C++ data structures directly in SHM) and additional channels alongside the RPC conversation.
* `-> stream` methods (capnp-RPC streaming): The flow-control window is a knob, `Session_vat_network::streaming_flow_window_ki()`.

### `ipc::shm::stat` (new)
* Namespace containing stat-set `struct`s, documented in detail, common to SHM-classic, SHM-jemalloc, and SHM-providers in general.
* The `struct`s (`Owner_obj_stats`, `Lender_obj_stats`, `Borrower_obj_stats`, `Live_obj_stats`, `Shared_arena_obj_stats`): Objects constructed, lent, borrowed, destroyed; live object counts and relevant high-water marks.
* They are accessible as part of each SHM-provider's stats, on its arena (reachable via `Session::session_shm()`, `app_shm()`): SHM-classic `Pool_arena::arena_stats()`, `local_stats()`; SHM-jemalloc `Ipc_arena::sharded_stats()`, and more. `info_dump()` on the arena grabs everything, ready to print.

### `ipc::shm::classic` (SHM-classic)
* Fix `Pool_arena::construct<T>()`: For certain object sizes, it took a few bytes fewer from the SHM-pool than necessary, leading to unpredictable behavior.
  * This affects all consumers of SHM-classic: direct native storage or SHM-classic-backed `struc::Channel`; `ipc::session`-created or by direct `Pool_arena` construction.
* Fix: SHM-classic pools were not sparse; now they are sparse by default, as documented. *Sparse* here means RAM is committed only when a page is first touched (e.g., by allocation); until then it is only vaddr space.
  * Add `Pool_arena::commit()`: Take the RAM for the entire arena (<=> pool), at any time; a shortage (if any) is reported as an error at that point.
    * In effect, this gives the choice to work in either fashion. Initially any `Pool_arena` (alias `ipc::session::shm::classic::Session::Arena`) is sparse, taking only a small amount of RAM (even at the `ipc::session`-default size, 2Gi bytes). Calling `commit()` at any time makes it non-sparse.
  * Add `Pool_arena::close_shm_object_handle()`: Release the SHM-pool handle (FD) kept for `commit()`, if it will not be needed subsequently.
  * Correct the docs that related `ENOSPC` to the (sparse) pool size.
  * Guided Manual: Explain how the Linux SHM-use limit works, what happens on hitting it, and the trade-off between working with sparse versus non-sparse SHM-arenas.
* Fix `Pool_arena::construct<T>()`: If the `T` ctor threw, its allocation was not undone.
* Performance improvements:
  * Skip certain internal steps of `construct<T>()` and later destruction, when `T` is trivially-destructible.
  * Use less strict atomic operations on the object reference count.
  * Omit an unnecessary Boost.Interprocess index; avoid a certain heap allocation.
* Hardening: `Pool_arena::borrow_object()` rejects a corrupt or erroneous input blob, returning null and logging a WARNING: wrong size; pointee outside the SHM-pool vaddr range; or pointee plus `sizeof(T)` and internal metadata beyond the end of that range. Reported by Sebastián Alba Vives (@Sebasteuo).
  * Also: Defensively handle unusually-aligned inputs and atypical vaddrs, avoiding processor exceptions or exotic code generation.
* Add stats (for what is tracked, see `ipc::shm::stat` above):
  * `Pool_arena::arena_stats[_reset]()`: Per-arena/pool, kept in SHM, so readable from any process having the pool open. A read-only `Pool_arena` is enough, so a monitoring process can observe a pool with no ability to corrupt it.
  * `Pool_arena::local_stats[_reset]()`: Per-`Pool_arena` object.
  * `Pool_arena::arena_size()`, `arena_stat_free_size()`: Arena/pool size; how much of it is available now.
  * `Pool_arena::info_dump()`: All of the above.
* Out of pool space: `Pool_arena::allocate()` (and so `construct<T>()`) throws `std::bad_alloc`, not `bipc::bad_alloc` (consistently with SHM-jemalloc).
* Add aliases `Pool_arena::Activator`, `Allocator<T>` (replacing `Pool_arena_activator`, `Pool_arena_allocator<T>`).
* Minor fixes:
  * `Pool_arena::is_handle_in_arena()` undefined behavior if the ctor had failed to open or create the SHM-pool. Now returns `false`, matching other `Pool_arena` APIs.
  * Rare `error::Code::S_SHM_BIPC_MISC_LIBRARY_ERROR`, emitted when Boost.Interprocess reports a failure with no system error code (such as finding a corrupt pool on open), had value `0`, so the failure erroneously read as success; now it has an appropriate non-zero value.
  * `Pool_arena` logs using proper log-component `SHM` (was erroneously `TRANSPORT`).
  * The SHM-classic error category is named `ipc/shm/classic` (was erroneously `ipc/shm`).

### `ipc::shm::stl`
* Work around a Boost.Container bug, for those storing containers in SHM via `Stateless_allocator`: Erasing from a `vector` (or `flat_{map|set}`) of `basic_string`, `deque`, or `stable_vector` elements leaked the memory of moved-from elements.
  * Attn: The worked-around bug *is in Boost*; and it is not limited to SHM/this allocator. With other allocators, including `std::allocator`, it remains through Boost 1.92 (fixed upstream since).
  * What happened: A new unit test of in-SHM container combinations reproduced this. We reported it upstream (https://github.com/boostorg/container/issues/351) and added this work-around for Flow-IPC SHM users.
* `Stateless_allocator`: Add `difference_type`, `size_type` aliases. Certain containers (at least `boost::container::deque`) failed to compile without them, when used with SHM-jemalloc.
* Hardening: `Stateless_allocator` with `alignof(T)` greater than `alignof(std::max_align_t)` is now a compile error, rather than risking a misaligned object. Arenas (from SHM-classic, SHM-jemalloc) guarantee only the latter, like `malloc()`.

### `ipc::transport::struc::shm` (SHM-backed structured messages, notably for `struc::Channel` transmission)
* Add `Capnp_message_reader`, counterpart of `Capnp_message_builder`, for those working directly with capnp serialization in SHM.
  * Internally `shm::Reader` (from which it was split out) and capnp-RPC (new module; see above) use it.
* `Capnp_message_builder`:
  * Add optional initial-segment-size ctor arg.
  * `lend()` works with any capnp `struct` having a `shmHandleSerialization` field, not only the one internal type; so it can be used with your own schema.
    * Free functions `capnp_set_lent_shm_handle()`, `capnp_get_shm_handle_to_borrow()`: Likewise.
* Add stats: SHM-backed message building and reading, process-wide per SHM-provider (messages built and outstanding, capnp-segment allocation sizes and counts, oversized leaves, and more).
  * `stat::serializer_info_dump()`: Grab a printable snapshot of all of it (plus the heap-backed counterpart).
  * Stat-set `struct`s `stat::Core_serializer_stats` (message payload in SHM), `stat::Outer_serializer_stats` (small heap-side envelope storing the SHM-handle to that payload).
  * `stat::{Core|Outer}_serializer_global_stats<Arena>`: The underlying singletons. `reset()` is accessible here.
* Performance: Reduce by a few bytes the per-message payload sent over low-level IPC.

### `ipc::session::shm::classic` (SHM-classic-enabled sessions)
* Hardening: `Session::borrow_object()` rejects (returning null) a wrong-size blob or an invalid internal scope-ID. The latter could trip an assertion.
* Improvement: A PEER-state `Server_session` can outlive the `Session_server` that generated it, even if using app-scope arenas (`app_shm()`). It was previously undefined behavior.
  * We still recommend, here and in docs, keeping `Session_server` alive while any IPC is ongoing.
* Fix `Client_session::sync_connect()`: On failure to open a SHM-pool created by the opposing process, behavior was undefined (at best an assertion trip). Now it emits the error.
* Fix: If a `Session` was destroyed just as it was starting up, a data race could occur in its dtor. Found by a sanitizer, with errors contrived by a stress test.
* `session_shm_builder_config()`, `app_shm_builder_config()`: Add optional `segment1_sz` arg (cf. `struc::Channel::heap_fixed_builder_config()`).

### `ipc::shm::arena_lend::jemalloc`, `Shm_session` (SHM-jemalloc)
* Note: Normal use of `Ipc_arena` and `Shm_session` (`construct()`, `lend_object()`, `borrow_object()`), and of SHM-jemalloc through `ipc::session`, is unchanged in this release. See "API notes" for the items that may need attention on upgrade.
* Performance deep-dive.
  * Goal in short: Ensure SHM-jemalloc performs viably under heavy multi-threaded load.
  * Background: jemalloc is a commercial-grade memory manager, by default working in regular heap. SHM-jemalloc hooks into and wraps jemalloc to achieve a safe, garbage-collecting cross-process SHM system that benefits from jemalloc's efficiencies as a mature memory manager. The design required to achieve this integration is complex, and SHM-jemalloc's performance has been an open topic. Here we aimed to test SHM-jemalloc's speed under load and improve it as needed.
  * This project: We executed a detailed load-testing/profiling/optimization dive into SHM-jemalloc. We ran load tests on a large multi-threaded server application (split into two with heavy use of SHM-jemalloc, transmitting both native structures and `struc::Channel` messages), analyzing flame graphs and eliminating hot spots. This resulted in major (internal) changes.
    * Before the changes, throughput under load quickly collapsed.
    * After the changes -- with no change to the essential APIs -- the application performed well overall, and no particular perf hot spots remained.
    * A summary of these internal changes follows.
  * Cross-process garbage collection (GC) of `arena.construct()`ed objects now internally involves incrementing/decrementing a per-object atomic use-count stored in SHM.
    * Previously: GC involved an (internal) IPC borrower=>owner message per `shm_session.borrow_object()` call per object, resulting in potentially massive processor churn. (This is now replaced by a single atomic decrement.)
  * Pointer lookups (pointer=>vaddr, vaddr=>pointer) do not involve thread contention. All relevant (internal) data structures are thread-local.
    * Previously: Every translation involved a map lookup behind a central per-process lock. Despite the brevity of the lookup, under load this bogged the system down.
  * `Ipc_arena::Pointer` (alias to `Shm_pool_offset_ptr`): Comparison operators and pointer-minus-pointer work from the pointers' internal representation, avoiding lookups in most cases. These operations execute frequently, e.g., in iterator-based loops over SHM-stored STL-compliant containers.
  * Thread caching: `Ipc_arena::construct()`, `allocate()`, `deallocate()` use a jemalloc thread cache, automatically and correctly per thread and arena.
    * Remove `Ipc_arena::construct_thread_cached()` API. (It was experimental: it required opt-in and did not apply to `Ipc_arena::allocate()`, hence to STL-allocator allocations.)
  * Eliminate overzealous logging: Every `Shm_session::lend_object()` (so for each SHM-jemalloc-backed `struc::Channel` message, among other uses), logged an INFO-level message.
* Add `Thread_cache`: Manages one jemalloc tcache per thread per arena.
  * SHM-jemalloc uses it (internally) automatically.
  * It can also be used directly by jemalloc users unrelated to SHM.
    * Its docs explain how jemalloc tcaches work, including how these relate to jemalloc arenas.
* Add stats. (Visibility into SHM-jemalloc was a key goal: See the stats section of the Manual's SHM chapter.)
  * `Ipc_arena::info_dump()`, `Shm_session::info_dump()`: All of the below, ready to print. The former optionally includes jemalloc's native stats dump.
  * Each stat-set can also be consumed individually as a `struct`, and most can be reset:
    * Per arena, object-level activity: Owner, lender, live objects, deallocation internals.
    * Per arena, SHM-pools: Stats, as well as a listing of live pools.
    * Per arena, jemalloc: A curated, documented selection of jemalloc's own stats relevant to this arena.
      * jemalloc API's full stats output is enormous and not straightforward to consume safely.
    * Process-wide, owner side: Pointer lookups, use-count SHM-pools.
    * Borrower side, per session and process-wide: SHM-pools (including a live listing), pointer lookups.
* Add `Ipc_arena::this_thread_gc()`: See "Behavior changes" under "API notes."
* Fix `Shm_session`: Could trip an assertion and/or become unstable under intense concurrent allocation load, internally due to an unprotected concurrent access.
* `Ipc_arena::allocate()` (hence also `construct<T>()`) throws `std::bad_alloc` on failure, instead of `allocate()` returning null (which is not a valid way for an STL allocator to report failure).
* `Ipc_arena::construct<T>()`: Gracefully handle potential errors (most notably `T` ctor throwing), undoing any preceding steps as needed.
* `Shm_session`: Enable liveness checks on its internal channel, so that an opposing process that is stuck (not dead) cannot block SHM-jemalloc APIs (even potentially across threads) indefinitely.
* Hardening:
  * `deallocate()` methods are `noexcept`, as allocators formally require.
  * `Shm_session::borrow_object()`: Defensively handle unusually-aligned inputs.
  * `Ipc_arena::create()` throws if jemalloc's `opt.retain` is disabled. (It is enabled by default in 64-bit Linux.)
  * A very large allocation, which would require a SHM-pool too large for `Ipc_arena::Pointer` to address, fails normally (`std::bad_alloc`).
* Fix: For each destroyed `Ipc_arena`, avoid leaking certain resources (not RAM) associated with jemalloc's metadata block: a native handle (FD) and a mapped vaddr area (until process exit); a pool name in `/dev/shm` (until the application next executes).
* Documentation: Most of SHM-jemalloc code is now in the generated Reference.
  * Includes key user-facing items: `Ipc_arena`, `Shm_session`, as well as `Thread_cache`, `Memory_manager`.

### `ipc::session::shm::arena_lend::jemalloc` (SHM-jemalloc `ipc::session` support)
* Fix session opening, on either side: If an internal socket error occurred at just the wrong moment, behavior was undefined (at best an assertion trip).
  * Add `error::Code::S_MASTER_SHM_UNEXPECTED_TRANSPORT_ERROR`, emitted when the session's own channel was found to be hosed while setting up internal resources.
  * A related internal error during session opening is detected as early as possible.
* Fix `Session_server::async_accept()`: On certain unlikely errors in session setup, it could leak a native handle (FD).
* `session_shm_builder_config()`, `app_shm_builder_config()`: Add optional `segment1_sz` arg (cf. `struc::Channel::heap_fixed_builder_config()`).
* `rpc`: Add aliases for zero-copy capnp-RPC. (See `ipc::transport::struc::shm::rpc`.)

### Build/dependencies
* Dependency bump: Flow v2.0.0 to v3.0.0.
  * Transitive dependency bump of `fmt` v10.0.0 to 11.0.2.
* Dependency bump: capnp v1.0.1 to 1.0.2.
* Dependency bump: jemalloc v5.2.1 to v5.3.0.
  * v5.2.1 (from 2019) has a locking bug in arena destruction, which can cause a crash when SHM-jemalloc stats are read while an arena is being destroyed.
* `ipc_shm` now links `libcapnp-rpc`, which is part of a capnp install.
* Compatibility with Boost 1.91.
* Avoid `boost::interprocess` container aliases recently deprecated by Boost, which caused verbose build messages.
* Eliminate issues in CMake builds of test/demo programs which made them unreliable in parallelized builds.

### Tests
* `unit_test`: In v2.0.0 only SHM-jemalloc had test cases (`transport_test` et al covered the rest); now every module does (450+ test cases):
  * `ipc::util`, unstructured transport: Including `Channel`, `Native_socket_stream_acceptor`, `Persistent_mq_handle` implementations, `Protocol_negotiator`, transport stats, `Native_handle`, `Shared_name`.
  * Structured transport: Including `struc::[sync_io::]Channel` (log-in phase, `sync_request()` with timeout/concurrency/closure, hosing and end-sending corner cases, unexpected-response machinery, adversarial headers, stats), `Msg_out`, `Msg_in`, `Heap_fixed_builder`, `Heap_reader`, serializer stats.
  * `ipc::session`: Including session connect and identity checks (stale or corrupt CNS, rejected identities, server self-check), `open_channel()` (crossing, peer closes immediately), `Session_server` destroyed mid-accept, session end during connect, concurrent accepts, `sync_io` adapter guards, graceful end, and kernel-persistent resource cleanup after civilized exit and crash.
  * SHM-classic and general SHM: Including `Pool_arena` (sparseness and `commit()`, read-only open, out of space, ctor that throws), `Pool_arena` stats, `ipc::shm::stl` (allocator and containers, with each SHM-provider), `borrow_object()` hardening, capnp-RPC streaming flow control.
  * SHM-jemalloc: Including use-count tracking, thread-local object database, `Ipc_arena` errors and stats, `Thread_cache`, `Memory_manager`, `Shm_pool_offset_ptr` operators, `Shm_session` lifetime and stress, lend and borrow across processes.
* Integration tests:
  * Expand `transport_test` scripted mode, including batch-receive and graceful-close scenarios.
  * Add test/demo program `rpc_demo` (capnp-RPC).

### GitHub CI pipeline
* Conan 1 => 2 migration.
* Reduce pipeline run time: Dependency caching; parallelized core build step.
* Add "early-warning" jobs against newest (as opposed to only min-supported) dependency versions: Boost, capnp, jemalloc.
* Core-dump stack traces, as artifacts next to the logs; refinements to the sanitizer configurations; job filter for manually-triggered runs.

### Guided Manual
* Add chapter "Zero-copy capnp-RPC" (new module).
* Add Stats sections to the relevant chapters and a stats overview (new cross-module feature).
* Add chapter on multi-split IPC universes (was a stub chapter).
* Recommend the proper session teardown order.
* Broad review, including fixes to example code.
* Unix domain sockets are the default transport in examples; stated to be a reasonable default choice.

## Submodule repository `flow`
* Release v3.0.0 (previously: v2.0.0).

## API notes
### `ipc::util`
* New/improved APIs:
  * `ipc::util::Native_handle`:
    * Add `close()`, `is_open()`, `dup()`; `make_default()`, `is_allocated()`.
    * Add `constexpr` + `noexcept` to ctors, assignment, comparisons, `null()`, `swap()`.
    * `constexpr`ify `S_NULL_HANDLE`.
  * `Own_native_handle` owns/auto-closes a `Native_handle`. `duped_native_handle()` makes one from a `.dup()`; `disowned_native_handle()` releases it.
  * `Shared_name`:
    * Add `Shared_name::ct_from_int<Integer>()`.
    * `constexpr`ify `Shared_name::S_MAX_LENGTH`, `S_SEPARATOR`.
  * `NULL_PROCESS_CREDENTIALS`.
  * `process_running(pid)`: `EPERM` => `true`; `pid <= 0` => `false`.
* Breaking changes:
  * Remove `ipc::util::Use_counted_object`. (Not IPC-related; Flow-IPC no longer uses it.)
  * Move `ipc::util::Default_init_allocator` to `flow::util::Default_init_allocator` (Flow). It gained `rebind` in the move.

### `ipc::transport` (unstructured)
* New/improved APIs:
  * `asio_local_stream_socket`:
    * `Protocol_byte_stream` (stream, `SOCK_STREAM`), `Protocol_pkt_stream` (connected-datagram, `SOCK_SEQPACKET`).
    * `Msg_batch_in` (centered around `Msg_batch_in::nb_read()`).
  * `nb_{read|write}_some_with_native_handle()`: Work with either protocol (deduced from `Peer_socket<Protocol>*` arg); allow buffer sequences (scatter/gather).
    * With `Protocol_pkt_stream`: Detect truncation if target buffer too small => emit `error::Code::S_MESSAGE_SIZE_EXCEEDS_USER_STORAGE`.
  * `[sync_io::]Native_socket_stream`:
    * Add `async_receive_{blob|native_handle}_batch()`, `{Blob|Native_handle}_batch_in` (batch type), `S_RCV_{BLOB|NATIVE_HANDLE}_BATCH_SZ_RECOMMENDATION`.
    * Add `{blob|native_handle}_send_stats[_reset]()`, `{blob|native_handle}_receive_stats[_reset]()`.
    * `send_native_handle()`: Handle may be closed immediately after the call returns.
    * `remote_peer_process_credentials()`: Callable concurrently with one send-op and/or one receive-op.
    * `remote_peer_process_credentials(const util::Process_credentials&)`: Mutator.
  * `Native_socket_stream_cfg` (compile-time knobs, including `S_MAX_META_BLOB_LENGTH`).
  * `[sync_io::]Blob_stream_mq_receiver`: Add `async_receive_blob_batch()`, `Blob_batch_in`, `S_RCV_BLOB_BATCH_SZ_RECOMMENDATION`; `blob_receive_stats[_reset]()`.
  * `[sync_io::]Blob_stream_mq_sender`: Add `blob_send_stats[_reset]()`.
  * `Channel`: Add `async_receive_{blob|native_handle}_batch()`, `{Blob|Native_handle}_batch_in`, `S_RCV_{BLOB|NATIVE_HANDLE}_BATCH_SZ_RECOMMENDATION`; `{blob|native_handle}_send_stats[_reset]()`, `{blob|native_handle}_receive_stats[_reset]()`.
  * Concepts `[sync_io::]Blob_sender`, `Blob_receiver`, `Native_handle_sender`, `Native_handle_receiver`:
    * `send_native_handle()` contract as above.
    * The above batch-receive and stats APIs.
      * Concrete `struct`s: `stat::Blob_snd_stats`, `Blob_rcv_stats`.
  * `sync_io::Async_adapter_sender`, `sync_io::Async_adapter_receiver` (was internal-use; now public).
  * `sync_io::Native_socket_stream_acceptor::async_accept()`: Returns `false` if `start_accept_ops()` not yet called.
  * `Protocol_negotiator::S_VER_ALREADY_SENT` (alias for clarity).
  * `transport::error::Code`: Add `S_LOW_LVL_INTERNAL_PROTOCOL_INVALID_HEADER`, `S_PEER_PROCESS_NO_LONGER_EXISTS`, `S_SHM_BIPC_MISC_LIBRARY_ERROR`.
* Breaking changes:
  * Rename `asio_local_stream_socket::Protocol` to `Protocol_byte_stream`; make Boost.Asio aliases `Acceptor`, `Peer_socket`, `Endpoint` into alias templates.
    * Use (e.g.) `Peer_socket<Protocol_byte_stream>` for the previous meaning.
  * Remove `asio_local_stream_socket::async_write_with_native_handle()`, `async_read_with_target_func()`, `async_read_interruptible()`.
    * Use the `nb_*()` functions with your own async-wait (as `Native_socket_stream` does).
  * Remove `asio_local_stream_socket::release_native_peer_socket()`.
    * Use `util::Native_handle::close()`.
  * Move `sync_io::Native_socket_stream::S_MAX_META_BLOB_LENGTH` to `Native_socket_stream_cfg::S_MAX_META_BLOB_LENGTH` (now `constexpr`; value 64Ki, was 64Ki minus 2).
  * `[sync_io::]Native_socket_stream::remote_peer_process_credentials()` returns `const&` (was by-value).
  * Custom implementations of the `Blob_*`/`Native_handle_*` concepts: Must add the new batch-receive and stats APIs (and honor the upgraded `send_native_handle()` contract).

### `ipc::transport` (structured)
* New/improved APIs:
  * `struc::[sync_io::]Channel`:
    * Add `remote_peer_process_credentials()`, `remote_peer_process_liveness_check()`.
    * Add `create_msg(size_t msg_sz_cap_estimate_or_zero, Native_handle&& hndl_or_null = {})`.
    * Add `stats()`, `stats_reset()`, `stats_configure_rcv_one_off_request_rtt_histogram()`.
    * Tag ctors, `heap_fixed_builder_config()`: Add optional `segment1_sz` arg.
  * `struc::Channel`: Add `stats_configure_sync_req_latency_histogram()`; `Owned_channel_proxy`, `Owned_channel_proxy_mutable`.
  * `struc::Channel`: All methods may execute concurrently on the same object, including `sync_request()` (see above).
  * `struc::Channel_base`: Add `S_SYNC_OP_PEER_PROCESS_LIVENESS_CHECK_PERIOD`, `S_PROTOCOL_MIN_VERSION`, `S_PROTOCOL_MAX_VERSION`.
  * `struc::Msg_in`:
    * Add `emit_native_handle_or_null()` (replaces `native_handle_or_null() const`).
    * Add `serialization_size()`.
  * `struc::ostreamable_capnp_full()`, `ostreamable_capnp_brief()`.
  * `struc::Capped_sz_capnp_message_builder`, `Capped_sz_capnp_message_reader`: Capnp builder, reader over 1 user-supplied memory area.
  * `struc::[sync_io::]stat::Channel_stats`, `struc::Channel_sync_req_stats`, `Serializer_stats`, `Heap_serializer_stats`, `Heap_serializer_global_stats`, `heap_serializer_info_dump()`.
  * `struc::error::Code`: Add `S_INVALID_ARGUMENT`, `S_SYNC_OP_INTERRUPTED_BY_CONCURRENT_NB_ERROR`, `S_DESERIALIZE_FAILED_REASSEMBLY_FAILED`, `S_SERIALIZE_FAILED_SPLIT_ENCODING_TOO_BIG`, `S_SERIALIZE_FAILED_SPLIT_SEGS_TOO_MANY`.
    * `S_SERIALIZE_FAILED_SPLIT_*`: From `send()` (etc.) if a message needs too many splits (limit ~20).
    * `S_INTERNAL_ERROR_SERIALIZE_LEAF_TOO_BIG` is no longer emitted by `struc::[sync_io::]Channel`.
      * Remains for direct use of `Heap_fixed_builder::emit_serialization()` with auto-splitting disabled.
* Breaking changes:
  * `struc::[sync_io::]Channel`:
    * `send()`, `[a]sync_request()`: Take `Msg_out*`, not `const Msg_out&`.
      * Use `ch.send(&msg)`, not `ch.send(msg)`. The encoded message is not modified, only internal-use metadata. So do not send the same `Msg_out` concurrently, even via different `Channel`s; sequentially remains fine.
    * `struc::Channel::owned_channel()`, `owned_channel_mutable()`: Return locking proxies, not `const Owned_channel&` and `Owned_channel*`.
      * `owned_channel()`: Use `->`, not `.`. `owned_channel_mutable()`: `->` continues to work.
  * `struc::Msg_in`: Remove `native_handle_or_null() const`.
    * Use `emit_native_handle_or_null()`.
  * `struc::Msg_out`: Self-move-assignment is now undefined behavior.
  * `struc::error::Code`: Remove `S_INTERNAL_ERROR_DESERIALIZE_TARGET_ALLOC_FAILED`.
    * A particular internal redesign means this condition is not conceptually possible.
  * Relevant only to custom implementations of `Struct_builder`, `Struct_reader` concepts (advanced):
    * `emit_serialization()` signature changes (`Segment_bufs*`, header blob, `Split_segments*`).
    * `Struct_reader`: `add_serialization_segment(Segment_blob_in&&)`; `deserialization()` takes `const Split_segments*`; no longer movable or default-constructible.
    * Frame-postfix configuration removed.
  * Direct use of `Heap_fixed_builder`, `Heap_reader`:
    * `Heap_fixed_builder::Config`: `m_segment_sz` replaced by `m_seg_and_frame_sz_cap`, `m_segment_sz_init`; `m_frame_postfix_sz` removed; `m_stats` added.
      * Recommend (as before) `Channel::heap_fixed_builder_config()` rather than filling out fields manually.
    * `Heap_reader::Config`: `m_stats` added (pointer to target stat-set, or null).
    * `Heap_fixed_builder_capnp_message_builder`: Ctor args changed.

### `ipc::session`
* New/improved APIs:
  * `Session` (concept and all impls):
    * Add `info_collector()`, `Info_collector`.
    * Add `remote_peer_process_credentials()`.
    * Add `S_IS_SRV_ELSE_CLI`, `Sync_io_obj`, `Async_io_obj`.
  * `Info_collector`: Add `master_channel_stats[_reset]()`, `master_channel_transport_{snd|rcv}_stats[_reset]()`, `master_channel_live()`.
  * `Session_server`: Add `mq_msg_size_limit()` (accessor, mutator; each new `Session` uses the value as of its `async_accept()`), `Sync_io_obj`, `Async_io_obj`.
  * `Session_cfg`: Add `S_PING_PERIOD`, `S_IDLE_TIMEOUT`.
  * `error::Code`: Add `S_CLIENT_MASTER_LOG_IN_SERVER_APP_INCONSISTENT_CREDS`, `S_SERVER_APP_EXEC_PATH_INCONSISTENT`.
  * `Client_session::open_channel()`: Can emit a non-fatal system error (see above).
* Breaking changes:
  * `Session_mv`: Remove `heap_fixed_builder_config()` (both overloads). The max message size it assumed is no longer constant (see `Session_server::mq_msg_size_limit()`).
    * Use `struc::Channel::heap_fixed_builder_config()`, which requires only an unstructured `Channel`; or fill out `Heap_fixed_builder::Config` values directly.
  * `Session`: Rename `S_SHM_TYPE` to `S_SHM_TYPE_OR_NONE`.
* Behavior changes that may need attention:
  * `Server_app::m_exec_path` must now exactly match how the session-server was invoked (as already required of `Client_app::m_exec_path`). A wrong value previously had no effect; whereas now:
    * `Session_server` ctor emits an error (and will not open sessions) if the `Server_app` arg, describing the server itself, has the mismatch.
    * `Client_session::sync_connect()` emits an error if the connect-target `Server_app` arg has this mismatch.
  * `Server_app::m_user_id`, `m_group_id`: `Client_session::sync_connect()` now likewise emits an error on a mismatch in the connect-target `Server_app` arg.
  * `App::m_name` must be non-empty and not contain the `Shared_name` separator (underscore); otherwise `Session_server` ctor now emits an error.
    * Notably: Underscores are disallowed, even though earlier docs, alongside the separator rule, allowed them by mistake. Such a name could cause trouble, as it interferes with resource naming. For example, a session-server `foo` at startup could remove kernel-persistent resources (MQs, SHM-classic pools) of a running session-server `foo_bar`.
  * `async_accept(&S)`: `S` is made empty immediately. If it held an open session (unlikely but formally allowed), that session ends at that point.

### `ipc::transport::struc::shm::rpc`
All new (see above): `Session_vat_network`, `Ez_rpc_client`, `Ez_rpc_server`, `Client_context`, `Context_server`, `Server_context`.

### `ipc::shm::stat`
Add `Owner_obj_stats`, `Lender_obj_stats`, `Borrower_obj_stats`, `Live_obj_stats`, `Shared_arena_obj_stats`.

### `ipc::shm::classic`
* New/improved APIs:
  * `Pool_arena`: Add `commit()`, `close_shm_object_handle()`, `arena_size()`, `arena_stat_free_size()`, `arena_stats[_reset]()`, `local_stats[_reset]()`, `info_dump()`, `Info_dump`, `Activator`, `Allocator<T>`.
  * `Pool_arena::is_handle_in_arena()`: Returns `false`, not undefined behavior, if the SHM-pool was not opened or created.
  * Add `stat::Arena_stats`, `Local_stats`, `Arena_info_dump`.
  * `error::Code`: Add `S_SHM_POOL_OPEN_READ_ONLY_FOUND_BUT_UNINIT`.
* Breaking changes:
  * Remove `Pool_arena_activator`, `Pool_arena_allocator<T>`.
    * Use `Pool_arena::Activator`, `Pool_arena::Allocator<T>`.
  * `Pool_arena`: Remove `construct_at()`.
    * Use `flow::util::construct_at()` (Flow).
  * `Pool_arena::allocate()`, `construct<T>()`: Out of space throws `std::bad_alloc`, not `bipc::bad_alloc`.
* Behavior changes that may need attention:
  * Pools are sparse, as documented (see above). If the system is out of RAM for SHM (Linux: the size limit of the tmpfs underlying POSIX SHM) when a page is first touched, there is no opportunity to report an error: The touching process is killed (`SIGBUS`).
    * If you prefer to learn of a shortage up-front, as an error, call `commit()`. In this case be mindful of pool size: All of it is then taken from general availability (with `ipc::session`, 2Gi bytes per pool by default; see `pool_size_limit_mi()`).
  * Each `Pool_arena` keeps an SHM-pool handle (FD) open, for `commit()`, until `close_shm_object_handle()` or destruction.
  * `Pool_arena` read-only ctor (`read_only = true`): Do not use until a creating ctor has completed. Doing so can cause undefined behavior, or `S_SHM_POOL_OPEN_READ_ONLY_FOUND_BUT_UNINIT`; do not rely on the latter.
    * Other ctor forms are unaffected, and typically a read-only opener learns of a pool only after its creation anyway.

### `ipc::shm::stl`
* Breaking changes:
  * `Stateless_allocator`, `Arena_activator`: Rename `Arena_obj` alias to `Arena`.
  * `Stateless_allocator`: `alignof(T)` greater than `alignof(std::max_align_t)` is a compile error.

### `ipc::transport::struc::shm`
* New/improved APIs:
  * Add `Capnp_message_reader`.
  * `Capnp_message_builder` ctor: Add optional `segment_sz_init_pre_growth`, `stats` args.
  * Add `stat::Core_serializer_stats`, `Outer_serializer_stats`, `Core_serializer_global_stats`, `Outer_serializer_global_stats`, `Serializer_info_dump`, `serializer_info_dump()`.
  * `Builder::Config`: Add `m_segment_sz_init`, `m_core_stats`, `m_stats`.
* Breaking changes:
  * `schema::ShmHandle`: Rename field `serialization` to `shmHandleSerialization`. Encoding is unaffected.
  * `Capnp_message_builder`: Move `Borrower_allocator`, `Segment_in_shm_borrowed`, `Segments_in_shm_borrowed` to `Capnp_message_reader`.
  * `Capnp_message_builder::lend()`: Now a template, taking the capnp root `struct` and `Session` types.
  * `capnp_set_lent_shm_handle()`, `capnp_get_shm_handle_to_borrow()`: Now templates, on the capnp root `struct` type.
  * `Builder::Config`: Rename `m_top_builder_frame_prefix_sz` to `m_frame_prefix_sz`; remove `m_top_builder_frame_postfix_sz`.
  * `Builder::emit_serialization()`, `Reader::add_serialization_segment()`, `Reader::deserialization()`: Signatures changed (cf. `Struct_builder`, `Struct_reader` above).

### `ipc::session::shm::classic`
* New/improved APIs:
  * `Session`, `Server_session`: Add `Vat_network`.
  * `session_shm_builder_config()`, `app_shm_builder_config()`: Add optional `segment1_sz` arg.
  * `rpc` (new namespace): Add `Client_session`, `Server_session`, `Session_server` aliases.

### `ipc::shm::arena_lend::jemalloc`, `ipc::session::shm::arena_lend::jemalloc::Shm_session`
* New/improved APIs:
  * Add `Thread_cache`.
  * `Ipc_arena`: Add `Allocator<T>`, `Activator`, `this_thread_gc()`, `info_dump()`, `Info_dump`, `sample_hi_wmarks()`.
    * Stats: `sharded_stats[_reset]()`, `pool_stats[_reset]()`, `shm_pool_live_info()`, `memory_manager_stats[_reset]()`, `owner_pool_lookup_global_stats()`, `obj_db_aux_pool_global_stats()`, `global_stats_reset()`.
  * `Shm_session`: Add `Arena`, `Handle<T>`, `Borrower_arena_allocator<T>`, `info_dump()`, `Info_dump`.
    * Stats: `borrower_pool_stats[_reset]()`, `borrower_pool_stats_process_wide()`, `borrower_pool_lookup_global_stats()`, `global_stats_reset()`, `borrowed_shm_pool_live_info()`.
  * `ipc::shm::arena_lend::stat`, `ipc::shm::arena_lend::jemalloc::stat`: Add stat-sets and info-dump `struct`s.
    * `ipc::shm::stat` holds those common to the SHM-providers (SHM-jemalloc, SHM-classic).
* Breaking changes:
  * `Ipc_arena`:
    * Remove `construct_thread_cached()`.
      * Use `construct()`; tcache use is automatic.
    * `allocate()` throws on failure instead of returning null (see above).
    * Rename `get_arena_id()` to `get_jemalloc_arena_id()`.
  * `Memory_manager`: Its API is replaced; please ignore what was there previously. The default ctor (no `Logger*`) remains what `Ipc_arena::create()` needs. It now offers, for jemalloc power users, `allocate[_no_tcache]()`, `deallocate[_no_tcache]()`, `create_arena()`, `destroy_arena()`, `stats_dump_to_ostream()`, `stats_dump_to_string()`, `log_stats_dump()`.
    * Remove `ipc::shm::arena_lend::Memory_manager`, its base class.
* Breaking changes, only if setting up SHM-jemalloc directly, instead of via `ipc::session`:
  * `Ipc_arena::create()`: Takes `Shared_name pool_name_base`, not a name-generator function.
  * `Shm_session::create()`: Takes, and owns, a plain unstructured channel (`Shm_channel`) and a session token. Remove the borrower-repository, channel-timeout args.
    * Channel-related error reporting is simplified: The first disconnection is reported via the handler, uniformly.
  * `Shm_session`: Remove `set_disconnected()`.
* Behavior changes that may need attention:
  * If a process crashes while holding `app_shm()` objects borrowed from a session-server, those objects stay allocated until that `Session_server` is destroyed. Previously, ending that session released them. Session-scope arenas are unaffected. We plan to address this.
  * If the last reference to an object is dropped in a thread or process other than the one that constructed it, then the object is destroyed and deallocated by the constructing thread: during its subsequent SHM-jemalloc activity, or after its exit. So while that thread is alive but idle, the object remains allocated.
    * If that matters to you, call `this_thread_gc()` from such a thread as it goes idle.
  * Up to about 1 million `construct()`ed objects can be live per constructing-thread per arena. Beyond that `construct()` throws `std::bad_alloc`.

### `ipc::session::shm::arena_lend::jemalloc`
* New/improved APIs:
  * `Session`, `Server_session`: Add `Vat_network`.
  * `session_shm_builder_config()`, `app_shm_builder_config()`: Add optional `segment1_sz` arg.
  * `error::Code`: Add `S_MASTER_SHM_UNEXPECTED_TRANSPORT_ERROR`.
  * `rpc`: Add `Client_session`, `Server_session`, `Session_server` aliases.
