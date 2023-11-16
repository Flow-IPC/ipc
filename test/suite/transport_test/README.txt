transport_test.exec is an integration-test program that tests ipc::transport, ipc::session, and ipc::shm features.
- It is not a unit test suite.
- It has 2 basic modes which I describe now in order.

### SCRIPTED MODE ###

In this mode it is an interactive(ish) tool that:
  - So far tests much of the *unstructured* layer (not *structured* layer) of ipc::transport.
  - So far avoids any dependence on ipc::session (i.e., it establishes varius IPC pipes manually, though it does get
    up to the transport::Channel level (wherein various pipes are bundled together)).
  - Can be used at a whim to test various patterns of API use without constant laborious code editing and recompiling.
    - This is already achieved.  I (ygoldfel) wrote it b/c testing what I wanted to test the usual way seemed painful.
    - C/f EXERCISE MODE below.
  - Can be used as *part* of a test suite.
    - This is partially achieved but not fully.  It does test all major things, as I keep adding them.
      - Some things cannot be tested without having hooks inside the actual code that instrument certain conditions that
      are hard-to-nearly-impossible to trigger "naturally": e.g., would-block conditions for writing; some errors.
      - The solution here is to add such hooks and then test them either using this tool or plain unit tests.
      - The 2 methods together should fully cover all code paths.  This is a TODO (a ticket probably).

As of this writing, it is not necessarily meant for, like, an outside test team to mess with.  Probably it needs better
--help, etc.  Still it is fairly slick IMO.

The tool:
  - writes informational logs about test execution to console (sev = 3rd arg); [info] sev is usually best unless
  debugging tool itself and/or you want to see which line of the in-script is being invoked when;
  - writes Flow-IPC logs to file log (2nd arg) with sev (4th arg); [data] and [trace] are useful; [info] is good as a
  way to check realistically the verbosity level of the library APIs (do note sometimes timeouts in in-script are so
  tight that using [data] or [trace] may lead to a test failure due to timeout);
  - reads/parses the entire script for STDIN, then executes it in order (so either type all lines, Ctlr-D; or redirect
  from a file);
  - acts synchronously: each command completes before the next starts (and if it fails, the program exits).

Therefore -- as well as for IPC-realism -- one must run at least 2 invocations of the program.  As of this writing I've
been going with one guy acting as server, the other as client; but really they can each do whatever; and there can be 3+
invocations too.  For now though I have a couple of scripts meant to be invoked like this:

Terminal 1:
$ # When invoking `cmake` use `-DCFG_ENABLE_TEST_SUITE=ON`, so that we get built/installed.
$ cd (install root)/bin/transport_test
$ rm -f *.log && ./transport_test.exec scripted transport_test.srv.log info info < srv-script.txt

then within a few seconds (before it times out waiting for 1st connect):

Terminal 2:
$ ./transport_test.exec scripted transport_test.cli.log info info < cli-script.txt

It'll be done in a few seconds and say whether it passed or failed (in each terminal).  One can then check out the
console 2x and Flow-IPC *.log 2x for details.

### EXERCISE MODE ###

In this mode it is a non-interactive tool that:
  - Tests (1) *structured* layer of ipc::transport (indirectly of course this tests unstructured layer as well which
  sits below it); (2) ipc::session; and (3) ipc::shm.  So it opens sessions, including but not limited to SHM-enabled
  ones; channels within sessions; sends structured data through channels.  When SHM-enabled it tests both sending
  zero-copy structured data as structured (capnp) messages as well as handles to native C++/STL data structures
  sitting in SHM.
  - Requires writing C++ code and recompiling to add/modify tests.
    - C/f SCRIPTED MODE above.  A scriptable/interactive tool I (ygoldfel) deemed too hairy to write for the
    ipc::session and structured-layer-ipc::transport areas of Flow-IPC.  Could be wrong.
  - Can be used as *part* of a test suite.  (See notes for SCRIPTED MODE regarding this.  They apply here too.)

The tool:
  - writes informational logs about test execution to console (sev = 3rd arg); [info] is fine as of this writing;
  - writes Flow-IPC logs to file log (2nd arg) with sev (4th arg); [data] and [trace] are useful; [info] is good as
  a way to check realistically the verbosity level of the library APIs;
  - executes certain hard-coded steps;
  - can be invoked in 1 of 3 modes, controlling whether/how SHM (SHared Memory) is exercised:
    - exercise-{srv|cli}: SHM is disabled.
      - Channels transmit messages sans zero-copy (each msg is internally copied into IPC transport; then out of it).
      - Tests of manual transmission of SHM-stored objects via channels = skipped.
    - exercise-{srv|cli}-shm-{c|j}:
      - Channels transmit messages w/ full zero-copy (only a small SHM handle per msg is transmitted internally).
      - Tests of manual transmission of SHM-stored objects via channels = enabled.
      - shm-c => SHM-classic provider is used; shm-j => SHM-jemalloc.  (The 2 are similar but have some different
        capabilities -- generally shm-c is more flexible in some advanced ways and thus some tests apply to it
        but not shm-j.)

Therefore -- as well as for IPC-realism -- one must run at least 3 invocations of the program.  ipc::session has
certain finicky (by design) requirements about binaries executing from well-known, hard-coded locations; so you
will need to make some slightly annoying preparations each time you execute the tests.  Like this:

One-time:
$ mkdir -p ~/bin/ex_srv_run ~/bin/ex_cli_run # ~/bin stores executables; ~/bin/*_run are convenient CWDs for execution.

One-time (per boot):
$ mkdir -p /tmp/var/run # PID (Current Namespace Store = CNS) files will live there.

Stage:
$ # When invoking `cmake` use `-DCFG_ENABLE_TEST_SUITE=ON`, so that we get built/installed.
$ cp -vf (install root)/bin/transport_test/transport_test.exec ~/bin/ex_srv.exec && \
  cp -vf ~/bin/ex_srv.exec ~/bin/ex_cli.exec
# Tip: Use `cmake -DCMAKE_BUILD_TYPE=Debug` for a much faster (possibly 10x) build.  Of course an unoptimized build
#   will have slowness; but there are times when one won't care about that, and build time is more important.
#   Usually you do want an optimized build but when debugging less so -- you get the idea.
# Tip: For good perf realism but a faster build use `cmake -DCMAKE_BUILD_TYPE=Release -DCFG_NO_LTO=YES`; it will
#   be optimized but without LTO (link-time optimization).
# Tip: When debugging it can be extremely helpful to enable assert()s.  The easiest way is to use
#   CMAKE_BUILD_TYPE=Debug; but if one wants perf realism it should be possible to use standard CMake knobs to
#   not-define macro NDEBUG together with the Release build type.

Terminal 1:
# Test without SHM: leave off the `-shm-$x`.  Otherwise include `-shm-$x`.
# $x can be `c`: Will use SHM-classic SHM-provider.
# $x can be `j`: Will use SHM-jemalloc SHM-provider.
$ cd ~/bin/ex_srv_run && rm -f *.log && ~/bin/ex_srv.exec exercise-srv[-shm-{c|j}] transport_test.srv.log info info

then (as it waits for 1st connect):

Terminal 2:
# Leave off/include the `-shm-$x` in sync with the above command line from terminal 1.
$ cd ~/bin/ex_cli_run && rm -f *.log && ~/bin/ex_cli.exec exercise-cli[-shm-{c|j}] transport_test.cli.log info info

It'll be done in some time and say whether it passed or failed (in each terminal).  One can then check out the
console 2x and Flow-IPC *.log 2x for details.  If it/they crashed, especially from assert() failures in certain fail
situations, and core(s) was/were generated, it/they'll be named `core` in one/both of ~/bin/ex_*_run.  Hence one
might `gdb ex_*.exec core`.

Quick note about logging (you can also run the program without args for some minimal help): The last arg (set to
`info` above) determines the verbosity of the main log of the run.  It should indeed be set to `info` normally.  However
if debugging one can change it to `trace` or `data` for more detail (or even more still, respectively); and this can
be done independently for the server and/or client invocation.  Beware that this may affect the timing of certain things
and possibly trigger failures not seen normally; but usually you won't care at that point, as you'll know what you're
doing and will be looking for specific information.  Having resolved the issue put it back to `info`.
