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

#include "ipc/shm/arena_lend/borrower_shm_pool_collection.hpp"
#include "ipc/shm/arena_lend/shm_pool.hpp"
#include "ipc/shm/arena_lend/test/test_borrower.hpp"
#include "ipc/test/test_common_util.hpp"
#include "ipc/test/test_config.hpp"
#include "ipc/test/test_logger.hpp"
#include <boost/program_options.hpp>
#include <flow/log/log.hpp>

using std::string;
using std::cout;
using std::cerr;
using std::size_t;
using std::shared_ptr;
using std::make_shared;

using ipc::Log_component;
using ipc::shm::arena_lend::Borrower_shm_pool_collection;
using ipc::shm::arena_lend::Shm_pool;
using ipc::shm::arena_lend::Shm_pool_collection;
using ipc::shm::arena_lend::test::Test_borrower;
using ipc::test::Test_logger;
using ipc::test::Test_config;

using Collection_id = ipc::shm::arena_lend::Collection_id;
using pool_id_t = ipc::shm::arena_lend::Shm_pool::pool_id_t;

namespace
{

enum class Status_code : int
{
  S_INVALID_PARAM = -1,
  S_SUCCESS = 0,
  S_HELP,
  S_FAILED,
};

static bool execute_read_check(Test_logger& test_logger,
                               Collection_id shm_pool_collection_id,
                               pool_id_t shm_pool_id,
                               const string& shm_object_name,
                               size_t shm_object_size,
                               size_t data_offset,
                               const string& expected_data);

static int parse_and_execute(int argc, char** argv)
{
  namespace po = boost::program_options;

  Collection_id shm_pool_collection_id;
  pool_id_t shm_pool_id;
  string shm_object_name;
  size_t shm_object_size;
  size_t data_offset;
  string expected_data;

  Test_config::get_singleton(); // Touching it just below.

  po::options_description cmd_line_opts("unit test options");
  cmd_line_opts.add_options()
    ((Test_config::S_HELP_PARAM + ",h").c_str(), "help")
    ((Test_config::S_LOG_SEVERITY_PARAM + ",l").c_str(), po::value<flow::log::Sev>(&Test_config::get_singleton().m_sev),
      "minimum log severity")
    ((Test_borrower::S_SHM_POOL_COLLECTION_ID_PARAM + ",c").c_str(),
     po::value<Collection_id>(&shm_pool_collection_id),
     "SHM pool collection id")
    ((Test_borrower::S_SHM_POOL_ID_PARAM + ",i").c_str(), po::value<pool_id_t>(&shm_pool_id), "SHM pool ID")
    ((Test_borrower::S_SHM_OBJECT_NAME_PARAM + ",o").c_str(), po::value<string>(&shm_object_name), "SHM object name")
    ((Test_borrower::S_SHM_OBJECT_SIZE_PARAM + ",z").c_str(), po::value<size_t>(&shm_object_size), "SHM object size")
    ((Test_borrower::S_DATA_OFFSET_PARAM + ",f").c_str(), po::value<size_t>(&data_offset), "data offset")
    ((Test_borrower::S_EXPECTED_DATA_PARAM + ",d").c_str(), po::value<string>(&expected_data), "expected data");

  try
  {
    po::variables_map vm;
    po::parsed_options cmd_line_parsed_opts = po::parse_command_line(argc, argv, cmd_line_opts);
    po::store(cmd_line_parsed_opts, vm);
    po::notify(vm);

    if (vm.count(Test_config::S_HELP_PARAM) > 0)
    {
      cout << cmd_line_opts << "\n";
      return static_cast<int>(Status_code::S_HELP);
    }
  }
  catch(const po::error& err)
  {
    cerr << err.what() << "\n";
    return static_cast<int>(Status_code::S_INVALID_PARAM);
  }

  Test_logger test_logger;
  bool result = execute_read_check(test_logger,
                                   shm_pool_collection_id,
                                   shm_pool_id,
                                   shm_object_name,
                                   shm_object_size,
                                   data_offset,
                                   expected_data);

  return static_cast<int>(result ? Status_code::S_SUCCESS : Status_code::S_FAILED);
}

bool execute_read_check(Test_logger& test_logger,
                        Collection_id shm_pool_collection_id,
                        pool_id_t shm_pool_id,
                        const string& shm_object_name,
                        size_t shm_object_size,
                        size_t data_offset,
                        const string& expected_data)
{
  Borrower_shm_pool_collection borrower_collection(&test_logger, shm_pool_collection_id);

  FLOW_LOG_SET_CONTEXT(&test_logger, Log_component::S_TEST);

  FLOW_LOG_TRACE("Checking SHM object pool collection id " << shm_pool_collection_id <<
                 ", id " << shm_pool_id << ", name '" << shm_object_name << "', size " << shm_object_size <<
                 ", data offset " << data_offset << ", expected_data '" << expected_data << "'");

  if (expected_data.size() > shm_object_size)
  {
    FLOW_LOG_WARNING("Expected size of data " << expected_data.size() << " > SHM object size " << shm_object_size);
    return false;
  }

  if (data_offset > (shm_object_size - expected_data.size()))
  {
    FLOW_LOG_WARNING("Data offset " << data_offset << " + " << " data size " << expected_data.size() <<
                     " extends past SHM object size " << shm_object_size);
    return false;
  }

  shared_ptr<Shm_pool> shm_pool = borrower_collection.open_shm_pool(shm_pool_id, shm_object_name, shm_object_size);
  if (shm_pool == nullptr)
  {
    return false;
  }

  if (memcmp(shm_pool->get_address(), expected_data.c_str(), expected_data.size()) != 0)
  {
    FLOW_LOG_WARNING("SHM pool data != expected");
    return false;
  }

  FLOW_LOG_TRACE("SHM pool data == expected");

  if (!borrower_collection.release_shm_pool(shm_pool))
  {
    return false;
  }

  return true;
}

} // Anonymous namespace

int main(int argc, char** argv)
{
  return parse_and_execute(argc, argv);
}
