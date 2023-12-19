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

#include "ex_guy.hpp"
#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

namespace ipc::transport::test
{

const std::string Ex_guy::S_CLI_NAME = "excli";
const std::string Ex_guy::S_CLI_NAME_2 = "excli2"; // See notes at m_*_apps ctor init.
const std::string Ex_guy::S_SRV_NAME = "exsrv";
const std::string Ex_guy::S_VAR_RUN = "/tmp/var/run"; // Avoid /var/run so can run as non-root.

Ex_guy::Ex_guy(flow::log::Logger* logger_ptr, flow::log::Logger* ipc_logger_ptr) :
  flow::log::Log_context(logger_ptr),
  m_ipc_logger(ipc_logger_ptr),

  // IPC universe description:

  /* The way we do this isn't 100% realistic, just to keep
   * it as one source program for testing ease, but the deviations from reality don't affect any code paths
   * in Flow-IPC, so it's fine.  @todo Consider modeling each of these closer to reality.
   *   - Really App doc header recommends an App master set, then copy those into Client_app, Server_app master sets,
   *     with possible duplication.  (@todo Would be good to make some simple Flow-IPC utilities to make that easier;
   *     then test that here.)  But we model simple case, where no Client_app and Server_app refer to the same
   *     App (i.e., copy the same App values from an App master set).  Hence we don't have a separate App set.
   *   - Really a session ends normally when either the server process or client process quits; but we
   *     delete session/maybe server objects to model the same thing without necessarily exiting process yet
   *     (and start another session, as-if from a new process).
   *     This is allowed but not really realistic.
   *   - We *do* model that client versus server is a different app (executable) but in cheesy fashion:
   *     it's really the same executable but copied into 2 different ~/bin/... paths/names and invoked
   *     with different cmd-line args to act as server versus client.
   *   - We declare 1 Server_app and 2 `Client_app`s.  However the same process pretends to be each of the 2
   *     `Client_app`s.  It's fine, the server doesn't care, but it's somewhat a fake setup. */
  // Universe of client apps: 2, with caveats above.
  m_cli_apps({ { S_CLI_NAME,
                 { { S_CLI_NAME, std::string(getenv("HOME")) + "/bin/ex_cli.exec", ::geteuid(), ::getegid() } } },
               { S_CLI_NAME_2,
                 { { S_CLI_NAME_2, std::string(getenv("HOME")) + "/bin/ex_cli.exec", ::geteuid(), ::getegid() } } } }),
  // Universe of server apps: Just one.
  m_srv_apps{ { S_SRV_NAME,
                { { S_SRV_NAME, std::string(getenv("HOME")) + "/bin/ex_srv.exec", ::geteuid(), ::getegid() },
                  { S_CLI_NAME, S_CLI_NAME_2 }, // Allowed cli-apps that can open sessions.
                  S_VAR_RUN,
                  util::Permissions_level::S_GROUP_ACCESS } } },

  m_loop(get_logger(), "guy")
{
  // OK.
}

void Ex_guy::done_and_done(bool ok)
{
  m_exit_promise.set_value(ok);
}

void Ex_guy::stop_worker()
{
  m_loop.stop();
}

flow::util::Task_engine* Ex_guy::task_engine()
{
  return m_loop.task_engine().get();
}

} // namespace ipc::transport::test
