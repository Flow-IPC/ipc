# Installing the software

## Organization

The directory containing the present `README.md` is the root of the Flow-IPC meta-project.  It is most
typically bundled as one thing, with all necessary (`ipc_*` and `flow`) sub-projects included as
subdirectories.  The build/installation procedure will take care of everything for you
(not including the handful of third-party prerequisites like Boost which will need to be installed
separately; instructions below).

We do assume you've grabbed everything per [README: Obtaining the source code](./README.md#obtaining-the-source-code).

## Installation

An exported Flow-IPC consists of C++ header files installed under "ipc/..." and "flow/..." in the
include-root; and libraries such as `libflow.a` and `lipipc_*.a` (as of this writing 5 of the latter).
Certain items are also exported for people who use CMake to build their own
projects; we make it particularly easy to use Flow and Flow-IPC proper in that case
(`find_package(Flow)`, `find_package(IpcCore)`, `find_package(IpcTransportStructured)`, `find_package(IpcSession)`,
`find_package(IpcShm)`, and `find_package(IpcShmArenaLend)`).  Lastly documentation is included in the
source tree for local perusal (for Flow and the totality of Flow-IPC proper, separately; so 2 sets of docs);
and can be optionally re-generated from local source (more on that below).

The basic prerequisites for *building* the above:

  - Linux;
  - a C++ compiler with C++ 17 support;
  - Boost headers (plus certain libraries) install;
  - {fmt} install;
  - CMake;
  - Cap'n Proto (a/k/a capnp);
  - jemalloc;
  - (optional, only if running unit tests) GoogleTest;
  - (optional, only if generating docs) Doxygen and Graphviz.

**Note regarding capnp and jemalloc:** At this time, capnp and jemalloc are indeed required to build
the meta-project Flow-IPC.  That said, in actuality, jemalloc is really needed *only* if one wishes
to use `ipc_shm_arena_lend` (a/k/a **SHM-jemalloc** feature); this feature is (if we do say so ourselves)
impressive, but it *is* optional; in many cases one could use `ipc_shm` only, with almost no code changes
and adequate (in some ways superior; there is a trade-off) functionality.  As for capnp: if one uses
only `flow` and `ipc_core`, then capnp is also optional.  That said, working with structured non-native data,
sessions, and SHM are very much key features; `ipc_core` will supply low-level un-structured IPC only and
without SHM support (because internally capnp is used in `ipc_session` and `ipc_shm*`).
So capnp is more or less required to get the real Flow-IPC goodness.

All of that said: *At this time* the meta-project build will build *all* the modules (even if you, quite
reasonably, aim to not use all of them), and therefore both capnp and jemalloc are necessary.  However, it is
a quite simple task to add CMake build knobs to disable the building of some subset of the `flow` and `ipc_*`
sub-projects; it's just a little hacking of `./CMakeLists.txt`.  For example some users would exercise the
ability to skip `ipc_shm_arena_lend` (not needing SHM-jemalloc) and therefore not need jemalloc installed
either.  This is a *to-do*: watch this space.  **End of note**

The basic prerequisites for *using* the above:

  - Linux, C++ compiler, Boost, {fmt}, capnp, jemalloc (but CMake is not required); plus:
  - your source code `#include`ing any exported `flow/` and/or `ipc/` headers must be itself built in C++ 17 mode;
  - any executable using the `flow` and/or `ipc_*` libraries must be linked with certain Boost and ubiquitous
    system libraries.

We intentionally omit version numbers and even specific compiler types in the above description; the CMake run
should help you with that.

To build Flow-IPC (including Flow):

  1. Ensure a Boost install is available.  If you don't have one, please install the latest version at
     [boost.org](https://boost.org).  If you do have one, try using that one (our build will complain if insufficient).
     (From this point on, that's the recommended tactic to use when deciding on the version number for any given
     prerequisite.  E.g., same deal with CMake in step 2.)
  2. Ensure a {fmt} install is available (available at [{fmt} web site](https://fmt.dev/) if needed).
  3. Ensure a CMake install is available (available at [CMake web site](https://cmake.org/download/) if needed).
  4. Ensure a capnp install is available (available at [Cap'n Proto web site](https://capnproto.org/) if needed).
  5. Ensure a jemalloc install is available (available at [jemalloc web site](https://jemalloc.net/) if needed).
     - If you are already using jemalloc in your under-development executable(s), great.  We will work
       whether you're using it to replace default `malloc()`/`free()` and (orthogonally) `new`/`delete`; or
       not.
     - If you are *not* already using jemalloc: When building jemalloc, during its `configure` step, you
       have 2 choices to make.
       - Whether to replace default `malloc()`/`free()` in your application(s).  This is entirely orthogonal
         to Flow-IPC's operation per se; rather it will affect general heap use in your application.
         Most likely converting your heap provider to jemalloc is a separate mission versus using Flow-IPC;
         so your answer will then be no, you don't want to replace `malloc()` and `free()`.  In that case,
         when building jemalloc, use its `configure` feature wherein one supplies an API-name prefix to that
         script.
         - We recommend the prefix: `je_`.  (Then the API will include `je_malloc()`, `je_free()`, and others.)
         - If you do (now, or later) intend to replace the default `malloc()`/`free()` with jemalloc's, then
           do not supply any prefix to `configure`.
       - Whether to replace `new`/`delete` (though the default impls may forward to `malloc()`/`free()`; in which
         case even if you do *not* replace them, the choice in the previous bullet will still have effect).
         This is a binary decision: Most likely, again, you don't want this replacement quite yet;
         so tell jemalloc's `configure` that via particular command-line flag.  If you do, then tell `configure`
         *that*.
     - Flow-IPC will automatically build in the way compatible with the way you've built jemalloc.
       (Our CMake script(s) will, internally, use `jemalloc_config` program to determine the chosen API-name
       prefix.)
  6. (Optional, only if running unit tests) Have GoogleTest install available.
  7. (Optional, only if generating docs) Have Doxygen and Graphviz installs available.
  8. Use CMake `cmake` (command-line tool) or `ccmake` (interactive text-UI tool) to configure and generate
     a build system (namely a GNU-make `Makefile` and friends).  Details on using CMake are outside our scope here;
     but the basics are as follows.  CMake is very flexible and powerful; we've tried not to mess with that principle
     in our build script(s).
     1. Choose a tool.  `ccmake` will allow you to interactively configure aspects of the build system, including
        showing docs for various knobs our CMakeLists.txt (and friends) have made available.  `cmake` will do so without
        asking questions; you'll need to provide all required inputs on the command line.  Let's assume `cmake` below,
        but you can use whichever makes sense for you.
     2. Choose a working *build directory*, somewhere outside the present `ipc` distribution.  Let's call this
        `$BUILD`: please `mkdir -p $BUILD`.  Below we'll refer to the directory containing the present `README.md` file
        as `$SRC`.
     3. Configure/generate the build system.  The basic command line:
        `cd $BUILD && cmake -DCMAKE_INSTALL_PREFIX=... -DCMAKE_BUILD_TYPE=... -DCFG_ENABLE_DOC_GEN=ON $SRC`,
        where `$CMAKE_INSTALL_PREFIX/{include|lib|...}` will be the export location for headers/library/goodies;
        `CMAKE_BUILD_TYPE={Release|RelWithDebInfo|RelMinSize|Debug|}` specifies build config;
        `CFG_ENABLE_DOC_GEN` makes it possible to locally generate documentation (if so desired).
        More options are available -- `CMAKE_*` for CMake ones; `CFG_*` for Flow-IPC/Flow ones -- and can be
        viewed with `ccmake` or by glancing at `$BUILD/CMakeCache.txt` after running `cmake` or `ccmake`.
        - Regarding `CMAKE_BUILD_TYPE`, you can use the empty "" type to supply
          your own compile/link flags, such as if your organization or product has a standard set suitable for your
          situation.  With the non-blank types we'll take CMake's sensible defaults -- which you can override
          as well.  (See CMake docs; and/or a shortcut is checking out `$BUILD/CMakeCache.txt`.)
        - This is the likeliest stage at which CMake would detect lacking dependencies.  See CMake docs for
          how to tweak its robust dependency-searching behavior; but generally if it's not in a global system
          location, or not in the `CMAKE_INSTALL_PREFIX` (export) location itself, then you can provide more
          search locations by adding a semicolon-separated list thereof via `-DCMAKE_PREFIX_PATH=...`.
        - Alternatively most things' locations can be individually specified via `..._DIR` settings.
     4. Build using the build system generated in the preceding step:  In `$BUILD` run `make`.
        - (To generate documentation run `make ipc_doc_public ipc_doc_full flow_doc_public flow_doc_full`.)
     5. Install (export):  In `$BUILD` run `make install`.

To use Flow-IPC/Flow:

  - `#include` the relevant exported header(s).
  - Link the exported libraries (such as `libipc_*.a`, `libflow.a`) and the required other libraries to
    your executable(s).
    - If using CMake to build such executable(s):
      1. Simply use `find_package($X)`, where `$X` is each of: `Flow`, `IpcCore`, `IpcTransportStructured`,
         `IpcSession`, `IpcShm`, and `IpcShmArenaLend` to find it.
      2. Then use `target_link_libraries(... $X)` on your target to ensure all necessary libraries are linked.
         Here `$X` is each of: `Flow::flow`, `IpcCore::ipc_core`, `IpcTransportStructured::ipc_transport_structured`,
         `IpcSession::ipc_session`, `IpcShm::ipc_shm`, and `IpcShmArenaLend::ipc_shm_arena_lend`.
         (This will include the libraries themselves and the dependency libraries it needs to avoid undefined-reference
         errors when linking.  Since `ipc_shm_arena_lend` depends on all the others including `flow`, it may be
         possible to just specify that one, and the others will be picked up by CMake's transitive dependency
         tracking.  Details on such things can be found in CMake documentation; and/or you may use
         our CMake script(s) for inspiration; after all we do build all the libraries and a `*_link_test.exec`
         executable for each except `flow`.)
    - Otherwise specify it manually based on your build system of choice (if any).  To wit, in order:
      - Link against `libipc_shm_arena_lend.a`, `libipc_shm.a`, `libipc_session.a`, `libipc_transport_structured.a`,
        `libipc_core.a`, and `libflow.a`.
      - Link against Boost libraries mentioned in a `CMakeLists.txt` line (search `$SRC` for it):
        `set(BOOST_LIBS ...)`.
      - Link against the {fmt} library, `libfmt`.
      - Link against the system pthreads library, `librt`, and (for `ipc_shm_arena_lend`) `libdl`.
  - Read the documentation to learn how to use Flow-IPC's (and/or Flow's) various features.
    (See Documentation below.)

## Documentation

The documentation consists of:
  - (minor) this README (and less interesting sub-project `*/README.md`s, except arguably `flow/README.md` may be of
    interest possibly);
  - (minor) comments, about the build, in `CMakeLists.txt`, `*.cmake`, `conanfile.py` (in various directories
    including this one where the top-level `CMakeLists.txt` lives);
  - (major/main) documentation directly in the comments throughout the source code; these have been,
    and can be again, conveniently generated using certain tools (namely Doxygen and friends), via the
    above-shown `make ipc_doc_public ipc_doc_full flow_doc_public flow_doc_full` command.
    - The generated documentation consists of:
      - (Flow-IPC proper) a clickable guided Manual + Reference.
      - (Flow) a clickable Reference.
    - Seeing the doc comments in the various .hpp files works.
    - Browsing the clickable generated documentation is probably quite a bit nicer.

To read the latter -- the guided Manual + References -- consider the following.
  - The latest generated docs from the source code in `*/src/*` has been included nearby:
    - Flow-IPC proper: Use a browser to open `doc/ipc_doc/index.html`.
    - Flow: Use a browser to open `flow/doc/flow_doc/index.html`.
  - Or see the online documentation at the [project web site](https://flow-ipc.github.io).  This will just mirror what
    the above from the corresponding source code: the tip of the master branch; and for each released version of
    the project.
  - If you're perusing docs only, that's all.  You're done!

In contrast, if you have changed the source code (for Flow-IPC proper, Flow, or both): See
[CONTRIBUTING](./CONTRIBUTING.md) file.
It includes instructions on how docs are generated and updated.  Spoiler alert: Most things are automatic;
the only manual part is that you should peruse any changed docs for your visual satisfaction before
submitting your code.
