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
#include <ipc/session/app.hpp>
#include <flow/util/util.hpp>
#include <flow/error/error.hpp>
#include <boost/filesystem/operations.hpp>

/* These programs are doing some things that are counter-indicated for production server
 * applications; namely it is enforced that it is invoked from the dir where both session-server and -client apps
 * reside; and it uses that same directory in-place of /var/run for storing internal PID file and such.  Similarly
 * using the *actual* current effective-UID/GID as part of the ipc::session::App-loaded values is not properly secure.
 * All these shortcuts are to ease the execution of these guys as a test; not to show off best practices. */

const fs::path WORK_DIR = fs::canonical(fs::current_path().lexically_normal());

// Has to match CMakeLists.txt-stored executable name.
static const std::string S_EXEC_PREFIX = "rpc_demo_";
static const std::string S_EXEC_POSTFIX = ".exec";
#if JEM_ELSE_CLASSIC
static const std::string S_EXEC_PRE_POSTFIX = "_shm_jemalloc";
#else
static const std::string S_EXEC_PRE_POSTFIX = "_shm_classic";
#endif
const std::string SRV_NAME = "srv";
/* We use a little trick: If an arg specifies we want to *not* do zero-copy RPC (--no-zc), then as client we ID
 * ourselves as CLI_NAME_NO_ZC; otherwise as CLI_NAME; but that's just the App::m_name; really both `Client_app`s
 * refer to the same actual executable and are therefore equal (as seen below).  So the server is coded (main_srv.cpp)
 * to accordingly expect no-zero-copy for CLI_NAME_NO_ZC app, yes-zero-copy for CLI_NAME. */
const std::string CLI_NAME = "cli";
const std::string CLI_NAME_NO_ZC = "cliNoZc";

// Universe of server apps: Just one.
const ipc::session::Server_app::Master_set SRV_APPS
        ({ { SRV_NAME,
             /* Also... we actually expect the two apps to be placed as follows:
              * cli/...cli....exec and srv/...srv....exec, with two 2 dirs next to each other.
              * It's so that on a crash/exception the potential core files don't collide, to be honest.
              * Not that it crashes!  But when trying stuff, it can, and then it's nice to get the core file sans pain.
              *
              * OK... so the point is, WORK_DIR is either cli/ or srv/; hence the server's location -- to either guy --
              * can be written as <WORK_DIR>/../srv. */
             { { SRV_NAME,
                 WORK_DIR / ".." / SRV_NAME / (S_EXEC_PREFIX + SRV_NAME + S_EXEC_PRE_POSTFIX + S_EXEC_POSTFIX),
                 ::geteuid(), ::getegid() },
               { CLI_NAME, CLI_NAME_NO_ZC }, // Allowed cli-apps that can open sessions.
               WORK_DIR / ".." / "srv",
               ipc::util::Permissions_level::S_GROUP_ACCESS } } });

// Universe of client apps: Just one... actually 2 but referring to the same real app (see "trick" note above).

/* The ipc::session security model is such that the binary must be invoked *exactly* using the
 * command listed here.  In *nix land at least this is how that is likely to look.
 * (In a production scenario this would be a canonical (absolute, etc.) path.) */
const auto CLI_PATH = fs::path(".") / (S_EXEC_PREFIX + CLI_NAME + S_EXEC_PRE_POSTFIX + S_EXEC_POSTFIX);
const ipc::session::Client_app::Master_set CLI_APPS
        {
          {
            CLI_NAME,
            { { CLI_NAME, CLI_PATH, ::geteuid(), ::getegid() } }
          },
          {
            CLI_NAME_NO_ZC,
            { { CLI_NAME_NO_ZC, CLI_PATH, ::geteuid(), ::getegid() } }
          }
        };

void ensure_run_env(const char* argv0, bool srv_else_cli)
{
  const auto exp_path = WORK_DIR /
                        (S_EXEC_PREFIX + (srv_else_cli ? SRV_NAME : CLI_NAME) + S_EXEC_PRE_POSTFIX + S_EXEC_POSTFIX);
  if (fs::canonical(fs::path(argv0)) != exp_path)
  {
    throw flow::error::Runtime_error
            (flow::util::ostream_op_string("Resolved/normalized argv0 [", argv0, "] should "
                                           "equal our particular executable off the CWD, namely [", exp_path, "]; "
                                           "try again please.  I.e., the CWD must contain the executable."));
  }
}

void setup_logging(std::optional<flow::log::Simple_ostream_logger>* std_logger,
                   std::optional<flow::log::Async_file_logger>* log_logger,
                   int argc, char const * const * argv, bool srv_else_cli)
{
  using flow::util::String_view;
  using flow::util::ostream_op_string;
  using flow::log::Config;
  using flow::log::Sev;
  using flow::Flow_log_component;

  // `static`s below because must exist throughout the logger's existence; this is an easy way in our little app.

  // Console logger setup.
  static Config std_log_config;
  std_log_config.init_component_to_union_idx_mapping<Flow_log_component>
    (1000, Config::standard_component_payload_enum_sparse_length<Flow_log_component>(), true);
  std_log_config.init_component_to_union_idx_mapping<ipc::Log_component>
    (2000, Config::standard_component_payload_enum_sparse_length<ipc::Log_component>(), true);
  std_log_config.init_component_names<Flow_log_component>(flow::S_FLOW_LOG_COMPONENT_NAME_MAP, false, "flow-");
  std_log_config.init_component_names<ipc::Log_component>(ipc::S_IPC_LOG_COMPONENT_NAME_MAP, false, "ipc-");
  std_logger->emplace(&std_log_config);
  FLOW_LOG_SET_CONTEXT(&(**std_logger), Flow_log_component::S_UNCAT);

  // This is separate: the IPC/Flow logging will go into this file.
  const auto LOG_FILE = ostream_op_string(S_EXEC_PREFIX, srv_else_cli ? SRV_NAME : CLI_NAME, ".log");
  constexpr size_t ARG_IDX = 1;
  const auto log_file = (size_t(argc) > ARG_IDX) ? String_view(argv[ARG_IDX]) : String_view(LOG_FILE);
  auto sev = Sev::S_INFO;
  if (size_t(argc) > ARG_IDX + 1)
  {
    sev = boost::lexical_cast<Sev>(argv[ARG_IDX + 1]);
  }
  FLOW_LOG_INFO("Opening log file [" << log_file << "] for IPC/Flow logs only.");
  static auto log_config = std_log_config;
  log_config.configure_default_verbosity(sev, true);
  log_logger->emplace(nullptr, &log_config, log_file, false /* No rotation; we're no serious business. */);
}
