# Flow-IPC: Modern C++ toolkit for fast inter-process communication (IPC)

**Flow-IPC** makes IPC code both performant and easy to write.  At this time it is for **C++17** (or higher)
programs built for **Linux**, running on the x86-64 architecture.  (Support for macOS/BSD and ARM64 is planned
as an incremental task.)

## Background

*IPC (inter-process communication)* is the practice of transmitting or sharing data structures between at least
two running programs, whether *locally* (within a single machine) or networked.  In the C++ world of
systems programming, *high-speed local IPC* is a common coding task.  Modern OS provide
various IPC-transport choices (such as pipes and stream sockets), and in-place schema-based serialization
tools (such as [Cap'n Proto](https://capnproto.org/language.html)) hugely help in representing structuring data
in an organized, forward-compatible fashion.  Some IPC-transports (e.g., Unix domain sockets) allow transmission of
*native handles* (FDs in \*nix) to, e.g., network sockets which is a key technique used in web servers.

Unfortunately, IPC code -- at least in C++ systems programming when low latencies are of value:
  - is **painful to write/maintain** and is **rarely reusable**;
  - is **slow**, as it is common to **copy a message twice** (sender -> transport -> receiver).
    - Shared memory (SHM) can defeat this but increases development pain substantially.

## Abstract

This is **Flow-IPC**: a C++ toolkit that makes IPC code:
  - **easy** to write in reusable fashion -- transmitting **Cap'n Proto**-encoded structured data, **STL-compliant
    native C++ data structures**, binary blobs, and **native handles (FDs)**;
  - **highly performant** by seamlessly eliminating *all* copying of the transmitted data (**end-to-end zero-copy**).
    - If you transmit [Cap'n Proto schema](https://capnproto.org/language.html)-based messages, you get zero-copy
      performance with Flow-IPC.
    - If you share native C++ data structures, including arbitrarily nested STL-compliant containers and pointers,
      you also get zero-copy performance with Flow-IPC.  We provide access to shared memory for this purpose and manage
      SHM arenas automatically, so you need not worry about cleanup, naming, or allocation details.
      - In particular we integrate with [jemalloc](https://jemalloc.net), a commercial-grade thread-caching memory
        manager at the core of FreeBSD, Meta, and others.

Here's an *example* of the performance gains you can expect when using Flow-IPC zero-copy transmission, from
the included `perf_demo` tool.  (Here we use Cap'n Proto-described data.  Native C++ structures behave similarly.)

![graph: perf_demo capnp-classic versus capnp-Flow-IPC](./src/doc/manual/assets/img/capnp_perf_v1.png)

Here, app 1 is a memory-caching server that has pre-loaded into RAM a few
files ranging in size from 100kb to 1Gb.  App 2 (client) requests a file of some size.  App 1 (server) responds
with a single message containing the file's data structured as a list of chunks, each along with that chunk's hash.
App 2 receives the message and reports the round-trip time (RTT): from just before issuing the request to just after
accessing some of the file data.  This RTT is the **IPC-induced latency**.  We compare the RTTs (latencies) of two
techniques:
  - The *blue line* shows the latency (RTT) when using "classic" IPC over a Unix-domain stream socket.  The server
    `::write()`s all the chunks in sequence into the socket FD; the client `::read()`s them out of it.
  - The *orange line* shows the RTT when using Flow-IPC with zero-copy enabled.

Observations (tested using server-grade hardware):
  - With Flow-IPC: the round-trip latency = ~100 microseconds *regardless of the size of the payload*.
  - Without Flow-IPC: the latency is about 1 *milli*second for a 1-megabyte payload and approaching a *full second*
    for a 1-gigabyte file.
  - For very small messages the two techniques perform similarly: ~100 microseconds.

The code for this, when using Flow-IPC, is straighforward.  First one defines a capnp schema in normal fashion
(this is not Flow-IPC-specific):

  ~~~{.capnp}
  # ...
  struct GetCacheReq { fileName @0 :Text; }
  struct GetCacheRsp
  {
    # We simulate the server returning files in multiple equally-sized chunks, each sized at its discretion.
    struct FilePart
    {
      data @0 :Data;
      dataSizeToVerify @1 :UInt64; # Recipient can verify that `data` blob's size is indeed this.
      dataHashToVerify @2 :Hash; # Recipient can hash `data` and verify it is indeed this.
    }
    fileParts @0 :List(FilePart);
  }
  # ...
  ~~~

Client-side, issue a request, in this case expecting a reply immediately.  (One can also issue async requests;
notifications -- no response expected; responses; and so on.)

  ~~~
  // Specify that we *do* want zero-copy:
  using Session = ipc::session::shm::classic::Client_session;

  // Open session e.g. near start of program:

  // CLI_APP and SRV_APP are simple structs naming the 2 apps, so we know with whom to engage in IPC.
  Session session{ CLI_APP, SRV_APP, ... };
  // Ask for 1 communication *channel* to be immediately available.
  Session::Channels ipc_channels(1);
  session.sync_connect(session.mdt_builder(), &ipc_channels); // Instant.
  auto& ipc_channel = ipc_channels[0];
  // (Can also non-blockingly open more channel(s) anytime: session.open_channel().)

  // ...

  // Issue request and process response.  TIMING STARTS HERE -->
  auto req_msg = ipc_channel.create_msg();
  req_msg.body_root()->initGetCacheReq().setFileName("huge-file.bin"); // Vanilla Cap'n Proto-using code.
  const auto rsp
    = ipc_channel.sync_request(req_msg) // Send message; get ~instant reply.
        ->body_root().getGetCacheRsp(); // Back to vanilla capnp work.
  // <-- TIMING STOPS HERE.
  // ...
  verify_hash(rsp, some_file_chunk_idx);

  // ...

  void verify_hash(perf_demo::schema::GetCacheRsp::Reader rsp, size_t idx)
  {
    const auto file_part = rsp.getFileParts()[idx];
    if (file_part.getHashToVerify() != compute_hash(file_part.getData()))
    {
      throw BadHashException(...);
    }
  }
  ~~~

In comparison, without Flow-IPC: To achieve the same thing, with
end-to-end zero-copy performance, a large amount of difficult code would be required, including management of
SHM segments whose names and cleanup have to be coordinated between the 2 applications.  Even *without*
zero-copy -- i.e., simply `::write()`ing a copy of the capnp serialization of `req_msg` to and `::read()`ing
`rsp` from a Unix domain socket FD -- sufficiently robust code would be significant in length and complexity;
and challenging to make reusable.

---

Please see [Documentation](#documentation) and/or [Quick tour](#quick-tour) below for a more in-depth look into
Flow-IPC.

## Obtaining the source code

- As a tarball/zip: The [project web site](https://flow-ipc.github.io) links to individual releases with notes, docs,
  download links.
- For Git access: `git clone --recurse-submodules git@github.com:Flow-IPC/ipc.git`
  - Forgot `--recurse-submodules`?  Fix it with: `cd ipc && git submodule update --init --recursive`.

## Installation

See [nearby INSTALL file](./INSTALL.md).

## Contributing

See [nearby CONTRIBUTING file](./INSTALL.md).

## Documentation

The [guided Manual](https://flow-ipc.github.io/doc/flow-ipc/versions/main/generated/html_public/about.html)
explains how to use Flow-IPC.  A comprehensive [Reference](https://flow-ipc.github.io/doc/flow-ipc/versions/main/generated/html_public/namespaceipc.html)
is inter-linked with that Manual.

The [project web site](https://flow-ipc.github.io) contains links to the same for each individual release as well.

## Quick tour

Flow-IPC is comprehensive, aiming to be useful in black-box fashion but providing public APIs and customization
at lower layers also.  The guided Manual (linked just above) covers everything...

...but here are a few specific topics of potential high interest, *in case* you're looking to jump into some
code right away.

  - Perhaps the median topic of interest is transmission of structured-data messages, as described
    by a [schema language](https://capnproto.org/language.html), namely Cap'n Proto a/k/a *capnp*.
    capnp by itself provides best-in-class *serialization*.  However: If you'd like to *transmit* your
    serialized capnp-encoded message between processes, you're on your own: capnp provides only rudimentary
    capabilities.  With Flow-IPC, however, it becomes easy:
    - See the [synopsis/example](https://flow-ipc.github.io/doc/flow-ipc/versions/main/generated/html_public/api_overview.html#api_overview_transport_struc_synopsis)
      of transmitting structured Cap'n Proto-described messages between processes.
    - Yet that still involves (behind the scenes) having to *copy the content of the data twice*: sender user memory =>
      IPC transport (e.g., Unix domain socket) => receiver user memory.  Messages can be very large including,
      e.g., entire images or videos in a web cache server.
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
  - The above example jumps into the middle of things, after you've connected from one program to another
    (established a *session*) and have at least 1 *channel* opened.  Without Flow-IPC, accomplishing this is no mean
    feat either and tends to be written and rewritten by each project that wants to do IPC, again and again.
    With Flow-IPC, it is easy: see the
    [synopsis about sessions](https://flow-ipc.github.io/doc/flow-ipc/versions/main/generated/html_public/api_overview.html#api_overview_sessions_synopsis).
  - You can also check out a [simple complete example](https://github.com/Flow-IPC/ipc_shm/tree/main/test/basic/link_test),
    namely one of our functional tests, in which one program
    connects to another, establishes a session and channel, then transmits exchanges a capnp-encoded hello-world, with
    end-to-end zero copy.

We guess those are the likely topics of highest interest.  That said, Flow-IPC provides entry points at every layer
of operation, both higher and lower than the above.  Flow-IPC is *not* designed as merely a "black box" of capabilities.
E.g., for advanced users:
  - Various lower-level APIs, such as low-level transports (Unix domain sockets, MQs) and SHM operations can be
    accessed directly.  You can also plug-in your own.
  - By implementing or accessing some of the handful of key concepts, you can customize behaviors at all layers,
    including serialization-memory backing, additional SHM providers, and C-style native data structures that use raw
    pointers.

Flow-IPC is comprehensive and flexible, as well as performance-oriented with an eye to safety.
The [API tour page of the Manual](https://flow-ipc.github.io/doc/flow-ipc/versions/main/generated/html_public/api_overview.html)
will show you around.  The rest of the [guided Manual](https://flow-ipc.github.io/doc/flow-ipc/versions/main/generated/html_public/pages.html)
and the [Reference](https://flow-ipc.github.io/doc/flow-ipc/versions/main/generated/html_public/namespaceipc.html)
go deeper.
