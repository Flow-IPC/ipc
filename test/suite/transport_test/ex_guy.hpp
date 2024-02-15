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

#pragma once

#include "ipc/session/app.hpp"
#include "ipc/shm/stl/stateless_allocator.hpp"
#include <flow/async/single_thread_task_loop.hpp>
#include <flow/util/basic_blob.hpp>
#include <flow/common.hpp>

// Yeah, it's an internal header.  We wanna print some `Text::Reader`s, and we're inside ipc::transport{} so sue me.
#include "ipc/util/detail/util.hpp"

#include <boost/interprocess/containers/list.hpp>
#include <boost/interprocess/containers/string.hpp>
#include <boost/move/make_unique.hpp>

// @todo This is lame.  Use gtest or something civilized like that.
#define ASSERT(expr) \
  FLOW_UTIL_SEMICOLON_SAFE \
  ( \
    if (!(expr)) \
    { \
      std::cerr << FLOW_UTIL_WHERE_AM_I() << ": Condition failed; aborting.  The condition: [" << #expr << "].\n"; \
      std::abort(); \
    } \
  )

namespace ipc::transport::test
{

class Ex_guy : public flow::log::Log_context
{
protected:
  template<typename T>
  using Uptr = boost::movelib::unique_ptr<T>;
  template<typename T>
  using Sptr = boost::shared_ptr<T>;

  using Seconds = boost::chrono::seconds;
  using Void_func = Function<void()>;

  Ex_guy(flow::log::Logger* logger_ptr, flow::log::Logger* ipc_logger_ptr);

  template<typename Task>
  void post(Task&& task);
  template<typename Task>
  void delay_and(util::Fine_duration from_now, Task&& task);

  template<typename T, typename... Args>
  static Uptr<T> make_uptr(Args&&... args);
  template<typename T, typename... Args>
  static Sptr<T> make_sptr(Args&&... args);

  void done_and_done(bool ok);
  void stop_worker();

  template<typename Task>
  bool run(bool srv_else_cli, Task&& body);

  template<typename T>
  void check_scalar_set(const T& val, util::String_view ctx);

  flow::util::Task_engine* task_engine();

  static const std::string S_CLI_NAME;
  static const std::string S_CLI_NAME_2; // See notes at m_*_apps ctor init.
  static const std::string S_SRV_NAME;
  static const std::string S_VAR_RUN; // Avoid /var/run so can run as non-root.

  /* (As noted in the test code proper: this isn't necessary knowledge in general -- it's discoverable through
   * session:: API including through mdt and just the init-channel counts in the end -- but we've contrived some
   * checks for demo/fun). */
  static constexpr size_t S_N_INIT_CHANS_B = 3;
  static constexpr size_t S_N_CHANS_B = 4;
  static constexpr size_t S_N_INIT_CHANS_A = 2;
  static constexpr size_t S_N_CHANS_A = 4;

  flow::log::Logger* const m_ipc_logger;

  const session::Client_app::Master_set m_cli_apps;
  const session::Server_app::Master_set m_srv_apps;

private:
  flow::async::Single_thread_task_loop m_loop;

  boost::promise<bool> m_exit_promise;
}; // class Ex_guy

// SHM types (unused if !S_SHM_ENABLED).

/* User probably wouldn't need to do this kind of thing, as their code most likely would be either
 * SHM-backed or not, whereas we're testing both with some `if constexpr(S_SHM_ENABLED)` clauses.
 * std::conditional<A, B, C> does not work if B wouldn't compile with A=false, so we do some manual
 * specializing to avoid those errors.  Point is, user probably wouldn't need to wrap it in a specialized
 * struct template; could just have the aliases defined directly (or not defined at all, if they're not doing
 * SHM-backed things). */
template<typename Session_t, bool S_SHM_ENABLED_V>
struct Shm_traits
{
  using Payload_list_ptr = int;
  using Payload_list_brw_ptr = int;
};
template<typename Session_t>
struct Shm_traits<Session_t, true>
{
  using Session = Session_t;

  using Arena = typename Session::Arena;
  using Arena_activator = shm::stl::Arena_activator<Arena>;
  template<typename T>
  using Allocator = typename Session::template Allocator<T>;
  template<typename T>
  using Borrower_allocator = typename Session::template Borrower_allocator<T>;
  using Shm_string = bipc::basic_string<char, std::char_traits<char>, Allocator<char>>;
  // Borrower-side equivalent.  (SHM-classic: Simply same type.  Jemalloc: Not.)
  using Shm_string_brw = bipc::basic_string<char, std::char_traits<char>, Borrower_allocator<char>>;
  using Shm_blob = flow::util::Basic_blob<Allocator<uint8_t>>; // Non-sharing-capable Blob.
  // Borrower-side equivalent.  (SHM-classic: Simply same type.  Jemalloc: Not.)
  using Shm_blob_brw = flow::util::Basic_blob<Borrower_allocator<uint8_t>>;
  using Shm_sharing_blob = flow::util::Basic_blob<Allocator<uint8_t>, true>; // Sharing-capable Blob.
  // Borrower-side equivalent.  (SHM-classic: Simply same type.  Jemalloc: Not.)
  using Shm_sharing_blob_brw = flow::util::Basic_blob<Borrower_allocator<uint8_t>, true>;
  template<typename T>
  using Shm_handle = typename Session::Arena::template Handle<T>; // It's just ...::shared_ptr<>.

  struct Payload_t
  {
    Shm_string m_str;
    Shm_blob m_blob;
    Shm_sharing_blob m_pool;
    int m_num;
  };
  struct Payload_brw_t
  {
    Shm_string_brw m_str;
    Shm_blob_brw m_blob;
    Shm_sharing_blob_brw m_pool;
    int m_num;
  };

  using Payload = Payload_t;
  // Borrower-side equivalent.  (SHM-classic: Simply same type.  Jemalloc: Not.)
  using Payload_brw = Payload_brw_t;
  using Payload_list = bipc::list<Payload, Allocator<Payload>>;
  // Borrower-side equivalent.  (SHM-classic: Simply same type.  Jemalloc: Not.)
  using Payload_list_brw = bipc::list<Payload_brw, Borrower_allocator<Payload_brw>>;

  using Payload_list_ptr = Shm_handle<Payload_list>;
  using Payload_list_brw_ptr = Shm_handle<Payload_list_brw>;
};

template<typename Task>
void Ex_guy::post(Task&& task)
{
  m_loop.post([this, task = std::move(task)]() mutable
  {
    try
    {
      task();
    }
    catch (const std::exception& exc)
    {
      FLOW_LOG_WARNING("Exception caught at loop-thread top-level; exiting loop.  Message: [" << exc.what() << "].");
      done_and_done(false);
    }
  });
} // Ex_guy::post()

template<typename Task>
void Ex_guy::delay_and(util::Fine_duration from_now, Task&& task)
{
  m_loop.schedule_from_now(from_now, [this, task = std::move(task)](bool) mutable
  {
    try // @todo Code reuse versus post().
    {
      task();
    }
    catch (const std::exception& exc)
    {
      FLOW_LOG_WARNING("Exception caught at loop-thread top-level; exiting loop.  Message: [" << exc.what() << "].");
      done_and_done(false);
    }
  });
}

template<typename T, typename... Args>
Ex_guy::Uptr<T> Ex_guy::make_uptr(Args&&... args)
{
  return boost::movelib::make_unique<T>(std::forward<Args>(args)...);
}

template<typename T, typename... Args>
Ex_guy::Sptr<T> Ex_guy::make_sptr(Args&&... args)
{
  return boost::make_shared<T>(std::forward<Args>(args)...);
}

template<typename Task>
bool Ex_guy::run(bool srv_else_cli, Task&& body)
{
  using std::string;

  try
  {
    const auto& exp_bin_path
      = (srv_else_cli ? static_cast<const session::App&>(m_srv_apps.find(string(S_SRV_NAME))->second)
                      : static_cast<const session::App&>(m_cli_apps.find(string(S_CLI_NAME))->second))
          .m_exec_path;

    FLOW_LOG_INFO("Determining our location to ensure it matches [" << exp_bin_path << "].");
#ifndef FLOW_OS_LINUX
#  error "We expect Linux in this test/demo program."
#endif
    const auto bin_path = fs::read_symlink("/proc/self/exe"); // Can throw.
    if (bin_path != exp_bin_path)
    {
      FLOW_LOG_WARNING("Path [" << bin_path << "] does not.  Please copy executable to that name/place; retry.  "
                       "The ipc::session setup expects these apps to be daemons, basically, and act a certain way "
                       "including living in a particular place with a particular name configured in code.");
      return false;
    }
    // else

    FLOW_LOG_INFO("Loop-thread start.  In original-thread I await a loop-thread task to signal its doneness.");
    m_loop.start();

    body();

    auto done_future = m_exit_promise.get_future();
    done_future.wait();
    const bool ok = done_future.get();

    if (ok)
    {
      FLOW_LOG_INFO("Loop-thread joined.  Final result: OK.");
    }
    else
    {
      FLOW_LOG_WARNING("Loop-thread joined.  Final result: FAIL.");
    }
    return ok;
  }
  catch (const std::exception& exc)
  {
    FLOW_LOG_WARNING("Exception caught at original-thread top-level; aborting.  Message: [" << exc.what() << "].");
    return false;
  }

  ASSERT(false);
  return true;
} // Ex_guy::run()

template<typename T>
void Ex_guy::check_scalar_set(const T& val, util::String_view ctx)
{
  if (val == T())
  {
    FLOW_LOG_FATAL("Value (ctx [" << ctx << "]) [" << val << "] is not set as expected (is default-valued).  FAIL.");
    ASSERT(false);
    return;
  }
  // else
  FLOW_LOG_TRACE("Value (ctx [" << ctx << "]) [" << val << "] is not-default-valued (cool).");
}

} // namespace ipc::transport::test
