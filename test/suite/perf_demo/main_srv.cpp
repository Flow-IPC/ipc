/* Flow-IPC
 * Copyright 2023 Akamai Technologies, Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy
 * of the License at
 *
 *   https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in
 * writing, software distributed under the License is
 * distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing
 * permissions and limitations under the License. */

#include "common.hpp"

/* perf_demo_srv (this guy) and perf_demo_cli (main_cli.cpp) are two programs to be executed from
 * the same CWD, where they should both be placed.  First run the server program; once it says one can now
 * connect, start the client program.  (The server program has some optional args, including one controlling the
 * size of the transmitted data; it'll print the usage message at the top always.)  Launching the client
 * will begin the benchmark run; once it is done, both programs will exit.  The client's console shall print
 * a benchmark summary.
 *
 * If macro JEM_ELSE_CLASSIC is set to 1, the SHM-provider providing zero-copy mechanics (internally) will be
 * SHM-jemalloc.  Otherwise (e.g., undefined or set to 0) it will SHM-classic.  (In our experience the benchmark
 * results so far are pretty similar, but it's still nice to exercise both.)  As of this writing the nearby
 * build script shall generate both pairs of programs:
 * perf_demo_{srv|cli}_shm_classic.exec and perf_demo_{srv|cli}_shm_jemalloc.exec.  So pick which type you want
 * and then execute that pair, in server-then-client order.
 *
 * Bit of a disclaimer
 * -------------------
 * *As of this writing* this meta-app perf_demo (the 2 programs together) just test a couple of things -- at least
 * one potential "before-and-after" comparison.  There are undoubtedly more benchmarks and comparisons to make over
 * time (most of these have been made manually by the original developers but not necessarily memorialized in
 * a "shipped" program).  Given that, these programs don't yet try super-hard to be structured like fine, maintainable
 * server software: we're going for brevity, maximized perf, and easily available numeric results here, not
 * necessarily best practices of server application design.  In particular: 
 *   - We don't follow the Process/App_session/etc. program organization as recommended
 *     "Sessions: Teardown; Organizing Your Code" in the guided Manual.  Serious applications should do that, to
 *     make your life easier through robustly stable and safe software!  Here, though, it's a rigid test run, so we
 *     don't have to worry about tons of possible runtime scenarios... we can take some shortcuts for brevity's sake;
 *     the applications remain stable and safe, and the code's easier to follow at a glance.
 *     - Same deal with setting up session and channel error handlers.  Serious applications should!  We need not.
 *   - We use a few stylistic "shortcuts" like (file-local) global variables.  Serious applications should not (and the
 *     actual Flow-IPC code do not) make such shortcuts!
 *
 * This is common for test code and doesn't usually engender this kind of of disclaimer.  Why make say the above
 * then?  Answer: It seems likely that once we add some more benchmarks, both applications (including the shared code,
 * currently common.[hc]pp) in the meta-app should morph into a more stylistically beautiful thing.  Please keep
 * this in mind when/if expanding this meta-application. */

using Session = Session_server::Server_session_obj;
// For when we test "classic" use of Cap'n Proto (capnp), sans Flow-IPC structured-transport layer.
using Capnp_heap_engine = ::capnp::MallocMessageBuilder;

/* In this app we stubbornly stick to the original thread without creating a new ones.  This is partially to show
 * an example that it can be done if desired, via use of sync_io-pattern API; and more importantly to not even
 * give away the slight latency increase (due to context switching and inter-thread signaling) endemic to the
 * simpler async-I/O API, which involves background threads being created.  We are testing max perf here.
 *
 * This boost::asio::io_context's .run() is executed from the original thread, and the various Flow-IPC async ops
 * hook into this event loop.
 *
 * It doesn't need to be global; it's just for coding expediency (for now at least), as it's referenced in a few
 * benchmarks.  Same with g_capnp_msg. */
static Task_engine g_asio;
/* This is where we keep large capnp-structured data.  We fill it up at the start of main(), and at least one benchmark
 * transmits its backing serialization capnp-segments over an IPC channel (local stream socket).  Then at least one
 * other benchmark *deep-copies* it into a Flow-IPC SHM-backed MessageBuilder (*not* a capnp::MallocMessageBuilder like
 * this guy) and sends that.  If we add more benchmarks that need large capnp-structured data, we'll likely similarly
 * deep-copy this into whatever MessageBuilder is applicable. */
static Capnp_heap_engine g_capnp_msg;

void run_capnp_over_raw(flow::log::Logger* logger_ptr, Channel_raw* chan);
void run_capnp_zero_copy(flow::log::Logger* logger_ptr, Channel_struc* chan, Session* session_ptr);

int main(int argc, char const * const * argv)
{
  using flow::log::Simple_ostream_logger;
  using flow::log::Async_file_logger;
  using flow::log::Config;
  using flow::log::Sev;
  using flow::Flow_log_component;
  using flow::util::String_view;
  using flow::util::ceil_div;
  using boost::promise;
  using boost::lexical_cast;
  using std::exception;

  constexpr String_view LOG_FILE = "perf_demo_srv.log";
  constexpr float TOTAL_SZ_MI = 1 * 1000;
  constexpr int BAD_EXIT = 1;

  // Set up logging.
  Config std_log_config;
  std_log_config.init_component_to_union_idx_mapping<Flow_log_component>(1000, 999);
  std_log_config.init_component_names<Flow_log_component>(flow::S_FLOW_LOG_COMPONENT_NAME_MAP, false, "perf_demo-");
  std_log_config.init_component_to_union_idx_mapping<ipc::Log_component>(2000, 999);
  std_log_config.init_component_names<ipc::Log_component>(ipc::S_IPC_LOG_COMPONENT_NAME_MAP, false, "perf_demo-");
  Simple_ostream_logger std_logger(&std_log_config);
  FLOW_LOG_SET_CONTEXT(&std_logger, Flow_log_component::S_UNCAT);
  FLOW_LOG_INFO("FYI -- Usage: " << argv[0] << " [msg payload size in MiB] [log file]");
  FLOW_LOG_INFO("FYI -- Defaults: " << TOTAL_SZ_MI << " " << LOG_FILE);
  // This is separate: the IPC/Flow logging will go into this file.
  const auto log_file = (argc >= 3) ? String_view(argv[2]) : LOG_FILE;
  FLOW_LOG_INFO("Opening log file [" << log_file << "] for IPC/Flow logs only.");
  Config log_config = std_log_config;
  log_config.configure_default_verbosity(Sev::S_TRACE, true); // XXX
  Async_file_logger log_logger(nullptr, &log_config, log_file, false);

#if JEM_ELSE_CLASSIC
  /* Instructed to do so by ipc::session::shm::arena_lend public docs (short version: this is basically a global,
   * and it would not be cool for ipc::session non-global objects to impose their individual loggers on it). */
  ipc::session::shm::arena_lend::Borrower_shm_pool_collection_repository_singleton::get_instance()
    .set_logger(&log_logger);
#endif

  try
  {
    ensure_run_env(argv[0], true);

    {
      const auto total_sz_mi = (argc >= 2) ? lexical_cast<float>(argv[1]) : TOTAL_SZ_MI;

      /* Fill out vanilla capnp::MallocMessageBuilder g_capnp_msg with a whole bunch of data.
       * A given benchmarks can then transmit this directly; or prepare a perhaps-non-vanilla MessageBuilder
       * (perhaps a fancy Flow-IPC SHM-backed one!) and deep-copy this guy into that, for identical data
       * that the opposing side can (upon receipt) access and verify using the exact same code.
       *
       * See schema.capnp: It's a simple example structure that mimics a file cache server perhaps;
       * there are N "file-parts"; within each file-part is a blob of bytes (the data) plus a couple fields the client
       * can use to verify (size, hash) the data.
       *
       * So a benchmark run might essentially time something like this:
       *   - We prepare a response structure, simulating the act of (e.g.) already having file-read or downloaded
       *     the file (all its file-parts) into memory earlier.
       *   - We inform client (them) we're ready for the benchmark (via short message).
       *   - [Client (they) issue a short message, the get-cache-request.
       *   - We receive it and immediately response with the prepared structure as the get-cache-response message.
       *   - Client receives the short message and briefly accesses some part of it.]
       *   - Client has *timed* the parts in [] brackets.  That's the RTT result of the benchmark.  So it prints
       *     that result.
       *   - Client then runs through the whole structure and checks file-part hashes and sizes and what-not.
       *
       * So g_capnp_msg will be the "gold copy" of the data structure being used in such benchmarks.
       * Note that this is *completely* vanilla capnp-using code; there's nothing Flow-IPC-ish going on here
       * at all.  Even the backing MessageBuilder is just good ol' capnp::MallocMessageBuilder. */
      FLOW_LOG_INFO("Prep: Patience!  No need to try running client until we say server is up.");
      FLOW_LOG_INFO("Prep: Filling capnp MallocMessageBuilder (rough size [" << total_sz_mi << " Mi]): START.");
      constexpr size_t FILE_PART_SZ = 16 * 1024;

      auto file_parts_list = g_capnp_msg.initRoot<perf_demo::schema::Body>().initGetCacheRsp()
                               .initFileParts
                                  (ceil_div(size_t(total_sz_mi * 1024.f * 1024.f), FILE_PART_SZ));
      for (size_t idx = 0; idx != file_parts_list.size(); ++idx)
      {
        auto file_part = file_parts_list[idx];
        auto data = file_part.initData(FILE_PART_SZ);
        for (size_t byte_idx = 0; byte_idx != FILE_PART_SZ; ++byte_idx)
        {
          data[byte_idx] = uint8_t(byte_idx % 256);
        }
        file_part.setDataSizeToVerify(FILE_PART_SZ);
        /* Obviously a Boost string hash is not a cryptographically sound hash.  Fine for our purposes
         * of sanity-checking that whatever the client received and accessed was at least mutually consistent nad
         * not junk that accidentally didn't cause capnp to throw an exception during an accessor. */
        file_part.setDataHashToVerify(boost::hash<String_view>()
                                        (String_view(reinterpret_cast<char*>(data.begin()), FILE_PART_SZ)));
      }

      /* Note total_sz_mi is just a rough guide; we only count the GetCacheRsp.data field as "taking space";
       * there's also nearby hash and size fields, plus capnp format overhead.  That said even with small
       * sizes like 10kib it's a pretty decent estimate, it turns out.  (And it's certainly proportional at least.) */

      FLOW_LOG_INFO("Prep: Filling capnp MallocMessageBuilder: DONE.");
    }

    /* Accept one session.  Use the async-I/O API, as perf for this part really doesn't matter to anyone ever,
     * and we're not timing it anyway, and it doesn't affect what happens after.  We don't start a thread; just
     * use promise/future pattern to wait until Session_server is ready with success or failure. */
    Session_server srv(&log_logger, SRV_APPS.find(SRV_NAME)->second, CLI_APPS);
    FLOW_LOG_INFO("Session-server started.  You can now invoke session-client executable from same CWD; "
                  "it will open session with some channel(s).");

    Session session;
    promise<void> accepted_promise;
    bool ok = false;
    Session_server::Channels chans;
    srv.async_accept(&session, &chans, nullptr, nullptr,
                     [](auto&&...) -> size_t { return 2; }, // 2 init-channels to open.
                     [](auto&&...) {},
                     [&](const Error_code& err_code)
    {
      if (err_code)
      {
        FLOW_LOG_WARNING("Error is totally unexpected.  Error: [" << err_code << "] [" << err_code.message() << "].");
      }
      else
      {
        FLOW_LOG_INFO("Session accepted: [" << session << "].");
        ok = true;
      }
      // Either way though:
      accepted_promise.set_value();
    });
    accepted_promise.get_future().wait();
    if (!ok)
    {
      return BAD_EXIT;
    }
    // else

    /* Ignore session errors (see disclaimer comment at top of for general justification).
     * Basically we know it'll be, if anything, just the client disconnecting from us when it's done; and by that
     * point we'll be shutting down anyway.  And any transmission error will be detected along the channel of
     * transmission.  As a not-serious-production-app, no need for this stuff. */
    session.init_handlers([](const Error_code&) {});
    // Session in PEER state (opened fully); so channels are ready too.

    /* For now there are just these two channels.  (See above where we specified `return 2`.)
     * You'll see in common.hpp that by setting a certain single type-alias, each channel is simply a
     * local-stream-socket (a/k/a Unix domain socket) full-duplex connection.  (We could as of this writing instead
     * set it to a POSIX MQ, or bipc MQ; it would be just a matter of changing that one alias.  We chose
     * local-stream-socket, because it's a popular choice for people by default, and we'd like to run our
     * no-Flow-IPC benchmark over that.)
     *
     * This one we'll just keep using in this raw form (no Flow-IPC transport::struc::Channel over it). */
    auto& chan_raw = chans[0];

    // And this one we immediately upgrade to a Flow-IPC transport::Struc::Channel.
    Channel_struc chan_struc(&log_logger, std::move(chans[1]), // Structured channel: SHM-backed underneath.
                             ipc::transport::struc::Channel_base::S_SERIALIZE_VIA_SESSION_SHM, &session);

    run_capnp_over_raw(&std_logger, &chan_raw); // Benchmark 1.  capnp data transmission without Flow-IPC zero-copy.
    run_capnp_zero_copy(&std_logger, &chan_struc, &session); // Benchmark 2.  Same but with it.

    FLOW_LOG_INFO("Exiting.");
  } // try
  catch (const exception& exc)
  {
    FLOW_LOG_WARNING("Caught exception: [" << exc.what() << "].");
    return BAD_EXIT;
  }

  return 0;
} // main()

void run_capnp_over_raw(flow::log::Logger* logger_ptr, Channel_raw* chan_ptr)
{
  using flow::Flow_log_component;
  using flow::log::Logger;
  using flow::log::Log_context;
  using flow::util::ceil_div;
  using ::capnp::word;
  using boost::asio::post;
  using std::vector;

  /* While the code below is easy enough to follow, hopefully, we do need to explain why it's written like this at
   * all.  So firstly see main() which summarizes the goal here; in short we prep some data to send to client;
   * inform client we're ready for it to start its timing run; client issues request; we receive it; we send the
   * large response; the client receives it; spits out RTT for the timing run; and verifies the data appears to
   * be fine.  Now specifically in *this* run:
   *   - "The data" is simply g_capnp_msg, a MallocMessageBuilder-backed (so, stored as N segments in heap, not SHM,
   *     as arranged by capnp-supplied MallocMessageBuilder).  So it's already prepared back in main(), we needn't
   *     do any more prep.
   *   - The "send" and "receive" transport mechanism is a local stream socket (Unix domain socket), as prepared
   *     for us by main() in *chan_ptr.
   *
   * The idea is we're simulating what a "vanilla" impl would do here: one where we have a big heap-stored
   * capnp tree, and we want to transmit it via a stream-socket and read it on the other side.
   * run_capnp_zero_copy() will do it with full-on-zero-copy Flow-IPC and show the difference in perf and (secondarily)
   * ease of coding.  (The latter really is secondary; as with async-I/O API instead of sync_io the Flow-IPC-using
   * code would've been *even* simpler.  But we want max available perf all around, so we'll do sync_io.  Do note
   * async-I/O doesn't add much overhead... but some, from context switching and inter-thread signaling.)
   *
   * There are a couple things to defend re. *how* we do that here, in terms of realism/appropriateness in this
   * perf_demo app.
   *   - Why a Flow-IPC transport::Channel?  Why not a raw FD obtained via native ::connect(), etc.?
   *     Answer: Well, it's a lot easier to open channels with Flow-IPC!  That's one of its "things"!
   *     It is orthogonal to the perf we're testing here anyway, so might as well do it the easy way.
   *   - Can't this be done in, like, one line using a capnp helper API?  Why all these lines?
   *     (NOTE: This particularly applies to the run_capnp_over_raw() counterpart in main_cli.cpp; we are mostly
   *     sending stuff, which Flow-IPC unstructured-transport API makes very easy; but reading involves potential
   *     asyncness; so that other run_capnpa_over_raw() is actually quite a few lines.)
   *     - Indeed; something in capnp file serialize.h -- perhaps writeMessageToFd(int fd, MessageBuilder&) to write
   *       and StreamFdMessageReader object to read -- probably will do it.  1-5 lines or so.  We could fish out
   *       the FD from *chan_ptr and have at it.  Reasons why I (ygoldfel) didn't:
   *     - I did want to show/feel what it's like to write such code "for real," without Flow-IPC.
   *       And in reality `fd` (local stream socket) we would in serious apps set to non-blocking mode; who knows
   *       if the other side is reading right now for sure; we could fill up the kernel buffer and would-block...
   *       can't have a serious program blocking in the middle of things.  When reading, of course ::read() can
   *       block; so a serious app would need to set FD to non-blocking mode for the reads; maybe not in *our test
   *       case*, but a real app with a real event loop probably would.  So now one would need to be ready
   *       for writeMessageToFd() and StreamFdMessageReader (at whatever point) to throw a would-block exception.
   *       Realistically, probably this serious app writer would then code their own versions with their own handling
   *       of errors -- including would-block -- which the capnp doc comments in fact suggest.  So it's not 1-5 lines
   *       at all, really -- even though to get the perf numbers we wanted, we could likely do that, more or less.
   *     - So why didn't we write *those* lines + a native ::read() or ::write()?  Why use the Flow-IPC
   *       unstructured-transport API?  Good question... the answer is: to show it is useful even if one merely wants
   *       to do some regular-old-non-zero-copy-unstructured work in the older-school way.  We use the
   *       no-background-threads API (sync_io-pattern API) for max perf.
   *     - But aren't we cheating?  What if Flow-IPC unstructured-transport layer is slow, so we make regular such work
   *       *look* slow by using it?  Answer: Well... it's not slow!  Just seemed silly to reimplement here in perf_demo
   *       as a show of purity.  The thinking is: This giant comment aside, this approach is a decent-enough mix
   *       between (1) realistic perf simulation of not-using-Flow-IPC-for-e2e-zero-copy; (2) not using unrealistically
   *       simple code that makes unrealistic assumptions about serious apps; (3) not going overboard with that and
   *       therefore using Flow-IPC lower layers (not the e2e-zero-copy layer) to make it short enough.
   *     
   * @todo In retrospect there is one thing that would simplify particularly the main_cli.cpp side, that we could've
   * done here.  (The server would take a bit longer to run, outside the benchmarked section when preparing, but who
   * cares?)  We could do capnp::messageToFlatArray() from g_capnp_msg (a slow copy) to encode the serialization into 1
   * contiguous array according to capnp's seg-count/seg-counts/segs format; then send the whole thing as one buffer;
   * then in main_cli.cpp read it using FlatArrayMessageReader.  Then neither side would need to worry about
   * specifically sending the segment count and each segment's size -- just one big buffer.  Code would be simpler...
   * but less realistic -- probably -- as this involves an extra copy of the whole serialization on the sender side.
   * Realistically probably one would want to avoid that... at which point it's back to doing some form of what we do
   * here.  So... toss-up whether to change this or not.  It would work as a ~equally valid perf measurement at any
   * rate. */

  // On to the code.

  /* Local class just so we can arrange functions in chronological order really; it can be tough to do without
   * this local-class trick, when working with boost.asio and similar (proactor pattern).  The different steps
   * tend to get reversed in the code which is confusing, whereas this way it's top-down order. */
  struct Algo :
    public Log_context
  {
    Channel_raw& m_chan;
    Error_code m_err_code;
    size_t m_sz;
    size_t m_n = 0;

    Algo(Logger* logger_ptr, Channel_raw* chan_ptr) :
      Log_context(logger_ptr, Flow_log_component::S_UNCAT),
      m_chan(*chan_ptr)
    {
      FLOW_LOG_INFO("-- RUN - capnp request/response over raw local-socket connection --");

    }

    void start()
    {
      /* sync_io-pattern API: Drop-in our async-wait provider which is good ol' boost.asio .async_wait()
       * over g_asio.  After this we can do sends and receives.  send()s in Flow-IPC are always synchronous,
       * non-blocking, and never yield would-block.  Receives naturally are asynchronous; in sync_io pattern
       * that means you give async_X() both a handler F to run later, if right now would-block results; and
       * the out-args to set if the result is available synchronously right now.  So either one happens, or
       * the other.  It's straightforward really, with one caveat to watch out for: avoid arbitrary-level
       * recursive when reading looping data.  However here on server side we don't have any looping data
       * to read (only send), so it's simple. */
      m_chan.replace_event_wait_handles([]() -> auto { return Asio_handle(g_asio); });
      m_chan.start_send_blob_ops(ev_wait);
      m_chan.start_receive_blob_ops(ev_wait);

      /* Send a dummy message to synchronize initialization.
       * It indicates we're for sure ready for this run to avoid any situation where, like,
       * client sends get-cache-request, but we're still setting something up after accepting the session;
       * so we only send the response once we're ready to do that; but client has already started timing. */
      FLOW_LOG_INFO("> Issuing handshake SYN for initialization sync.");
      m_chan.send_blob(Blob_const(&m_n, sizeof(m_n)));

      /* Receive a dummy message as a request signal.  Technically we should expect an actual capnp-encoded
       * (albeit small) GetCacheReq here; but we'll accept any message; in the big benchmark this detail does not
       * matter.  No need for all the extra code on both sides. */
      FLOW_LOG_INFO("< Expecting get-cache request via tiny message.");
      m_chan.async_receive_blob(Blob_mutable(&m_n, sizeof(m_n)), &m_err_code, &m_sz,
                                [&](const Error_code& err_code, size_t) { on_request(err_code); });
      if (m_err_code != ipc::transport::error::Code::S_SYNC_IO_WOULD_BLOCK) { on_request(m_err_code); }
    }

    void on_request(const Error_code& err_code)
    {
      if (err_code) { throw Runtime_error(err_code, "run_capnp_over_raw():on_request()"); }
      FLOW_LOG_INFO("= Got get-cache request.");

      /* The format is like this:
       *   - size_t - serialization's segment count
       *   - [segment][segment]... where each [segment] is:
       *     - size_t - segment size in bytes
       *     - the segment: that many bytes
       * BTW capnp's own format is not that different; but it has the seg-count, then the individual seg-sizes
       * one after another, then the segments themselves one after another... plus the counts are 32-bits
       * apparently.  @todo In retrospect the code on receiving side would've been somewhat simpler if we followed this
       * header-then-segments technique... less state-switching back and forth arguably.  Live and learn!
       *
       * BTW you'll notice the characteristic escalation in segment sizes: by default MallocMessageBuilder
       * will size each successive segment as equal to the sum of all preceding segment sizes... exponential growth. */

      const auto capnp_segs = g_capnp_msg.getSegmentsForOutput();
      m_n = capnp_segs.size();
      FLOW_LOG_INFO("> Sending get-cache response fragment: capnp segment count = [" << m_n << "].");
      m_chan.send_blob(Blob_const(&m_n, sizeof(m_n)));
      FLOW_LOG_INFO("> Sending get-cache response fragments x N: [seg size, seg content...].");

      /* Essentially (through Flow-IPC unstructured-transport layer) mostly do a bunch ~64k ::write()s.
       * That's reasonably realistic.  (Technically Flow-IPC adds extra semantics on top; namely it preserves
       * message boundaries; so send_blob() <=> async_receive_blob(), 1-to-1.  We use that just fine; and
       * internally Flow-IPC will send a few more bytes in there, namely 2-byte message sizes, 1 per message; but
       * it's minor in the big picture.  Real enough, I say!) */
      const auto chunk_max_sz = m_chan.send_blob_max_size();
      for (size_t idx = 0; idx != capnp_segs.size(); ++idx)
      {
        const auto capnp_seg = capnp_segs[idx].asBytes();
        m_n = capnp_seg.size();
        m_chan.send_blob(Blob_const(&m_n, sizeof(m_n)));

        auto start = capnp_seg.begin();
        do
        {
          const auto chunk_sz = std::min(chunk_max_sz, m_n);
          m_chan.send_blob(Blob_const(start, chunk_sz));
          start += chunk_sz;
          m_n -= chunk_sz;
        }
        while (m_n != 0);
        // It's e.g. 15 extra lines; let's not poison timing with that unless console logger turned up to TRACE+.
        FLOW_LOG_TRACE("= Sent segment [" << (idx + 1) << "] of [" << capnp_segs.size() << "]; "
                       "segment serialization size (capnp-decided) = "
                       "[" << ceil_div(capnp_seg.size(), size_t(1024)) << " Ki].");
      }
      FLOW_LOG_INFO("= Done.  Total allocated size = "
                    "[" << ceil_div(g_capnp_msg.sizeInWords() * sizeof(word), size_t(1024 * 1024)) << " Mi].");
    } // on_request()
  }; // class Algo

  Algo algo(logger_ptr, chan_ptr);
  post(g_asio, [&]() { algo.start(); });
  g_asio.run();
  g_asio.restart();
} // run_capnp_over_raw()

void run_capnp_zero_copy(flow::log::Logger* logger_ptr, Channel_struc* chan_ptr, Session* session_ptr)
{
  using flow::Flow_log_component;
  using flow::log::Logger;
  using flow::log::Log_context;
  using boost::asio::post;

  /* And now we do just the same thing as run_capnp_over_raw()... except over full-on Flow-IPC, with zero-copy!
   * Obviously you'll see -- especially on the client side -- how much simpler it is.  And it'll be much, much, much
   * faster for most sizes above, like, 10k: as the data are *never* copied.
   *
   * Keeping comments light, since it's the same thing but much simpler; unless there's a difference of course. */

  struct Algo :
    public Log_context
  {
    Channel_struc& m_chan;
    Session::Structured_msg_builder_config::Builder m_capnp_builder;
    Channel_struc::Msg_out m_capnp_msg;

    Algo(Logger* logger_ptr, Channel_struc* chan_ptr, Session* session_ptr) :
      Log_context(logger_ptr, Flow_log_component::S_UNCAT),
      m_chan(*chan_ptr),
      m_capnp_builder(session_ptr->session_shm_builder_config())
    {
      FLOW_LOG_INFO("-- RUN - zero-copy (SHM-backed) capnp request/response using Flow-IPC --");
    }

    void start()
    {
      FLOW_LOG_INFO("= Prep: Deep-copying heap-backed capnp message into Flow-IPC SHM-backed message: START.");
      m_capnp_builder.payload_msg_builder()->setRoot(g_capnp_msg.getRoot<perf_demo::schema::Body>().asReader());
      m_capnp_msg = Channel_struc::Msg_out(std::move(m_capnp_builder));
      FLOW_LOG_INFO("= Prep: Deep-copying heap-backed capnp message into Flow-IPC SHM-backed message: DONE.");

      m_chan.replace_event_wait_handles([]() -> auto { return Asio_handle(g_asio); });
      m_chan.start_ops(ev_wait);
      m_chan.start_and_poll([](const Error_code&) {});

      // Send a dummy message to synchronize initialization.
      FLOW_LOG_INFO("> Issuing handshake SYN for initialization sync.");
      m_chan.send(m_chan.create_msg());

      // Receive a dummy message as a request signal.
      FLOW_LOG_INFO("< Expecting get-cache request.");
      Channel_struc::Msg_in_ptr req;
      m_chan.expect_msg(Channel_struc::Msg_which_in::GET_CACHE_REQ, &req,
                        [&](Channel_struc::Msg_in_ptr&& req) { on_request(std::move(req)); });
      if (req) { on_request(std::move(req)); }
    }

    void on_request(Channel_struc::Msg_in_ptr&& req)
    {
      /* Unlike run_capnp_over_raw(), where to avoid unnecessary code, we accepted any message as a request --
       * here we require GetCacheReq specifically (hence the Msg_which_in::GET_CACHE_REQ arg value above).
       * We don't check its contents, but we might as well print it.  If you're keeping score we're doing some
       * stuff here that run_capnp_over_raw() entirely skips... but good enough for our purposes, we think;
       * at least we're not skewing results in Flow-IPC's favor. */
      FLOW_LOG_INFO("= Got get-cache request [" << *req << "].");

      FLOW_LOG_INFO("> Sending get-cache (quite large) response.");
      m_chan.send(m_capnp_msg, req.get());
      FLOW_LOG_INFO("= Done.");
      m_capnp_msg = {};

      /* There's a little subtlety here; basically everything is cool with the backing memory, until
       * the Session_server dtor runs, specifically in SHM-jemalloc's case; as it'll deinitialize jemalloc
       * and... stuff; the details don't matter (actually the fact that SHM-classic doesn't have this to worry about
       * is merely an internal property; formally one should still not access SHM-backed items once session-server
       * goes away -- even with SHM-classic).  What matters = the principles explained in
       * "Sessions: Teardown; Organizing Your Code" in the guided Manual; which is that when a session-hosing
       * error is reported, one should nullify any SHM-backed objects related to that session; then sometime later
       * (usually soon but w/e) destroy the Session object (have its dtor run).  If it's done in that order,
       * and it's not a crash/zombie situation on the other side, then everything will be safe.  (If it's a crash/zombie
       * situation, then one should still follow that procedure; and there's a good -- as good as possible -- chance
       * one would escape bad consequences.)  As we explained in the "disclaimer" comment at the top: we didn't
       * arrange this app in that nice organized way (for reasons); and our session and channel error handlers
       * literally just no-op (see above).  It is OK though; we control the order of things; all we need to do
       * is nullify SHM-backed stuff before server goes down.  That's why we let the client do that and then
       * send us a message after that... and then we stop.  To be clear: This isn't unsafe; it is safe and formally
       * at that.  Just, don't do this kind of stuff in serious applications.
       *
       * Also one should (not "must") use .async_end_sending() before Channel dtor.  Again though... doesn't matter for
       * us!  But serious apps should do all the good stuff as recommended. */

#if 0 // XXX
      FLOW_LOG_INFO("< Expecting client to signal they are done; so we can blow everything away.");
      req.reset();
      m_chan.expect_msg(Channel_struc::Msg_which_in::GET_CACHE_REQ, &req,
                        [&](auto&&) { g_asio.stop(); });
      if (req) { g_asio.stop(); }
#else
      g_asio.stop();
#endif

      /* The .stop() is needed here, because struc::Channel is always reading all internally incoming messages ASAP;
       * so it always has an .async_wait() outstanding.  Hence the .run() never runs out of work, unless we
       * flip the g_asio internal "is-stopped" switch which causes it to in fact return the moment
       * the .stop()ping task (function, such as this one) returns. */
    } // on_request()
  }; // class Algo

  Algo algo(logger_ptr, chan_ptr, session_ptr);
  post(g_asio, [&]() { algo.start(); });
  g_asio.run();
  g_asio.restart();
  /* These next 2 lines aren't really important; technically it's true that when we issue .stop() that'll prevent
   * any already-queued handlers from running once the .stop()ping task `return`s, so this issues an extra .poll()
   * to "flush" those, if any... but not block after that's done. */
  g_asio.poll();
  g_asio.restart();
} // run_capnp_zero_copy()
