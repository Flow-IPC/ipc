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

#include "ex_guy.hpp"
#include "ex.capnp.h"
#include "ipc/session/session_server.hpp"
#include "ipc/session/error.hpp"
#include "ipc/transport/struc/channel.hpp"
#include "ipc/session/sync_io/session_server_adapter.hpp"

#include <boost/array.hpp>
#include <algorithm>

namespace ipc::transport::test
{

template<typename Server_t, bool S_SHM_ENABLED_V>
class Ex_srv : public Ex_guy
{
public:
  using Server = Server_t;

  Ex_srv(flow::log::Logger* logger_ptr, flow::log::Logger* ipc_logger_ptr);
  ~Ex_srv();

  bool run();

private:
  using Base = Ex_guy;
  using Session = typename Server::Server_session_obj;
  using Server_sio = session::sync_io::Session_server_adapter<Server>;
  using Session_sio = typename Server_sio::Session_obj;
  using Channel_obj = typename Session::Channel_obj;
  using Channels = typename Session::Channels;
  using Mdt_reader_ptr = typename Session::Mdt_reader_ptr;
  using Mdt_builder = typename Session::Mdt_builder;

  static constexpr bool S_SHM_ENABLED = Session::S_SHM_ENABLED;
  // Unused if !S_SHM_ENABLED.
  static constexpr bool S_CLASSIC_ELSE_JEM = Session::S_SHM_TYPE == session::schema::ShmType::CLASSIC;

  template<typename Body>
  using Structured_channel = typename Session::template Structured_channel<Body>;

  using Channel_b = Structured_channel<capnp::ExBodyB>;
  using Msg_out_b = typename Channel_b::Msg_out;

  // Complements Ex_cli::App_session (see that guy for background).
  class App_session : public flow::log::Log_context
  {
  public:
    template<typename Task>
    App_session(Ex_srv* guy, unsigned int test_idx, // test_idx = 0 => test most stuff; 1 => test the rest.
                Task&& i_am_done_func);
  private:
    using Channel_a = Structured_channel<capnp::ExBodyA>;
    using Channels_a = std::vector<Uptr<Channel_a>>;
    using Channels_b = std::vector<Uptr<Channel_b>>;
    using Msg_in_a = typename Channel_a::Msg_in;
    using Msg_in_ptr_a = typename Channel_a::Msg_in_ptr;
    using Msg_out_a = typename Channel_a::Msg_out;
    using Msg_in_b = typename Channel_b::Msg_in;
    using Msg_in_ptr_b = typename Channel_b::Msg_in_ptr;

    void on_chan_open(Channel_obj&& new_chan, const Mdt_reader_ptr& mdt_cli); // Enough of these => go to:

    void use_channels_if_ready();
    void use_channels_round_1(); // Main tests, part 1.  (test_idx == 0)
    void use_channels_round_2(); // Main tests, part 2.
    void use_channels_round_1a(); // Remaining tests, part 1.  (test_idx == 1)
    void use_channels_round_2a(); // Remaining tests, part 2.

    void start_chan_a(size_t chan_idx);
    void start_chan_b(size_t chan_idx);
    template<typename Channel>
    void on_chan_err(const Channel& chan, const unsigned int& expectations, const Error_code& err_code);

    void check_notif_payload_a(Msg_in_ptr_a&& msg_in, size_t chan_idx, bool alt = false);
    void ping_a(size_t chan_idx);
    void ping_b(size_t chan_idx);
    template<typename On_msg_handler>
    void send_req_b(size_t chan_idx, util::String_view ctx, bool reuse_msg, bool reuse_shm_payload,
                    bool omit_handle, bool session_lend_else_arena_lend, bool alt, On_msg_handler&& on_msg_func);
    template<typename On_msg_handler>
    void send_req_a(size_t chan_idx, On_msg_handler&& on_msg_func);
    template<typename Task>
    void expect_ping_and_b(size_t chan_idx, Task&& task);

    Ex_srv* const m_guy;
    const unsigned int m_test_idx;
    const flow::async::Task m_i_am_done_func;
    // The actual Server_session<>!  Well, the Server_session_adapter<that>, since we're doing sync_io pain (see m_srv).
    Uptr<Session_sio> m_ses;
    Channels m_chans_a; // From Ex_srv::m_init_chans_srv from last async_accept(); + srv-opened additional channels.
    Channels m_chans_b; // From Ex_srv::m_init_chans_cli from last async_accept(); + cli-opened additional channels.
    /* And then they are upgraded into these Structured_channel<>s.  Note the 2 sets use different schemas;
     * hence the different types at compile-time.  Sane exact thing in Ex_cli::App_session. */
    Channels_a m_struct_chans_a;
    Channels_b m_struct_chans_b;

    // Place to save an out-request msg ID so undo_expect_responses() can be called on it to stop expecting respones.
    struc::Channel_base::msg_id_out_t m_saved_req_id_out;

    /* Not-very-rigorous expectations count: e.g., expect some message = ++m_expectations_a;
     * got it = --m_expectations_a; got it again = --m_expectations_a; result is negative => something went wrong;
     * result is positive => something went wrong in the other direction; result is 0 = OK probably. */
    boost::array<unsigned int, S_N_CHANS_A> m_expectations_a = { 0, 0, 0, 0 };
    boost::array<unsigned int, S_N_CHANS_B> m_expectations_b = { 0, 0, 0, 0 };

    static constexpr size_t S_RESERVED_CHAN_IDX = decltype(m_expectations_b)::static_size - 1;
  }; // class App_session

  void server_listen();
  void server_accept_one();
  void server_accept_loop();
  void ev_wait(util::sync_io::Asio_waitable_native_handle* hndl_of_interest,
               bool ev_of_interest_snd_else_rcv, util::sync_io::Task_ptr&& on_active_ev_func);

  static constexpr size_t S_CLI_2_N_INIT_CHANS_US = 0;
  static constexpr size_t S_CLI_2_N_INIT_CHANS_THEM = 0;

  // Generates sessions.  We exercise the sync_io:: interface.
  Uptr<Server_sio> m_srv;
  // One test session.
  Uptr<Session_sio> m_ses;
  // Mostly we will instead use this (which server_accept() fills out progressively):
  std::vector<Uptr<Session_sio>> m_sessions;
  // And most of those we will move() into these `App_session`s:
  std::vector<Uptr<App_session>> m_app_sessions;

  /* Each m_srv->async_accept() targets the following guys.  Note only one async ->a_a() is in progress at a time,
   * so these cannot be touched by anything concurrently. */
  Channels m_init_chans_srv; // Init-channels opened by our request.
  Channels m_init_chans_cli; // Init-channels opened by opposing client request.
  Mdt_reader_ptr m_mdt_cli; // At-session-open metadata filled-out by client.

  /* The following 2 items are cross-session, so they're not in App_session but outside it.
   * App_session sets them via its App_session::m_guy. */

  // Place to save a message intended for testing of resends of same msg across different test cases.
  Msg_out_b m_saved_msg;
  /* Place to save a SHM handle to a payload STL structure intended for testing of resends of same handle across
   * different messages as well as modifying the structure without even resending the handle which should register
   * on the other side. */
  std::optional<typename Shm_traits<Session, S_SHM_ENABLED>::Payload_list_ptr> m_saved_payload_list_ptr;
}; // class Ex_srv

#define TEMPLATE \
  template<typename Server_t, bool S_SHM_ENABLED_V>
#define CLASS \
  Ex_srv<Server_t, S_SHM_ENABLED_V>

TEMPLATE
CLASS::Ex_srv(flow::log::Logger* logger_ptr, flow::log::Logger* ipc_logger_ptr) :
  Base(logger_ptr, ipc_logger_ptr)
{
  // Yeah.
}

TEMPLATE
CLASS::~Ex_srv()
{
  /* A bit subtle but: Our base Ex_guy has the Single_thread_task_loop (thread W), where things are post()ed
   * (directly or via async_wait() at least) to execute; those things will touch parts of *this.  *this is
   * about to be destroyed; so there is a race wherein last-second stuff can try to touch parts that are
   * being destroyed at the same time (data race, caught by TSAN, yay!), or even after they are destroyed
   * (use-after-free, which ASAN would've caught).  At any rate this will synchronously stop thread W before
   * such things can happen. */
  Base::stop_worker();
}

TEMPLATE
bool CLASS::run()
{
  return Base::run(true, [&]()
  {
    post([this]() { server_listen(); });
  }); // Base::run()
} // CLASS::run()

TEMPLATE
void CLASS::server_listen()
{
  using util::sync_io::Asio_waitable_native_handle;
  using util::sync_io::Task_ptr;

  FLOW_LOG_INFO("Starting session-server listen.");

  /* Here we choose to exercise the sync_io-style counterpart to Server_session (our `Server`), which is
   * sync_io::Session_server_adapter<Server>.  It is really easier to use the async-I/O one,
   * but here we test this style for better test coverage.  (Internally S_s_a uses a
   * S_s<> anyway, so we are exercising both this way.)  As a result we also have to exercise
   * Server_session_adapter<Session> instead of "plain" Session -- same deal there.
   *
   * Since we've got a boost.asio loop, it's pretty easy for us to set it up, just a matter of
   * .replace_event_wait_handles() and .start_ops(); then it's pretty much the same.  (Same for each
   * Session it generates via .async_accept(). */

  m_srv = make_uptr<Server_sio>(m_ipc_logger, m_srv_apps.find(S_SRV_NAME)->second, m_cli_apps);

  /* We make a bunch of sessions and have a few apps; the virtual pool sizes (note: not actual physical RAM)
   * can add up and hit kernel limit for active pool vaddr space (in sum).  So we reduce this to about how much
   * we need for the largest pool in practice.  That is only relevant to SHM-classic; SHM-jemalloc adjusts
   * automatically and uses many smaller pools (and obv it's totally irrelevant if SHM not involved). */
  if constexpr(S_SHM_ENABLED && S_CLASSIC_ELSE_JEM)
  {
    m_srv->core()->pool_size_limit_mi(320);
  }

  m_srv->replace_event_wait_handles([this]() -> auto
                                      { return Asio_waitable_native_handle(*(task_engine())); });
  m_srv->start_ops([this](Asio_waitable_native_handle* hndl_of_interest,
                          bool ev_of_interest_snd_else_rcv, Task_ptr&& on_active_ev_func)
                     { ev_wait(hndl_of_interest, ev_of_interest_snd_else_rcv, std::move(on_active_ev_func)); });

  server_accept_one();
}

TEMPLATE
void CLASS::ev_wait(util::sync_io::Asio_waitable_native_handle* hndl_of_interest,
                    bool ev_of_interest_snd_else_rcv, util::sync_io::Task_ptr&& on_active_ev_func)
{
  using util::sync_io::Asio_waitable_native_handle;

  // They want us to async-wait.  Oblige.
  ASSERT(hndl_of_interest);
  hndl_of_interest->async_wait(ev_of_interest_snd_else_rcv
                                 ? Asio_waitable_native_handle::Base::wait_write
                                 : Asio_waitable_native_handle::Base::wait_read,
                               [on_active_ev_func = std::move(on_active_ev_func)]
                                 (const Error_code& err_code)
  {
    if (err_code == boost::asio::error::operation_aborted)
    {
      return; // Stuff is shutting down.  GTFO.
    }
    // else

    // They want to know about completed async_wait().  Oblige.
    (*on_active_ev_func)();
  }); // hndl_of_interest->async_wait()
} // void CLASS::ev_wait()

TEMPLATE
void CLASS::server_accept_one()
{
  FLOW_LOG_INFO("Accepting one session (simple async_accept() overload, in particular expecting 0 init-channels).");

  m_srv->async_accept((m_ses = make_uptr<Session_sio>()).get(), [this](const Error_code& err_code)
  {
    ASSERT((err_code != session::error::Code::S_OBJECT_SHUTDOWN_ABORTED_COMPLETION_HANDLER)
           && "We are not killing m_srv, so this should not happen.");

    post([this, err_code]()
    {
      if (err_code != session::error::Code::S_INVALID_ARGUMENT)
      {
        FLOW_LOG_WARNING("Expected invalid-argument; client should have requested 1+ init-channels; "
                         "got [" << err_code << "] [" << err_code.message() << "] instead.");
        done_and_done(false);
        return;
      }
      // else
      FLOW_LOG_INFO("As expected got invalid-argument error, as client requested 1+ init-channels.");

      server_accept_loop();
    });
  });
} // CLASS::server_accept_one()

TEMPLATE
void CLASS::server_accept_loop()
{
  using util::sync_io::Asio_waitable_native_handle;
  using util::sync_io::Task_ptr;
  using boost::shared_ptr;
  using Client_app = session::Client_app;

  // As noted near m_sessions decl, only 1 ->async_accept() outstanding at a time, targeting these guys.
  m_sessions.emplace_back(make_uptr<Session_sio>()); // The session being opened.
  auto n_init_chans_func = [this]
                             (const Client_app& app, size_t,
                              Mdt_reader_ptr&& mdt_from_cli) -> size_t
  {
    // Different # of init-channels-by-srv-request depending on Client_app.
    const size_t n = (app.m_name == S_CLI_NAME_2)
                       ? S_CLI_2_N_INIT_CHANS_US
                       : S_N_INIT_CHANS_A;
    /* For demo purposes only, in-mdt they set how many we will want... let's check 'em on that.
     * Note we get the in-mdt before the session is even ready. */
    ASSERT((n == mdt_from_cli->getPayload().getNumChansYouWant())
           && "Client should have magically known how many init-channels we want on our behalf.");
    FLOW_LOG_INFO("Client_app [" << app.m_name << "] opening session; for that app we shall open on our behalf "
                  "[" << n << "] init-channels; and indeed we have ensured client sent a metadata value "
                  "indicating it knew that, for fun.");
    return n;
  };
  auto mdt_load_func = [this]
                         (const Client_app& app, size_t n_init_channels_by_cli_req,
                          Mdt_reader_ptr&&, Mdt_builder* mdt_srv)
  {
    /* Different # of init-channels-by-cli-request depending on Client_app.
     * We do not have to pre-know this info at all; for contrived demo purposes we do and can check our
     * own mind-reading powers.  We will send it there in our mdt, and they can do the same. */
    mdt_srv->initPayload().setNumChansYouWant((app.m_name == S_CLI_NAME_2)
                                                ? S_CLI_2_N_INIT_CHANS_THEM : S_N_INIT_CHANS_B);
    ASSERT(mdt_srv->getPayload().getNumChansYouWant() == n_init_channels_by_cli_req);
    FLOW_LOG_INFO("Client_app [" << app.m_name << "] opening session; for that app we shall open on *their* behalf "
                  "[" << n_init_channels_by_cli_req << "] init-channels; and indeed we will have sent a metadata value "
                  "indicating we knew that, for fun.");
  };
  m_srv->async_accept(m_sessions.back().get(),
                      &m_init_chans_srv, &m_mdt_cli, &m_init_chans_cli,
                      std::move(n_init_chans_func), std::move(mdt_load_func),
                      [this](const Error_code& err_code)
  {
    if (err_code == session::error::Code::S_OBJECT_SHUTDOWN_ABORTED_COMPLETION_HANDLER)
    {
      // We are ending ourselves.  GTFO.  This is typical code for boost.asio-style async_*() operation_aborted.
      return;
    }

    post([this, err_code]()
    {
      if (err_code)
      {
        FLOW_LOG_WARNING("async_accept() failed; should succeed from now on; please check Flow-IPC logs for "
                         "details.  Usually this happens due to opposing ex_cli.exec running from the wrong CWD "
                         "or named wrong; but just check the logs.  Error emitted: "
                         "[" << err_code << "] [" << err_code.message() << "].");
        done_and_done(false);
        return;
      }
      // else if (!err_code):

      auto& sess = *(m_sessions.back());
      const auto& app = *(sess.core()->client_app());

      // sync_io setup junk.  Similar to m_srv setup.
      sess.replace_event_wait_handles([this]() -> auto
                                        { return Asio_waitable_native_handle(*(task_engine())); });
      sess.start_ops([this](Asio_waitable_native_handle* hndl_of_interest,
                            bool ev_of_interest_snd_else_rcv, Task_ptr&& on_active_ev_func)
                       { ev_wait(hndl_of_interest, ev_of_interest_snd_else_rcv, std::move(on_active_ev_func)); });

      if (app.m_name == S_CLI_NAME_2)
      {
        /* This one we treat lamely; mostly we do real stuff with S_CLI_NAME-guy which App_session handles (below).
         * So for S_CLI_NAME_2-guy just kludge a few things here and let it go.  Actually let it live; just don't
         * do anything further with it for now at least. */

        // This is the same thing as mdt_from_cli passed-to n_init_chans_func() earlier.
        ASSERT(m_mdt_cli->getPayload().getNumChansYouWant() == S_CLI_2_N_INIT_CHANS_THEM);
        ASSERT(m_init_chans_cli.size() == S_CLI_2_N_INIT_CHANS_THEM);
        ASSERT(m_init_chans_srv.size() == S_CLI_2_N_INIT_CHANS_US);
        FLOW_LOG_INFO("Client_app [" << app.m_name << "] session ready.  Cool; everything is as expected; but for this "
                      "Client_app we do not do much further.  "
                      "Note: It is actually in almost-PEER state (we have not done init_handlers() yet).  "
                      "Let us get it to PEER state for good measure.  Then we are just letting it sit there.");

        sess.init_handlers([this](const Error_code& err_code)
        {
          post([this, err_code]()
          {
            FLOW_LOG_INFO("(Idle quick-session) Oh good; session-end trigger from opposing side detected.  "
                          "Probably it's so-called EOF: [" << err_code << "] [" << err_code.message() << "].  "
                          "Whatever.");
          }); // post()
        });
      }
      else // if (app.m_name == S_CLI_NAME)
      {
        ASSERT(app.m_name == S_CLI_NAME);

        // Delegate further work for this type of Client_app to this class of ours.
        const unsigned int idx = m_app_sessions.size();
        m_app_sessions.emplace_back(make_uptr<App_session>(this, idx, [this, idx]()
        {
          /* Already in our thread W....  However, this code is being invoked as (App_session::m_i_am_done_func)(),
           * and our plan is to delete that App_session -- along with m_i_am_done_func.  At least clang's
           * ASAN (-fsanitize=address) complains/aborts just past the .reset() call causing that deletion.
           * However if we post() the deletion (and everything else following it) onto thread W as a separate task,
           * then m_i_am_done_func can be safely deleted along with its containing App_session (its *this).
           *
           * @todo In general this code (*waves at Ex_srv and Ex_cli*) is way too insane.  I (ygoldfel) tend to be
           * very deliberate and careful in production code itself but then get lax with test code; I really shouldn't
           * do that, at least not to this extent.  More specifically, all this code should be following most
           * best practices from (my own) guided Manual for Flow-IPC, regarding setup and teardown of sessions and
           * how to organize one's code nicely.  There's a whole manual page on this topic (to be fair, I wrote it
           * after this code). */
          post([this, idx]()
          {
            FLOW_LOG_INFO("App_session in slot [" << idx << "] reports it's done-for.  Deleting object from outside "
                          "its own code.");

            ASSERT(m_app_sessions[idx]);

            m_app_sessions[idx].reset();

            ASSERT(!m_app_sessions[idx]);

            /* This is completely contrived, but we happen to stop after 2 sessions (1st one tests most stuff; 2nd one
             * test the rest -- including reusing some ASSERTs intentionally reused from the 1st one). */
            if (m_app_sessions.size() != 2)
            {
              return;
            }
            // else

            FLOW_LOG_INFO("App_session in slot [" << idx << "] is the last expected one.  Ensuring all sessions, not "
                          "just this one, are done-for; if not it's a FAIL; if so then we quit nicely.");

            if (!std::all_of(m_app_sessions.begin(), m_app_sessions.end(),
                             [](const auto& app_session) { return !app_session; }))
            {
              FLOW_LOG_WARNING("One of the sessions is still alive (unexpected).  FAIL.");
              ASSERT(false);
              return;
            }
            // else

            FLOW_LOG_INFO("That's the last App_session to have been deleted.  Quitting.");
            done_and_done(true);
          }); // post()
        })); // App_session::ctor() i_am_done_func() body
      } // else if (app.m_name == S_CLI_NAME)

      /* Regardless -- listen for the next one.
       * But if we decide to die (and we will eventually), it'll be interrupted with
       * S_OBJECT_SHUTDOWN_ABORTED_COMPLETION_HANDLER (see above). */
      server_accept_loop();
    }); // post()
  }); // m_srv->async_accept()
} // CLASS::server_accept_loop()

TEMPLATE
template<typename Task>
CLASS::App_session::App_session(Ex_srv* guy, unsigned int test_idx, Task&& i_am_done_func) :
  flow::log::Log_context(guy->get_logger(), guy->get_log_component()),
  m_guy(guy), m_test_idx(test_idx),
  m_i_am_done_func(std::move(i_am_done_func)),
  m_ses(std::move(m_guy->m_sessions.back())), // Eat the raw Server_session<> into *this!
  m_chans_a(std::move(m_guy->m_init_chans_srv)), // Eat the 2 init-channel lists too.
  m_chans_b(std::move(m_guy->m_init_chans_cli))
{
  FLOW_LOG_INFO("App_session [" << this << "]: I am open!  Well, my session::Session [" << *m_ses << "] is open, and "
                "I have 2 init-channel sets sized [" << m_chans_a.size() << ", " << m_chans_b.size() << "] "
                "pre-opened too.  Let's check some things about it.");
  ASSERT(m_chans_a.size() == S_N_INIT_CHANS_A);
  ASSERT(m_chans_b.size() != 0);
  FLOW_LOG_INFO("Let's set up the error handler via init_handlers(), to officially get the Server_session into "
                "PEER-state (it is in almost-PEER state) -- which makes it just a plain Session.");

  auto on_err_func = [this](const Error_code& err_code)
  {
    m_guy->post([this, err_code]()
    {
      FLOW_LOG_INFO("App_session [" << this << "]: Oh good; session-end trigger from opposing side detected.  "
                    "Probably it's so-called EOF: [" << err_code << "] [" << err_code.message() << "].  "
                    "Indicating I am done and can be deleted.");
      m_i_am_done_func();
    }); // post()
  };

  /* We'll open some init-channels; but also after that will accept some manual passive-opens in the PEER-state session.
   * We'll also invoke some manual active-opens ourselves. */
  auto on_chan_open_func = [this](Channel_obj&& new_chan, Mdt_reader_ptr&& mdt_cli)
  {
    m_guy->post([this, new_chan_ptr = make_sptr<Channel_obj>(std::move(new_chan)),
                 mdt_cli = std::move(mdt_cli)]() mutable
                  { on_chan_open(std::move(*new_chan_ptr), mdt_cli); });
  };

  m_ses->init_handlers(std::move(on_err_func), std::move(on_chan_open_func));

  FLOW_LOG_INFO("Good!  Now let's active-open some type-A channels (and passive-open some type-B ones).");

  for (size_t idx = 0; idx != S_N_CHANS_A - S_N_INIT_CHANS_A; ++idx)
  {
    m_chans_a.emplace_back();

    auto mdt = m_ses->core()->mdt_builder();
    mdt->initPayload().initMdtOne().setOnePayload("A mEsSaGe -- s2c through MDT!");
    Error_code err_code_non_fatal;
    const bool ok = m_ses->core()->open_channel(&(m_chans_a.back()), mdt, &err_code_non_fatal);
    ASSERT(ok && "Error fired now or earlier -- unexpected.");
    ASSERT((!err_code_non_fatal) && "Non-fatal error emitted by open_channel(), but we expect total success.");

    FLOW_LOG_INFO("Cool, active-opened channel (to [" << m_chans_a.size() << "] type-A channels total): "
                  "[" << m_chans_a.back() << "].");
  }

  use_channels_if_ready();
} // CLASS::App_session::App_session()

TEMPLATE
void CLASS::App_session::on_chan_open(Channel_obj&& new_chan, const Mdt_reader_ptr& mdt_cli)
{
  m_chans_b.emplace_back(std::move(new_chan));

  const auto in_val = mdt_cli->getPayload().getMdtTwo().getTwoPayload();
  m_guy->check_scalar_set(in_val, "cli-mdt"); // Check it's not default-valued.

  FLOW_LOG_INFO("App_session [" << this << "]: Cool, passive-opened channel (to [" << m_chans_b.size() << "] "
                "type-B channels total): [" << m_chans_b.back() << "]; non-default val in MDT = "
                "[" << boost::lexical_cast<std::string>(in_val) << "].  "
                "Overall MDT contents: [" << kj::str(*mdt_cli).cStr() << "].");

  use_channels_if_ready();
} // CLASS::App_session::on_chan_open()

TEMPLATE
void CLASS::App_session::use_channels_if_ready()
{
  if ((m_chans_a.size() != S_N_CHANS_A) || (m_chans_b.size() != S_N_CHANS_B))
  {
    // Not there yet.
    return;
  }
  // else
  if (m_chans_a.empty() && m_chans_b.empty())
  {
    // Been here already.
    return;
  }
  // else: We're there for the first time.

  FLOW_LOG_INFO("App_session [" << this << "]: The desired total A/B channel counts ready: "
                "[" << S_N_CHANS_A << '/' << S_N_CHANS_B << "].  Upgrading them to structured-channels, "
                "each set at compile-time to use a different schema (type A or type B).");

  m_struct_chans_a.reserve(m_chans_a.size()); // Optimization.  For demonstration purposes... I guess.
  m_struct_chans_b.reserve(m_chans_b.size());

  /* Note: Use the ctor form as suggested by concept Session::Structured_channel<> doc header.  That's a good cheat
   * sheet.  What to do -- that cheat-sheet says -- depends on basically whether Session uses SHM versus not. */

  if constexpr(S_SHM_ENABLED)
  {
    for (auto& chan : m_chans_a)
    {
      /* Subtlety!  We will, later, be exercising the ability to reuse an out-message (resend it, maybe even modified).
       * It can be reused across different channels, definitely, no matter what (though with SHM, but not otherwise,
       * modifying it post-send will also modify the in-message on the other side) -- within the *same* session.
       * It can *also* be reused in a *different* session... BUT, in the case of SHM-enabled sessions, this is
       * only possible using the per-app-scope SHM arena, not the per-session-scope one.
       *
       * So for type-A channels we choose to:
       *   - use SHM to store out-messages, for speed;
       *   - use per-session-scope.
       *
       * For type-B below we do the same except per-app-scope (which is more advanced, allowing cross-session message
       * reuse). */
      m_struct_chans_a.emplace_back(make_uptr<Channel_a>(m_guy->m_ipc_logger, std::move(chan),
                                                         struc::Channel_base::S_SERIALIZE_VIA_SESSION_SHM,
                                                         m_ses->core()));
    }
    for (auto& chan : m_chans_b)
    {
      /* See above.  Use SHM but per-app-scope, so out-messages can be reused cross-session.
       * We will specifically save some messages and resend them via another App_session that is not *this. */
      m_struct_chans_b.emplace_back(make_uptr<Channel_b>(m_guy->m_ipc_logger, std::move(chan),
                                                         struc::Channel_base::S_SERIALIZE_VIA_APP_SHM,
                                                         m_ses->core()));
    }
  } // if constexpr(S_SHM_ENABLED)
  else // if constexpr(!S_SHM_ENABLED)
  {
    // No SHM available: Just keep out-messages in heap; and they're always copied when transmitting.
    for (auto& chan : m_chans_a)
    {
      m_struct_chans_a.emplace_back(make_uptr<Channel_a>(m_guy->m_ipc_logger, std::move(chan),
                                                         struc::Channel_base::S_SERIALIZE_VIA_HEAP,
                                                         m_ses->core()->session_token()));
    }
    for (auto& chan : m_chans_b)
    {
      m_struct_chans_b.emplace_back(make_uptr<Channel_b>(m_guy->m_ipc_logger, std::move(chan),
                                                         struc::Channel_base::S_SERIALIZE_VIA_HEAP,
                                                         m_ses->core()->session_token()));
    }
  } // else if constexpr(!S_SHM_ENABLED)

  // They're full of nulls anyway now.
  m_chans_a.clear();
  m_chans_b.clear();

  if (m_test_idx == 0)
  {
    use_channels_round_1();
  }
  else
  {
    use_channels_round_1a();
  }
} // CLASS::App_session::use_channels_if_ready()

TEMPLATE
void CLASS::App_session::use_channels_round_1()
{
  /* Here's a master list of stuff to exercise, before I forget.  # = tested.
   *   - Setting up expect_msg[s]() before start() (and receiving as a result -- at start() and after start()) #
   *     - and also after start() (and receiving as a result -- already-queued and after a bit) #
   *       - and also undo_expect_msgs() #
   *         - but then receive immediately on re-expect_msgs(). #
   *   - Specifically expect_msg[s](one) vs expect_msg[s](two). #
   *   - send()ing without expecting response. #
   *   - async_request()ing and expecting response (and receiving one), one-off #
   *     - and also not one-off
   *       - and also undo_expect_responses().
   *   - async_request()ing and expecting response, before start() (and receiving at start()).
   *   - sync_request() (with timeout)
   *     - successful
   *     - failed (due to error during wait)
   *     - timing out
   *     - and also without timeout
   *       - successful
   *       - failed (due to error during wait).
   *   - set_unexpected_response_handler()
   *     - and also set_remote_unexpected_response_handler()
   *     - and also unset_*().
   *   - Send huge thing (SHM-backed) #
   *     - and also less huge thing (non-SHM-backed). #
   *  - Send huge thing (SHM handle). #
   *  - Resend msg within same session. #
   *    - Same channel. #
   *      - Different channel. #
   *    - Modify msg itself (e.g., a scalar) (SHM-backed: should be reflected without resend).
   *    - Modify msg itself (e.g., a scalar), resend (should be reflected in new in-message). #
   *  - Resend msg in dif session (non-SHM-backed; SHM-backed per-app-scope; but not per-session-scope).
   *  - (SHM-backed) Send SHM-handle to multi-level STL structure. #
   *    - Modify it (should be reflected on other side). #
   *      - Modify on receiver side (SHM-classic allows; SHM-jemalloc does not) (should be reflected on sender side). #
   *    - session->lend_object()/borrow_object() versus #
   *      - session->*_shm()->lend_object()/borrow_object(). (SHM-classic.  SHM-jemalloc is diff but similar idea.) #
   *  - Auto-ping and liveness-check.
   *  - send() with Native_handle, possible race against one without Native_handle with dual pipes.
   * UPDATE: All the stuff without # is tested in use_channels_round_2().
   *
   * P.S. We also want to test advanced struc::Msg_out APIs:
   *  - 2nd/advanced ctor form (that subsumes a schema-less MessageBuilder).
   *    - Where it does initRoot<>() for us, because we did not by the time struc::Msg_out ctor called.
   *    - Where it does not, because we already did it ourselves.
   *  - Orphans (via struc::Msg_out itself).
   * These are opportunistically tested; as of this writing in Ex_cli. */

  /* We'll do a first round of parallel tests and ping on this reserved channel at the end of each.
   * Once enough pings are received they'll ping us back here to proceed to the next round of tests. */
  start_chan_b(S_RESERVED_CHAN_IDX);
  expect_ping_and_b(S_RESERVED_CHAN_IDX, [this]()
  {
    FLOW_LOG_INFO("App_session [" << this << "]: Special ping received.  Will proceed "
                  "to next big step.");
    use_channels_round_2();
  });

  /* We'll do a bunch of things in type-A channels.  We will switch-over to type-B channels when sending SHM
   * handles explicitly.  Roughly speaking for each related group of tests we will do something on channel 0,
   * something different on channel 1, etc., to possibly all 4 channels.
   *
   * Test group -- expect_msg[s](), undo_expect_msgs(), start().
   *
   * Channel 0: srv expect_msg(), cli send() notif x 2, srv wait, start(), immediately rcv 1,
   *            wait, expect_msg(), immediately rcv 2.
   *
   * Channel 1: srv expect_msgs(), cli send() notif x 2, srv wait, start(), immediately rcv 1 & 2,
   *            undo_expect_msgs(), ping cli, cli send() notif x 2, srv wait, expect_msgs(), immediately rcv 1 & 2.
   * ...continued below. */

  FLOW_LOG_INFO("App_session [" << this << "]: Chan A0: expecting 1-msg; not starting; delaying.");
  m_struct_chans_a[0]->expect_msg(capnp::ExBodyA::MSG, [this](Msg_in_ptr_a&& msg_in) mutable
  {
    m_guy->post([this, msg_in = std::move(msg_in)]() mutable
    {
      check_notif_payload_a(std::move(msg_in), 0); // Will --m_expectations_a[0].
      // Got message 1; message 2 should be queued but not yet received.

      FLOW_LOG_INFO("App_session [" << this << "]: "
                    "Chan A0: got 1-msg; another 1-msg is queued; not expecting yet; delaying.");

      m_guy->delay_and(Seconds(1), [this]() mutable
      {
        FLOW_LOG_INFO("App_session [" << this << "]: Chan A0: expecting another 1-msg; should come now.");

        ++m_expectations_a[0];
        m_struct_chans_a[0]->expect_msg(capnp::ExBodyA::MSG, [this](Msg_in_ptr_a&& msg_in) mutable
        {
          m_guy->post([this, msg_in = std::move(msg_in)]() mutable
          {
            check_notif_payload_a(std::move(msg_in), 0); // Will --m_expectations_a[0].

            FLOW_LOG_INFO("App_session [" << this << "]: Chan A0: All good.");

            ping_b(S_RESERVED_CHAN_IDX); // Test done!  Ping them on the reserved channel for this.
          }); // post()
        }); // expect_msg()
      }); // delay_and()
    }); // post()
  }); // expect_msg()
  // That shouldn't arrive yet though (we have not yet start()ed the channel).

  m_guy->delay_and(Seconds(2), [this]()
  {
    FLOW_LOG_INFO("App_session [" << this << "]: Chan A0: Starting (+ register err-handler); 1-msg should come now.");

    ++m_expectations_a[0];
    start_chan_a(0);
    // NOW the 1st expect_msg() handler should trigger.
  }); // delay_and()

  // --

  FLOW_LOG_INFO("App_session [" << this << "]: Chan A1: expecting 2x-msg; not starting; delaying.");
  m_struct_chans_a[1]->expect_msgs(capnp::ExBodyA::MSG, [this](Msg_in_ptr_a&& msg_in) mutable
  {
    m_guy->post([this, msg_in = std::move(msg_in)]() mutable
    {
      check_notif_payload_a(std::move(msg_in), 1); // Will --m_expectations_a[1].
      // Got message 1 or 2.

      if (m_expectations_a[1] != 0)
      {
        FLOW_LOG_INFO("App_session [" << this << "]: Chan A1: got 2x-msg 1/2; another should be rcv-ed *immediately*.");
        return;
      }
      // else

      FLOW_LOG_INFO("App_session [" << this << "]: Chan A1: got 2x-msg 2/2; un-expecting x-msg; "
                    "pinging client to send another 2x-msg; delaying while 2x-msg are queued.");
      m_struct_chans_a[1]->undo_expect_msgs(capnp::ExBodyA::MSG);
      ping_a(1);

      m_guy->delay_and(Seconds(2), [this]() mutable
      {
        FLOW_LOG_INFO("App_session [" << this << "]: Chan A1: expecting another 2x-msg; should come now.");

        m_expectations_a[1] += 2;
        m_struct_chans_a[1]->expect_msgs(capnp::ExBodyA::MSG, [this](Msg_in_ptr_a&& msg_in) mutable
        {
          m_guy->post([this, msg_in = std::move(msg_in)]() mutable
          {
            check_notif_payload_a(std::move(msg_in), 1); // Will --m_expectations_a[1].

            if (m_expectations_a[1] != 0)
            {
              FLOW_LOG_INFO("App_session [" << this << "]: "
                            "Chan A1: got 2x-msg 1/2; another should be rcv-ed *immediately*.");
              return;
            }
            // else
            FLOW_LOG_INFO("App_session [" << this << "]: "
                          "Chan A1: got 2x-msg 2/2; all good; un-expecting x-msg.");
            m_struct_chans_a[1]->undo_expect_msgs(capnp::ExBodyA::MSG);

            ping_b(S_RESERVED_CHAN_IDX); // Test done!  Ping them on the reserved channel for this.
          }); // post()
        }); // expect_msgs()
      }); // delay_and()
    }); // post()
  }); // expect_msgs()
  // That shouldn't arrive yet though (we have not yet start()ed the channel).

  m_guy->delay_and(Seconds(2), [this]()
  {
    FLOW_LOG_INFO("App_session [" << this << "]: Chan A1: Starting (+ register err-handler); 2x-msg should come now.");

    m_expectations_a[1] += 2;
    start_chan_a(1);
    // NOW the 1st expect_msgs() handler should trigger x 2.
  }); // delay_and()

  // --

  /* ...test group continued -- expect_msg[s](), undo_expect_msgs(), start().
   *
   * Channel 2: start(), expect_msg(one), expect_msgs(two), ping cli, cli send(one), cli send(two) x 2, srv
   *            receive all, undo_expect_msgs().
   *
   * Channel 3: start(), expect_msgs(one), expect_msg(two), ping cli, cli send(one) x 2, cli send(two), srv
   *            receive all, undo_expect_msgs().
   *
   * Let's write them both out in parallel. */

  FLOW_LOG_INFO("App_session [" << this << "]: Chans A2+3: Starting (+ register err-handler); "
                "A2: expecting 1-msg(type 1), 2x-msg(type 2); A3: expecting 2x-msg(type 1), 1-msg(type 2).");

  start_chan_a(2);
  start_chan_a(3);
  m_expectations_a[2] += 3; // One x 1, Two x 2.
  m_expectations_a[3] += 3; // One x 2, Two x 1.

  m_struct_chans_a[2]->expect_msg(capnp::ExBodyA::MSG, [this](Msg_in_ptr_a&& msg_in) mutable
  {
    m_guy->post([this, msg_in = std::move(msg_in)]() mutable
    {
      check_notif_payload_a(std::move(msg_in), 2, false); // false => type 1.
      FLOW_LOG_INFO("App_session [" << this << "]: Chan A2: got 1-msg(type 1); this should be it.");
    }); // post()
  }); // m_struct_chans_a[2]->expect_msg()
  m_struct_chans_a[2]->expect_msgs(capnp::ExBodyA::MSG_TWO, [this](Msg_in_ptr_a&& msg_in) mutable
  {
    m_guy->post([this, msg_in = std::move(msg_in)]() mutable
    {
      check_notif_payload_a(std::move(msg_in), 2, true); // true => type 2.
      FLOW_LOG_INFO("App_session [" << this << "]: Chan A2: got 2x-msg(type 1).");
      if (m_expectations_a[2] == 0)
      {
        FLOW_LOG_INFO("App_session [" << this << "]: Seems this test passed; undoing expect-msgs.");
        m_struct_chans_a[2]->undo_expect_msgs(capnp::ExBodyA::MSG_TWO);

        ping_b(S_RESERVED_CHAN_IDX); // Test done!  Ping them on the reserved channel for this.
      }
    }); // post()
  }); // m_struct_chans_a[2]->expect_msg()

  m_struct_chans_a[3]->expect_msgs(capnp::ExBodyA::MSG, [this](Msg_in_ptr_a&& msg_in) mutable
  {
    m_guy->post([this, msg_in = std::move(msg_in)]() mutable
    {
      check_notif_payload_a(std::move(msg_in), 3, false); // false => type 1.
      FLOW_LOG_INFO("App_session [" << this << "]: Chan A3: got 2x-msg(type 1).");
    }); // post()
  }); // m_struct_chans_a[3]->expect_msg()
  m_struct_chans_a[3]->expect_msg(capnp::ExBodyA::MSG_TWO, [this](Msg_in_ptr_a&& msg_in) mutable
  {
    m_guy->post([this, msg_in = std::move(msg_in)]() mutable
    {
      check_notif_payload_a(std::move(msg_in), 3, true); // true => type 2.
      FLOW_LOG_INFO("App_session [" << this << "]: Chan A3: got 1-msg(type 1); this should be it.");
      FLOW_LOG_INFO("App_session [" << this << "]: Seems this test passed; undoing expect-msgs.");
      m_struct_chans_a[3]->undo_expect_msgs(capnp::ExBodyA::MSG);

      ping_b(S_RESERVED_CHAN_IDX); // Test done!  Ping them on the reserved channel for this.
    }); // post()
  }); // m_struct_chans_a[2]->expect_msg()

  // Go!
  ping_a(2);
  ping_a(3);

  // --

  if constexpr(S_SHM_ENABLED)
  {
    start_chan_b(0);
    start_chan_b(1);

    /* This sequence tests a bunch of SHM-handle-sending-and-reusing and out-message-reusing stuff fairly
     * economically.  Send message with big SHM-stored STL structure; get ack reply; reuse msg but with
     * a new structure (get ack...); new message but send same structure after modifying it (both additively
     * and subtractively for fun at the same time); reuse both the message and the (again modified)
     * structure; finally same thing... except don't even send the handle and make client check its own saved
     * handle -- since the structure was changed in-place!  Lastly use a different form of lend/borrow_object().
     *
     * Lastly reuse message (and STL structure for good measure) one more time but over another channel (the above
     * are all over channel B0, so that one is over B1).  It demonstrates the out-message is independent of
     * the channel; it is a container of sorts. */
    send_req_b(0, "baseline send of SHM-handle to big STL structure",
               false, false, false, false, false, [this]()
    {
      send_req_b(0, "new message + SHM-handle to modified (unless SHM-jemalloc) existing STL data",
                 false, true, false, false, false, [this]()
      {
        send_req_b(0, "reuse out-message + SHM-handle to new STL data",
                   true, false, false, false, false, [this]()
        {
          send_req_b(0, "reuse out-message + SHM-handle to modified (unless SHM-jemalloc) existing STL data",
                     true, true, false, false, false, [this]()
          {
            send_req_b(0, "in-place modification (unless SHM-jemalloc) w/o sending SHM-handle",
                       true, true, true, false, false, [this]()
            {
              send_req_b(0, "use Arena::lend/borrow instead of Session::lend/borrow",
                         false, true, false, true, false, [this]()
              {
                send_req_b(1, "reuse out-message + SHM-handle: send over dif channel",
                           true, true, false, true, false, [this]()
                {
                  send_req_b(0, "baseline send of SHM-handle to big STL structure (to save for session 2)",
                             false, false, false, false, false, [this]()
                  {
                    ping_b(S_RESERVED_CHAN_IDX); // Test done!  Ping them on the reserved channel for this.
                  });
                });
              });
            });
          });
        });
      });
    });
  } // if constexpr(S_SHM_ENABLED)
  else
  {
    ping_b(S_RESERVED_CHAN_IDX); // Skipped SHM tests -- indicate "done."
  }
} // CLASS::App_session::use_channels_round_1()

TEMPLATE
void CLASS::App_session::use_channels_round_2()
{
  FLOW_LOG_INFO("App_session [" << this << "]: Tests round two begins.");

  /* All right... here's what's left to test, from the master list in use_channels_if_ready().
   *   - async_request()ing and expecting response (and receiving 2+), not one-off #
   *     - and also undo_expect_responses(). #
   *   - async_request()ing and expecting response, before start() (and receiving at start()).
   *   - set_unexpected_response_handler() #
   *     - and also set_remote_unexpected_response_handler() #
   *     - and also unset_*(). #
   *   - Resend msg in dif session (non-SHM-backed; SHM-backed per-app-scope; but not per-session-scope).
   *   - sync_request() (with timeout)
   *     - successful
   *     - failed (due to error during wait)
   *     - timing out
   *     - and also without timeout
   *       - successful
   *       - failed (due to error during wait).
   *  - Auto-ping and liveness-check.
   *  - send() with Native_handle, possible race against one without Native_handle with dual pipes.
   * # = tested. */

  /* Let's get to it (this tests up to, not including, "Resend msg in dif session..." above):
   * Channel 0: srv expect_msg(), set_unexpected_response_handler(), async_request() req w/ response handler
                (not one-off), cli send() response x 2 and identical notif x 1, srv receives all 3 in handlers,
   *            undo_expect_responses(), ping cli, cli send() response x 2 and identical notif x 1,
   *            srv unexpected-response handler fires x 2, expect_msg() handler fires.
   *            - Also: cli will set_remote_unexpected_response_handler() and be informed of its own malfeasance. */

  FLOW_LOG_INFO("App_session [" << this << "]: Chan A0: expecting 1-msg.");
  ++m_expectations_a[0]; // Once we send the request shortly we'll expect response x 2 but also this just because.
  m_struct_chans_a[0]->expect_msg(capnp::ExBodyA::MSG_TWO, [this](Msg_in_ptr_a&& msg_in) mutable
  {
    m_guy->post([this, msg_in = std::move(msg_in)]() mutable
    {
      check_notif_payload_a(std::move(msg_in), 0, true); // Will --m_expectations_a[0].

      FLOW_LOG_INFO("App_session [" << this << "]: "
                    "Chan A0: got 1-msg -- *not* a response.  Response x 2 should follow immediately.");
    }); // post()
  }); // expect_msg()

  FLOW_LOG_INFO("App_session [" << this << "]: Chan A0: setting unexpected-response handler.");
  m_struct_chans_a[0]->set_unexpected_response_handler([this](Msg_in_ptr_a&& msg_in_bad_boy) mutable
  {
    m_guy->post([this, msg_in_bad_boy = std::move(msg_in_bad_boy)]() mutable
    {
      FLOW_LOG_INFO("App_session [" << this << "]: "
                    "Chan A0: unexpected-response handler fired!  Will check payload of the malfeasant in-msg "
                    "(why not?).");
      check_notif_payload_a(std::move(msg_in_bad_boy), 0, true); // Will --m_expectations_a[0].

      if (m_expectations_a[0] == 0)
      {
        FLOW_LOG_INFO("App_session [" << this << "]: That's the 2nd unexpected response.  "
                      "Seems this test passed; undoing set-unexpected-response-handler.");
        m_struct_chans_a[0]->unset_unexpected_response_handler();

        /* Await one last ping, so they get their remote-unexpected-response-handler firing and tell us we can
         * move on. */
        m_struct_chans_a[0]->expect_msg(capnp::ExBodyA::MSG_TWO, [this](Msg_in_ptr_a&&) mutable
        {
          m_guy->post([this]() { ping_b(S_RESERVED_CHAN_IDX); });// Test done!  Ping 'em on reserved channel for this.
        });
      } // if (m_expectations_a[0] == 0)
    }); // post()
  }); // set_unexpected_response_handler()
  // That shouldn't arrive yet though (we have not yet pinged client to have them send unexpected rsp).

  m_expectations_a[0] += 2; // We shall expect 2 responses (following the notif; see above).
  send_req_a(0, [this](Msg_in_ptr_a&& rsp_in)
  {
    check_notif_payload_a(std::move(rsp_in), 0, true); // Will --m_expectations_a[0].

    FLOW_LOG_INFO("App_session [" << this << "]: "
                  "Chan A0: Got response.  There should be 2 total which should follow the notif.");

    if (m_expectations_a[0] == 0)
    {
      FLOW_LOG_INFO("App_session [" << this << "]: That's the 2nd response.  "
                    "Undo expect-responses; expect 1-msg again; ping cli to induce notif + 2 unexpected responses.");

      m_struct_chans_a[0]->undo_expect_responses(m_saved_req_id_out);
      m_expectations_a[0] += 2; // Expect 2 bad-boy responses now.

      ++m_expectations_a[0]; // And another 1-msg notif.
      m_struct_chans_a[0]->expect_msg(capnp::ExBodyA::MSG_TWO, [this](Msg_in_ptr_a&& msg_in) mutable
      {
        m_guy->post([this, msg_in = std::move(msg_in)]() mutable
        {
          check_notif_payload_a(std::move(msg_in), 0, true); // Will --m_expectations_a[0].

          FLOW_LOG_INFO("App_session [" << this << "]: "
                        "Chan A0: got 1-msg -- *not* a response.  Bad response x 2 should follow immediately.");
        }); // post()
      }); // expect_msg()

      ping_a(0); // Go!  The test will finish once the 2nd bad-boy response arrives (handler higher-up).
    } // if (m_expectations_a[0] != 0)
  }); // send_req_a()
} // CLASS::App_session::use_channels_round_2()

TEMPLATE
void CLASS::App_session::use_channels_round_1a()
{
  FLOW_LOG_INFO("App_session [" << this << "]: Test round (for session 2) begins.");

  /* All right... here's what's left to test, from the master list in use_channels_if_ready(). &&&
   *   - send() with Native_handle, possible race against one without Native_handle with dual pipes.
   *   - sync_request() (with timeout) ***
   *     - successful
   *     - failed (due to error during wait)
   *     - timing out
   *     - and also without timeout
   *       - successful
   *       - failed (due to error during wait).
   *   - Resend msg in dif session (non-SHM-backed; SHM-backed per-app-scope; but not per-session-scope). ###
   *   - Auto-ping and liveness-check. */

  /* 1st part of the tests, as in use_channels_round_[12](), is concerned with not-specifically-SHM topics.
   * We do the *** and &&& parts then (sync_request() and Native_handle tests respectively).
   * @todo Looking at this a couple months later I (ygoldfel) don't really understand this comment.  Rewrite. */

  /* In each sub-test we expect one of the following sequences:
   * Success-case:
   *   - We sync_request(M, timeout=N).
   *   - It succeeds in time, because client is coded to respond before N elapses.
   *     - We ensure that indeed occurred.
   *     - We pause a bit, just to ensure they've registered for the next one (cheesy but OK for testing).
   *     - Go to next one if any.
   *
   * Timeout-case:
   *   - We sync_request(M, timeout=N).
   *   - It times out, because client is coded to respond but T time after N elapses.
   *     - We ensure that indeed occurred.
   *     - We pause a bit, just to ensure they've registered for the next one (cheesy but OK for testing).
   *     - Go to next one if any.
   *
   * Error-before-timeout-case:
   *   - We sync_request(M, timeout=N).
   *   - It fails in time, because client is coded to close channel before N elapses (thus forcing error for us).
   *     - We ensure that indeed occurred.
   *     - We destroy channel object on our end too (it is hosed anyway).
   *     - Ping-B to signal end of test. */

  /* expect_timeout==true => Timeout-case.  (task_else_kill must not be empty)
   * Else: task_else_kill.empty() => Error-before-timeout-case
   *       Else: Success-case. */
  auto test_and = [this](size_t chan_idx, Seconds timeout, bool expect_timeout,
                         Void_func&& task_else_kill, Void_func&& kill_and = Void_func()) mutable
  {
    FLOW_LOG_INFO("App_session [" << this << "]: Chan A[" << chan_idx << "]: Sending sync_request(); "
                  "timeout [" << timeout << "]; then "
                  "[" << (expect_timeout
                            ? "expecting sync-timeout; then continue"
                            : ((!task_else_kill.empty())
                                 ? "expecting success in time; then continue"
                                 : "expecting channel-closed error in time; then done")) << "].");
    auto& chan = *m_struct_chans_a[chan_idx];

    auto req = chan.create_msg();
    req.body_root()->initMsgTwo(0); // It'll be expecting this union-which; otherwise don't worry about the payload.

    Error_code err_code;
    const auto rsp
      = (timeout == Seconds::max())
          ? chan.sync_request(req, nullptr, &err_code)
          : chan.sync_request(req, nullptr, timeout, &err_code);
    ASSERT(rsp || err_code);

    if (expect_timeout)
    {
      ASSERT(!task_else_kill.empty());

      ASSERT((err_code == error::Code::S_TIMEOUT) && "Expected timeout -- did not get it.");
      FLOW_LOG_INFO("App_session [" << this << "]: Chan A[" << chan_idx << "]: "
                    "Got timeout as expected.  Continuing to next thing after short delay.");

      m_guy->delay_and(Seconds(1), [task_else_kill = std::move(task_else_kill)]
      {
        task_else_kill();
      });
      return;
    } // if (expect_timeout)
    // else:

    if (!task_else_kill.empty())
    {
      ASSERT((!err_code) && "Expected success but got something else.");
      FLOW_LOG_INFO("App_session [" << this << "]: Chan A[" << chan_idx << "]: "
                    "Got successful ack as expected.  Continuing to next thing after short delay.");

      m_guy->delay_and(Seconds(1), [task_else_kill = std::move(task_else_kill)]
      {
        task_else_kill();
      });
      return;
    }
    // else if (task_else_kill.empty())
    ASSERT(!kill_and.empty());

    ASSERT((err_code == error::Code::S_RECEIVES_FINISHED_CANNOT_RECEIVE)
           && "Expected channel-ended failure within timeout; got something else.");
    FLOW_LOG_INFO("App_session [" << this << "]: Chan A[" << chan_idx << "]: "
                  "Got channel-ended error as expected.  "
                  "Closing hosed channel object; signaling client sub-test done.");
    m_struct_chans_a[chan_idx].reset();

    kill_and();
  }; // auto test_and =

  start_chan_a(0);
  start_chan_a(1);

  /* Test handle-sending including races between handle-bearing and non-handle-bearing messages in
   * 2-pipe channel. */
  {
    auto& chan = *m_struct_chans_a[0];
    const int N = 100;
    const auto seed = static_cast<unsigned int>(flow::util::time_since_posix_epoch().count());
    flow::util::Rnd_gen_uniform_range rng(seed, 0, 1);

    boost::asio::readable_pipe rdr(*(m_guy->task_engine()));
    boost::asio::writable_pipe wrt(*(m_guy->task_engine()));
    Error_code sys_err_code;
    boost::asio::connect_pipe(rdr, wrt, sys_err_code);
    if (sys_err_code)
    {
      FLOW_LOG_WARNING("Connect-pipe failed.  Details follow.");
      FLOW_ERROR_SYS_ERROR_LOG_WARNING();
      ASSERT(false && "Connect-pipe failed.");
      return;
    }
    // else

    FLOW_LOG_INFO("App_session [" << this << "]: Going to send random patterns of [" << N << "] msgs, each with a "
                  "50/50 probability of containing a pipe-writer Native_handle.  Seed (reuse if probably seen "
                  "but not reproducible) = [" << seed << "].");
    std::vector<int> exp_back_ids;
    for (int id = 0; id != N; ++id)
    {
      const auto load_hndl = bool(rng());
      FLOW_LOG_INFO("App_session [" << this << "]: Chan A0: "
                    "Send msg ID [" << id << "]; with handle? = [" << load_hndl << "].");
      auto msg = chan.create_msg();
      msg.body_root()->setMsgHandle(int32_t(id));

      Native_handle hndl;
      if (load_hndl)
      {
        /* Subtlety: We are reusing the same handle-writer again and again, but the other side doesn't know that;
         * it just receives an FD, writes through it, and closes it.  This would widow the pipe and make it unusable
         * at best.  By sending a ::dup() we raise the ref-count of FDs referring to the writer and keep said writer
         * alive and the pipe un-widowed. */
        hndl = Native_handle(::dup(wrt.native_handle()));
        ASSERT((hndl.m_native_handle != -1) && "Ran out of FDs or something?!");

        exp_back_ids.push_back(id);
      }
      // else { Store no handle (no-op); expect no such ID to be bounced back to us via pipe. }
      msg.store_native_handle_or_null(std::move(hndl));

      Error_code err_code;
      bool ok = chan.send(msg, nullptr, &err_code);
      FLOW_LOG_INFO("App_session [" << this << "]: Chan A0: Sent.");
      ASSERT(ok && "send() yielded false, meaning error emitted earlier by chan.");
      if (err_code)
      {
        FLOW_LOG_WARNING("send() yielded error code [" << err_code << "] [" << err_code.message() << "].  FAIL.");
        ASSERT(false);
      }

      /* msg destroyed here.  msg-stored FD gets closed, but not before its doppelganger is sent to other process;
       * and meanwhile `wrt` stays alive throughout, as it was dup()ed. */
    } // for (int id = 0; id != N; ++id)

    FLOW_LOG_INFO("App_session [" << this << "]: Verifying the handle-paired IDs got bounced back to us via "
                  "associated pipe in the same order as we sent them.  If this blocks at some point, there is a bug.");
    for (int exp_id : exp_back_ids)
    {
      Error_code sys_err_code;
      uint8_t payload;
      const bool ok = rdr.read_some(util::Blob_mutable(&payload, 1), sys_err_code) == 1;

      if (sys_err_code || (!ok))
      {
        FLOW_LOG_WARNING("Pipe sync-read failed; this should never happen.  Details follow.");
        FLOW_ERROR_SYS_ERROR_LOG_WARNING();
        ASSERT(false && "Pipe-read bug?");
        return;
      }
      // else

      FLOW_LOG_INFO("Expecting ID [" << exp_id << "]; got ID [" << int(payload) << "].");
      ASSERT(exp_id == int(payload));
    } // for (int exp_id : exp_back_ids)

    FLOW_LOG_INFO("App_session [" << this << "]: All good.  Closing pipe; moving on.");
  }

  /* Channel A0: [Send req, timeout 4s, cli wait 2s, success], [Send req, timeout 1s, cli wait 2s, timeout],
   *             [Send req, timeout 1s, cli wait 0s, success],
   *             [Send req, timeout 4s, cli wait 2s, dead channel]. */
  test_and(0, Seconds(4), false, [this, test_and]() mutable
  {
    test_and(0, Seconds(1), true, [this, test_and]() mutable
    {
      test_and(0, Seconds(1), false, [this, test_and]() mutable
      {
        test_and(0, Seconds(4), false, Void_func(), [this, test_and]() mutable
        {
          /* Channel A1: [Send req, inf timeout, cli wait 2s, success],
           *             [Send req, inf timeout, cli wait 0s, success],
           *             [Send req, inf timeout, cli wait 2s, dead channel]. */
          test_and(1, Seconds::max(), false, [this, test_and]() mutable
          {
            test_and(1, Seconds::max(), false, [this, test_and]() mutable
            {
              test_and(1, Seconds::max(), false, Void_func(), [this]() mutable
              {
                ping_b(S_RESERVED_CHAN_IDX);

                /* Normally we'd just do this in parallel with all of those test_and()s, but they do blocking waits
                 * (which is not yet seen prior to this point; everything has been non-blocking) which will interfere
                 * with use_channels_round_2a()'s non-blocking-once-again nature. */
                use_channels_round_2a();
              });
            });
          });
        });
      });
    });
  });
} // CLASS::App_session::use_channels_round_1a()

TEMPLATE
void CLASS::App_session::use_channels_round_2a()
{
  // 2nd part of the tests, as in use_channels_round_[12](), is concerned with SHM stuff (### part above).
  if constexpr(S_SHM_ENABLED)
  {
    start_chan_b(0);
    start_chan_b(1);

    /* Repeat some similar tests from use_channels_round_2(); but the key is we start off with a message and
     * STL structure left-over from another session. */
    send_req_b(0, "reuse out-message + SHM-handle to modified (unless SHM-jemalloc) existing STL data",
               true, true, false, false, false, [this]()
    {
      send_req_b(0, "in-place modification w/o sending SHM-handle",
                 true, true, true, false, false, [this]()
      {
        send_req_b(0, "use Arena::lend/borrow instead of Session::lend/borrow",
                   false, true, false, true, false, [this]()
        {
          send_req_b(1, "reuse out-message + SHM-handle: send over dif channel",
                     true, true, false, true, false, [this]()
          {
            ping_b(S_RESERVED_CHAN_IDX); // Test done!  Ping them on the reserved channel for this.
          });
        });
      });
    });
  } // if constexpr(S_SHM_ENABLED)
  else
  {
    m_guy->delay_and(Seconds(1), [this]() mutable
    {
      ping_b(S_RESERVED_CHAN_IDX); // Skipped SHM tests -- indicate "done."
    });
  }

  {
    FLOW_LOG_INFO("App_session [" << this << "]: Testing auto-ping/liveness-check.  Opening 2 channels, X with "
                  "auto-pings to us, Y without.  Enabling liveness-check on the in-pipe.");

    /* And lastly test auto-pings and liveness.  Open 2 channels: one with auto-pings to us, one without.
     * Then wait a while and try to use channel. */
    Channel_obj x_sio, y_sio;
    Sptr<typename Channel_obj::Async_io_obj> x, y;
    x.reset(new typename Channel_obj::Async_io_obj);
    y.reset(new typename Channel_obj::Async_io_obj);

    auto mdt = m_ses->core()->mdt_builder();
    mdt->initPayload().initMdtOne().setOnePayload("A mEsSaGe -- s2c through MDT!");
    mdt->getPayload().setAutoPing(true);
    Error_code err_code_non_fatal;
    bool ok = m_ses->core()->open_channel(&x_sio, mdt, &err_code_non_fatal);
    ASSERT(ok && "Error fired now or earlier -- unexpected.");
    ASSERT((!err_code_non_fatal) && "Non-fatal error emitted by open_channel(), but we expect total success.");
    const Seconds TIMEOUT(5);
    *x = x_sio.async_io_obj();
    x->idle_timer_run(TIMEOUT);

    mdt = m_ses->core()->mdt_builder();
    mdt->initPayload().initMdtOne().setOnePayload("A mEsSaGe -- s2c through MDT!");
    mdt->getPayload().setAutoPing(false);
    ok = m_ses->core()->open_channel(&y_sio, mdt, &err_code_non_fatal);
    ASSERT(ok && "Error fired now or earlier -- unexpected.");
    ASSERT((!err_code_non_fatal) && "Non-fatal error emitted by open_channel(), but we expect total success.");
    *y = y_sio.async_io_obj();
    y->idle_timer_run(TIMEOUT);

    FLOW_LOG_INFO("Opened.");

    using Dummy = boost::array<uint8_t, 8192>;
    Sptr<Dummy> dummy(new Dummy);
    Sptr<Native_handle> dummy2(new Native_handle);

    FLOW_LOG_INFO("Channel Y [" << *y << "]: Should be dead due to no auto-pings once idle-timeout period passes.  "
                  "Trying async-receive (for each channel pipe); expecting idle-timeout error in a few seconds.");
    const auto started_at = flow::Fine_clock::now();
    y->async_receive_blob(util::Blob_mutable(dummy.get(), sizeof(Dummy)),
                          [this, y, dummy, TIMEOUT, started_at]
                            (const Error_code& err_code, size_t) mutable
    {
      /* Mark it now; we might be doing horrible SHM-related blocking computations (filling/checking giant structured)
       * in our thread W which tends to delay the present test's tasks in thread W. */
      const auto elapsed = flow::Fine_clock::now() - started_at;
      m_guy->post([this, TIMEOUT, elapsed, err_code]() mutable
      {
        ASSERT((elapsed > (TIMEOUT - Seconds(1))) && "Timeout fired too early?"); // Leave some room.

        ASSERT((err_code == error::Code::S_RECEIVER_IDLE_TIMEOUT)
               && "Channel Y receive: Should yield idle-timeout.");
        FLOW_LOG_INFO("Channel Y receive(pipe1) got idle-timeout as expected; timeout = [" << TIMEOUT << "]; "
                      "elapsed = [" << boost::chrono::round<boost::chrono::milliseconds>(elapsed) << "].");
        ping_b(S_RESERVED_CHAN_IDX);
      }); // post()
    }); // y->async_receive_blob()
    y->async_receive_native_handle(dummy2.get(), util::Blob_mutable(dummy.get(), sizeof(Dummy)),
                                   [this, y, dummy, dummy2, TIMEOUT, started_at]
                                     (const Error_code& err_code, size_t) mutable
    {
      // Mark it now (see above cmnt).
      const auto elapsed = flow::Fine_clock::now() - started_at;
      m_guy->post([this, TIMEOUT, elapsed, err_code]() mutable
      {
        ASSERT((elapsed > (TIMEOUT - Seconds(1))) && "Timeout fired too early?"); // Leave some room.

        ASSERT((err_code == error::Code::S_RECEIVER_IDLE_TIMEOUT)
               && "Channel Y receive(pipe2): Should yield idle-timeout.");
        FLOW_LOG_INFO("Channel Y receive(pipe2) got idle-timeout as expected; timeout = [" << TIMEOUT << "]; "
                      "elapsed = [" << boost::chrono::round<boost::chrono::milliseconds>(elapsed) << "].");
        ping_b(S_RESERVED_CHAN_IDX);
      }); // post()
    }); // y->async_receive_native_handle()

    FLOW_LOG_INFO("Channel X [" << *x << "]: Should stay alive due to auto-pings.  Trying async-receive x 2, waiting "
                  "a bit (then deleting channel; async-receive should yield operation-aborted at that time).");
    x->async_receive_blob(util::Blob_mutable(dummy.get(), sizeof(Dummy)),
                                             [this, dummy, TIMEOUT, started_at]
                                               (const Error_code& err_code, size_t) mutable
    {
      // Mark it now (see above cmnt).
      const auto elapsed = flow::Fine_clock::now() - started_at;
      m_guy->post([this, TIMEOUT, elapsed, err_code]() mutable
      {
        ASSERT((err_code == error::Code::S_OBJECT_SHUTDOWN_ABORTED_COMPLETION_HANDLER)
               && "Channel X receive(pipe1): should yield operation-aborted.");

        ASSERT((elapsed > TIMEOUT) && "Why did (pipe1) abort occur before we nullified channel X?");

        FLOW_LOG_INFO("Channel X receive(pipe1) got operation-aborted as expected.");
        ping_b(S_RESERVED_CHAN_IDX);
      }); // post()
    }); // x->async_receive_blob()
    x->async_receive_native_handle(dummy2.get(), util::Blob_mutable(dummy.get(), sizeof(Dummy)),
                                   [this, dummy, dummy2, TIMEOUT, started_at]
                                     (const Error_code& err_code, size_t) mutable
    {
      // Mark it now (see above cmnt).
      const auto elapsed = flow::Fine_clock::now() - started_at;
      m_guy->post([this, TIMEOUT, elapsed, err_code]() mutable
      {
        ASSERT((err_code == error::Code::S_OBJECT_SHUTDOWN_ABORTED_COMPLETION_HANDLER)
               && "Channel X receive(pipe2): should yield operation-aborted.");

        ASSERT((elapsed > TIMEOUT) && "Why did (pipe2) abort occur before we nullified channel X?");

        FLOW_LOG_INFO("Channel X receive(pipe2) got operation-aborted as expected.");
        ping_b(S_RESERVED_CHAN_IDX);
      }); // post()
    }); // x->async_receive_native_handle()

    m_guy->delay_and(TIMEOUT + Seconds(1), [this, x, y]() mutable
    {
      FLOW_LOG_INFO("After timeout passed:");
      FLOW_LOG_INFO("Channel X [" << *x << "]: Should be alive due to auto-pings.  Deleting channel; "
                    "async-receive should yield operation-aborted very soon.");
      x.reset();

      FLOW_LOG_INFO("Channel Y [" << *y << "]: Nullifying; but receive should have already "
                    "failed with idle-timeout error code.");
      y.reset();
      ping_b(S_RESERVED_CHAN_IDX);
    }); // delay_and()
  }
} // CLASS::App_session::use_channels_round_2a()

TEMPLATE
template<typename On_msg_handler>
void CLASS::App_session::send_req_b(size_t chan_idx, util::String_view ctx, bool reuse_msg, bool reuse_shm_payload,
                                    bool omit_handle,
                                    bool session_lend_else_arena_lend, bool alt, On_msg_handler&& on_msg_func)
{
  if constexpr(S_SHM_ENABLED)
  {
    using Shm_t = Shm_traits<Session, true>;

    auto& chan = *m_struct_chans_b[chan_idx];
    FLOW_LOG_INFO("App_session [" << this << "]: Chan B[" << chan_idx << "]: "
                  "Filling/send()ing payload (description = [" << ctx << "]; alt-payload? = [" << alt << "]; "
                  "reusing msg? = [" << reuse_msg << "]; reusing SHM payload? = [" << reuse_shm_payload << "]).");

    auto msg_out = reuse_msg ? std::move(m_guy->m_saved_msg) : chan.create_msg();
    msg_out.body_root()->setDescription(std::string(ctx));
    if (alt)
    {
      msg_out.body_root()->setMsgTwo(reuse_msg ? (msg_out.body_root()->getMsgTwo() + 1)
                                               : 1776);
    }
    else // if (!alt)
    {
      constexpr size_t SZ = 32;
      constexpr char FILLER_CH = 'X';
      constexpr char END_CAP_CH = 'A';

      // Make message reusable even in other sessions (vs session_shm()).
      typename Shm_t::Arena_activator arena_ctx(m_ses->core()->app_shm());

      // Test setting up a value on stack but allocating its internals in SHM.  We'll then move it altogether into SHM.
      typename Shm_t::Payload payload;
      {
        typename Shm_t::Shm_string str(SZ, FILLER_CH);
        str.front() = str.back() = END_CAP_CH;

        typename Shm_t::Shm_blob blob(SZ);
        std::memset(blob.data(), FILLER_CH, blob.size());
        blob.front() =  blob.back() = uint8_t(END_CAP_CH);

        typename Shm_t::Shm_sharing_blob pool(SZ);
        std::memset(pool.data(), FILLER_CH, pool.size());
        pool.front() = pool.back() = uint8_t(END_CAP_CH);

        payload.m_str = std::move(str);
        payload.m_blob = std::move(blob);
        payload.m_pool = std::move(pool);
        payload.m_num = 42;
      }

      // (Watch out when looking at timings: filling this out is probably slow; not Flow-IPC's fault.
      const auto arena = m_ses->core()->app_shm();
      auto payload_list_ptr = reuse_shm_payload ? *m_guy->m_saved_payload_list_ptr // The list is fully in SHM:
                                                : arena->template construct<typename Shm_t::Payload_list>();

      if (reuse_shm_payload)
      {
        // Reusing existing structure: delete a character from the first node already there, for fun.
        payload_list_ptr->front().m_str.erase(1, 1); // Note: SZ needs to be pretty high, or we might run out of chars.

        /* Since we are reusing this, client must have received it before; they are programmed to mess with it;
         * let's check for that.
         * They are to add an extra character in the final m_str.  We'll ensure it happened and delete it again.
         * (Jemalloc as of this writing would not allow this, so skip in that case.) */
        if constexpr(S_CLASSIC_ELSE_JEM)
        {
          auto& payload = payload_list_ptr->back();
          ASSERT((payload.m_str.size() == (SZ + 1))
                 && "Opposing side was supposed to modify the last m_str in reused STL structure.");
          payload.m_str.erase(SZ);
          ASSERT(payload.m_str.size() == SZ);

          // This is the other way it's been messed-with; but the size didn't change, so we need not undo it.
          ASSERT((payload.m_pool.size() == (SZ / 2)));
          ASSERT((payload.m_blob.size() == ((SZ * 3) / 2)));
        }
      } // if (reuse_shm_payload)

      // Add (to empty or non-empty) another big amount of stuff.
      constexpr size_t N = 125 * 1000;
      for (size_t idx = 0; idx != N; ++idx)
      {
        payload_list_ptr->emplace_back(payload); // Make a deep copy of all of that struct!
      }

      auto root = reuse_msg ? msg_out.body_root()->getMsg() : msg_out.body_root()->initMsg();

      // The session_lend_else_arena_lend dichotomy is explained in ex.capnp on the following field.
      root.setSessionLendElseArenaLend(session_lend_else_arena_lend); // Then see just below where lend_object() called.

      root.setOmitHandleRead(omit_handle);
      if (reuse_msg)
      {
        root.setNumVal(root.getNumVal() + 1);
      }
      else
      {
        root.setNumVal(uint64_t(1) * 1000 * 1000 * 1000 * 1000);
      }

      auto shm_handle_root = reuse_msg ? root.getStringBlobListDirect()
                                       : root.initStringBlobListDirect();
      if (!omit_handle)
      {
        if (session_lend_else_arena_lend)
        {
          struc::shm::capnp_set_lent_shm_handle(&shm_handle_root,
                                                m_ses->core()->lend_object(payload_list_ptr));
        }
        else if constexpr(S_CLASSIC_ELSE_JEM)
        {
          // SHM-classic: The non-Session way of "directly" lending is via Arena itself.
          struc::shm::capnp_set_lent_shm_handle(&shm_handle_root, arena->lend_object(payload_list_ptr));
        }
        else
        {
          // Arena-lend-SHM: The non-Session way of "directly" lending is via separate Shm_session guy.
          struc::shm::capnp_set_lent_shm_handle(&shm_handle_root,
                                                m_ses->core()->shm_session()->lend_object(payload_list_ptr));
        }
      }

      // Count each string, blob x 2; account for the removed chars for each reuse (after the initial construct).
      const auto n_repeats = payload_list_ptr->size() / N;
      root.setCumulativeSz((N * SZ * 3) * n_repeats - (n_repeats - 1));

      m_guy->m_saved_payload_list_ptr = payload_list_ptr;
    } // else if (!alt)

    ++m_expectations_b[chan_idx]; // Ack rsp expected.

    FLOW_LOG_INFO("App_session [" << this << "]: Chan B[" << chan_idx << "]: Filling done.  Now to send.");

    Error_code err_code;
    bool ok = chan.async_request(msg_out, nullptr, nullptr /* 1-off rsp exp */,
                                 [this, chan_idx, on_msg_func = std::move(on_msg_func),
                                  ctx = std::string(ctx)](Msg_in_ptr_b&& msg_in) mutable
    {
      m_guy->post([this, chan_idx, on_msg_func = std::move(on_msg_func), ctx = std::move(ctx),
                   msg_in = std::move(msg_in)]() mutable
      {
        FLOW_LOG_INFO("App_session [" << this << "]: Chan B[" << chan_idx << "]: Ack received (description = "
                      "[" << ctx << "]).");

        // Quick check of contents.
        m_guy->check_scalar_set(msg_in->body_root().getDescription(), "b-description");
        m_guy->check_scalar_set(msg_in->body_root().getMsgTwo(), "b-payload-num");

        ASSERT((m_expectations_b[chan_idx] >= 1) && "Payload contents OK; but unexpected payload: "
                                                    "b-exp-count already at 0 before decrement.");
        --m_expectations_b[chan_idx]; // Ack rsp expected.

        on_msg_func();
      });
    }, &err_code);
    ASSERT(ok && "async_request() yielded false, meaning error emitted earlier by chan.");
    if (err_code)
    {
      FLOW_LOG_WARNING
        ("async_request() yielded error code [" << err_code << "] [" << err_code.message() << "].  FAIL.");
      ASSERT(false);
    }

    m_guy->m_saved_msg = std::move(msg_out); // Save in case a test case wants to test re-sending it.

    FLOW_LOG_INFO("App_session [" << this << "]: Chan B[" << chan_idx << "]: Sending done.");
  } // else if constexpr(S_SHM_ENABLED)
  else // if constexpr(!S_SHM_ENABLED)
  {
    ASSERT(false
           && "Must not be called unless S_SHM_ENABLED.  Didn't feel like using SFINAE to forbid at compile-time.");
  }
} // CLASS::App_session::send_req_b()

TEMPLATE
void CLASS::App_session::start_chan_a(size_t chan_idx)
{
  m_struct_chans_a[chan_idx]->start([this, chan_idx](const Error_code& err_code)
  {
    m_guy->post([this, chan_idx, err_code]()
                  { on_chan_err(m_struct_chans_a[chan_idx], m_expectations_a[chan_idx], err_code); });
  });
}

TEMPLATE
void CLASS::App_session::start_chan_b(size_t chan_idx)
{
  m_struct_chans_b[chan_idx]->start([this, chan_idx](const Error_code& err_code)
  {
    m_guy->post([this, chan_idx, err_code]()
                  { on_chan_err(m_struct_chans_b[chan_idx], m_expectations_b[chan_idx], err_code); });
  });
}

TEMPLATE
template<typename Channel>
void CLASS::App_session::on_chan_err(const Channel& chan, const unsigned int& expectations, const Error_code& err_code)
{
  FLOW_LOG_INFO("App_session [" << this << "]: Chan A[" << *chan << "]: Error handler fired with code "
                "[" << err_code << "] [" << err_code.message() << "].  This may be expected; e.g., winding down at "
                "the end... but will check expectation counter (must be 0; else will fail now).");
  if (expectations != 0)
  {
    FLOW_LOG_WARNING("Should be zero but is [" << expectations << "].");
    ASSERT(false);
  }
} // CLASS::App_session::on_chan_err()

TEMPLATE
void CLASS::App_session::check_notif_payload_a(Msg_in_ptr_a&& msg_in, size_t chan_idx, bool alt)
{
  const auto& msg = msg_in->body_root();
  const auto desc = msg.getDescription();
  FLOW_LOG_INFO("App_session [" << this << "]: Chan A[" << chan_idx << "]: "
                "Payload received; description: [" << desc << "].  Ensuring values OK.");
  m_guy->check_scalar_set(desc, "a-description");

  if (alt)
  {
    ASSERT(msg.isMsgTwo() && "Expected alt-payload top-union-which selector; got something else.");
    m_guy->check_scalar_set(msg.getMsgTwo(), "a-alt-payload");
  }
  else
  {
    ASSERT(msg.isMsg() && "Expected mainstream-payload top-union-which selector; got something else.");
    const auto root = msg.getMsg();

    m_guy->check_scalar_set(root.getNumVal(), "a-num-val"); // Check this random-ass number.

    // Now check the string-list.

    const auto exp_total_sz = root.getCumulativeSz();
    m_guy->check_scalar_set(exp_total_sz, "a-cumulative-sz");

    const auto str_list = root.getStringList();
    size_t total_sz = 0;
    for (size_t idx = 0; idx != str_list.size(); ++idx)
    {
      const auto str_root = str_list[idx];
      const auto str_sz = str_root.size();
      total_sz += str_sz;
      ASSERT((str_root[0] != '\0') && "End-cap check 1 fail.");
      ASSERT((str_root[str_sz - 1] != '\0') && "End-cap check 2 fail.");
    }

    m_guy->check_scalar_set
      (total_sz == exp_total_sz,
       flow::util::ostream_op_string("str-list exp-sz [", exp_total_sz, "] actual-sz [", total_sz, ']'));

    FLOW_LOG_INFO("Values OK (it may have taken a while to check).  "
                  "Rough total payload byte count = [" << total_sz << "].");
  } // else if (!alt)

  ASSERT((m_expectations_a[chan_idx] >= 1) && "Payload contents OK; but unexpected payload: "
                                              "a-exp-count already at 0 before decrement.");
  --m_expectations_a[chan_idx];
} // CLASS::App_session::check_notif_payload_a()

TEMPLATE
void CLASS::App_session::ping_a(size_t chan_idx)
{
  auto& chan = *m_struct_chans_a[chan_idx];
  FLOW_LOG_INFO("App_session [" << this << "]: Chan A[" << chan_idx << "]: Pinging opposing side to proceed.");

  auto msg_out = chan.create_msg();
  msg_out.body_root()->setDescription("ping to proceed");
  msg_out.body_root()->setMsgTwo("alt text");

  Error_code err_code;
  bool ok = chan.send(msg_out, nullptr, &err_code);
  FLOW_LOG_INFO("App_session [" << this << "]: Chan A[" << chan_idx << "]: Sent.");
  ASSERT(ok && "send() yielded false, meaning error emitted earlier by chan.");
  if (err_code)
  {
    FLOW_LOG_WARNING("send() yielded error code [" << err_code << "] [" << err_code.message() << "].  FAIL.");
    ASSERT(false);
  }
} // CLASS::App_session::ping_a()

TEMPLATE
template<typename On_msg_handler>
void CLASS::App_session::send_req_a(size_t chan_idx, On_msg_handler&& on_msg_func)
{
  auto& chan = *m_struct_chans_a[chan_idx];
  FLOW_LOG_INFO("App_session [" << this << "]: Chan A[" << chan_idx << "]: Sending request (registering response "
                "handler -- not one-off).");

  auto msg_out = chan.create_msg();
  msg_out.body_root()->setDescription("multi-response request");
  msg_out.body_root()->setMsgTwo("alt text");

  Error_code err_code;
  bool ok = chan.async_request(msg_out, nullptr, &m_saved_req_id_out, // Set so we can undo_expect_responses() later.
                               [this, on_msg_func = std::move(on_msg_func)](Msg_in_ptr_a&& msg_in) mutable
  {
    m_guy->post([on_msg_func = std::move(on_msg_func), msg_in = std::move(msg_in)]() mutable
    {
      on_msg_func(std::move(msg_in));
    });
  }, &err_code);

  ASSERT(ok && "async_request() yielded false, meaning error emitted earlier by chan.");
  if (err_code)
  {
    FLOW_LOG_WARNING
      ("async_request() yielded error code [" << err_code << "] [" << err_code.message() << "].  FAIL.");
    ASSERT(false);
  }

  FLOW_LOG_INFO("Channel A[" << chan_idx << "] out-request ID [" << m_saved_req_id_out << "] saved.");
} // CLASS::App_session::send_req_a()

TEMPLATE
void CLASS::App_session::ping_b(size_t chan_idx)
{
  auto& chan = *m_struct_chans_b[chan_idx];
  FLOW_LOG_INFO("App_session [" << this << "]: Chan B[" << chan_idx << "]: Pinging opposing side to proceed.");

  auto msg_out = chan.create_msg();
  msg_out.body_root()->setDescription("ping to proceed");
  msg_out.body_root()->setMsgTwo(1776);

  Error_code err_code;
  bool ok = chan.send(msg_out, nullptr, &err_code);
  ASSERT(ok && "send() yielded false, meaning error emitted earlier by chan.");
  FLOW_LOG_INFO("App_session [" << this << "]: Chan B[" << chan_idx << "]: Sent.");
  if (err_code)
  {
    FLOW_LOG_WARNING("send() yielded error code [" << err_code << "] [" << err_code.message() << "].  FAIL.");
    ASSERT(false);
  }
} // CLASS::App_session::ping_b()

TEMPLATE
template<typename Task>
void CLASS::App_session::expect_ping_and_b(size_t chan_idx, Task&& task)
{
  FLOW_LOG_INFO("App_session [" << this << "]: Chan B[" << chan_idx << "]: Awaiting ping before proceeding.");
  m_struct_chans_b[chan_idx]->expect_msg(capnp::ExBodyB::MSG_TWO,
                                         [this, chan_idx, task = std::move(task)](Msg_in_ptr_b&& msg_in) mutable
  {
    m_guy->post([this, chan_idx, task = std::move(task), msg_in = std::move(msg_in)]()
    {
      const auto& msg = msg_in->body_root();
      const auto desc = msg.getDescription();
      FLOW_LOG_INFO("App_session [" << this << "]: Chan B[" << chan_idx << "]: "
                    "Ping received; description: [" << desc << "].  Ensuring values OK before proceeding.");
      m_guy->check_scalar_set(desc, "b-description");
      ASSERT(msg.isMsgTwo() && "Expected alt-payload top-union-which selector; got something else.");
      m_guy->check_scalar_set(msg.getMsgTwo(), "b-alt-payload");

      task();
    }); // post()
  }); // expect_msg()
} // CLASS::App_session::expect_ping_and_b()

#undef CLASS
#undef TEMPLATE

} // namespace ipc::transport::test
