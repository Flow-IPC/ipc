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

#include <gtest/gtest.h>
#include <boost/program_options.hpp>
#include <iostream>
#include <flow/log/log.hpp>
#include "ipc/test/test_config.hpp"

using std::cout;
using std::cerr;
using std::string;
using ipc::test::Test_config;

// const char* malloc_conf = "tcache:false";

/**
 * Configure logging for the unit tests based on command-line parameters.
 *
 * @param argc The number of command-line arguments
 * @param argv The command-line arguments
 *
 * @return 0 upon success; otherwise, 1 for help and < 0 upon error
 */
static int configure_logging(int argc, char* argv[]);

/**
 * Unit test starting hook.
 *
 * @param argc The number of command-line arguments
 * @param argv The command-line arguments
 *
 * @return 0 upon success; otherwise, 1 for help and < 0 upon error
 */
int main(int argc, char **argv)
{
  /* Let gtest parse command line first. InitGoogleTest() will parse --gtest_*
     parameters and also remove them from argv so that boost::program_options
     won't see them (it would otherwise treat them as illegal unknown options).

     Also, in the case of "death tests", the unit test program is
     forked and rerun as invoked with additional --gtest_* command line
     parameters */
  ::testing::InitGoogleTest(&argc, argv);
  // Death tests sometimes run into an issue when kicking off multiple threads
  // ::testing::FLAGS_gtest_death_test_style = "threadsafe";

  int result = configure_logging(argc, argv);
  if (result != 0)
  {
    return result;
  }

  // Start the unit tests
  return RUN_ALL_TESTS();
}

int configure_logging(int argc, char* argv[])
{
  namespace po = boost::program_options;
  using namespace ipc::test;

  Test_config::get_singleton(); // Touching it just below.

  po::options_description cmd_line_opts("unit test options");
  cmd_line_opts.add_options()
    ((Test_config::S_HELP_PARAM + ",h").c_str(), "help")
    ((Test_config::S_LOG_SEVERITY_PARAM + ",l").c_str(), po::value<flow::log::Sev>(&Test_config::get_singleton().m_sev),
     "minimum log severity");

  try
  {
    po::variables_map vm;
    po::parsed_options cmd_line_parsed_opts = po::parse_command_line(argc, argv, cmd_line_opts);
    po::store(cmd_line_parsed_opts, vm);
    po::notify(vm);

    if (vm.count(Test_config::S_HELP_PARAM) > 0)
    {
      cout << cmd_line_opts << "\n";
      return 1;
    }
  }
  catch(const po::error& err)
  {
    cerr << err.what() << "\n";
    return -1;
  }

  return 0;
}
