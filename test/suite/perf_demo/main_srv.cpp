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
#include "schema.capnp.h"
#include <ipc/transport/bipc_mq_handle.hpp>
#include <ipc/session/shm/arena_lend/jemalloc/session_server.hpp>
#include <flow/log/simple_ostream_logger.hpp>
#include <flow/log/async_file_logger.hpp>

void run_capnp_over_raw(flow::log::Logger* logger_ptr, Channel_raw* chan);
void run_capnp_zero_copy(flow::log::Logger* logger_ptr, Channel_struc* chan);

using Capnp_heap_engine = ::capnp::MallocMessageBuilder;
Task_engine g_asio;
Capnp_heap_engine g_capnp_msg;

int main(int argc, char const * const * argv)
{
  using Session = Session_server::Server_session_obj;
  using flow::log::Simple_ostream_logger;
  using flow::log::Async_file_logger;
  using flow::log::Config;
  using flow::log::Sev;
  using flow::Flow_log_component;

  using boost::promise;

  using std::string;
  using std::exception;

  const string LOG_FILE = "perd_demo_srv.log";
  const int BAD_EXIT = 1;

  /* Set up logging within this function.  We could easily just use `cout` and `cerr` instead, but this
   * Flow stuff will give us time stamps and such for free, so why not?  Normally, one derives from
   * Log_context to do this very trivially, but we just have the one function, main(), so far so: */
  Config std_log_config;
  std_log_config.init_component_to_union_idx_mapping<Flow_log_component>(1000, 999);
  std_log_config.init_component_names<Flow_log_component>(flow::S_FLOW_LOG_COMPONENT_NAME_MAP, false, "link_test-");

  Simple_ostream_logger std_logger(&std_log_config);
  FLOW_LOG_SET_CONTEXT(&std_logger, Flow_log_component::S_UNCAT);

  // This is separate: the IPC/Flow logging will go into this file.
  string log_file((argc >= 2) ? string(argv[1]) : LOG_FILE);
  FLOW_LOG_INFO("Opening log file [" << log_file << "] for IPC/Flow logs only.");
  Config log_config = std_log_config;
  log_config.configure_default_verbosity(Sev::S_INFO, true);
  Async_file_logger log_logger(nullptr, &log_config, log_file, false /* No rotation; we're no serious business. */);

  /* Instructed to do so by ipc::session::shm::arena_lend public docs (short version: this is basically a global,
   * and it would not be cool for ipc::session non-global objects to impose their individual loggers on it). */
  ipc::session::shm::arena_lend::Borrower_shm_pool_collection_repository_singleton::get_instance()
    .set_logger(&log_logger);

  try
  {
    ensure_run_env(argv[0], true);

    Session_server srv(&log_logger, SRV_APPS.find(SRV_NAME)->second, CLI_APPS);

    {
      FLOW_LOG_INFO("Prep: Filling capnp MallocMessageBuilder: START.");
      constexpr size_t TOTAL_SZ = 1 * 1000 * 1024 * 1024;
      constexpr size_t FILE_PART_SZ = 128 * 1024;
      constexpr size_t HASH_SZ = 256 / 8;

      auto file_parts_list = g_capnp_msg.initRoot<perf_demo::schema::Body>().initGetCacheRsp()
                               .initFileParts(TOTAL_SZ / FILE_PART_SZ);
      for (size_t idx = 0; idx != file_parts_list.size(); ++idx)
      {
        auto file_part = file_parts_list[idx];
        auto data = file_part.initData(FILE_PART_SZ);
        for (size_t byte_idx = 0; byte_idx != FILE_PART_SZ; ++byte_idx)
        {
          data[byte_idx] = uint8_t(byte_idx % 256);
        }
        auto hash = file_part.initData(HASH_SZ);
        for (size_t byte_idx = 0; byte_idx != HASH_SZ; ++byte_idx)
        {
          hash[byte_idx] = uint8_t(byte_idx % 256);
        }
      }

      FLOW_LOG_INFO("Prep: Filling capnp MallocMessageBuilder: DONE.");
    }

    FLOW_LOG_INFO("Session-server started; invoke session-client executable from same CWD; it will open session with "
                  "some init-channel(s).");

    Session session;
    promise<void> accepted_promise;
    bool ok = false;
    Session_server::Channels chans;
    srv.async_accept(&session, &chans, nullptr, nullptr,
                     [](auto&&, auto&&, auto&&) -> size_t { return 2; }, // 2 init-channels to open.
                     [](auto&&, auto&&, auto&&, auto&&) {},
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

    session.init_handlers([](const Error_code&) {});
    // Session in PEER state (opened fully); so channels are ready too.

    auto& chan_raw = chans[0]; // Binary channel for raw-ish tests.XXX
    Channel_struc chan_struc(&log_logger, std::move(chans[1]), // Structured channel: SHM-backed underneath.XXX
                             ipc::transport::struc::Channel_base::S_SERIALIZE_VIA_SESSION_SHM, &session);

    run_capnp_over_raw(&std_logger, &chan_raw);
    run_capnp_zero_copy(&std_logger, &chan_struc);

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
  using boost::asio::post;
  using std::vector;

  struct Algo :// Just so we can arrange functions in chronological order really.
    public Log_context
  {
    Channel_raw& m_chan;
    Error_code m_err_code;
    size_t m_sz;
    size_t m_n = 0;
    Capnp_heap_engine m_capnp_msg;

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

      // Send a dummy message to synchronize initialization.
      FLOW_LOG_INFO("> Issuing handshake SYN for initialization sync.");
      m_chan.send_blob(Blob_const(&m_n, sizeof(m_n)));

      // Receive a dummy message as a request signal.
      FLOW_LOG_INFO("< Expecting get-cache request via tiny message.");
      m_chan.async_receive_blob(Blob_mutable(&m_n, sizeof(m_n)), &m_err_code, &m_sz,
                                [&](const Error_code& err_code, size_t) { on_request(err_code); });
      if (m_err_code != ipc::transport::error::Code::S_SYNC_IO_WOULD_BLOCK) { on_request(m_err_code); }
    }

    void on_request(const Error_code& err_code)
    {
      if (err_code) { throw Runtime_error(err_code, "run_capnp_over_raw():on_request()"); }
      FLOW_LOG_INFO("= Got get-cache request.");

      
    }
  }; // class Algo

  Algo algo(logger_ptr, chan_ptr);
  post(g_asio, [&]() { algo.start(); });
  g_asio.run();
} // run_capnp_over_raw()

void run_capnp_zero_copy(flow::log::Logger*, Channel_struc*)// chan_ptr)
{
} // run_capnp_zero_copy()
