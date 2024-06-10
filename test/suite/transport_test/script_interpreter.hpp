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

#include "ipc/transport/native_socket_stream_acceptor.hpp"
#include "ipc/transport/blob_stream_mq_snd.hpp"
#include "ipc/transport/blob_stream_mq_rcv.hpp"
#include "ipc/transport/posix_mq_handle.hpp"
#include "ipc/transport/bipc_mq_handle.hpp"
#include "ipc/transport/channel.hpp"
#include <flow/util/shared_ptr_alias_holder.hpp>

namespace ipc::transport::test
{

// Types.

/*
 * Given an istream, reads commands in a simple script-y language which tell it what actions to take and what events
 * to expect in what time frame (e.g., write a buffer of N length, then sleep M msec, then expect so-and-so).
 * When an action has an unexpected immediate result (e.g., send-over-IPC call failed), or an expected event did not
 * come to pass (e.g., expected response of 5 bytes within 2 sec, but nothing happened), it points out where in the
 * stream this happened.  Similarly complain if a command was illegal.
 *
 * ### Rationale ###
 * Rationale for having this script language layer, as opposed to "simply" writing it in terms of code: Well, it's not
 * a slam dunk.  However anyone testing, say, a protocol over TCP knows the convenience of using a `telnet` session
 * to try a few things interactively.  This tool can also be used interactively; but even by changing a script file
 * without recompiling one can get creative quickly in what one can test; and then this can be turned into relatively
 * easy to understand (and maintain!) script files for official unit tests.  The cost is, of course, having to write
 * this intermediate code (which takes time) and introducing a new (if simple) language.  It just felt right to
 * the author (ygoldfel).
 */
class Script_interpreter :
  public flow::log::Log_context,
  public flow::util::Shared_ptr_alias_holder
           <boost::shared_ptr<Script_interpreter>, boost::shared_ptr<const Script_interpreter>>,
  public boost::enable_shared_from_this<Script_interpreter>,
  private boost::noncopyable
{
public:

  /* logger_ptr -> Our (Script_interpreter) logs (console logger is a good choice).
   * ipc_logger_ptr -> Flow-IPC logs: for when we invoke ipc:: APIs, and they log (file logger is a good choice). */
  static Ptr create(flow::log::Logger* logger_ptr, flow::log::Logger* ipc_logger_ptr, std::istream& is);
  bool go();

private:
  using Lines = std::vector<std::string>;
  using Lines_iter = Lines::const_iterator;
  using Keyword_to_interpret_func_map
    = boost::unordered_map<util::String_view, Function<void (Script_interpreter* this_obj)>>;

  struct Token
  {
    std::string m_str;
    Lines_iter m_line;
    size_t m_line_idx;
    size_t m_line_pos;
  };

  static const Keyword_to_interpret_func_map S_CMD_TO_HANDLER_MAP;

  // Mq_peer is one of the 4 args to Mq_peer_list inside Mq_peer_list_variant.
  template<typename Mq_peer>
  using Mq_peer_list = std::vector<boost::shared_ptr<Mq_peer>>;
  // @todo unique_ptr--^ better but map initializer complains about wanting to use unique_ptr copy ctor (in our ctor).
  using Mq_peer_list_variant = std::variant<Mq_peer_list<Posix_mq_sender>,
                                            Mq_peer_list<Bipc_mq_sender>,
                                            Mq_peer_list<Posix_mq_receiver>,
                                            Mq_peer_list<Bipc_mq_receiver>>;
  /* To get the list of `Mq_peer`s, given an Mq_peer_lists M:
   * auto& vec = get<Mq_peer_list<Mq_peer>>(M[type_index(typeid(Mq_peer))]);
   * vec.push_back(std::move(some_mq_peer_unique_ptr)); // Add a slot. */
  using Mq_peer_lists = std::map<std::type_index, Mq_peer_list_variant>;

  // Similar stuff here but simpler; there are only 2 variants: POSIX vs. Bipc; a Chan_bundle is both snder and receiver.
  template<typename Chan_bundle>
  using Chan_bundle_list = std::vector<boost::shared_ptr<Chan_bundle>>;
  using Chan_bundle_list_variant = std::variant<Chan_bundle_list<Posix_mqs_socket_stream_channel>,
                                                Chan_bundle_list<Bipc_mqs_socket_stream_channel>>;
  using Chan_bundle_lists = std::map<std::type_index, Chan_bundle_list_variant>;

  // Used for timeouts at least:

  // Promise that's either done on time (call .set_value()); or not.
  using Done_promise_t = boost::promise<void>;
  using Done_promise = boost::shared_ptr<Done_promise_t>;
  using Future_status = boost::future_status; // Result (ready, timeout) of P.get_future().wait(), where P is a promise.
  /* Function that (potentially, albeit not necessarily) asynchronously (in other thread) does something and lastly
   * does `p->set_value()` to mark completion.  So it should either do the latter synchronously *or* async (not
   * both). */
  using Async_task = Function<void (const Done_promise& p)>;
  using Task = flow::async::Task; // No-arg void callable.

  explicit Script_interpreter(flow::log::Logger* logger_ptr, flow::log::Logger* ipc_logger_ptr, std::istream& is);

  void interpret_cmd();

  bool next_token(Token* tok_ptr, bool keyword);
  bool next_branch(const Keyword_to_interpret_func_map& keyword_to_interpret_func_map);
  void next_required_branch(const Keyword_to_interpret_func_map& keyword_to_interpret_func_map);

  Token next_required_token(bool keyword, util::String_view exp_token_description);

  template<typename Value>
  Value next_required_typed_value();
  util::Fine_duration next_required_dur();
  Error_code next_required_err_code();
  template<typename Peer_list>
  const typename Peer_list::value_type& next_required_peer(const Peer_list& peers, util::String_view peer_type) const;
  template<typename Peer_list>
  typename Peer_list::value_type& next_required_peer_ref(Peer_list& peers, util::String_view peer_type);

  void failed(bool parse_error, util::String_view description, const Error_code& err_code = Error_code());
  void test_with_timeout(util::Fine_duration timeout,
                         const Async_task& async_do_and_set_promise_func,
                         const Task& on_done_func);

  template<typename Strings_map>
  std::string map_keys_str(const Strings_map& map);
  unsigned int user_line_idx() const;
  static unsigned int user_line_idx(size_t line_idx);
  static unsigned int user_col_idx(size_t col_idx);

  void cmd_sleep();
  void cmd_socket_stream_connect();
  void cmd_socket_stream_acceptor_accept();
  void cmd_socket_stream_acceptor_listen();
  void cmd_handle_connect_pair();
  void cmd_socket_stream_create_from_hndl();
  void cmd_socket_stream_send();
  void cmd_socket_stream_send_blob();
  void cmd_socket_stream_send_end();
  void cmd_socket_stream_recv();
  void cmd_socket_stream_recv_blob();
  template<typename Mq_peer>
  void cmd_blob_stream_mq_peer_create();
  template<typename Mq_peer>
  void cmd_blob_stream_mq_peer_destroy();
  template<typename Mq_peer>
  void cmd_blob_stream_mq_peer_send_blob();
  template<typename Chan_bundle>
  void cmd_chan_bundle_send_blob();
  template<typename Chan_bundle>
  void cmd_chan_bundle_send();
  template<typename Mq_peer>
  void cmd_blob_stream_mq_peer_send_end();
  template<typename Chan_bundle>
  void cmd_chan_bundle_send_end();
  template<typename Mq_peer>
  void cmd_blob_stream_mq_peer_recv_blob();
  template<typename Chan_bundle>
  void cmd_chan_bundle_recv_blob();
  template<typename Chan_bundle>
  void cmd_chan_bundle_recv();
  template<typename Peer_list>
  void cmd_blob_sender_send_blob_impl(const Peer_list& peers, util::String_view peer_type);
  template<typename Peer_list>
  void cmd_socket_stream_sender_send_impl(const Peer_list& peers, util::String_view peer_type);
  template<typename Peer_list>
  void cmd_blob_sender_send_end_impl(const Peer_list& peers, util::String_view peer_type);
  template<typename Peer_list>
  void cmd_blob_receiver_recv_blob_impl(const Peer_list& peers, util::String_view peer_type);
  template<typename Peer_list>
  void cmd_socket_stream_receiver_recv_impl(const Peer_list& peers, util::String_view peer_type);
  template<typename Chan_bundle>
  void cmd_chan_bundle_create();

  flow::util::Blob test_blob(size_t n) const;
  void validate_rcvd_blob_contents(const flow::util::Blob& blob, size_t exp_blob_n);

  flow::log::Logger* const m_ipc_logger;
  std::istream& m_is;
  Lines m_lines;
  boost::movelib::unique_ptr<std::istringstream> m_cur_line_is;
  size_t m_cur_line_idx;
  Lines_iter m_cur_line_it;
  Token m_last_tok;

  /* socket-stream-connect cmd adds a slot at the back of this and attempts to connect and save to that slot.
   * socket-stream-accept cmd does similarly except attempts to accept a connection via a specified
   * m_test_sock_stm_acceptors[] element.
   * socket-stream-create-from-from-hndl cmd does similarly except by adopting a handle m_test_native_hndls[]. */
  std::vector<boost::shared_ptr<Native_socket_stream>> m_test_sock_streams;
  /* socket-stream-listen cmd adds a slot at the back of this and attempts to start listening; if successful saves to
   * that slot. */
  std::vector<boost::shared_ptr<Native_socket_stream_acceptor>> m_test_sock_stm_acceptors;
  /* For various commands cmd: cmd adds a slot at the back of this and stores a native handle there.
   * At least:
   *   - connect-pair cmd creates 2 mutually connected ones and pushes them here.
   *   - One can receive a handle from another process via Native_socket_stream. */
  std::vector<Native_handle> m_test_native_hndls;
  /* See Mq_peer_lists comment above.  This stores the 4 lists of
   * Blob_stream_mq_{send|receiv}er<{Posix|Bipc}_mq_handle>s. */
  Mq_peer_lists m_test_blob_stream_mq_peers;
  // Similarly this stores the 2 lists of `{Posix|Bipc}_mqs_socket_stream_channel`s.
  Chan_bundle_lists m_test_chan_bundles;
}; // class Script_interpreter

} // namespace ipc::transport::test
