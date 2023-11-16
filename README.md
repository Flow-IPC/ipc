# Flow-IPC  -- Modern C++ toolkit for high-speed inter-process communication (IPC); plus general-purpose toolkit, Flow

Multi-process microservice systems need to communicate between processes efficiently.  Existing microservice
communication frameworks are elegant at a high level but add unacceptable latency out of the box.  Low-level
interprocess communication (*IPC*) solutions, typically custom-written on-demand to address this problem,
struggle to do so comprehensively and in reusable fashion.  Teams repeatedly spend resources on challenges
like structured data and session cleanup.  These issues make it difficult to break monolithic systems into
more resilient multi-process systems that are also performant.

Flow-IPC is a modern C++ library that solves these problems.  It adds virtually zero latency.  Structured
data are represented using the high-speed Cap’n Proto (*capnp*) serialization library, which is integrated
directly into our shared memory (SHM) system.  The Flow-IPC SHM system extends a commercial-grade memory
manager (*jemalloc*, as used by FreeBSD and Meta).  Overall, this approach eliminates all memory copying
(end-to-end *zero copy*).

Flow-IPC features a session-based channel management model.  A *session* is a conversation between two
programs; to start talking one only needs the name of the other program.  Resource cleanup, in case of exit or
failure of either program, is automatic.  Flow-IPC’s sessions are also safety-minded as to the identities
and permissions at both ends of the conversation.

Flow-IPC’s API allows developers to easily adapt existing code to a multi-process model.  Instead of each
dev team writing their own IPC implementation piecemeal, Flow-IPC provides a highly efficient standard that
can be used across many projects.

Also included is the general-purpose C++ toolkit named Flow.  While it is bundled for convenience simply
because (1) it is needed and (2) shares the same original authorship and DNA as Flow-IPC proper, it may
nevertheless prove useful in its own right.  For more information please see a similar-purpose `README.md`
in the `flow/` sub-directory (or, if you so choose, another location where you've decided to place Flow
yourself; more on this below under Organization).

## Organization

The directory containing the present `README.md` is the root of the Flow-IPC meta-project.  It is most
typically bundled as one thing, with all necessary (`ipc_*` and `flow`) sub-projects included as
subdirectories.  The build/installation and doc generation procedure will take care of everything for you
(not including the handful of third-party prerequisites like Boost which will need to be installed
separately; instructions below).

If you've obtained this project as a pre-packaged source tarball (.tgz), then you have everything you need
(other than potentially third-party prerequisites like Boost), located where it needs to be (namely,
the subdirectories `ipc_*` and `flow`).  This is great and easy.  We recommend working this way in all
situations *except if your aim is to make changes that would be checked-in to the public open-source project*.
(If that is your aim eventually, but not right now, it might make sense to work in the monolithic model for
a while; then switch-over to the Git setup described in the following paragraphs, once you're ready to
contribute to the public project.)

If you do not aim to immediately contribute to the public project, we encourage you to skip to the next section.

If you *do* aim to immediately contribute to the public project, please see Contributing in the present
document.

## Installation

An exported Flow-IPC consists of C++ header files installed under "ipc/..." and "flow/..." in the
include-root; and a libraries such as `libflow.a` and `lipipc_*.a` (as of this writing 5 of the latter).
Certain items are also exported for people who use CMake to build their own
projects; we make it particularly easy to use Flow and Flow-IPC proper in that case
(`find_package(Flow)`, `find_package(IpcCore)`, `find_package(IpcTransportStructured)`, `find_package(IpcSession)`,
`find_package(IpcShm)`, and `find_package(IpcShmArenaLend)`).  Lastly documentation
can be optionally generated (for Flow and the totality of Flow-IPC proper, separately; so 2 sets of docs).

The basic prerequisites for *building* the above:

  - Linux;
  - a C++ compiler with C++ 17 support;
  - Boost headers (plus certain libraries) install;
  - CMake;
  - Cap'n Proto (a/k/a capnp);
  - jemalloc;
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

  - Linux, C++ compiler, Boost, capnp, jemalloc (but CMake is not required); plus:
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
  2. Ensure a CMake install is available (available at [CMake web site](https://cmake.org/download/) if needed).
  3. Ensure a capnp install is available (available at [Cap'n Proto web site](https://capnproto.org/) if needed).
  4. Ensure a jemalloc install is available (available at [jemalloc web site](https://jemalloc.net/) if needed).
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
  5. (Optional, only if generating docs) Have Doxygen and Graphviz installs available.
  6. Use CMake `cmake` (command-line tool) or `ccmake` (interactive text-UI tool) to configure and generate
     a build system (namely a GNU-make `Makefile` and friends).  Details on using CMake are outside our scope here;
     but the basics are as follows.  CMake is very flexible and powerful; we've tried not to mess with that principle
     in our build script(s).
     1. Choose a tool.  `ccmake` will allow you to interactively configure aspects of the build system, including
        showing docs for various knobs our CMakeLists.txt (and friends) have made availale.  `cmake` will do so without
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
        - (To "install" the regenerated documentation in its proper location please follow the simple steps in
          Documentation below.)

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
      - Link against the system pthreads library, `librt`, and (for `ipc_shm_arena_lend`), `libdl`.
  - Read the documentation to learn how to use Flow-IPC's (and/or Flow's) various features.
    (See Documentation below.)

## Documentation

The documentation consists of:
  - (minor) this README (and less interesting sub-project `*/README.md`s, except arguably `flow/README.md` may be of
    interest possibly);
  - (minor) comments, about the build, in `CMakeLists.txt` (in various directories including this one, the top-level
    build script);
  - (major/main) documentation directly in the comments throughout the source code; these have been,
    and can be again, conviently generated using certain tools (namely Doxygen and friends), via the
    above-shown `make ipc_doc_public ipc_doc_full flow_doc_public flow_doc_full` command.
    - The generated documentation consists of:
      - (Flow-IPC proper) a clickable guided Manual + Reference.
      - (Flow) a clickable Reference.
    - Seeing the doc comments in the various .hpp files works.
    - Browsing the clickable generated documentation is probably quite a bit nicer.

To read the latter -- the guided Manual + References -- consider the following.
  - The latest generated docs from the source code in `*/src/*` has been included nearby;
    unpack them via:
    - Flow-IPC proper: `cp doc/ipc_doc.tgz <some-place> && cd <some-place> && tar xzf ipc_doc.tgz`; use a browser to
      open `<some-place>/ipc_doc/index.html`.
    - Flow: `cp flow/doc/flow_doc.tgz <some-place> && cd <some-place> && tar xzf flow_doc.tgz`; use a browser to
      open `<some-place>/flow_doc/index.html`.
  - If you're perusing docs only, that's all.  If you have changed the source code (for Flow-IPC proper, Flow, or
    both):
  - Upon changing the source code, the documentation can and should be regenerated and saved nearby again (by you).
    First, follow the aforementioned `cd $BUILD && make ipc_doc_public ipc_doc_full flow_doc_public flow_doc_full`
    procedure in Installation.  You may encounter Doxygen warnings; you should fix your code accordingly.
    Next:
    - Flow-IPC proper: `cd $SRC/doc/ipc_doc` (`$SRC` being the directory with this README) and lastly:
      `$SRC/tools/doc/stage_generated_docs.sh $BUILD/ipc`.  This will place the `make`-generated Flow-IPC
      docs under `$SRC/doc/ipc_doc` in the proper relative location; plus a nearby tarball with everything
      packaged up.
    - Flow: `cd $SRC/flow/doc/flow_doc` (`$SRC` being the directory with this README) and lastly:
      `$SRC/flow/tools/doc/stage_generated_docs.sh $BUILD/flow`.  This will place the `make`-generated Flow
      docs under `$SRC/flow/doc/flow_doc` in the proper relative location; plus a nearby tarball with everything
      packaged up.
  - It would now be prudent to open the result in the browser (open `$SRC/flow/doc/flow_doc/index.html` and/or
    `$SRC/doc/ipc_doc/index.html`) to ensure things you've changed look good; rinse/repeat if not.  (Don't forget
    to `make` to ensure no compile errors have accidentally appeared as a result of tinkering with comments.
    It happens.)
  - If you now `git add`, you can commit both the source and refreshed docs (in the form of a single .tgz for Flow,
    and/or a single .tgz for Flow-IPC proper) to source control.  (2 `.gitignore`s will ensure the individual
    generated files are ignored; only the 2 `.tgz`s will be tracked by Git.)

Or see the online documentation at the GitHub repository (details TBD).  This will just mirror what someone did
in the above steps.

## What if I have a separate source-tree for Flow alone?

Flow has been around a bit (whereas, as of this writing in 11/2023, Flow-IPC is new -- and, we suspect, of greater
popular appeal in spite of a narrower mission).  It is possible you have a standalone Flow elsewhere.  It is
pretty easy to have this Flow-IPC meta-project work with such a source tree.  There are a couple ways.
  - You can place a symbolic link (`ln -s`) named `./flow`, pointing to the compatible open-source Flow
    directory (typically also named `flow` but technically not necessarily so) outside `.`.
    - This one is really easy; but naturally you can't have a `./flow` already there; so if you've got it from
      an Flow-IPC meta-tarball, you can rename or delete the existing `./flow` before setting the symlink.
  - You can specify a different Flow project root entirely; use the CMake knob (cache setting) named
    `IPC_META_ROOT_flow` to specify the path to this directory.  So `cmake -DIPC_META_ROOT_flow=...`
    (or `ccmake ...same...`).
    - In this case `./flow` will be ignored (whether or not it exists).

Actually, one can use either of those 2 technique for some or all of the
`ipc_{core|transport_structured|session|shm|shm_arena_lend}` sub-projects as well.
The symlink or cache-setting name would be not `flow` or `IPC_META_ROOT_flow` but instead
`ipc_{...|...|...|...|...}` or `IPC_META_ROOT_ipc_{...|...|...|...|...}`, respectively,
depending on the sub-project in question.

That said it will only work if it is indeed a bona-fide open-source tree with a root `CMakeLists.txt`
and so on.

## Contributing

As mentioned in Organization, you may wish to contribute to the project.  Of course, in your own setting, it
may be perfectly reasonable to simply get a packaged tarball (.tgz), make the changes in this monolithic,
non-SCS-mirrored copy; test them locally.  Eventually, though, you'll want to work with the central
Git-SCS-mirrored project.  This section is about that.  It assumes basic familiarity with Git.

As noted in Organization, Flow-IPC is conveniently packaged in a monolithic meta-project.  "Normally"
working with Git in a given repo is straightforward: You clone with a command, you create branch and switch to
it, you make changes/commits, and lastly you issue a pull request (PR) to merge this into the master
development branch.  If this is accepted and merged, you're done!

However, as also noted in Organization, Flow-IPC is a meta-project composed of -- primarily -- a handful
of sub-projects: `flow` and `ipc_*`.  (In fact, as of this writing, the dependency tree between them is
a mere queue: `flow` <= `ipc_core` <= `ipc_transport_structured` <= `ipc_session` <= `ipc_shm` <=
`ipc_shm_arena_lend`.)  The meta-project itself bundles them together for convenience and tracking purposes;
and adds a monolithic set of documentation (for `ipc_*`; `flow` has its own, similarly generated,
documentation) and demos/tests.

This organization somewhat (not too much, we think) complicates the aforementioned Git workflow.  Namely:
this meta-project tracks the sub-projects via the Git **submodule** system.  Have a look at
`./.gitmodules`: you'll notice the handful of sub-projects would be cloned to similarly named
directories off the meta-project root (alongside this very `README.md`): `./flow/`, `./ipc_core`,
`./ipc_session`, etc.

Assume the present `README.md` is at Git repo URL `${URL}/ipc.git`.
To clone the entire thing: 
  - `git clone --recurse-submodules ${URL}/ipc.git`

If you've cloned it already, without `--recurse-submodules`, then to get the submodules:
  - `git submodule update --init --recursive`

Either way, `.gitmodules` will be consulted as to where to get them (most likely `${URL}/flow.git`,
`${URL}/ipc_core.git`, etc.) and where to place them (which will always be `./flow`, `./ipc_core`, etc.).

A further tutorial regarding how to work with submodules is beyond our scope here (please read official
Git documentation).  However, ultimately, it will still come down to pull requests (PR) to update (most
importantly) the individual sub-projects' master branches; and at times the meta-project's master branch.
The only real added complexity will come from the way the submodule system tracks the most current commit
of each submodule in the parent repo.  That said, for now at least, we'll leave it to the official Git
documentation.
