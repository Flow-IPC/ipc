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

#include "ipc/session/shm/arena_lend/jemalloc/session_server.hpp"
#include "ipc/session/shm/arena_lend/jemalloc/client_session.hpp"
#include "ipc/session/standalone/shm/arena_lend/borrower_shm_pool_collection_repository.hpp"
#include "ipc/session/shm/classic/session_server.hpp"
#include "ipc/session/shm/classic/client_session.hpp"
#include "script_interpreter.hpp"
#include "ex_srv.hpp"
#include "ex_cli.hpp"
#include "ex.capnp.h"
#include <flow/log/simple_ostream_logger.hpp>
#include <flow/log/async_file_logger.hpp>

namespace ipc::transport::test
{

class Driver : private boost::noncopyable
{
public:
  explicit Driver();
  int main(int argc, char const * const * argv);

private:
  flow::log::Logger* get_logger() const;
  const flow::log::Component& get_log_component() const;

  std::unique_ptr<flow::log::Config> m_console_logger_cfg; // Null until top of main().
  std::unique_ptr<flow::log::Logger> m_console_logger; // Ditto.
  const flow::log::Component m_log_component;
}; // class Driver

Driver::Driver() :
  m_log_component(Log_component::S_TEST)
{
  // Loggers set up at top of main().
}

flow::log::Logger* Driver::get_logger() const
{
  return m_console_logger.get();
}
const flow::log::Component& Driver::get_log_component() const
{
  return m_log_component;
}

int Driver::main(int argc, char const * const * argv)
{
  namespace log = flow::log;
  using log::Sev;
  using std::unique_ptr;
  using boost::filesystem::path;
  using util::String_view;
  constexpr int BAD_EXIT = 1;
  constexpr String_view MODE_SCRIPTED = "scripted";
  constexpr String_view MODE_EX_SRV = "exercise-srv";
  constexpr String_view MODE_EX_SRV_SHM_C = "exercise-srv-shm-c";
  constexpr String_view MODE_EX_SRV_SHM_J = "exercise-srv-shm-j";
  constexpr String_view MODE_EX_CLI = "exercise-cli";
  constexpr String_view MODE_EX_CLI_SHM_C = "exercise-cli-shm-c";
  constexpr String_view MODE_EX_CLI_SHM_J = "exercise-cli-shm-j";

  // Set up console logging.
  {
    const auto sev = (argc < 4) ? Sev::S_INFO : boost::lexical_cast<Sev>(argv[3]);

    m_console_logger_cfg.reset(new log::Config(sev));
    m_console_logger.reset(new log::Simple_ostream_logger(m_console_logger_cfg.get()));
    // get_logger() now works; so logging can begin.
    log::Logger::Logger::this_thread_set_logged_nickname("main", get_logger());
    log::beautify_chrono_logger_this_thread(get_logger());

    FLOW_LOG_INFO_WITHOUT_CHECKING("ipc::transport test tool started; logging to console; severity [" << sev << "].");
  }

  // Set up Flow-IPC logging (to file) / get the test mode.
  unique_ptr<log::Config> ipc_logger_cfg;
  unique_ptr<log::Async_file_logger> ipc_logger;

  const auto quit = [&]() -> int
  {
    FLOW_LOG_WARNING("Usage: transport_test <test-mode> <ipc-log-file> [<console-log-sev>] [<ipc-log-sev>]");
    FLOW_LOG_WARNING("  <test-mode> = scripted => read transport-test script from stdin (see script files nearby, "
                     "redirect)");
    FLOW_LOG_WARNING("  <test-mode> = exercise-srv[-shm-{c|j}] => demo/exercise procedure 1, "
                     "as server [with SHM-classic/SHM-jemalloc]");
    FLOW_LOG_WARNING("  <test-mode> = exercise-cli[-shm-{c|j}] => demo/exercise procedure 2, "
                     "as client [with SHM-classic/SHM-jemalloc]");
    return BAD_EXIT;
  };

  if (argc < 3)
  {
    return quit();
  }
  // else
  const String_view mode(argv[1]);
  bool mode_scripted = mode == MODE_SCRIPTED;
  bool mode_ex_srv = mode == MODE_EX_SRV;
  bool mode_ex_srv_shm_b = mode == MODE_EX_SRV_SHM_C;
  bool mode_ex_srv_shm_j = mode == MODE_EX_SRV_SHM_J;
  bool mode_ex_cli = mode == MODE_EX_CLI;
  bool mode_ex_cli_shm_b = mode == MODE_EX_CLI_SHM_C;
  bool mode_ex_cli_shm_j = mode == MODE_EX_CLI_SHM_J;
  if (!(mode_scripted || mode_ex_srv || mode_ex_cli
        || mode_ex_srv_shm_b || mode_ex_srv_shm_j || mode_ex_cli_shm_b || mode_ex_cli_shm_j))
  {
    return quit();
  }
  // else

  const auto ipc_logger_file = boost::lexical_cast<path>(argv[2]);
  const auto sev = (argc < 5) ? Sev::S_INFO : boost::lexical_cast<Sev>(argv[4]);

  FLOW_LOG_INFO("Creating/opening Flow-IPC log file; Flow-IPC will log there with severity [" << sev << "].");

  ipc_logger_cfg.reset(new log::Config(sev));
  ipc_logger.reset(new log::Async_file_logger(0, // Could pass in m_console_logger here, but it's boring stuff.
                                              ipc_logger_cfg.get(),
                                              ipc_logger_file,
                                              false)); // Don't worry about rotation.

  {
    FLOW_LOG_SET_CONTEXT(ipc_logger.get(), this->get_log_component()); // For the following only:
    beautify_chrono_logger_this_thread(get_logger());
    FLOW_LOG_INFO("ipc::transport test tool shall log Flow-IPC logs here; severity [" << sev << "].");
  }

  // Proceed.

  if (mode_scripted)
  {
    FLOW_LOG_INFO("Creating Script_interpreter:  Will read all lines until EOF (Ctrl-D in console); then execute.");
    auto script_interpreter = Script_interpreter::create(get_logger(), ipc_logger.get(), std::cin);

    FLOW_LOG_INFO("Script_interpreter: go!  Above-scanned lines shall now be interpreted.");
    return script_interpreter->go() ? 0 : BAD_EXIT;
  }
  // else if (mode_ex_*):

  // `guy` is a guy who does stuff.
  bool ok;

  /* @todo Cheesy; do better; maybe make configurable with cmd line too.
   * @todo It is pretty important to be able to try 1-pipe of each type and 2-pipe configurations.  A ton of stuff
   *       (low-level) gets exercised differently for MQs versus sockets, and for 1 pipe present versus 2.
   *
   * Subtlety:
   *   - In SHM-mode, use POSIX MQs.  Why not?  They're allegedly somewhat faster than bipc MQs (internally
   *     which are SHM-backed using boost.interprocess's own SHM support).
   *     (@todo This could be configurable via cmd line though as noted just above.)
   *   - In heap-mode, use bipc MQs.  Why not POSIX (or configurable)?  Answer:
   *     Each MQ's max-message-size is set internally to 8,192, with max-n-messages=10;
   *     and these are *each* typically the limit and cannot be set higher; but a typical limit
   *     for the *product* of these, summed over all MQs opened at a given time in a given process
   *     is usually 819,200 -- hence up to 10 MQs open at a time.  The test in
   *     practice needs to create more than that and therefore fails when trying to open the ~5th channel after a bit.
   *     bipc MQs have no such issue.  And hey -- using heap-mode means one is giving up a bunch of real performance,
   *     so at that point whatever is the difference between POSIX MQs and bipc MQs, perf-wise, is not too germane.
   *
   * And hey: This way we get to test POSIX MQs with cmd line switch config 1 and bipc MQs with config 2 -- no
   * recompile/code change needed.  Not bad. */
#define SESSION_CFG_NO_SHM \
  /* Type of MQs in channels opened via sessions: NONE, BIPC (boost.interprocess), POSIX: */ \
  session::schema::MqType::BIPC, \
  /* Whether to be able to transmit sockets over those channels (iff yes, and MqType=/=NONE, then 2-pipe channels */ \
  /* are opened (and transparently muxed/demuxed by struc::Channel if used for a given Channel)): */ \
  true, \
  /* The metadata mini-schema optionally used in: open_channel() (either direction) at channel-open time, */ \
  /* async_connect() (c2s) at session-open time, async_accept() (s2c) at session-open time: */ \
  capnp::ExMdt
#define SESSION_CFG_SHM \
  session::schema::MqType::POSIX, true, capnp::ExMdt

  /* Instructed to do so by ipc::session::shm::arena_lend public docs (short version: this is basically a global,
   * and it would not be cool for ipc::session non-global objects to impose their individual loggers on it). */
  session::shm::arena_lend::Borrower_shm_pool_collection_repository_singleton::get_instance()
    .set_logger(ipc_logger.get());

  if (mode_ex_srv)
  {
    Ex_srv<session::Session_server<SESSION_CFG_NO_SHM>, false> guy(get_logger(), ipc_logger.get());
    ok = guy.run(); // Do the stuff (may start thread(s) then join it/them); then return.
  }
  else if (mode_ex_srv_shm_b) // Same but with SHM-classic.
  {
    Ex_srv<session::shm::classic::Session_server<SESSION_CFG_SHM>, true> guy(get_logger(), ipc_logger.get());
    ok = guy.run();
  }
  else if (mode_ex_srv_shm_j) // Same but with SHM-jemalloc.
  {
    Ex_srv<session::shm::arena_lend::jemalloc::Session_server<SESSION_CFG_SHM>, true>
      guy(get_logger(), ipc_logger.get());
    ok = guy.run();
  }
  else if (mode_ex_cli)
  {
    Ex_cli<session::Client_session<SESSION_CFG_NO_SHM>, false> guy(get_logger(), ipc_logger.get());
    ok = guy.run();
  }
  else if (mode_ex_cli_shm_b) // Same but with SHM-classic.
  {
    Ex_cli<session::shm::classic::Client_session<SESSION_CFG_SHM>, true> guy(get_logger(), ipc_logger.get());
    ok = guy.run();
  }
  else if (mode_ex_cli_shm_j) // Same but with SHM-jemalloc.
  {
    Ex_cli<session::shm::arena_lend::jemalloc::Client_session<SESSION_CFG_SHM>, true>
      guy(get_logger(), ipc_logger.get());
    ok = guy.run();
  }
  else
  {
    assert(false);
    ok = false; // Avoid warning.
  }
  return ok ? 0 : BAD_EXIT;
} // Driver::main()

} // namespace ipc::transport::test

int main(int argc, char const * const * argv)
{
  return (ipc::transport::test::Driver()).main(argc, argv);
}
