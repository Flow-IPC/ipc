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

#include "schema.capnp.h"
#include <ipc/transport/bipc_mq_handle.hpp>
#include <ipc/session/shm/arena_lend/jemalloc/client_session.hpp>
#include <ipc/session/shm/arena_lend/jemalloc/session_server.hpp>
#include <ipc/session/shm/classic/client_session.hpp>
#include <ipc/session/shm/classic/session_server.hpp>
#include <ipc/session/app.hpp>
#include <flow/log/simple_ostream_logger.hpp>
#include <flow/log/async_file_logger.hpp>
#include <boost/filesystem/path.hpp>
#include <string>

namespace fs = boost::filesystem;

extern const fs::path WORK_DIR;

// Common ipc::session::App-related data used on both sides (the "IPC universe" description).
extern const std::string SRV_NAME;
extern const std::string CLI_NAME;
extern const ipc::session::Server_app::Master_set SRV_APPS;
extern const ipc::session::Client_app::Master_set CLI_APPS;

using Error_code = flow::Error_code;
using Runtime_error = flow::error::Runtime_error;
using Blob = flow::util::Blob_sans_log_context;

// Session will emit Unix-domain-socket-transport-based channels.  Structured-channels will be zero-copy-enabled.
#if JEM_ELSE_CLASSIC
namespace ssn = ipc::session::shm::arena_lend::jemalloc;
#else
namespace ssn = ipc::session::shm::classic;
#endif

using Client_session = ssn::Client_session<ipc::session::schema::MqType::NONE, false>;
using Session_server = ssn::Session_server<ipc::session::schema::MqType::NONE, false>;
// We'll use an unstructured channel of this type (again, Unix domain socket underneath) to time non-zero-copy xmission.
using Channel_raw = Client_session::Channel_obj;
// We'll use a structured channel of this type to time non-zero-copy transmission of capnp-backed structured data.
using Channel_struc = Client_session::Structured_channel<perf_demo::schema::Body>::Sync_io_obj;

// A/k/a boost::asio::io_context.
using Task_engine = flow::util::Task_engine;
using Asio_handle = ipc::util::sync_io::Asio_waitable_native_handle;
using Blob_const = ipc::util::Blob_const;
using Blob_mutable = ipc::util::Blob_mutable;

// Invoke from main() from either application to ensure it's being run directly from the expected CWD.
void ensure_run_env(const char* argv0, bool srv_else_cli);

void ev_wait(Asio_handle* hndl_of_interest,
             bool ev_of_interest_snd_else_rcv, ipc::util::sync_io::Task_ptr&& on_active_ev_func);
