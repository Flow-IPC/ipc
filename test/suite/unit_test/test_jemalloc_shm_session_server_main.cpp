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

#include "ipc/session/standalone/shm/arena_lend/jemalloc/test/test_shm_session_server.hpp"
#include "ipc/session/standalone/shm/arena_lend/jemalloc/test/test_shm_session_server_executor.hpp"
#include "ipc/session/standalone/shm/arena_lend/jemalloc/test/test_shm_session_server_launcher.hpp"
#include "ipc/test/test_logger.hpp"
#include <ipc/common.hpp>
#include <flow/test/test_common_util.hpp>

using ipc::session::shm::arena_lend::jemalloc::test::Test_shm_session_server;
using ipc::session::shm::arena_lend::jemalloc::test::Test_shm_session_server_executor;
using ipc::session::shm::arena_lend::jemalloc::test::Test_shm_session_server_launcher;

using ipc::Log_component;
using flow::test::to_underlying;
using std::string_view;

static int usage_exit_code();
static int usage_error(flow::log::Logger& logger, const string_view& program_name);

int main(int argc, char** argv)
{
  using Object_type = Test_shm_session_server::Object_type;
  using Operation_mode = Test_shm_session_server::Operation_mode;

  ipc::test::Test_logger logger(flow::log::Sev::S_INFO);
  Test_shm_session_server_executor executor(&logger);

  if (argc != 4)
  {
    return usage_error(logger, argv[0]);
  }

  int object_type_int;
  try
  {
    object_type_int = boost::lexical_cast<int>(argv[1]);
  }
  catch (const boost::bad_lexical_cast& error)
  {
    return usage_error(logger, argv[0]);
  }

  Object_type object_type;
  switch (object_type_int)
  {
    case to_underlying(Object_type::S_ARRAY):
      object_type = Object_type::S_ARRAY;
      break;

    case to_underlying(Object_type::S_VECTOR):
      object_type = Object_type::S_VECTOR;
      break;

    case to_underlying(Object_type::S_STRING):
      object_type = Object_type::S_STRING;
      break;

    case to_underlying(Object_type::S_LIST):
      object_type = Object_type::S_LIST;
      break;

    default:
      return usage_error(logger, argv[0]);
  }

  int operation_mode_int;
  try
  {
    operation_mode_int = boost::lexical_cast<int>(argv[2]);
  }
  catch (const boost::bad_lexical_cast& error)
  {
    return usage_error(logger, argv[0]);
  }

  Operation_mode operation_mode;
  switch (operation_mode_int)
  {
    case to_underlying(Operation_mode::S_NORMAL):
      operation_mode = Operation_mode::S_NORMAL;
      break;

    case to_underlying(Operation_mode::S_DISCONNECT):
      operation_mode = Operation_mode::S_DISCONNECT;
      break;

    case to_underlying(Operation_mode::S_ERROR_HANDLING):
      operation_mode = Operation_mode::S_ERROR_HANDLING;
      break;

    default:
      return usage_error(logger, argv[0]);
  }

  pid_t client_process_id;
  try
  {
    client_process_id = boost::lexical_cast<pid_t>(argv[3]);
  }
  catch (const boost::bad_lexical_cast& error)
  {
    return usage_error(logger, argv[0]);
  }

  return static_cast<int>(executor.run(object_type, client_process_id, operation_mode));
}

int usage_exit_code()
{
  return to_underlying(Test_shm_session_server_launcher::Result::S_INVALID_ARGUMENT);
}

int usage_error(flow::log::Logger& logger, const string_view& program_name)
{
  FLOW_LOG_SET_CONTEXT(&logger, Log_component::S_TEST);
  FLOW_LOG_WARNING("Usage: " << program_name << " ObjectType OperationMode ClientProcessId");

  return usage_exit_code();
}
