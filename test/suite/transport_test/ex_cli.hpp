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
#include "ipc/transport/struc/channel.hpp"
#include "ipc/session/sync_io/client_session_adapter.hpp"

namespace ipc::transport::test
{

template<typename Session_t, bool S_SHM_ENABLED_V>
class Ex_cli : public Ex_guy
{
public:
  using Session = Session_t;

  Ex_cli(flow::log::Logger* logger_ptr, flow::log::Logger* ipc_logger_ptr);
  ~Ex_cli();

  bool run();

private:
  using Base = Ex_guy;
  using Channel_obj = typename Session::Channel_obj;
  using Channels = typename Session::Channels;
  using Mdt_reader_ptr = typename Session::Mdt_reader_ptr;
  using Mdt_builder = typename Session::Mdt_builder;

  static constexpr bool S_SHM_ENABLED = Session::S_SHM_ENABLED;
  // Unused if !S_SHM_ENABLED.
  static constexpr bool S_CLASSIC_ELSE_JEM = Session::S_SHM_TYPE == session::schema::ShmType::CLASSIC;

  /* Attempt at maybe/possibly a realistic in-app class encapsulating a session.  In a client app like this
   * one would only have one of these at a time and store a Client_session<> (Session, we called it).  If
   * session ends, one would probably destroy this guy and create a new one, to keep track of any per-session
   * resources or resource handle lifetimes reasonably conveniently.
   *
   * We are modeling somewhat unrealistically two different `Client_app`s within one process, but we are gonna
   * do most real work via just one Client_app, so we'll just keep that in this App_session.  However we might
   * less realistically model 2+ of these running ~simultaneously at times.
   *
   * In Ex_srv there is similarly a complementary App_session. */
  class App_session : public flow::log::Log_context
  {
  public:
    template<typename Task>
    App_session(Ex_cli* guy, unsigned int test_idx, // test_idx = 0 => test most stuff; 1 => test the rest.
                Task&& i_am_done_func);
  private:
    template<typename Body>
    using Structured_channel = typename Session::template Structured_channel<Body>;
    using Channel_a = Structured_channel<capnp::ExBodyA>;
    using Channel_b = Structured_channel<capnp::ExBodyB>;
    using Channels_a = std::vector<Uptr<Channel_a>>;
    using Channels_b = std::vector<Uptr<Channel_b>>;
    using Msg_in_a = typename Channel_a::Msg_in;
    using Msg_in_ptr_a = typename Channel_a::Msg_in_ptr;
    using Msg_out_a = typename Channel_a::Msg_out;
    using Msg_in_b = typename Channel_b::Msg_in;
    using Msg_in_ptr_b = typename Channel_b::Msg_in_ptr;
    using Msg_out_b = typename Channel_b::Msg_out;

    void on_chan_open(Channel_obj&& new_chan, const Mdt_reader_ptr& mdt_srv,
                      size_t n_chans_a, size_t n_chans_b);

    void use_channels_if_ready(size_t n_chans_a, size_t n_chans_b);
    void use_channels_round_2();

    void send_notif_a(size_t chan_idx, util::String_view ctx, bool alt = false,
                      const Msg_in_ptr_a& originating_msg_in_or_null = Msg_in_ptr_a());

    void start_chan_a(size_t chan_idx);
    void start_chan_b(size_t chan_idx);
    template<typename Channel>
    void on_chan_err(const Channel& chan, const Error_code& err_code);

    template<typename Task>
    void expect_ping_and_a(size_t chan_idx, Task&& task);
    template<typename Task>
    void expect_pings_and_b(size_t chan_idx, Task&& task);
    void handle_req_b(Msg_in_ptr_b&& msg_in, size_t chan_idx, bool alt = false);
    void ping_b(size_t chan_idx);

    Ex_cli* const m_guy;
    const unsigned int m_test_idx;
    const flow::async::Task m_i_am_done_func;
    // The actual Client_session<>!  For this one we exercise the sync_io:: interface.
    std::optional<session::sync_io::Client_session_adapter<Session>> m_ses;
    Mdt_reader_ptr m_mdt_srv; // Srv->cli metadata.
    Channels m_chans_a; // Srv-requested init-channels from last async_accept(); + srv-opened additional channels.
    Channels m_chans_b; // Cli-requested init-channels from last async_accept(); + cli-opened additional channels.
    /* And then they are upgraded into these Structured_channel<>s.  Note the 2 sets use different schemas;
     * hence the different types at compile-time. */
    Channels_a m_struct_chans_a;
    Channels_b m_struct_chans_b;

    std::vector<typename Channel_obj::Async_io_obj> m_aio_chans_a;

    using Builder_config = typename Session::Structured_msg_builder_config;
    Builder_config m_struct_chans_a_builder_config;
    Builder_config m_struct_chans_b_builder_config;

    /* Place to save a SHM handle to a payload STL structure, so other side changes it in-place; we check it
     * without re-borrowing any handle. */
    std::optional<typename Shm_traits<Session, S_SHM_ENABLED>::Payload_list_brw_ptr> m_saved_payload_list_ptr;

    unsigned int m_ping_b_count = 0; // Count how many of these pings we receive; once done we move on to next big step.

    /* Not-very-rigorous expectations count: e.g., expect some message = ++m_expectations;
     * got it = --m_expectations; got it again = --m_expectations; result is negative => something went wrong;
     * result is positive => something went wrong in the other direction; result is 0 = OK probably. */
    unsigned int m_expectations = 0;
  }; // class App_session

  void client_connect_one();
  void client_connect_two();
  void client_connect_2();

  Session m_ses; // A test Client_session outside of App_session, just to test some quick stuff.
  Mdt_reader_ptr m_mdt_srv; // At-session-open metadata filled-out by server.
  typename Session::Channels m_init_chans_cli; // At-session-open channels for a certain quick test.

  Uptr<App_session> m_app_ses; // A more extensive one (encapsulates various).
}; // class Ex_cli

#define TEMPLATE \
  template<typename Session_t, bool S_SHM_ENABLED_V>
#define CLASS \
  Ex_cli<Session_t, S_SHM_ENABLED_V>

TEMPLATE
CLASS::Ex_cli(flow::log::Logger* logger_ptr, flow::log::Logger* ipc_logger_ptr) :
  Base(logger_ptr, ipc_logger_ptr)
{
  // OK.
}

TEMPLATE
CLASS::~Ex_cli()
{
  // See Ex_srv dtor.  Same reasoning here.
  Base::stop_worker();
}

TEMPLATE
bool CLASS::run()
{
  return Base::run(false, [&]()
  {
    post([this]() { client_connect_one(); });
  }); // Base::run()
} // CLASS::run()

TEMPLATE
void CLASS::client_connect_one()
{
  m_ses = Session(m_ipc_logger, m_cli_apps.find(S_CLI_NAME)->second, m_srv_apps.find(S_SRV_NAME)->second,
                  [this](const Error_code&)
  {
    post([this]()
    {
      FLOW_LOG_WARNING("Should not have gotten as far as error handler: async_connect() should have failed.");
      done_and_done(false);
    });
  });

  FLOW_LOG_INFO("Connecting session; requesting 1 init-channel (expecting failure).");

  m_init_chans_cli.resize(1); // Expect 1.
  const bool ok = m_ses.async_connect(m_ses.mdt_builder(), &m_init_chans_cli, nullptr, nullptr,
                                      [this](const Error_code& err_code)
  {
    if (err_code == session::error::Code::S_OBJECT_SHUTDOWN_ABORTED_COMPLETION_HANDLER)
    {
      return;
    }
    // else

    post([this, err_code]()
    {
      if (!err_code)
      {
        FLOW_LOG_WARNING("Expected an async_connect() error; got success instead.");
        done_and_done(false);
        return;
        /* By the way, below, we might not always do this kind of "nicer" handling of an unexpected situations;
         * we might just ASSERT().  I guess I just wanted to set up some precedent for how it might look to
         * not simply crash.  I don't know.  It's just tedious to keep doing this all over: this isn't production
         * code. */
      }
      // else
      FLOW_LOG_INFO("As expected got error ([" << err_code << "] [" << err_code.message() << "]).");

      client_connect_two();
    }); // post()
  }); // m_ses.async_connect()
  ASSERT(ok);
} // CLASS::client_connect_one()

TEMPLATE
void CLASS::client_connect_two()
{
  /* OK, so see what Ex_srv::server_accept_loop() does.  It does this:
   *   - Accept a session
   *     - Handle it by setting it up independently of any others
   *   - Go again (rinse/repeat)
   *
   * So we can open as many as we want (though it's not super-realistic, but hopefully that's clear by now;
   * we model multiple things just to keep it compact as a demo/test).
   *
   * The "Handle it" part can accept a CLI_NAME-guy or a CLI_NAME_2 guy (different `Client_app`s).
   * CLI_NAME_2 we don't do too much with, while App_session handles CLI_NAME; similarly
   * they have their own counterparts to that (including Ex_srv::App_session).
   *
   * For now let's just keep it to one session of each kind. */

  client_connect_2(); // The quick one.
  // The more-involved one (App_session):
  m_app_ses = make_uptr<App_session>(this, 0, [this]()
  {
    post([this]()
    {
      FLOW_LOG_INFO("App_session 1 reports it's done-for.  Deleting object from outside its own code.");
      m_app_ses.reset();

      FLOW_LOG_INFO("Let us start App_session 2 to test the remaining things including reusing some cross-session "
                    "assets maintained by the opposing side (server).");
      m_app_ses = make_uptr<App_session>(this, 1, [this]()
      {
        // Already in our thread W....
        FLOW_LOG_INFO("App_session 2 reports it's done-for.  Deleting object from outside its own code; quitting.");
        m_app_ses.reset();

        done_and_done(true);
      });
    });
  });
} // CLASS::client_connect_two()

TEMPLATE
void CLASS::client_connect_2()
{
  // Replace the Session from client_connect_one() (it did not open OK anyway BTW).
  m_ses = Session(m_ipc_logger, m_cli_apps.find(S_CLI_NAME_2) // <-- Attn.
                                  ->second,
                  m_srv_apps.find(S_SRV_NAME)->second,
                  [this](const Error_code&)
  {
    post([this]()
    {
      FLOW_LOG_FATAL("Not expecting opened Client_app [" << S_CLI_NAME_2 << "] session to error-out.");
      ASSERT(false);
    });
  });

  auto mdt_cli = m_ses.mdt_builder();
  mdt_cli->initPayload().setNumChansYouWant(0); // They're gonna verify this contrived thing.
  const bool ok = m_ses.async_connect(m_ses.mdt_builder(), nullptr, // Open 0 from cli.
                                      &m_mdt_srv, // Do get some mdt.
                                      nullptr, // Open 0 from srv (consistent with mdt_cli payload).
                                      [this](const Error_code& err_code)
  {
    if (err_code == session::error::Code::S_OBJECT_SHUTDOWN_ABORTED_COMPLETION_HANDLER)
    {
      return;
    }
    // else

    post([this, err_code]()
    {
      if (err_code)
      {
        FLOW_LOG_WARNING("async_connect() failed; should succeed from now on; please check Flow-IPC logs for "
                         "details.  Usually this happens due to running from the wrong CWD or named "
                         "wrong; but just check the logs.  Error emitted: "
                         "[" << err_code << "] [" << err_code.message() << "].");
        done_and_done(false);
        return;
      }
      // else if (!err_code):

      FLOW_LOG_INFO("Client_app [" << S_CLI_NAME_2 << "] session ready.  Cool; everything is as expected; but for this "
                    "Client_app we do not do much further.");
    });
  });
  ASSERT(ok);
} // CLASS::client_connect_2()

TEMPLATE
template<typename Task>
CLASS::App_session::App_session(Ex_cli* guy, unsigned int test_idx, Task&& i_am_done_func) :
  flow::log::Log_context(guy->get_logger(), guy->get_log_component()),
  m_guy(guy), m_test_idx(test_idx), m_i_am_done_func(std::move(i_am_done_func))
{
  using util::sync_io::Asio_waitable_native_handle;

  FLOW_LOG_INFO("App_session [" << this << "]: I am not yet open!  Gonna async_connect() now.");

  /* Could've done much/all of this in the ctor initializer but just didn't feel like it.  You can at any rate
   * see one can default-ct a Client_session (initialized) and just overwrite it with a move-assigned one. */

  auto on_err_func = [this](const Error_code&)
  {
    m_guy->post([this]()
    {
      FLOW_LOG_FATAL("Not expecting opened Client_app [" << S_CLI_NAME << "] session to error-out.");
      ASSERT(false);
    });
  };

  /* We'll open some init-channels; but also after that will accept some manual passive-opens in the PEER-state session.
   * We'll also invoke some manual active-opens ourselves. */
  auto on_chan_open_func = [this]
                             (Channel_obj&& new_chan, Mdt_reader_ptr&& mdt_srv) mutable
  {
    m_guy->post([this, new_chan_ptr = make_sptr<Channel_obj>(std::move(new_chan)),
                 mdt_srv = std::move(mdt_srv)]() mutable
                  { on_chan_open(std::move(*new_chan_ptr), mdt_srv, S_N_CHANS_A, S_N_CHANS_B); });
  };

  /* Here we choose to exercise the sync_io-style counterpart to Client_session (our `Session`), which is
   * sync_io::Client_session_adapter<Session>.  It is really easier to use the async-I/O one, and we do test that
   * elsewhere in this program, but here we test this style for better test coverage.  (Internally C_s_a uses a
   * C_s<> anyway, so we are exercising both this way.) */

  m_ses.emplace(m_guy->m_ipc_logger,
                m_guy->m_cli_apps.find(S_CLI_NAME)->second,
                m_guy->m_srv_apps.find(S_SRV_NAME)->second,
                std::move(on_err_func), std::move(on_chan_open_func));

  m_ses->replace_event_wait_handles([this]() -> auto
  {
    return Asio_waitable_native_handle(*(m_guy->task_engine()));
  });

  m_ses->start_ops([](Asio_waitable_native_handle* hndl_of_interest,
                          bool ev_of_interest_snd_else_rcv, util::sync_io::Task_ptr&& on_active_ev_func)
  {
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
  }); // m_ses->start_ops()

  // OK, since our event loop is boost.asio, from here using m_ses is about the same as using an async-I/O Session.

  /* We are gonna do a full-on fancy async_connect() with all the features in-use, much like the opposing
   * async_accept().  Basically this matches complementary logic in Ex_srv; but in that code the
   * 2 Client_apps' stuff is a little bit mixed together in nearby code, whereas we just worry about
   * *this Client_app (CLI_NAME-guy).  (The other side's code could be separated out by Client_app a bit
   * more nicely, but I digress.) */

  // Cli->srv metadata!
  auto mdt_cli = m_ses->core()->mdt_builder();
  mdt_cli->initPayload().setNumChansYouWant(S_N_INIT_CHANS_A);

  // Srv->cli metadata: &m_mdt_srv.

  // We want this many init-channels on our behalf!
  m_chans_b.resize(S_N_INIT_CHANS_B);

  // Init-channels on their behalf: &m_chans_a.  They decide how many (but we know it's S_N_INIT_CHANS_A).
  ASSERT(m_chans_a.empty());

  // Go!
  const bool ok = m_ses->async_connect(mdt_cli, &m_chans_b, &m_mdt_srv, &m_chans_a,
                                       [this](const Error_code& err_code)
  {
    if (err_code == session::error::Code::S_OBJECT_SHUTDOWN_ABORTED_COMPLETION_HANDLER)
    {
      return;
    }
    // else:

    m_guy->post([this, err_code]()
    {
      if (err_code)
      {
        FLOW_LOG_WARNING("async_connect() failed; should succeed from now on; please check Flow-IPC logs for deets.  "
                         "Usually this happens due to running from the wrong CWD or named "
                         "wrong; but just check the logs.  Error emitted: "
                         "[" << err_code << "] [" << err_code.message() << "].");
        m_guy->done_and_done(false);
        return;
      }
      // else if (!err_code):

      FLOW_LOG_INFO("App_session [" << this << "]: I am open!  Well, my session::Session [" << *m_ses << "] is open, "
                    "and I have 2 init-channel sets sized "
                    "[" << m_chans_a.size() << ", " << m_chans_b.size() << "] "
                    "pre-opened too.  Also I have some srv->cli mdt.  Let's check some things about it ~all.");

      ASSERT(m_chans_b.size() == S_N_INIT_CHANS_B); // This guy is pre-sized: size() should not change.
      // And yeah, our contrived/demo thing is that they send this number as part of mdt also.
      ASSERT(m_mdt_srv->getPayload().getNumChansYouWant() == m_chans_b.size());

      ASSERT(m_chans_a.size() == S_N_INIT_CHANS_A);

      FLOW_LOG_INFO("Looks good!  Now let's active-open some type-B channels (and passive-open some type-A ones).");

      for (size_t idx = 0; idx != S_N_CHANS_B - S_N_INIT_CHANS_B; ++idx)
      {
        m_chans_b.emplace_back();

        auto mdt = m_ses->core()->mdt_builder();
        mdt->initPayload().initMdtTwo().setTwoPayload(6767);
        Error_code err_code_non_fatal;
        const bool ok = m_ses->core()->open_channel(&(m_chans_b.back()), mdt, &err_code_non_fatal);
        ASSERT(ok && "Error fired now or earlier -- unexpected.");
        ASSERT((!err_code_non_fatal) && "Non-fatal error emitted by open_channel(), but we expect total success.");

        FLOW_LOG_INFO("Cool, active-opened channel (to [" << m_chans_b.size() << "] type-B channels total): "
                      "[" << m_chans_b.back() << "].");
      }

      use_channels_if_ready(S_N_CHANS_A, S_N_CHANS_B);
    }); // post()
  }); // m_ses->async_connect()
  ASSERT(ok);
} // CLASS::App_session::App_session()

TEMPLATE
void CLASS::App_session::on_chan_open(Channel_obj&& new_chan, const Mdt_reader_ptr& mdt_srv,
                                      size_t n_chans_a, size_t n_chans_b)
{
  m_chans_a.emplace_back(std::move(new_chan));

  const auto root = mdt_srv->getPayload();
  const auto in_val = root.getMdtOne().getOnePayload();
  m_guy->check_scalar_set(in_val, "srv-mdt"); // Check it's not default-valued.

  FLOW_LOG_INFO("App_session [" << this << "]: Cool, passive-opened channel (to [" << m_chans_a.size() << "] "
                "type-A channels total): [" << m_chans_a.back() << "]; non-default val in MDT = [" << in_val << "].  "
                "Overall MDT contents: [" << kj::str(*mdt_srv).cStr() << "].");

  if (m_struct_chans_a.empty())
  {
    use_channels_if_ready(n_chans_a, n_chans_b);
  }
  else
  {
    FLOW_LOG_INFO("App_session [" << this << "]: The main structured channels (type A, B) have been set up already; "
                  "the new channel just-opened must be for a later test opening more channels for specific tests; "
                  "not upgrading to structured.  Though we do upgrade it to AIO channel from SIO core.");
    auto aio_chan = m_chans_a.back().async_io_obj();
    if (root.getAutoPing())
    {
      FLOW_LOG_INFO("Enabling auto-ping as instructed in metadata.");
      const bool ok = aio_chan.auto_ping();
      ASSERT(ok && "auto_ping() failed on new AIO channel?!");
    }
    m_aio_chans_a.emplace_back(std::move(aio_chan));
  }
} // CLASS::App_session::on_chan_open()

TEMPLATE
void CLASS::App_session::use_channels_if_ready(size_t n_chans_a, size_t n_chans_b)
{
  if ((m_chans_a.size() != n_chans_a) || (m_chans_b.size() != n_chans_b))
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
                "[" << n_chans_a << '/' << n_chans_b << "].");

  // The code below exactly mirrors ex_srv.hpp; comments omitted.  @todo Code reuse would be nice.  It's a demo tho....

  m_struct_chans_a.reserve(m_chans_a.size());
  m_struct_chans_b.reserve(m_chans_b.size());

  if constexpr(S_SHM_ENABLED)
  {
    for (auto& chan : m_chans_a)
    {
      m_struct_chans_a.emplace_back(make_uptr<Channel_a>(m_guy->m_ipc_logger, std::move(chan),
                                                         struc::Channel_base::S_SERIALIZE_VIA_SESSION_SHM,
                                                         m_ses->core()));
    }
    m_struct_chans_a_builder_config = m_ses->core()->session_shm_builder_config();

    for (auto& chan : m_chans_b)
    {
      Uptr<Channel_b> ch;
      if constexpr(S_CLASSIC_ELSE_JEM)
      {
        /* SHM-classic: Out-messages: Doesn't matter which arena; they're not rememembered by server beyond handler
         * (in our case).
         * In-messages: Server sends app-scope messages; in arena-sharing providers like SHM-classic receiver must
         * specify the in-messages' scope. */
        ch = make_uptr<Channel_b>(m_guy->m_ipc_logger, std::move(chan),
                                  struc::Channel_base::S_SERIALIZE_VIA_APP_SHM, m_ses->core());
      }
      else
      {
        /* SHM-jemalloc: Out-messages: Doesn't matter which arena; they're not rememembered by server beyond handler
         * (in our case).
         * In-messages: In arena-lending providers like SHM-jemalloc receiver will deserialize fine regardless of
         * in which (properly lent) arena sender constructed msg.
         * In any case: This SHM-provider does not provide .app_shm() in Client_session (us).  The above explains
         * why (for context/as a reminder).  Anyway: Use session-scope tag. */
        ch = make_uptr<Channel_b>(m_guy->m_ipc_logger, std::move(chan),
                                  struc::Channel_base::S_SERIALIZE_VIA_SESSION_SHM, m_ses->core());
      }
      m_struct_chans_b.emplace_back(std::move(ch));
    }
    if constexpr(S_CLASSIC_ELSE_JEM)
    {
      m_struct_chans_b_builder_config = m_ses->core()->app_shm_builder_config(); // Same deal as just above.
    }
    else
    {
      m_struct_chans_b_builder_config = m_ses->core()->session_shm_builder_config(); // Ditto.
    }
  } // if constexpr(S_SHM_ENABLED)
  else // if constexpr(!S_SHM_ENABLED)
  {
    for (auto& chan : m_chans_a)
    {
      m_struct_chans_a.emplace_back(make_uptr<Channel_a>(m_guy->m_ipc_logger, std::move(chan),
                                                         struc::Channel_base::S_SERIALIZE_VIA_HEAP,
                                                         m_ses->core()->session_token()));
    }
    m_struct_chans_a_builder_config = Session::heap_fixed_builder_config(m_guy->m_ipc_logger);

    for (auto& chan : m_chans_b)
    {
      m_struct_chans_b.emplace_back(make_uptr<Channel_b>(m_guy->m_ipc_logger, std::move(chan),
                                                         struc::Channel_base::S_SERIALIZE_VIA_HEAP,
                                                         m_ses->core()->session_token()));
    }
    m_struct_chans_b_builder_config = m_ses->core()->heap_fixed_builder_config(); // For variety use the non-static 1.
  } // else if constexpr(!S_SHM_ENABLED)

  m_chans_a.clear();
  m_chans_b.clear();

  /* This is quite arbitrary, but let's just reserve this channel B<last> for the server to indicate testing is done,
   * so it wants to quit.  We receive any message on that channel and go to next round; repeat...; then end the
   * session eventually. */
  const auto RESERVED_CHAN_IDX = m_struct_chans_b.size() - 1;
  start_chan_b(RESERVED_CHAN_IDX);
  expect_pings_and_b(RESERVED_CHAN_IDX, [this, RESERVED_CHAN_IDX]() mutable
  {
    const unsigned int PINGS_EXPECTED = (m_test_idx == 0) ? 5 : 1;

    ++m_ping_b_count;
    FLOW_LOG_INFO("App_session [" << this << "]: Special ping # [" << m_ping_b_count << "] received.  Will proceed "
                  "to next big step once that reaches [" << PINGS_EXPECTED << "].");
    if (m_ping_b_count == PINGS_EXPECTED)
    {
      FLOW_LOG_INFO("App_session [" << this << "]: Got exit-signaling ping on channel B<last>: proceeding to round 2.  "
                    "Letting server know via ping (will check `expectations` count too).");
      m_ping_b_count = 0;

      if (m_expectations != 0)
      {
        FLOW_LOG_FATAL("`expectations` count should be zero but is [" << m_expectations << "].");
        ASSERT(false);
      }

      ping_b(RESERVED_CHAN_IDX);

      use_channels_round_2();

      m_struct_chans_b[RESERVED_CHAN_IDX]->undo_expect_msgs(capnp::ExBodyB::MSG_TWO);
      expect_pings_and_b(RESERVED_CHAN_IDX, [this]()
      {
        const unsigned int PINGS_EXPECTED = (m_test_idx == 0) ? 1 : 6;

        ++m_ping_b_count;
        FLOW_LOG_INFO("App_session [" << this << "]: Special ping # [" << m_ping_b_count << "] received.  Will proceed "
                      "to next big step once that reaches [" << PINGS_EXPECTED << "].");
        if (m_ping_b_count == PINGS_EXPECTED)
        {
          FLOW_LOG_INFO("App_session [" << this << "]: Got exit-signaling ping on channel B<last>: "
                        "Indicating I am done and can be deleted (will check `expectations` count too).");

          if (m_expectations != 0)
          {
            FLOW_LOG_FATAL("`expectations` count should be zero but is [" << m_expectations << "].");
            ASSERT(false);
          }

          m_i_am_done_func();
        }
      }); // expect_pings_and_b()
    } // if (PINGS_EXPECTED reached)
  }); // expect_pings_and_b()

  if (m_test_idx == 0)
  {
    /* OK, let's test.  See master list of things to test in similar place in Ex_srv.
     * Follow what it's doing; comments relatively light here. */

    // For fun let's not even .start() anything; we are only send()ing for now.  Later we'll send() post-start() too.

    // m_struct_chans_a[0]->send(<a notification, meaning no response expected>).
    send_notif_a(0, "msg 1 - receive post-expect_msg()+start()");
    // Ditto.  The other side will confirm they are each received ~when expected.
    send_notif_a(0, "msg 2 - receive after 2nd 1-off expect_msg()");
    // OK, OK, let's start it.
    start_chan_a(0);

    // Same deal with chan A1; but it should receive them both right away; then we should wait for a ping to go again.
    send_notif_a(1, "msg 1 - receive post-expect_msgs()+start() - 1/2");
    // Ditto.  The other side will confirm they are each received ~when expected.
    send_notif_a(1, "msg 2 - receive post-expect_msgs()+start() - 2/2");
    start_chan_a(1);
    expect_ping_and_a(1, [this]()
    {
      send_notif_a(1, "Round 2: msg 1 - receive post-expect_msgs()+start() - 1/2");
      // Ditto.  The other side will confirm they are each received ~when expected.
      send_notif_a(1, "Round 2: msg 2 - receive post-expect_msgs()+start() - 2/2");
    });

    // For this test we just await a ping and then send messages in this pattern.

    start_chan_a(2);
    start_chan_a(3);
    expect_ping_and_a(2, [this]()
    {
      send_notif_a(2, "msg 1/3 - multi-type test - type 1");
      send_notif_a(2, "msg 2/3 - multi-type test - type 2", true);
      send_notif_a(2, "msg 3/3 - multi-type test - type 2", true);
    });
    expect_ping_and_a(3, [this]()
    {
      send_notif_a(3, "msg 1/3 - multi-type test - type 1");
      send_notif_a(3, "msg 2/3 - multi-type test - type 1");
      send_notif_a(3, "msg 3/3 - multi-type test - type 2", true);
    });
  } // if (m_test_idx == 0)
  else // if (m_test_idx == 1)
  {
    auto next_id = make_sptr<int>(0);
    m_struct_chans_a[0]->expect_msgs(capnp::ExBodyA::MSG_HANDLE,
                                     [this, next_id](Msg_in_ptr_a&& msg_in) mutable
    {
      m_guy->post([this, next_id, msg_in = std::move(msg_in)]() mutable
      {
        const Native_handle hndl = msg_in->native_handle_or_null();
        const auto id = msg_in->body_root().getMsgHandle();

        FLOW_LOG_INFO("App_session [" << this << "]: Chan A0: Got possibly handle-bearing in-msg; "
                      "hndl [" << hndl << "], ID [" << id << "].  Will verify ID against expected "
                      "[" << *next_id << "]; if non-null handle will write ID back to sender via IPC-pipe over "
                      "that handle.");

        ASSERT((id == (*next_id)++) && "ID does not match; something got reordered?");

        if (hndl.null())
        {
          return;
        }
        // else

        boost::asio::writable_pipe wrt(*(m_guy->task_engine()), hndl.m_native_handle); // Should throw on trouble.
        Error_code sys_err_code;
        const auto payload = uint8_t(id);
        const bool ok = wrt.write_some(util::Blob_const(&payload, 1), sys_err_code) == 1;

        if (sys_err_code || (!ok))
        {
          FLOW_LOG_WARNING("Pipe sync-write failed; this should never happen.  Details follow.");
          FLOW_ERROR_SYS_ERROR_LOG_WARNING();
          ASSERT(false && "Pipe-write bug?");
          return;
        }
        // else

        /* Note!  `wrt` gets destroyed here, hence the FD gets closed.  Server sends us ::dup(writer_fd) though,
         * so our closing it is not for us to worry about; they'll reuse their dup()ee, if they want. */
      }); // m_guy->post()
    }); // m_struct_chans_a[0]->expect_msgs(capnp::ExBodyA::MSG_HANDLE)

    /* In test 2 (m_test_idx==1), round 1 (before first ping-B received) we help test sync_request().
     * Server drives the action mainly; all we do is repeat this again and again:
     *   - expect 1 msg of a certain type
     *   - delay N time (possibly N=0)
     *   - respond with arbitrary ack message; or kill channel as the last action to cause error on other side.
     *
     * We coordinate N with what server expects; then server ensures it either timed out or got the reply. */

    auto test_and = [this](size_t chan_idx, Seconds delay_or_0, Void_func&& task_else_kill) mutable
    {
      FLOW_LOG_INFO("App_session [" << this << "]: Chan A[" << chan_idx << "]: Expecting 1 request; "
                    "upon receipt will delay [" << delay_or_0 << "]; then "
                    "[" << (task_else_kill.empty() ? "kill channel to induce error" : "ack and go again.") << "].");
      m_struct_chans_a[chan_idx]->expect_msg(capnp::ExBodyA::MSG_TWO, [this, chan_idx, delay_or_0,
                                                                       task_else_kill]
                                                                        (Msg_in_ptr_a&& msg_in) mutable
      {
        m_guy->post([this, chan_idx, task_else_kill, msg_in = std::move(msg_in),
                     delay_or_0]()
        {
          FLOW_LOG_INFO("Chan A[" << chan_idx << "]: Got request.  Delaying (or not).");

          auto action = [this, chan_idx, task_else_kill, msg_in = std::move(msg_in)]()
          {
            // After the delay (if any):
            if (task_else_kill.empty())
            {
              FLOW_LOG_INFO("Chan A[" << chan_idx << "]: Killing out-pipe.");
              /* We also brutally murder `chan` entirely (dtor), but let's make them get a graceful
               * S_RECEIVES_FINISHED_CANNOT_RECEIVE (with the other side verifying this) just for fun.
               * Otherwise they'd get a low-level EOF or similar. */
              m_struct_chans_a[chan_idx]->async_end_sending([](const Error_code&){});
              return;
            }
            // else

            // Ack it.  Opportunistically test sending a default-created message with zero further mods, as an easy ack.
            FLOW_LOG_INFO("Chan A[" << chan_idx << "]: Sending ack (didn't even modify default-created msg).");

            Error_code err_code;
            const bool ok = m_struct_chans_a[chan_idx]
                              ->send(m_struct_chans_a[chan_idx]->create_msg(), msg_in.get(), &err_code);
            ASSERT((ok && (!err_code)) && "Acks in this test are uncontroversial and should themselves work.");

            // And invoke task[_else_kill]() which will be calling us again.
            task_else_kill();
          }; // auto action =

          if (delay_or_0.count() == 0)
          {
            action(); // Avoid even the overhead of an async non-delay.
          }
          else
          {
            m_guy->delay_and(delay_or_0, std::move(action));
          }
        }); // post()
      }); // m_struct_chans_a[chan_idx]->expect_msg()
    }; // auto test_and =

    // Channel A0: [get req, wait 2s, ack] x 2, [get req, wait 0s, ack], [get req, wait 2s, kill channel].
    test_and(0, Seconds(2), [test_and]() mutable
    {
      test_and(0, Seconds(2), [test_and]() mutable
      {
        test_and(0, Seconds::zero(), [test_and]() mutable
        {
          test_and(0, Seconds(2), Void_func());
        });
      });
    });
    // Channel A1: [get req, wait 2s, ack], [get req, wait 0s, ack], [get req, wait 2s, kill channel].
    test_and(1, Seconds(2), [test_and]() mutable
    {
      test_and(1, Seconds::zero(), [test_and]() mutable
      {
        test_and(1, Seconds(2), Void_func());
      });
    });

    start_chan_a(0);
    start_chan_a(1);
  } // else if (m_test_idx == 1)

  // --

  if constexpr(S_SHM_ENABLED)
  {
    /* The SHM-handle/message-reuse tests begin over type-B channels (except the reserved one).  See server again.
     * Here we pretty much just react to whatever server decides to do, checking the payload (including giant
     * SHM-stored STL data) for correctness.  That is the case in both m_test_idx == 0, 1. */
    for (size_t chan_idx = 0; chan_idx != m_struct_chans_b.size() - 1; // Note RESERVED_CHAN_IDX excluded from this.
         ++chan_idx)
    {
      start_chan_b(chan_idx);
      m_struct_chans_b[chan_idx]->expect_msgs(capnp::ExBodyB::MSG,
                                              [this, chan_idx](Msg_in_ptr_b&& msg_in) mutable
      {
        m_guy->post([this, chan_idx, msg_in = std::move(msg_in)]() mutable
                      { handle_req_b(std::move(msg_in), chan_idx); });
      });
    }
  } // if constexpr(S_SHM_ENABLED)
} // CLASS::App_session::use_channels_if_ready()

TEMPLATE
void CLASS::App_session::use_channels_round_2()
{
  // As in use_channels_if_ready(), we're driven by the server side; see their use_channels_round_2().

  /* In the first test we first expect a request; we respond 2x plus send an unrelated notif.
   * Also, it's up to us -- in the 2nd half of the test, just below -- to test that *we* are informed
   * when *they* indicate they got the 2 unexpected responses.  So set up the handler right away;
   * but only increment m_expectations when we're actually expecting it. */
  m_struct_chans_a[0]->set_remote_unexpected_response_handler
    ([this](struc::Channel_base::msg_id_out_t msg_id_bad_boy, std::string&& description) mutable
  {
    m_guy->post([this, msg_id_bad_boy, description = std::move(description)]() mutable
    {
      FLOW_LOG_INFO("App_session [" << this << "]: "
                    "Chan A0: remote-unexpected-response handler fired; supposedly because we sent that.  Will "
                    "ensure we were expecting this.  Meanwhile the reported details from struc::Channel: "
                    "response msg-ID = [" << msg_id_bad_boy << "]; description = [\n" << description << "\n].");

      ASSERT((m_expectations >= 1) && "remote-unexpected-response unexpected firing.");
      --m_expectations;

      if (m_expectations == 0)
      {
        FLOW_LOG_INFO("App_session [" << this << "]: That's the 2nd unexpected response.  "
                      "Seems this test passed; undoing set-remote-unexpected-response-handler.");
        m_struct_chans_a[0]->unset_remote_unexpected_response_handler();

        // Lastly, basically, ping 'em, so they retake the driver's seat and go to the next test/whatever.
        send_notif_a(0, "sync msg", true);
      }
    }); // post()
  }); // set_remote_unexpected_response_handler()
  // That shouldn't arrive yet though (we have not yet been pinged to send unexpected rsp).

  m_struct_chans_a[0]->expect_msg(capnp::ExBodyA::MSG_TWO, [this](Msg_in_ptr_a&& msg_in) mutable
  {
    m_guy->post([this, msg_in = std::move(msg_in)]() mutable
    {
      send_notif_a(0, "msg 1/3 - multi-rsp test - notif", true);
      send_notif_a(0, "msg 2/3 - multi-rsp test - rsp 1", true, msg_in);
      send_notif_a(0, "msg 3/3 - multi-rsp test - rsp 2", true, msg_in);

      // 2nd half of that test, on ping, we do the same thing (they won't be expecting the responses this time though).
      m_expectations += 2;
      expect_ping_and_a(0, [this, msg_in  = std::move(msg_in)]()
      {
        send_notif_a(0, "msg 1/3 - multi-rsp test - notif", true);
        send_notif_a(0, "msg 2/3 - multi-rsp test - rsp 1 (unexpected)", true, msg_in);
        send_notif_a(0, "msg 3/3 - multi-rsp test - rsp 2 (unexpected)", true, msg_in);
      }); // expect_ping_and_a()
    }); // post()
  }); // expect_msg()
} // CLASS::App_session::use_channels_round_2()

TEMPLATE
void CLASS::App_session::send_notif_a(size_t chan_idx, util::String_view ctx, bool alt,
                                      const Msg_in_ptr_a& originating_msg_in_or_null)
{
  auto& chan = *m_struct_chans_a[chan_idx];
  FLOW_LOG_INFO("App_session [" << this << "]: Chan A[" << chan_idx << "]: "
                "Filling/send()ing payload (description = [" << ctx << "]; alt-payload? = [" << alt << "].)");

  Msg_out_a msg_out;

  if (alt)
  {
    /* Normally we would just do the following.
     *   msg_out = chan.create_msg();
     *   msg_out.body_root()->setDescription(std::string(ctx));
     *   msg_out.body_root()->setMsgTwo("alt text");
     * We'll instead do something more advanced, albeit contrived in this case: use the 2nd/advanced
     * struc::Msg_out ctor form, where one creates a schema-less MessageBuilder, sets some stuff in it,
     * and then feeds it into the struc::Msg_out ctor.  We'll even further modify it via body_root()
     * as well.  The other side will at least ensure that both fields aren't blank at any rate.
     * This also lets us use the Session builder-config returner method -- another thing tested.  The
     * builder-config has already been saved earlier, so we just use it. */
    typename Builder_config::Builder schemaless_msg(m_struct_chans_a_builder_config);
    auto schemaless_msg_builder = schemaless_msg.payload_msg_builder();
    schemaless_msg_builder->template initRoot<capnp::ExBodyA>() // <-- must match struc::Channel Body tparam.
      .setDescription(std::string(ctx));
    // First thing set directly via capnp::MessageBuilder *schemaless_msg_builder.  Set the rest in Msg_out_a:

    msg_out = Msg_out_a(std::move(schemaless_msg));
    msg_out.body_root()->setMsgTwo("alt text");
  }
  else
  {
    constexpr size_t STR_SZ = 8;
    constexpr char FILLER_CH = 'X';
    constexpr char END_CAP_CH = 'A';

    msg_out = chan.create_msg();
    msg_out.body_root()->setDescription(std::string(ctx));

    std::string str(STR_SZ, FILLER_CH);
    str.front() = str.back() = END_CAP_CH;

    // This many copies of str; so either ~4k chars or ~4M chars; only SHM-backed msg can store the latter.
    [[maybe_unused]] constexpr size_t SMALL_N = 500;
    [[maybe_unused]] constexpr size_t BIG_N = SMALL_N * 1000;

    /* Okay, so normally one builds a message top-down, like:
     *   auto root = msg_out.body_root()->initMsg();
     *   root.setNumVal(int32_t(1) * 1000 * 1000 * 1000);
     * However we'll opportunistically test the bottom-up orphan-based technique instead.  (The vanilla technique
     * is tested elsewhere; don't worry.  Actually within the present function we do it in the `alt=true` case.)
     * So, to wit, proceed with the orphan stuff: */
    auto orphanage = msg_out.orphanage();
    auto orphan = orphanage.template newOrphan<::ipc::transport::test::capnp::MultiPurposeMsgA>();
    auto root = orphan.get();
    root.setNumVal(int32_t(1) * 1000 * 1000 * 1000);

    // (Watch out when looking at timings: filling this out is probably slow; not Flow-IPC's fault.
    size_t n;
    if constexpr(S_SHM_ENABLED) { n = BIG_N; } else { n = SMALL_N; }
    auto str_list_root = root.initStringList(n);
    for (size_t idx = 0; idx != n; ++idx)
    {
      auto str_root = str_list_root.init(idx, STR_SZ);
      std::memcpy(str_root.begin(), &str[0], STR_SZ);
    }

    root.setCumulativeSz(n * STR_SZ);

    // Since we did the orphan thing, graft it into the actual message root:
    msg_out.body_root()->adoptMsg(std::move(orphan));
    /* Had we not done that, `orphan` would disappear, its data replaced with wasted zeroes in the serialization.
     * We did do it, so `orphan` is now null (inside), its previous contents hooked into body_root() and actually
     * accessible on the other side, indistinguishably from the alternative situation where we'd chosen to just
     * set all that stuff directly via body_root()->initMsg().set...(). */
  } // else if (!alt)

  FLOW_LOG_INFO("App_session [" << this << "]: Chan A[" << chan_idx << "]: Filling done.  Now to send.");

  Error_code err_code;
  bool ok = chan.send(msg_out,
                      originating_msg_in_or_null.get(), // It might be a response (or not).
                      &err_code);
  ASSERT(ok && "send() yielded false, meaning error emitted earlier by chan.");
  if (err_code)
  {
    FLOW_LOG_FATAL("send() yielded error code [" << err_code << "] [" << err_code.message() << "].  FAIL.");
    ASSERT(false);
  }

  FLOW_LOG_INFO("App_session [" << this << "]: Chan A[" << chan_idx << "]: Sending done.");
} // CLASS::App_session::send_notif_a()

TEMPLATE
void CLASS::App_session::handle_req_b(Msg_in_ptr_b&& msg_in, size_t chan_idx, bool alt)
{
  if constexpr(S_SHM_ENABLED)
  {
    const auto& msg = msg_in->body_root();
    const auto desc = msg.getDescription();
    FLOW_LOG_INFO("App_session [" << this << "]: Chan B[" << chan_idx << "]: "
                  "Payload received; description: [" << desc << "].  Ensuring values OK.");
    m_guy->check_scalar_set(desc, "b-description");

    if (alt)
    {
      ASSERT(msg.isMsgTwo() && "Expected alt-payload top-union-which selector; got something else.");
      m_guy->check_scalar_set(msg.getMsgTwo(), "b-alt-payload");
    }
    else
    {
      ASSERT(msg.isMsg() && "Expected mainstream-payload top-union-which selector; got something else.");
      const auto root = msg.getMsg();

      m_guy->check_scalar_set(root.getNumVal(), "b-num-val"); // Check this random-ass number.

      // Now check the string-list.

      const auto exp_total_sz = root.getCumulativeSz();
      m_guy->check_scalar_set(exp_total_sz, "b-cumulative-sz");

      typename Shm_traits<Session, true>::Payload_list_brw_ptr list_ptr;
      if (root.getOmitHandleRead())
      {
        // No borrow/lend_object()!  Just check its contents from the last time we saved it!
        list_ptr = *m_saved_payload_list_ptr;
      }
      else
      {
        flow::util::Blob_sans_log_context to_borrow;
        struc::shm::capnp_get_shm_handle_to_borrow(root.getStringBlobListDirect(), &to_borrow);
        if (root.getSessionLendElseArenaLend())
        {
          list_ptr
            = m_ses->core()->template borrow_object<typename Shm_traits<Session, true>::Payload_list_brw>(to_borrow);
        }
        else if constexpr(S_CLASSIC_ELSE_JEM)
        {
          // SHM-classic: The non-Session way of "directly" borrowing is via Arena itself.
          list_ptr
            = m_ses->core()->app_shm()
                ->template borrow_object<typename Shm_traits<Session, true>::Payload_list_brw>(to_borrow);
        }
        else
        {
          // Arena-lend-SHM: The non-Session way of "directly" borrowing is via separate Shm_session guy.
          list_ptr
            = m_ses->core()->shm_session()
                ->template borrow_object<typename Shm_traits<Session, true>::Payload_list_brw>(to_borrow);
        }

        m_saved_payload_list_ptr = list_ptr; // And save it for next time, possibly (getOmitHandleRead()=true case)!
      } // else if (!root.getOmitHandleRead())
      const auto& list = *list_ptr;

      size_t total_sz = 0;
      for (const auto& payload : list)
      {
        const auto& str = payload.m_str;
        const auto& blob = payload.m_blob;
        const auto& pool = payload.m_pool;
        const auto num = payload.m_num;

        m_guy->check_scalar_set(num, "b-payload-num");

        ASSERT((str.front() != '\0') && "payload-str: End-cap check 1 fail.");
        ASSERT((str.back() != '\0') && "payload-str: End-cap check 2 fail.");
        ASSERT((blob.front() != uint8_t(0)) && "payload-blob: End-cap check 1 fail.");
        ASSERT((blob.back() != uint8_t(0)) && "payload-blob: End-cap check 2 fail.");
        ASSERT((pool.front() != uint8_t(0)) && "payload-blob: End-cap check 1 fail.");
        ASSERT((pool.back() != uint8_t(0)) && "payload-blob: End-cap check 2 fail.");

        total_sz += str.size() + blob.size() + pool.size();
      } // for (payload : list)

      m_guy->check_scalar_set
        (total_sz == exp_total_sz,
         flow::util::ostream_op_string("str-list exp-sz [", exp_total_sz, "] actual-sz [", total_sz, ']'));

      /* Opportunistically mess with it; other side if it sends this again will check that it's been messed-with.
       * SHM-jemalloc, as of this writing, is read-only on the borrowing side, so we can only do it with SHM-classic. */
      if constexpr(S_CLASSIC_ELSE_JEM)
      {
        auto& node = list_ptr->back();

        // In-place messing-with.  Server will know to undo this.
        node.m_str += 'Z';

        { // Allocation by m_blob.get_allocator() (a SHM allocator) needed!  Also temp_blob.g_a() will deallocate.
          typename Shm_traits<Session, true>::Arena_activator ctx(m_ses->core()->app_shm());

          /* Allocation-involving messing-with.  We're gonna do some funky stuff, and really it would've been
           * arbitrary -- as long as server can "check our work" the next time (if any) it resends `list` to us.
           * It's not quite arbitrary, though, because we have to keep the total bytes the same to not affect
           * the total size; server will not be undoing this, as it's a pain in the butt.  So we just remove
           * some bytes from one and add some bytes to another.  Also I want to exercise the Sharing_blob (pool)
           * functionality, as that's fun, though it has nothing to do with IPC really.  Well, I did code
           * the allocator support due to IPC, so this is nice to exercise (really Flow unit tests should do it,
           * but they... um... don't exist as of this writing). */

          // TRACE-log to console by the way.  It's nice to see that stuff at least optionally.
          auto left_blob = node.m_pool.share_after_split_left(node.m_pool.size() / 2, get_logger());
          auto temp_blob = std::move(node.m_blob);
          ASSERT(node.m_blob.zero());
          node.m_blob.resize(temp_blob.size() + left_blob.size());
          node.m_blob.emplace_copy(node.m_blob.begin(), temp_blob.const_buffer(), get_logger());
          node.m_blob.emplace_copy(node.m_blob.begin() + temp_blob.size(), left_blob.const_buffer(), get_logger());
          /* temp_blob will disappear and deallocate its internal buffer which is not shared.
           * left_blob will disappear, but the shared buffer-sharing m_pool won't; it'll be 50% smaller though. */
        } // Arena_activator ctx()
      } // if constexpr(S_CLASSIC_ELSE_JEM)

      FLOW_LOG_INFO("Values OK (it may have taken a while to check); we also modified it ourselves a bit (unless "
                    "SHM-jemalloc which disallows that).  Rough total payload byte count = [" << total_sz << "].");
    } // else if (!alt)

    // Ack it.

    auto& chan = *m_struct_chans_b[chan_idx];

    /* Normally we'd just do: auto msg_out = chan.create_msg();
     * We'd like to test the struc::Msg_out 2nd ctor form though, where it takes a schema-less
     * MessageBuilder thingie.  We already do test it in even more advanced (arguably) form in
     * send_notif_a(); here we do a slight variation where we let that 2nd ctor form initRoot<>() for us,
     * because we did not do it.  Or maybe this is more advanced, since we use an orphan.  Whatever!  Just do it. */
    typename Builder_config::Builder schemaless_msg(chan.struct_builder_config());
    auto schemaless_msg_builder = schemaless_msg.payload_msg_builder();
    // FUN!  Create an orphan of unrelated schema!  Note this is type A schema (type B channel).
    auto schemaless_orphan_builder = schemaless_msg_builder->getOrphanage().template newOrphan<capnp::ExBodyA>().get();
    schemaless_orphan_builder.setDescription(std::string(desc));
    /* No initRoot<>() has occurred!  Yet subsume this MessageBuilder into the message.
     * This will initRoot<capnp::ExBodyB>(), because we didn't.  And then we mutate on top of it, even
     * using the orphan's contents somewhat. */
    Msg_out_b msg_out(std::move(schemaless_msg));
    /* It'd be cool to do `adopt...orphan...()` instead of copying, but good enough for this demo: at any rate
     * we are accessing stuff backed by the same MessageBuilder. */
    msg_out.body_root()->setDescription(std::string(schemaless_orphan_builder.getDescription().asReader()));
    msg_out.body_root()->setMsgTwo(123);

    Error_code err_code;
    bool ok = chan.send(msg_out, msg_in.get(), &err_code);
    ASSERT(ok && "send() yielded false, meaning error emitted earlier by chan.");
    if (err_code)
    {
      FLOW_LOG_FATAL("send() yielded error code [" << err_code << "] [" << err_code.message() << "].  FAIL.");
      ASSERT(false);
    }
  } // else if constexpr(S_SHM_ENABLED)
  else // if constexpr(!S_SHM_ENABLED)
  {
    ASSERT(false
           && "Must not be called unless S_SHM_ENABLED.  Didn't feel like using SFINAE to forbid at compile-time.");
  }
} // CLASS::App_session::handle_req_b()

TEMPLATE
void CLASS::App_session::ping_b(size_t chan_idx)
{
  auto& chan = *m_struct_chans_b[chan_idx];
  FLOW_LOG_INFO("App_session [" << this << "]: Chan B[" << chan_idx << "]: Pinging opposing side to proceed.");

  /* Yet another slight variation on how to create a message; this one uses a pre-manually-saved Builder_config
   * and immediately subsumes it into a struc::Msg_out.  The other one uses chan.struct_builder_config()
   * with the other struc::Msg_out ctor form; plus it actually modifies the MessageBuilder directly somewhat. */
  Msg_out_b msg_out{ typename Builder_config::Builder(m_struct_chans_b_builder_config) };

  msg_out.body_root()->setDescription("ping to proceed");
  msg_out.body_root()->setMsgTwo(1776);

  Error_code err_code;
  bool ok = chan.send(msg_out, nullptr, &err_code);
  ASSERT(ok && "send() yielded false, meaning error emitted earlier by chan.");
  if (err_code)
  {
    FLOW_LOG_FATAL("send() yielded error code [" << err_code << "] [" << err_code.message() << "].  FAIL.");
    ASSERT(false);
  }
} // CLASS::App_session::ping_b()

TEMPLATE
void CLASS::App_session::start_chan_a(size_t chan_idx)
{
  m_struct_chans_a[chan_idx]->start([this, chan_idx](const Error_code& err_code)
  {
    m_guy->post([this, chan_idx, err_code]()
                  { on_chan_err(m_struct_chans_a[chan_idx], err_code); });
  });
}

TEMPLATE
void CLASS::App_session::start_chan_b(size_t chan_idx)
{
  m_struct_chans_b[chan_idx]->start([this, chan_idx](const Error_code& err_code)
  {
    m_guy->post([this, chan_idx, err_code]()
                  { on_chan_err(m_struct_chans_b[chan_idx], err_code); });
  });
}

TEMPLATE
template<typename Channel>
void CLASS::App_session::on_chan_err(const Channel& chan, const Error_code& err_code)
{
  FLOW_LOG_INFO("App_session [" << this << "]: Chan A[" << *chan << "]: Error handler fired with code "
                "[" << err_code << "] [" << err_code.message() << "].  This may be expected; e.g., winding down at "
                "the end.");
} // CLASS::App_session::on_chan_err()

TEMPLATE
template<typename Task>
void CLASS::App_session::expect_ping_and_a(size_t chan_idx, Task&& task)
{
  FLOW_LOG_INFO("App_session [" << this << "]: Chan A[" << chan_idx << "]: Awaiting ping before proceeeding.");
  m_struct_chans_a[chan_idx]->expect_msg(capnp::ExBodyA::MSG_TWO,
                                         [this, chan_idx, task = std::move(task)](Msg_in_ptr_a&& msg_in) mutable
  {
    m_guy->post([this, chan_idx, task = std::move(task), msg_in = std::move(msg_in)]()
    {
      const auto& msg = msg_in->body_root();
      const auto desc = msg.getDescription();
      FLOW_LOG_INFO("App_session [" << this << "]: Chan A[" << chan_idx << "]: "
                    "Ping received; description: [" << desc << "].  Ensuring values OK before proceeding.");
      m_guy->check_scalar_set(desc, "a-description");
      ASSERT(msg.isMsgTwo() && "Expected alt-payload top-union-which selector; got something else.");
      m_guy->check_scalar_set(msg.getMsgTwo(), "a-alt-payload");

      task();
    }); // post()
  }); // expect_msg()
} // CLASS::App_session::expect_ping_and_a()

TEMPLATE
template<typename Task>
void CLASS::App_session::expect_pings_and_b(size_t chan_idx, Task&& task)
{
  FLOW_LOG_INFO("App_session [" << this << "]: Chan B[" << chan_idx << "]: Awaiting ping before proceeeding.");
  m_struct_chans_b[chan_idx]->expect_msgs(capnp::ExBodyB::MSG_TWO,
                                          [this, chan_idx, task = std::move(task)](Msg_in_ptr_b&& msg_in) mutable
  {
    m_guy->post([this, chan_idx, task /* copy! */, msg_in = std::move(msg_in)]() mutable
    {
      const auto& msg = msg_in->body_root();
      const auto desc = msg.getDescription();
      FLOW_LOG_INFO("App_session [" << this << "]: Chan B[" << chan_idx << "]: "
                    "Ping received; description: [" << desc << "].  Ensuring values OK before proceeding.");
      m_guy->check_scalar_set(desc, "b-description");
      ASSERT(msg.isMsgTwo() && "Expected alt-payload top-union-which selector; got something else.");
      m_guy->check_scalar_set(msg.getMsgTwo(), "b-alt-payload");

      /* Subtlety: task() might destroy *this and therefore m_ses, and with S_SHM_ENABLED=true *msg_in is
       * backed by m_ses-stored SHM arena and will become invalid once m_ses goes away, because the arena object
       * itself disappears; so when this functor is destroyed the program crashes in the msg_in shared_ptr<> deleter,
       * namely went it gets into arena deleter code (at least with SHM-classic Pool_arena).  It's a corner case purely
       * because of the somewhat odd setup of the way App_session interacts with its containing class; if msg_in were
       * simply a member of App_session declared after m_ses, then there'd be no issue.  So it's not really
       * anything about Flow-IPC itself.
       *
       * Anyway to avoid the crash just take care of it before the trouble starts. */
      msg_in.reset();

      task();
    }); // post()
  }); // expect_msgs()
} // CLASS::App_session::expect_ping_and_b()

#undef CLASS
#undef TEMPLATE

} // namespace ipc::transport::test
