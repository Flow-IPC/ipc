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

void run_capnp_over_raw(flow::log::Logger* logger_ptr, Channel_raw* chan);
void run_capnp_zero_copy(flow::log::Logger* logger_ptr, Channel_struc* chan);

int main(int argc, char const * const * argv)
{
  using Session = Client_session;
  using flow::log::Simple_ostream_logger;
  using flow::log::Async_file_logger;
  using flow::log::Config;
  using flow::log::Sev;
  using flow::Flow_log_component;

  using boost::promise;

  using std::string;
  using std::exception;

  const string LOG_FILE = "perf_demo_cli.log";
  const int BAD_EXIT = 1;

  /* Set up logging within this function.  We could easily just use `cout` and `cerr` instead, but this
   * Flow stuff will give us time stamps and such for free, so why not?  Normally, one derives from
   * Log_context to do this very trivially, but we just have the one function, main(), so far so: */
  Config std_log_config;
  std_log_config.init_component_to_union_idx_mapping<Flow_log_component>(1000, 999);
  std_log_config.init_component_names<Flow_log_component>(flow::S_FLOW_LOG_COMPONENT_NAME_MAP, false, "perf_demo-");

  Simple_ostream_logger std_logger(&std_log_config);
  FLOW_LOG_SET_CONTEXT(&std_logger, Flow_log_component::S_UNCAT);

  // This is separate: the IPC/Flow logging will go into this file.
  string log_file((argc >= 2) ? string(argv[1]) : LOG_FILE);
  FLOW_LOG_INFO("Opening log file [" << log_file << "] for IPC/Flow logs only.");
  Config log_config = std_log_config;
  log_config.configure_default_verbosity(Sev::S_INFO, true);
  Async_file_logger log_logger(nullptr, &log_config, log_file, false /* No rotation; we're no serious business. */);

  ipc::session::shm::arena_lend::Borrower_shm_pool_collection_repository_singleton::get_instance()
    .set_logger(&log_logger);

  try
  {
    ensure_run_env(argv[0], false);

    Session session(&log_logger,
                    CLI_APPS.find(CLI_NAME)->second,
                    SRV_APPS.find(SRV_NAME)->second, [](const Error_code&) {});

    FLOW_LOG_INFO("Session-client attempting to open session against session-server; "
                  "it'll either succeed or fail very soon.");

    Session::Channels chans; // Server shall offer us 2 channels.
    session.sync_connect(session.mdt_builder(), nullptr, nullptr, &chans); // Let it throw on error.
    FLOW_LOG_INFO("Session/channels opened.");

    auto& chan_raw = chans[0]; // Binary channel for raw-ish tests.
    Channel_struc chan_struc(&log_logger, std::move(chans[1]), // Structured channel: SHM-backed underneath.
                             ipc::transport::struc::Channel_base::S_SERIALIZE_VIA_SESSION_SHM, &session);

    run_capnp_over_raw(&std_logger, &chan_raw);
    run_capnp_zero_copy(&std_logger, &chan_struc);

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

Task_engine g_asio;

void run_capnp_over_raw(flow::log::Logger* logger_ptr, Channel_raw* chan_ptr)
{
  using flow::Flow_log_component;
  using flow::log::Logger;
  using flow::log::Log_context;
  using boost::asio::post;
  using std::vector;

  using Capnp_word_array_ptr = kj::ArrayPtr<const ::capnp::word>;
  using Capnp_word_array_array_ptr = kj::ArrayPtr<const Capnp_word_array_ptr>;
  using Capnp_heap_engine = ::capnp::SegmentArrayMessageReader;

  struct Algo :// Just so we can arrange functions in chronological order really.
    public Log_context
  {
    Channel_raw& m_chan;
    Error_code m_err_code;
    size_t m_sz;
    size_t m_n;
    size_t m_n_segs;
    vector<Blob> m_segs;
    bool m_new_seg_next = true;

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

      // Receive a dummy message to synchronize initialization.
      FLOW_LOG_INFO("< Expecting handshake SYN for initialization sync.");
      m_chan.async_receive_blob(Blob_mutable(&m_n, sizeof(m_n)), &m_err_code, &m_sz,
                                [&](const Error_code& err_code, size_t) { on_sync(err_code); });
      if (m_err_code != ipc::transport::error::Code::S_SYNC_IO_WOULD_BLOCK) { on_sync(m_err_code); }
    }

    void on_sync(const Error_code& err_code)
    {
      if (err_code) { throw Runtime_error(err_code, "run_capnp_over_raw():on_sync()"); }

      // Send a dummy message as a request signal, so we can start timing RTT before sending it.
      FLOW_LOG_INFO("> Issuing get-cache request via tiny message.");
      m_chan.send_blob(Blob_const(&m_n, sizeof(m_n)));

      FLOW_LOG_INFO("< Expecting get-cache response fragment: capnp segment count.");
      m_chan.async_receive_blob(Blob_mutable(&m_n, sizeof(m_n)), &m_err_code, &m_sz,
                                [&](const Error_code& err_code, size_t sz) { on_n_segs(err_code, sz); });
      if (m_err_code != ipc::transport::error::Code::S_SYNC_IO_WOULD_BLOCK) { on_n_segs(m_err_code, m_sz); }
    }

    void on_n_segs(const Error_code& err_code, size_t sz)
    {
      if (err_code) { throw Runtime_error(err_code, "run_capnp_over_raw():on_n_segs()"); }
      assert((sz == sizeof(m_n)) && "First in-message should be capnp-segment count.");
      assert(m_n != 0);

      m_n_segs = m_n;
      FLOW_LOG_INFO("= Got get-cache response fragment: capnp segment count = [" << m_n_segs << "].");

      m_segs.reserve(m_n_segs);
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

        m_segs.emplace_back(m_n);
        m_segs.back().clear();
      }
      else
      {
        auto& seg = m_segs.back();
        seg.resize(seg.size() + sz);
        if (seg.size() == seg.capacity())
        {
          if (m_segs.size() == m_n_segs)
          {
            on_complete_response();
            return true;
          }
          m_new_seg_next = true;
        }
      }

      return false;
    } // handle_blob()

    void on_complete_response()
    {
      using ::capnp::word;

      vector<Capnp_word_array_ptr> capnp_segs;
      capnp_segs.reserve(m_segs.size());

      for (const auto& seg : m_segs)
      {
        capnp_segs.emplace_back(reinterpret_cast<const word*>(seg.const_data()), // uint8_t* -> word*.
                                seg.size() / sizeof(word));
      }
      const Capnp_word_array_array_ptr capnp_segs_ptr(&(capnp_segs.front()), capnp_segs.size());
      Capnp_heap_engine capnp_msg(capnp_segs_ptr);

      [[maybe_unused]] auto rsp_root = capnp_msg.getRoot<perf_demo::schema::Body>().getGetCacheRsp(); // XXX
    } // on_complete_response()
  }; // class Algo

  Algo algo(logger_ptr, chan_ptr);
  post(g_asio, [&]() { algo.start(); });
  g_asio.run();
} // run_capnp_over_raw()

void run_capnp_zero_copy(flow::log::Logger*, Channel_struc*)// chan_ptr)
{
} // run_capnp_zero_copy()
