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

/* Please see main_srv.cpp top comment for an overview of this client program and the server counterpart.
 *
 * As is typical in these client-server test/demo programs, the 2 programs mirror each other.  So the comments
 * are generally in main_srv.cpp, and we keep it light here in main_cli.cpp; except where there's our-side-specific
 * stuff.  Please refer to the other file, as you go through this one. */

#include "common.hpp"
#include <flow/perf/checkpt_timer.hpp>

void run_capnp_over_raw(flow::log::Logger* logger_ptr, Channel_raw* chan);
void run_capnp_zero_cpy(flow::log::Logger* logger_ptr, Channel_struc* chan);
void verify_rsp(const perf_demo::schema::GetCacheRsp::Reader& rsp_root);

using Timer = flow::perf::Checkpointing_timer;
using Clock_type = flow::perf::Clock_type;

static Task_engine g_asio;
/* These globals (which, again, don't really need to be global but are just for expediency for now at least, as they're
 * referenced in a few benchmarks) are set by diff benchmarks and then summarized/analyzed a bit at the end of
 * main(). */
static flow::Fine_duration g_capnp_over_raw_rtt;
static flow::Fine_duration g_capnp_zero_cpy_rtt;
// Byte count inside the transmitted data.  1st benchmark sets it; 2nd benchmarks ensures it got same-sized data too.
static size_t g_total_sz = 0;

int main(int argc, char const * const * argv)
{
  using Session = Client_session;
  using flow::log::Simple_ostream_logger;
  using flow::log::Async_file_logger;
  using flow::log::Config;
  using flow::log::Sev;
  using flow::Flow_log_component;
  using flow::util::String_view;
  using flow::util::ceil_div;
  using boost::promise;
  using boost::chrono::microseconds;
  using boost::chrono::round;
  using std::exception;

  constexpr String_view LOG_FILE = "perf_demo_cli.log";
  constexpr int BAD_EXIT = 1;

  // Set up logging.
  Config std_log_config;
  std_log_config.init_component_to_union_idx_mapping<Flow_log_component>(1000, 999);
  std_log_config.init_component_names<Flow_log_component>(flow::S_FLOW_LOG_COMPONENT_NAME_MAP, false, "perf_demo-");
  std_log_config.init_component_to_union_idx_mapping<ipc::Log_component>(2000, 999);
  std_log_config.init_component_names<ipc::Log_component>(ipc::S_IPC_LOG_COMPONENT_NAME_MAP, false, "perf_demo-");
  Simple_ostream_logger std_logger(&std_log_config);
  FLOW_LOG_SET_CONTEXT(&std_logger, Flow_log_component::S_UNCAT);
  // This is separate: the IPC/Flow logging will go into this file.
  const auto log_file = (argc >= 2) ? String_view(argv[1]) : LOG_FILE;
  FLOW_LOG_INFO("Opening log file [" << log_file << "] for IPC/Flow logs only.");
  Config log_config = std_log_config;
  log_config.configure_default_verbosity(Sev::S_INFO, true);
  Async_file_logger log_logger(nullptr, &log_config, log_file, false);

#if JEM_ELSE_CLASSIC
  ipc::session::shm::arena_lend::Borrower_shm_pool_collection_repository_singleton::get_instance()
    .set_logger(&log_logger);
#endif

  try
  {
    ensure_run_env(argv[0], false);

    Session session(&log_logger,
                    CLI_APPS.find(CLI_NAME)->second,
                    SRV_APPS.find(SRV_NAME)->second, [](const Error_code&) {});

    FLOW_LOG_INFO("Session-client attempting to open session against session-server; "
                  "it'll either succeed or fail very soon.");

    Session::Channels chans;
    session.sync_connect(session.mdt_builder(), nullptr, nullptr, &chans); // Let it throw on error.
    FLOW_LOG_INFO("Session/channels opened.");

    assert(chans.size() == 2); // Server shall offer us 2 channels.  (We could also ask for some above, but we won't.)

    auto& chan_raw = chans[0]; // Binary channel for raw-ish tests.
    Channel_struc chan_struc(&log_logger, std::move(chans[1]), // Structured channel: SHM-backed underneath.
                             ipc::transport::struc::Channel_base::S_SERIALIZE_VIA_SESSION_SHM, &session);

    run_capnp_over_raw(&std_logger, &chan_raw); // Benchmark 1.  capnp data transmission without Flow-IPC zero-copy.
    run_capnp_zero_cpy(&std_logger, &chan_struc); // Benchmark 2.  Same but with it.

    /* They already printed detailed timing info; now let's summarize the total results.  As you can see it
     * just prints b1's RTT, b2's RTT, and the ratio; while reminding how much data was transmitted.
     * (Ultimately b2's RTT will always be about the same and small; whereas b1's involves a bunch of copying
     * into/out of tranport and hence will be proportional to data size.)
     *
     * The only subtlety is that we coarsen the RTT to be a multiple of 100us, rounding up.  Reason: It's not
     * bulletproof, and it might be different on slower machines, but for now I've found this to be decent in practice:
     * There's quite a bit of variation for a small message's RTT, maybe +/- 50us; and the total tends to be, if
     * rounded to nearest 100us, at least 100us.  Furthermore, if sending small messages, sometimes there are
     * paradoxical-ish results like b1-RTT/b2-RTT < 1, but really they're both around 100us, so it's more like 1.
     * Once total_sz is increased beyond 10k-or-so, this stuff falls away and the coarsening to 100us-multiples
     * doesn't really matter anyway and is easier to read.
     *
     * Maybe that's silliness.  In any case the un-coarsened detailed results are printed by run_*(); here
     * we're summarizing.  @todo Revisit. */

    const auto raw_rtt = ceil_div(round<microseconds>(g_capnp_over_raw_rtt).count(), microseconds::rep(100)) * 100;
    const auto zcp_rtt = ceil_div(round<microseconds>(g_capnp_zero_cpy_rtt).count(), microseconds::rep(100)) * 100;

    FLOW_LOG_INFO("Benchmark summary (rounded-up to 100-usec multiples): ");
    FLOW_LOG_INFO("Transmission of ~[" << (g_total_sz / 1024) << " ki] of Cap'n Proto structured data: ");
    FLOW_LOG_INFO("Via raw-local-stream-socket: RTT = [" << raw_rtt << " usec].");
    FLOW_LOG_INFO("Via-zero-copy-Flow-IPC-channel ("
#if JEM_ELSE_CLASSIC
                  "SHM-jemalloc-backed"
#else
                  "SHM-classic-backed"
#endif
                  "): RTT = [" << zcp_rtt << " usec].");
    FLOW_LOG_INFO("Ratio = [" << float(raw_rtt) / float(zcp_rtt) << "].");

    FLOW_LOG_INFO("Exiting.");
  } // try
  catch (const exception& exc)
  {
    FLOW_LOG_WARNING("Caught exception: [" << exc.what() << "].");
    FLOW_LOG_WARNING("(Perhaps you did not execute session-server executable in parallel, or "
                     "you executed one or both of us oddly?)");
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

  using Capnp_word_array_ptr = kj::ArrayPtr<const word>;
  using Capnp_word_array_array_ptr = kj::ArrayPtr<const Capnp_word_array_ptr>;
  using Capnp_heap_engine = ::capnp::SegmentArrayMessageReader;

  /* Reminder: see main_srv.cpp run_capnp_over_raw() counterpart; we keep comments light except for client-specifics.
   *
   * In particular the couple comments there about how we could've had simpler code, had we used this or that technique,
   * tends to apply more to *us* rather than main_srv.cpp counterpart (but to it too).  We have more receiving logic
   * including looping receiving, so we're a bit more complex; so those simplifications would've benefitted us more
   * (while trading off other stuff... anyway see that comment in main_srv.cpp!). */

  struct Algo :
    public Log_context
  {
    Channel_raw& m_chan;
    Error_code m_err_code;
    size_t m_sz;
    size_t m_n;
    size_t m_n_segs;
    vector<Blob> m_segs;
    bool m_new_seg_next = true;
    /* Server sends the stuff, but we time from just before sending request to just-after receiving and accessing reply.
     * Ctor call begins the timing; so wait until invoking it. */
    std::optional<Timer> m_timer;

    Algo(Logger* logger_ptr, Channel_raw* chan_ptr) :
      Log_context(logger_ptr, Flow_log_component::S_UNCAT),
      m_chan(*chan_ptr)      
    {
      FLOW_LOG_INFO("-- RUN - capnp request/response over raw local-socket connection --");
    }

    void start()
    {
      m_chan.replace_event_wait_handles([]() -> auto { return Asio_handle(g_asio); });
      m_chan.start_send_blob_ops(ev_wait);
      m_chan.start_receive_blob_ops(ev_wait);

      FLOW_LOG_INFO("< Expecting handshake SYN for initialization sync.");
      m_chan.async_receive_blob(Blob_mutable(&m_n, sizeof(m_n)), &m_err_code, &m_sz,
                                [&](const Error_code& err_code, size_t) { on_sync(err_code); });
      if (m_err_code != ipc::transport::error::Code::S_SYNC_IO_WOULD_BLOCK) { on_sync(m_err_code); }
    }

    void on_sync(const Error_code& err_code)
    {
      if (err_code) { throw Runtime_error(err_code, "run_capnp_over_raw():on_sync()"); }

      // Send a dummy message as a request signal, so we can start timing RTT before sending it.
      FLOW_LOG_INFO("= Got handshake SYN.");

      FLOW_LOG_INFO("> Issuing get-cache request via tiny message.");
      m_timer.emplace(get_logger(), "capnp-raw", Timer::real_clock_types(), 100); // Begin timing.
      m_chan.send_blob(Blob_const(&m_n, sizeof(m_n)));
      m_timer->checkpoint("sent request");

      FLOW_LOG_INFO("< Expecting get-cache response fragment: capnp segment count.");
      m_chan.async_receive_blob(Blob_mutable(&m_n, sizeof(m_n)), &m_err_code, &m_sz,
                                [&](const Error_code& err_code, size_t sz) { on_n_segs(err_code, sz); });
      if (m_err_code != ipc::transport::error::Code::S_SYNC_IO_WOULD_BLOCK) { on_n_segs(m_err_code, m_sz); }
    }

    void on_n_segs(const Error_code& err_code, [[maybe_unused]] size_t sz)
    {
      if (err_code) { throw Runtime_error(err_code, "run_capnp_over_raw():on_n_segs()"); }
      assert((sz == sizeof(m_n)) && "First in-message should be capnp-segment count.");
      assert(m_n != 0);

      m_n_segs = m_n;
      FLOW_LOG_INFO("= Got get-cache response fragment: capnp segment count = [" << m_n_segs << "].");
      FLOW_LOG_INFO("< Expecting get-cache response fragments x N: [seg size, seg content...].");
      m_timer->checkpoint("got seg-count");

      m_segs.reserve(m_n_segs);

      /* This is where the looping-read code is, and where we need to be careful to not start
       * a recursion-loop (stack overflows-oh my) but rather an iteration-loop.  That is, if an async_X() yields
       * would-block then return; if it yields error then explode; but if it yields success, then do *not*
       * call our own function, or some function that would call our own function (that did the async_X()).
       * Rather, loop around to the next async_X().
       *
       * So we just have a simple state machine (with 2 states):
       * m_new_seg_next = true; read seg-size; m_new_seg_next = false; read blobs until seg-size bytes are ready,
       * placing them contiguously into the currently-being-read segment;
       * repeat (until m_n_segs segs have been obtained).
       *
       * We use a flow::util::Blob (a-la vector<uint8_t>) for each segment; its .capacity() = seg-size, while
       * its .size() = how many bytes we've filled out already.  (It is formally allowed to write into the area
       * [.end(), .begin() + capacity()).)
       */
      assert(m_new_seg_next);
      read_segs();
    }

    void read_segs()
    {
      do
      {
        if (m_new_seg_next)
        {
          m_chan.async_receive_blob(Blob_mutable(&m_n, sizeof(m_n)), &m_err_code, &m_sz,
                                    [&](const Error_code& err_code, size_t sz) { on_blob(err_code, sz); });
        }
        else
        {
          auto& seg = m_segs.back();
          m_chan.async_receive_blob(Blob_mutable(seg.end(), seg.capacity() - seg.size()), &m_err_code, &m_sz,
                                    [&](const Error_code& err_code, size_t sz) { on_blob(err_code, sz); });
        }
        if (m_err_code == ipc::transport::error::Code::S_SYNC_IO_WOULD_BLOCK) { return; }
      }
      while (!handle_blob(m_err_code, m_sz));
    }

    void on_blob(const Error_code& err_code, size_t sz)
    {
      if (err_code) { throw Runtime_error(err_code, "run_capnp_over_raw():on_seg_sz()"); }
      if (!handle_blob(err_code, sz))
      {
        read_segs();
      }
    }

    bool handle_blob(const Error_code& err_code, size_t sz)
    {
      if (err_code) { throw Runtime_error(err_code, "run_capnp_over_raw():on_seg_sz()"); }
      if (m_new_seg_next)
      {
        m_new_seg_next = false;
        assert(m_n != 0);

        // New segment's size known; reserve the space and then set .size() = 0, while leaving .capacity() same.
        m_segs.emplace_back(m_n);
        m_segs.back().clear();
        assert(m_segs.back().capacity() == m_n); // Ensure it didn't dealloc.
      }
      else
      {
        // Register the received bytes; then see if we finished the segment with that; or maybe even the last one.
        auto& seg = m_segs.back();
        seg.resize(seg.size() + sz);
        if (seg.size() == seg.capacity())
        {
          // It's e.g. 15 extra lines; let's not poison timing with that unless console logger turned up to TRACE+.
          FLOW_LOG_TRACE("= Got segment [" << m_segs.size() << "] of [" << m_n_segs << "]; "
                         "segment serialization size (capnp-decided) = "
                         "[" << ceil_div(seg.size(), size_t(1024)) << " Ki].");

          if (m_segs.size() == m_n_segs)
          {
            m_timer->checkpoint("got last seg");
            on_complete_response(); // Yay!  Next step of algo.
            return true;
          }
          m_timer->checkpoint("got a seg");
          m_new_seg_next = true;
        }
      }

      return false;
    } // handle_blob()

    void on_complete_response()
    {
      /* Now for vanilla Cap'n Proto work: We have the segments; use SegmentArrayMessageReader as normal to
       * interpret it into a capnp-backed structured message. */
      vector<Capnp_word_array_ptr> capnp_segs;
      capnp_segs.reserve(m_segs.size());

      for (const auto& seg : m_segs)
      {
        capnp_segs.emplace_back(reinterpret_cast<const word*>(seg.const_data()), // uint8_t* -> word*.
                                seg.size() / sizeof(word));
      }
      const Capnp_word_array_array_ptr capnp_segs_ptr(&(capnp_segs.front()), capnp_segs.size());
      Capnp_heap_engine capnp_msg(capnp_segs_ptr,
                                  /* Defeat safety limit.  Search for ReaderOptions in (e.g.) heap_serializer.hpp
                                   * source code for explanation.  We do it here, since we are bypassing all that
                                   * Flow-IPC goodness in favor of direct capnp code (in this part of the demo). */
                                  ::capnp::ReaderOptions{ std::numeric_limits<uint64_t>::max() / sizeof(word), 64 });

      const auto rsp_root = capnp_msg.getRoot<perf_demo::schema::Body>().getGetCacheRsp();

      m_timer->checkpoint("accessed deserialization root");

      FLOW_LOG_INFO("= Done.  Total received size = "
                    "[" << ceil_div(capnp_msg.sizeInWords() * sizeof(word), size_t(1024 * 1024)) << " Mi].  "
                    "Will verify contents (sizes, hashes).");

      verify_rsp(rsp_root);

      FLOW_LOG_INFO("= Contents look good.  Timing results: [\n" << m_timer.value() << "\n].");
      g_capnp_over_raw_rtt = m_timer->since_start().m_values[size_t(Clock_type::S_REAL_HI_RES)];
    } // on_complete_response()
  }; // class Algo

  Algo algo(logger_ptr, chan_ptr);
  post(g_asio, [&]() { algo.start(); });
  g_asio.run();
  g_asio.restart();
} // run_capnp_over_raw()

void run_capnp_zero_cpy([[maybe_unused]] flow::log::Logger* logger_ptr, Channel_struc* chan_ptr)
{
  using flow::Flow_log_component;
  using flow::log::Logger;
  using flow::log::Log_context;
  using ::capnp::word;
  using boost::asio::post;

  // Reminder: see main_srv.cpp run_capnp_over_raw() counterpart; we keep comments light except for client-specifics.

  struct Algo :
    public Log_context
  {
    Channel_struc& m_chan;
    std::optional<Timer> m_timer;

    Algo(Logger* logger_ptr, Channel_struc* chan_ptr) :
      Log_context(logger_ptr, Flow_log_component::S_UNCAT),
      m_chan(*chan_ptr)      
    {
      FLOW_LOG_INFO("-- RUN - zero-copy (SHM-backed) capnp request/response using Flow-IPC --");
    }

    void start()
    {
      m_chan.replace_event_wait_handles([]() -> auto { return Asio_handle(g_asio); });
      m_chan.start_ops(ev_wait);
      m_chan.start_and_poll([](const Error_code&) {});

      // Receive a dummy message to synchronize initialization.
      FLOW_LOG_INFO("< Expecting handshake SYN for initialization sync.");
      Channel_struc::Msg_in_ptr req;
      m_chan.expect_msg(Channel_struc::Msg_which_in::GET_CACHE_REQ, &req,
                        [&](auto&&) { on_sync(); });
      if (req) { on_sync(); }
    }

    void on_sync()
    {
      // Send a dummy message as a request signal, so we can start timing RTT before sending it.
      FLOW_LOG_INFO("= Got handshake SYN.");

      auto req = m_chan.create_msg();
      req.body_root()->initGetCacheReq().setFileName("file.bin");

      FLOW_LOG_INFO("> Issuing get-cache request: [" << req << "].");
      m_timer.emplace(get_logger(), "capnp-flow-ipc-e2e-zero-copy", Timer::real_clock_types(), 100);

      m_chan.async_request(req, nullptr, nullptr,
                           [&](Channel_struc::Msg_in_ptr&& rsp) { on_complete_response(std::move(rsp)); });
      m_timer->checkpoint("sent request");
    }

    void on_complete_response(Channel_struc::Msg_in_ptr&& rsp)
    {
      const auto rsp_root = rsp->body_root().getGetCacheRsp();

      m_timer->checkpoint("accessed deserialization root");

      FLOW_LOG_INFO("= Done.  Will verify contents (sizes, hashes).");

      verify_rsp(rsp_root);

      FLOW_LOG_INFO("= Contents look good.  Timing results: [\n" << m_timer.value() << "\n].");
      g_capnp_zero_cpy_rtt = m_timer->since_start().m_values[size_t(Clock_type::S_REAL_HI_RES)];

      rsp.reset();
#if 0 // XXX
      FLOW_LOG_INFO("> Signaling server we are done; they can blow everything away now.");

      m_chan.send(m_chan.create_msg());
#endif
      g_asio.stop();
    } // on_complete_response()
  }; // class Algo

  Algo algo(logger_ptr, chan_ptr);
  post(g_asio, [&]() { algo.start(); });
  g_asio.run();
  g_asio.restart();
  g_asio.poll();
  g_asio.restart();
} // run_capnp_zero_cpy()

void verify_rsp(const perf_demo::schema::GetCacheRsp::Reader& rsp_root)
{
  using flow::util::String_view;

  size_t total_sz = 0;

  const auto file_parts_list = rsp_root.getFileParts();
  if (file_parts_list.size() == 0)
  {
    throw Runtime_error("Way too few file-parts... something is wrong.");
  }
  for (size_t idx = 0; idx != file_parts_list.size(); ++idx)
  {
    const auto file_part = file_parts_list[idx];
    const auto data = file_part.getData();
    const auto computed_hash = boost::hash<String_view>()
                                 (String_view(reinterpret_cast<const char*>(data.begin()), data.size()));
    if (file_part.getDataSizeToVerify() != data.size())
    {
      throw Runtime_error("A file-part's size does not match!");
    }
    if (file_part.getDataHashToVerify() != computed_hash)
    {
      throw Runtime_error("A file-part's hash does not match!");
    }

    total_sz += data.size();
  }

  if ((g_total_sz != 0) && (total_sz != g_total_sz))
  {
    throw Runtime_error("Total rough data sizes between different runs do not match!");
  }
  g_total_sz = total_sz;
}
