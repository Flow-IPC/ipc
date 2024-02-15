# Flow-IPC  -- Modern C++ toolkit for high-speed inter-process communication (IPC)

## (Plus, general-purpose toolkit, Flow)

What's this, you ask?
- We immediately point you to this
[introductory page](https://flow-ipc.github.io/doc/flow-ipc/versions/main/generated/html_public/about.html)
in the project's main documentation.
- The [project web site](https://flow-ipc.github.io) provides access to various released versions and their
corresponding sets of such documentation.

If you'd prefer to jump into some code right away:
  - We estimate the median topic of interest to be transmission of structured-data messages, as described
    by a schema language, [*Cap'n Proto a/k/a capnp*](https://capnproto.org).  capnp by itself provides best-in-class
    *serialization* but only rudimentary APIs for *transmission* of serialized data.  If you'd like to transmit your
    serialized capnp-encoded message, you're on your own.  With Flow-IPC, however, it becomes easy:
  - See the [synopsis/example of transmitting structured Cap'n Proto-described
    messages](https://flow-ipc.github.io/doc/flow-ipc/versions/main/generated/html_public/api_overview.html#api_overview_transport_struc_synopsis)
    between processes.
  - Yet it still involves (behind the scenes) having to *copy the content of the data twice*: sender user memory =>
    IPC transport (e.g., Unix domain socket) => receiver user memory.  Messages can be very large including things,
    e.g., like entire images or music files.
  - Ideally, instead, one wants **end-to-end zero-copy performance** and semantics.  I.e., receiver user memory *is*
    sender user memory: "Sender" simply writes the data; "receiver" (having been informed in some way) simply reads
    those same data, in-place.
    - Normally this requires shared memory (SHM) work which is difficult coding even in specialized scenarios.
    - Flow-IPC, however, makes it very easy...
  - ...as shown in this
    [explanation](https://flow-ipc.github.io/doc/flow-ipc/versions/main/generated/html_public/api_overview.html#api_overview_transport_struc_zero)
    and [code example](https://flow-ipc.github.io/doc/flow-ipc/versions/main/generated/html_public/api_overview.html#api_overview_transport_struc_zero_synopsis).
    Note the *2 changed lines of code* when setting up.
    - The "meat" of the earlier-linked example code remains unchanged; just regular capnp accessor/mutator calls
      familiar to vanilla-Cap'n Proto users.  (Users of Protocol Buffers and similar should also feel right at home.)
- We guess that's the likeliest topic of highest interest.  That said, Flow-IPC provides entry points at every layer of
  operation, both higher and lower than the
  above topic/example.  Flow-IPC is *not* designed as merely a "black box" of capabilities.  E.g., for advanced users:
  - Various lower-level APIs, such as low-level transports (Unix domain sockets, MQs) and SHM operations can be
    accessed directly.  You can also plug-in your own.
  - By implementing or accessing some of the handful of key concepts, you can customize behaviors at all layers,
    including serialization-memory backing, additional SHM providers, and C-style native data structures that use raw
    pointers.
- In general, we feel Flow-IPC is comprehensive and flexible, as well as performance-oriented with an eye to safety.
  The [API tour page of the Manual](https://flow-ipc.github.io/doc/flow-ipc/versions/main/generated/html_public/api_overview.html)
  will show you around.  The rest of the [guided Manual](https://flow-ipc.github.io/doc/flow-ipc/versions/main/generated/html_public/pages.html)
  and the [Reference](https://flow-ipc.github.io/doc/flow-ipc/versions/main/generated/html_public/namespaceipc.html)
  go deeper.

The text just below covers some of the same ground -- just in case -- but the true documentation is hosted online at
the aforementioned link(s) and is also bundled as part of the repository/archive containing the present README.

Having sampled those docs... are you interested in using or even developing Flow-IPC?  Then please read on.  To restate
Flow-IPC's mission from the above-linked introductory docs page:

> Flow-IPC:
>   - is a **modern C++** library with a concept-based API in the spirit of STL/Boost;
>   - enables near-zero-latency **zero-copy** messaging between processes (via behind-the-scenes use of the below SHM solution);
>   - transmits messages containing binary data, native handles, and/or **structured data** (defined via [Cap'n Proto](https://capnproto.org/language.html));
>   - provides a **shared memory (SHM)** solution
>     - with out-of-the-box ability to transmit arbitrarily complex combinations of scalars, `struct`s, and **STL-compliant containers** thereof;
>     - that integrates with **commercial-grade memory managers** (a/k/a `malloc()` providers).
>       - In particular we integrate with [jemalloc](https://jemalloc.net), a thread-caching memory manager at the core of FreeBSD, Meta, and others.
> 
> A key feature of Flow-IPC is pain-free setup of process-to-process conversations (**sessions**), so that users need not worry about coordinating individual shared-resource naming between processes, not to mention kernel-persistent resource cleanup.
> 
> Flow-IPC provides 2 ways to integrate with your applications' event loops.  These can be intermixed.
>   - The **async-I/O API** automatically starts threads as needed to offload work onto multi-processor cores.
>   - The `sync_io` **API** supplies lighter-weight objects allowing you full control over each application's thread structure, hooking into reactor-style (`poll()`, `epoll_wait()`, etc.) or proactor (boost.asio) event loops.  As a result context switching is minimized.
> 
> Lastly Flow-IPC supplies **lower-level utilities** facilitating work with POSIX and SHM-based **message queues (MQs)** and **local (Unix domain) stream sockets**.

Also included is the general-purpose C++ toolkit named Flow.  While it is bundled for convenience simply because (1)
it is needed and (2) shares the same original authorship and DNA as Flow-IPC proper, it may nevertheless prove
useful in its own right.  For more information please see a similar-purpose `README.md` in the `flow/` sub-directory.

## Organization

The directory containing the present `README.md` is the root of the Flow-IPC meta-project.  It is most
typically bundled as one thing, with all necessary (`ipc_*` and `flow`) sub-projects included as
subdirectories.  The build/installation procedure will take care of everything for you
(not including the handful of third-party prerequisites like Boost which will need to be installed
separately; instructions below).

If you've obtained this project as a pre-packaged source archive (named `*_full.tar.gz` or `*_full.zip`),
then you have everything you need
(other than potentially third-party prerequisites like Boost), located where it needs to be (namely,
the subdirectories `ipc_*` and `flow`).  This is great and easy.  We recommend working this way in all
situations *except if your aim is to make changes that would be checked-in to the public open-source project*.
(If that is your aim eventually, but not right now, it might make sense to work in the monolithic model for
a while; then switch-over to the Git setup described in the following paragraphs, once you're ready to
contribute to the public project.)

If you do not aim to immediately contribute to the public project, we encourage you to skip right to the
next section -- and you can later skip Contributing section(s).

If you *do* aim to immediately contribute to the public project, please at least glance at Contributing in the present
document first.

## Installation

An exported Flow-IPC consists of C++ header files installed under "ipc/..." and "flow/..." in the
include-root; and a libraries such as `libflow.a` and `lipipc_*.a` (as of this writing 5 of the latter).
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
  5. (Optional, only if running unit tests) Have GoogleTest install available.
  6. (Optional, only if generating docs) Have Doxygen and Graphviz installs available.
  7. Use CMake `cmake` (command-line tool) or `ccmake` (interactive text-UI tool) to configure and generate
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
    and can be again, conviently generated using certain tools (namely Doxygen and friends), via the
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

In contrast, if you have changed the source code (for Flow-IPC proper, Flow, or both): See Contributing below.
It includes instructions on how docs are generated and updated.  Spoiler alert: Most things are automatic;
the only manual part is that you should peruse any changed docs for your visual satisfaction before
submitting your code.

## What if I have a separate source-tree for Flow alone?

Flow has been around a bit (whereas, as of this writing in 11/2023, Flow-IPC is new -- and, we suspect, of greater
popular appeal in spite of a narrower mission).  It is possible you have a standalone Flow elsewhere.  It is
pretty easy to have this Flow-IPC meta-project work with such a source tree.  There are a couple ways.
  - You can place a symbolic link (`ln -s`) named `./flow`, pointing to the compatible open-source Flow
    directory (typically also named `flow` but technically not necessarily so) outside `.`.
    - This one is really easy; but naturally you can't have a `./flow` already there; so if you've got it from
      a Flow-IPC `*_full.tar.gz` or `*_full.zip` archive,
      you can rename or delete the existing `./flow` before setting the symlink.
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

## Contributing: Basics

As mentioned in Organization, you may wish to contribute to the project.  Of course, in your own setting, it
may be perfectly reasonable to simply get a packaged `*_full.tar.gz` or `*_full.zip` archive, make the changes in this
monolithic, non-SCS-mirrored copy; test them locally.  Eventually, though, you'll want to work with the central
Git-SCS-mirrored project.  This section is about that.  It assumes basic familiarity with Git.

As noted in Organization, Flow-IPC is conveniently packaged in a monolithic meta-project.  "Normally"
working with Git in a given repo is straightforward: You clone with a command, you create branch and switch to
it, you make changes/commits, and lastly you issue a pull request (PR) to merge this into the master
development branch.  If this is accepted and merged, you're done!  (Acceptance, as of this writing, means that
code reviewer(s) formally accept(s) your PR.  In addition an automated CI/CD pipeline shall execute for your
PR, initially and after any update; it this fails, then acceptance is unlikely.)

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
of each submodule in the parent repo.  The basic idea is, essentially, simple: The `ipc` repo stores not just
the actual files but also a "pointer" to the state of each submodule (`flow`, `ipc_*`) that would be cloned
if one were to invoke the `git clone --recurse-submodules` command above.  Our rule is that in the master
branch of `ipc`, the "pointer" for each submodule is to some commit to that submodule repo's master branch.
Thus the basic procedure is: merge all the relevant PRs into the submodule repos' (if applicable) master
branches; then lastly issue a PR in `ipc` which (possibly among other changes) updates the submodule
pointer(s) to the latest commit in each guy's master branch.

That said, for now at least, we'll leave it to the official Git documentation.

## Contributing: Some details

This section is not meant as a detailed/formal manual.  This project follows established conventions
and tools of open-source development, Git+GitHub specifically; so the exact steps to follow should come naturally.
We assume either familiarity with such processes or the willingness to learn them.

So in this section we will point out a few specifics that should help and may or may not otherwise be obvious.

The Flow-IPC product (including Flow dependency) lives in the [Flow-IPC organization](https://github.com/Flow-IPC)
at GitHub.  This is open-source.  There is also a [web site hosted using GitHub pages](https://flow-ipc.github.io)]
which hosts, at least, online copies of generated documentation.

The master branch in each repo is called `main`.  Thus any contribution will involve:
  - A change to the code in 0 or more of the submodule repos (`flow`, `ipc_*`).  (So that's 0 or more pull
    requests (PRs) to each relevant `main` branch.)
  - Possibly a change to the code in the `ipc` repo (usually tests).
  - Possibly a change to the submodule pointer(s) in `ipc` repo: 1 for each submodule repo changed above.
    (So that's potentially 1 PR total -- for the last 2 bullet points -- to the `ipc` repo main branch.)

We have some automated CI/CD pipelines.  Namely `flow`, being special as a self-contained project, has the
pipeline steps in `flow/.github/workflows/main.yml` -- this is Flow's dedicated CI/CD pipeline; and `ipc`,
covering Flow-IPC as an overall monolithic project, similarly has Flow-IPC's CI/CD pipeline steps in
`.github/worksflows/main.yml`.  Therefore:
  - Certain automated build/test/doc-generation runs occur when:
    - creating a PR against `flow` repo;
    - updating that PR;
    - finally merging that PR.
  - Certain automated build/test/doc-generation runs occur when:
    - creating a PR against `ipc` repo;
    - updating that PR;
    - finally merging that PR.
  - There are no individual CI/CD pipelines for the `ipc_*` repos; Flow (a special case) aside we treat Flow-IPC as
    a monolithic whole in that sense.

To contribute a change to the project, first of course you'll need to build it per various instructions above
and test the changes locally.  However before these changes will be officially accepted, certain automated
tests will need to pass in the GitHub project, and the changes will need to pass code review.  Here's roughly
how that works.
  1. You create a PR against a repo.
  2. If that repo is `flow` or `ipc`: automated pipeline runs.
     - It builds and tests the code in many configurations, such as Release, Debug, size-minimizing Release,
       Release with debug info, run-time sanitizers ASAN, UBSAN, TSAN.  The matrix includes all those configurations
       across several versions of clang compiler/linker and several versions of gcc compiler/linker.  All in all
       the build/test job runs across ~40 configuration as of this writing.
     - It generates documentation and makes it available as a downloadable artifact (a tarball to download and peruse).
     - If any of that fails, most likely you'll need to update the PR which will re-run the pipeline.  
       If it fails, then the pipeline output should make clear what went wrong.
       Could be a build error; could be a test failure; could be Doxygen doc-generation problem.
       Eventually it needs to pass (usually).  
  3. A code reviewer will look over your PR.  You know the drill.  Once it is marked approved by at least 1, then
     it becomes mergeable.
  4. You click Merge Pull Request which should now be no longer grayed out.
  5. If that repo is `flow` or `ipc`: automated pipeline runs again, this time off the code in `main`.
     - The generated documentation is checked back into `main`.
     - The web site host (GitHup Pages) is pinged, so that the generated documentation is reflected on
       the [project web site](https://flow-ipc.github.io).

That's the basic idea.  One aspect we haven't covered which bears more detailed explanation is doc generation.
After all, if you've changed the source, then the resulting generated documentation might change.  On that note:
  1. Before making a PR you may wish to locally generate, and visually check, the documentation -- which may
     have changed due to your source code changes.
     This may or may not be necessary, but for now let's assume it is.  Then:
  2. First, follow the aforementioned `cd $BUILD && make ipc_doc_public ipc_doc_full flow_doc_public flow_doc_full`
     procedure in Installation.  You may encounter Doxygen warnings; you should fix your code accordingly.
     Next:
  3. Open the result in a browser: `$BUILD/html_ipc_doc_public/index.html`
     and `$BUILD/html_ipc_doc_full/index.html` (the public and full doc sets respectively, for Flow-IPC); and/or
     `$BUILD/flow/html_flow_doc_public/index.html` and `$BUILD/flow/html_flow_doc_full/index.html` (same thing for Flow).
     Ensure things you've changed look good; rinse/repeat if not.
     - If you have changed something under 1 or more `ipc_*/`, then you'll want to check the Flow-IPC documentation.
       Otherwise you can skip that.  
     - If you have changed something under `flow/`, then you'll want to check the Flow documentation.  Otherwise
       you can skip that.

You *need not* and *should not* check-in the resulting documentation.  When source code changes are checked-in to
`main` of `flow`, the Flow documentation will be generated and checked-in using our CI/CD
pipeline under `flow/`.  Identically, if one checks-in to `main` of 1 or more of `ipc_*`, and then "seals the deal"
by checking-in the updated submodule pointer(s) to `main` of `ipc`, the Flow-IPC documentation will be generated
and checked-in using the `ipc/` pipeline.  (Search for `git push` in the two `main.yml` files to see what we mean.)
We have already mentioned this above.

The above steps for *locally* generating the documentation are provided only so you can locally test soure code changes' effects on the resulting docs.  Locally generating and verifying docs, after changing source code, is a good idea.
However it's also possible (and for some people/situations preferable) to skip it.
The CI/CD pipeline will mandatorily generate the docs, when a PR is created or updated, as we explained above.
If you did not locally verify the new docs by generating and perusing them, then you must peruse the
doc tarball artifact (mentioned earlier).  If you *did* verify it locally, then you can skip that step.

You can watch all these pipeline runs under Actions tab in GitHub:
  - `flow` repo will have "Flow pipeline" under Actions tab.  (You can view the workflow `main.yml` file which
    is the source code that controls each given pipeline run.)
  - `ipc` repo will have "Flow-IPC pipeline" under Actions tab.  (Ditto regarding `main.yml` viewing.)
