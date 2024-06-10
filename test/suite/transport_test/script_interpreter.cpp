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

#include "script_interpreter.hpp"
#include "ipc/transport/asio_local_stream_socket_fwd.hpp"
#include "ipc/util/util_fwd.hpp"
#include "ipc/transport/error.hpp"
#include <boost/array.hpp>
#include <boost/move/make_unique.hpp>

namespace ipc::transport::test
{

namespace
{
// Commands.

/* Sleep.
 *   <cmd> <time> <time-units> */
const util::String_view S_KEYWD_SLEEP = "SLEEP";

/* Test Native_socket_stream::sync_connect().  On success load peer into table as 0, 1, ....
 *   <cmd> <sh-name> <timeout> <timeout-units> */
const util::String_view S_KEYWD_SOCK_STREAM_CONNECT = "SOCK_STREAM_CONNECT";

// @todo Actually do this (at the moment this is only a stub; unused).
/* Test Native_socket_stream::dtor (closing 2-directional pipe).  On success peer is removed for n-th slot in table,
 * by nullifying that slot's stored pointer.  Test shall assume a quiet pipe at start and start some transmission
 * call(s) to see what happens at destruction.
 *   <cmd> <stm-slot> */
[[maybe_unused]] const util::String_view S_KEYWD_SOCK_STREAM_DESTROY = "SOCK_STREAM_DESTROY";

/* Test Native_socket_stream_acceptor::async_accept().  On success load peer(s) into table as 0, 1, ....
 * <acc-slot> specified slot 0, 1, ... from a successful ACCEPTOR_LISTEN (see below).
 * <n-calls> is the number of async_accept()s queued up (all must succeed, or overall timeout must occur,
 * before command finishes).
 *   <cmd> <acc-slot> <n-calls> <timeout> <timeout-units>  */
const util::String_view S_KEYWD_SOCK_STREAM_ACCEPTOR_ACCEPT = "SOCK_STREAM_ACC_ACCEPT";

/* Test Native_socket_stream_acceptor ctor, which starts listening.  On success load acceptor into table as 0, 1, ....
 *   <cmd> <sh-name> */
const util::String_view S_KEYWD_SOCK_STREAM_ACCEPTOR_LISTEN = "SOCK_STREAM_ACC_LISTEN";

/* Create a pre-mutually-connected pair of peer Unix domain socket native handles.  Load them into table as 0, 1, ...
 * (this command adds 2 to that sequence).  (In particular, one can IPC-send them over a Native_socket_stream.)
 *   <cmd> */
const util::String_view S_KEYWD_HNDL_CONNECT_PAIR = "HNDL_CONNECT_PAIR";

/* Test Native_socket_stream::create_from_pre_connected_native_handle() by calling that, feeding to it the
 * native handle stored at table at the given slot; e.g., can load one of the latter via HNDL_CONNECT_PAIR command.
 * On success load peer into table as 0, 1, ....
 *   <cmd> <hndl-slot> */
const util::String_view S_KEYWD_SOCK_STREAM_CREATE_FROM_HNDL = "SOCK_STREAM_CREATE_FROM_HNDL";

/* The following series of commands test Native_handle_sender::send_native_handle(), where Native_handle_sender
 * is a concept and is actually one of the several supported concrete `Native_handle_sender`s, depending on the
 * command name.
 *
 * *::send_native_handle() non-blockingly-but-synchronously sends
 * a native handle (or none) and a blob (or none) (but at least 1 of 2 must be specified).
 * <hndl-slot-or-none> is index into table of native handles, or "-1" to mean none.
 * <blob-sz-or-0> is positive size of the blob to auto-generate and send, or 0 to mean none.
 * <expected-ipc-err-or-success> is the expected result of the operation, namely "0" to mean success, or
 * the string serialization of an ipc::transport::error::Code.  Note that, outside of success, only
 * transport::error::Code errors can be expected (not, say, a boost.asio error or other system error).
 *   <cmd> <stm-slot> <hndl-slot-or-none> <blob-sz-or-0> <expected-ipc-err-or-success> */
const util::String_view S_KEYWD_SOCK_STREAM_SEND = "SOCK_STREAM_SEND";
const util::String_view S_KEYWD_CHAN_BUNDLE_POSIX_SEND = "CHAN_BUNDLE_POSIX_SEND";
const util::String_view S_KEYWD_CHAN_BUNDLE_BIPC_SEND = "CHAN_BUNDLE_BIPC_SEND";

// @todo Actually do this (at the moment this is only a stub; unused).
[[maybe_unused]] const util::String_view S_KEYWD_SOCK_STREAM_RECV_END = "SOCK_STREAM_RECV_END";

/* The following series of commands test Native_handle_receiver::async_receive_native_handle(), where
 * Native_handle_receiver is a concept and is actually one of the several supported concrete `Native_handle_receiver`s,
 * depending on the command name.
 *
 * *::async_receive_native_handle() which asynchronously receives a single
 * message, consisting of a native handle (or none) and a blob (or none) (but at least 1 of 2 must be transmitted).
 * <blob-sz-or-0> is size of the target buffer, meaning max size of the received message.
 * <expected-hndl?> is 1 or 0 depending on whether indeed a native handle was expected (if so, it is placed into table
 * as 0, 1, .... <expected-blob-sz> is the expected size of the meta-blob (0 means none).
 * <expected-ipc-err-or-success> is as in SOCK_STREAM_SEND.
 * Lastly if <expected-ipc-err-or-success> =/= success, then the other <expected-*> are ignored.
 *   <cmd> <stm-slot> <blob-sz-or-0> <expected-ipc-err-or-success> <expected-hndl?> <expected-blob-sz>
 *         <timeout> <timeout-units> */
const util::String_view S_KEYWD_SOCK_STREAM_RECV = "SOCK_STREAM_RECV";
const util::String_view S_KEYWD_CHAN_BUNDLE_POSIX_RECV = "CHAN_BUNDLE_POSIX_RECV";
const util::String_view S_KEYWD_CHAN_BUNDLE_BIPC_RECV = "CHAN_BUNDLE_BIPC_RECV";

/* Test Posix_mq_sender::ctor (creation), as well as Posix_mq_handle::ctor (creation), and
 * therefore POSIX MQ-itself-creation (on-demand).  On success: MQ is created (if not already created at that name);
 * Posix_mq_handle created and std::move()d into the _sender<> peer -- which is loaded into table as 0, 1, ....
 * <create-else-open?> is 1 to create named-MQ; 0 to open it.  <expected-create-or-open-fail?> is 0 to expect no error
 * in creating handle; 1 to expect either file-exists error or file-not-found error depending on <create-else-open?>.
 * <expected-ipc-err-or-success> is as in SOCK_STREAM_SEND, though probably the only reasonable non-success result
 * one could expect is Code::S_BLOB_STREAM_MQ_SENDER_EXISTS (or _MQ_RECEIVER_EXISTS in the _RCV_ counterparts below).
 * <max-*> are not-ignored if and only if <create-else-open?> is 1.
 *   <cmd> <sh-name> <create-else-open?> <max-n-msg> <max-msg-sz> <expected-create-or-open-fail?>
 *         <expected-ipc-err-or-success> */
const util::String_view S_KEYWD_BLOB_STREAM_MQ_POSIX_SND_CREATE = "BLOB_STREAM_MQ_POSIX_SND_CREATE";
// Ditto but for Bipc_mq_sender MQs.  (Test resource slots and namespaces are distinct from POSIX MQs.)
const util::String_view S_KEYWD_BLOB_STREAM_MQ_BIPC_SND_CREATE = "BLOB_STREAM_MQ_BIPC_SND_CREATE";

/* Ditto x2 but for _receiver<> respectively.  Note a single 1-dir pipe is created by means of creating 1 _sender and
 * 1 _receiver for the same <sh-name>.  A 2nd _sender or _receiver shall yield S_BLOB_STREAM_MQ_SENDER_EXISTS or
 * S_BLOB_STREAM_MQ_RECEIVER_EXISTS. */
const util::String_view S_KEYWD_BLOB_STREAM_MQ_POSIX_RCV_CREATE = "BLOB_STREAM_MQ_POSIX_RCV_CREATE";
const util::String_view S_KEYWD_BLOB_STREAM_MQ_BIPC_RCV_CREATE = "BLOB_STREAM_MQ_BIPC_RCV_CREATE";

/* Test the destruction of the 4 types of objects created by the cmds immediately preceding these respectively.
 * Test shall assume a quiet pipe at start and start some transmission call(s) to see what happens at destruction.
 * @todo Actually add that last thing.
 * Destructor is expected to be slightly blocking; hence <timeout*>.
 *   <cmd> <stm-slot> <timeout> <timeout-units> */
const util::String_view S_KEYWD_BLOB_STREAM_MQ_POSIX_SND_DESTROY = "BLOB_STREAM_MQ_POSIX_SND_DESTROY";
const util::String_view S_KEYWD_BLOB_STREAM_MQ_BIPC_SND_DESTROY = "BLOB_STREAM_MQ_BIPC_SND_DESTROY";
const util::String_view S_KEYWD_BLOB_STREAM_MQ_POSIX_RCV_DESTROY = "BLOB_STREAM_MQ_POSIX_RCV_DESTROY";
const util::String_view S_KEYWD_BLOB_STREAM_MQ_BIPC_RCV_DESTROY = "BLOB_STREAM_MQ_BIPC_RCV_DESTROY";

/* The following series of commands test Blob_sender::send_blob(), where Blob_sender is a concept
 * and is actually one of the several supported concrete `Blob_sender`s, depending on the command name.
 * They shall behave identically.
 *
 * *::send_blob() non-blockingly-but-synchronously sends a blob (size 1+).
 * <expected-ipc-err-or-success> is as in SOCK_STREAM_SEND.
 *   <cmd> <stm-slot> <blob-sz> <expected-ipc-err-or-success> */
const util::String_view S_KEYWD_SOCK_STREAM_SEND_BLOB = "SOCK_STREAM_SEND_BLOB";
const util::String_view S_KEYWD_BLOB_STREAM_MQ_POSIX_SEND_BLOB = "BLOB_STREAM_MQ_POSIX_SEND_BLOB";
const util::String_view S_KEYWD_BLOB_STREAM_MQ_BIPC_SEND_BLOB = "BLOB_STREAM_MQ_BIPC_SEND_BLOB";
const util::String_view S_KEYWD_CHAN_BUNDLE_POSIX_SEND_BLOB = "CHAN_BUNDLE_POSIX_SEND_BLOB";
const util::String_view S_KEYWD_CHAN_BUNDLE_BIPC_SEND_BLOB = "CHAN_BUNDLE_BIPC_SEND_BLOB";

/* The following series of commands test Blob_sender::end_sending(), where Blob_sender is a concept
 * and is actually one of the several supported concrete `Blob_sender`s, depending on the command name.
 *
 * *::end_sending() sends a graceful-close along the outgoing-direction
 * pipe, which other side's receive API would detect as a particular EOF-like Error_code.
 * Reminder: It's non-blocking-but-synchronous-looking like ::send_blob(); but optionally it has an
 * asynchronous component wherein it would report either success or failure (to a callback) at ultimately giving the
 * graceful-close message to the kernel (if something fails in sending payloads preceding graceful-close, then that
 * will be reported instead).  Just read the API docs for more info; but basically the callback here is where the
 * "chickens come home to roost": its (optional) on-done callback indicates either (1) everything was sent off
 * successfully -- no error shall occur in that-direction pipe; or (2) something failed in being sent, either
 * before graceful-close or graceful-close itself.  So the (optional) callback indicates the ultimate success or
 * failure at sending everything up to and including the graceful-close.  ::send_blob() lacks such
 * asynchronous reporting, but ::end_sending() is different, because it lets the user wait until
 * it's all for-sure done (since nothing can be sent after it, by definition).
 * <expected-ipc-err-or-success> is as in SOCK_STREAM_SEND.
 * <dupe-call?>, iff 1 (not 0), then this is a duplicate call for that stream; should immediately fail and
 * never execute on-done callback.
 *   <cmd> <stm-slot> <dupe-call?> <expected-ipc-err-or-success> <timeout> <timeout-units> */
const util::String_view S_KEYWD_SOCK_STREAM_SEND_END = "SOCK_STREAM_SEND_END";
const util::String_view S_KEYWD_BLOB_STREAM_MQ_POSIX_SEND_END = "BLOB_STREAM_MQ_POSIX_SEND_END";
const util::String_view S_KEYWD_BLOB_STREAM_MQ_BIPC_SEND_END = "BLOB_STREAM_MQ_BIPC_SEND_END";
const util::String_view S_KEYWD_CHAN_BUNDLE_POSIX_SEND_END = "CHAN_BUNDLE_POSIX_SEND_END";
const util::String_view S_KEYWD_CHAN_BUNDLE_BIPC_SEND_END = "CHAN_BUNDLE_BIPC_SEND_END";

/* The following series of commands test Blob_receiver::async_receive_blob(), where Blob_receiver is a concept
 * and is actually one of the several supported concrete `Blob_receiver`s, depending on the command name.
 * They shall behave identically.
 *
 * *::async_receive_blob() asynchronously receives a single
 * message, consisting of a non-empty blob.
 * <blob-sz> is size of the target buffer, meaning max size of the received message.
 * <expected-blob-sz> is the expected size of the blob.
 * <expected-ipc-err-or-success> is as in SOCK_STREAM_SEND.
 * Lastly if <expected-ipc-err-or-success> =/= success, then the other <expected-*> are ignored.
 *   <cmd> <stm-slot> <blob-sz> <expected-ipc-err-or-success> <expected-blob-sz> <timeout> <timeout-units> */
const util::String_view S_KEYWD_SOCK_STREAM_RECV_BLOB = "SOCK_STREAM_RECV_BLOB";
const util::String_view S_KEYWD_BLOB_STREAM_MQ_POSIX_RECV_BLOB = "BLOB_STREAM_MQ_POSIX_RECV_BLOB";
const util::String_view S_KEYWD_BLOB_STREAM_MQ_BIPC_RECV_BLOB = "BLOB_STREAM_MQ_BIPC_RECV_BLOB";
const util::String_view S_KEYWD_CHAN_BUNDLE_POSIX_RECV_BLOB = "CHAN_BUNDLE_POSIX_RECV_BLOB";
const util::String_view S_KEYWD_CHAN_BUNDLE_BIPC_RECV_BLOB = "CHAN_BUNDLE_BIPC_RECV_BLOB";

/* Test {Posix_mqs_socket_stream_channel|Bipc_mqs_socket_stream_channel}::ctor (creation).  Really this operation
 * composes a number of things which are more carefully tested by other commands above.  So mostly this is a setup
 * command, so that we can then test send/receive transmission over the Channel which bundles 2 unidirectional
 * MQ pipes and a full-fuplex Native_socket_stream pipe.  In particular it expects to always succeed: failure
 * of ctor call shall always fail the test.  As such, to successfully use this, the other side (in this or other
 * invocation of Script_interpreter) shall invoke a similar command:
 *   - Its <mq-sh-name-prefix> must be the same.
 *   - Its <stm-slot> must refer to the other end of the same, already-connected, socket stream.
 *     - Due to getting moved-from, the <stm-slot> shall be unusable for direct transmission after this command.
 *       Happily, it will be usable via the m_test_chan_bundles[Posix/Bipc] new slot at which the Chan_bundle
 *       shall be pushed on success.
 *   - Its <mq-create-else-open> must be the opposite.
 *     - The one with value 0 must be executed 2nd, not 1st.
 * <mq-create-else-open> controls whether the 2 MQs shall be created by this side or else opened.
 * MQs named <mq-sh-name-prefix>/a2b and <mq-sh-name-prefix>/b2a shall be created by the side with
 * <mq-create-else-open>=1.
 *   <cmd> <mq-sh-name-prefix> <mq-create-else-open?> <stm-slot> */
const util::String_view S_KEYWD_CHAN_BUNDLE_POSIX_CREATE = "CHAN_BUNDLE_POSIX_CREATE";
const util::String_view S_KEYWD_CHAN_BUNDLE_BIPC_CREATE = "CHAN_BUNDLE_BIPC_CREATE";

// Each blob generated to be sent, and expected to be received, will be this pattern repeated up-to desired length.
const util::String_view S_TEST_BLOB_CONTENTS = "<12345678>";
} // namespace (anon)

const Script_interpreter::Keyword_to_interpret_func_map Script_interpreter::S_CMD_TO_HANDLER_MAP =
  {
    { S_KEYWD_SLEEP, &Script_interpreter::cmd_sleep },
    { S_KEYWD_SOCK_STREAM_CONNECT, &Script_interpreter::cmd_socket_stream_connect },
    { S_KEYWD_SOCK_STREAM_ACCEPTOR_ACCEPT, &Script_interpreter::cmd_socket_stream_acceptor_accept },
    { S_KEYWD_SOCK_STREAM_ACCEPTOR_LISTEN, &Script_interpreter::cmd_socket_stream_acceptor_listen },
    { S_KEYWD_HNDL_CONNECT_PAIR, &Script_interpreter::cmd_handle_connect_pair },
    { S_KEYWD_SOCK_STREAM_CREATE_FROM_HNDL, &Script_interpreter::cmd_socket_stream_create_from_hndl },
    { S_KEYWD_SOCK_STREAM_SEND, &Script_interpreter::cmd_socket_stream_send },
    { S_KEYWD_SOCK_STREAM_SEND_BLOB, &Script_interpreter::cmd_socket_stream_send_blob },
    { S_KEYWD_SOCK_STREAM_SEND_END, &Script_interpreter::cmd_socket_stream_send_end },
    { S_KEYWD_SOCK_STREAM_RECV, &Script_interpreter::cmd_socket_stream_recv },
    { S_KEYWD_SOCK_STREAM_RECV_BLOB, &Script_interpreter::cmd_socket_stream_recv_blob },

    { S_KEYWD_BLOB_STREAM_MQ_POSIX_SND_CREATE,
      &Script_interpreter::cmd_blob_stream_mq_peer_create<Posix_mq_sender> },
    { S_KEYWD_BLOB_STREAM_MQ_BIPC_SND_CREATE,
      &Script_interpreter::cmd_blob_stream_mq_peer_create<Bipc_mq_sender> },
    { S_KEYWD_BLOB_STREAM_MQ_POSIX_RCV_CREATE,
      &Script_interpreter::cmd_blob_stream_mq_peer_create<Posix_mq_receiver> },
    { S_KEYWD_BLOB_STREAM_MQ_BIPC_RCV_CREATE,
      &Script_interpreter::cmd_blob_stream_mq_peer_create<Bipc_mq_receiver> },

    { S_KEYWD_BLOB_STREAM_MQ_POSIX_SND_DESTROY,
      &Script_interpreter::cmd_blob_stream_mq_peer_destroy<Posix_mq_sender> },
    { S_KEYWD_BLOB_STREAM_MQ_BIPC_SND_DESTROY,
      &Script_interpreter::cmd_blob_stream_mq_peer_destroy<Bipc_mq_sender> },
    { S_KEYWD_BLOB_STREAM_MQ_POSIX_RCV_DESTROY,
      &Script_interpreter::cmd_blob_stream_mq_peer_destroy<Posix_mq_receiver> },
    { S_KEYWD_BLOB_STREAM_MQ_BIPC_RCV_DESTROY,
      &Script_interpreter::cmd_blob_stream_mq_peer_destroy<Bipc_mq_receiver> },

    { S_KEYWD_BLOB_STREAM_MQ_POSIX_SEND_BLOB,
      &Script_interpreter::cmd_blob_stream_mq_peer_send_blob<Posix_mq_sender> },
    { S_KEYWD_BLOB_STREAM_MQ_BIPC_SEND_BLOB,
      &Script_interpreter::cmd_blob_stream_mq_peer_send_blob<Bipc_mq_sender> },

    { S_KEYWD_CHAN_BUNDLE_POSIX_SEND_BLOB,
      &Script_interpreter::cmd_chan_bundle_send_blob<Posix_mqs_socket_stream_channel> },
    { S_KEYWD_CHAN_BUNDLE_BIPC_SEND_BLOB,
      &Script_interpreter::cmd_chan_bundle_send_blob<Bipc_mqs_socket_stream_channel> },

    { S_KEYWD_CHAN_BUNDLE_POSIX_SEND,
      &Script_interpreter::cmd_chan_bundle_send<Posix_mqs_socket_stream_channel> },
    { S_KEYWD_CHAN_BUNDLE_BIPC_SEND,
      &Script_interpreter::cmd_chan_bundle_send<Bipc_mqs_socket_stream_channel> },

    { S_KEYWD_BLOB_STREAM_MQ_POSIX_SEND_END,
      &Script_interpreter::cmd_blob_stream_mq_peer_send_end<Posix_mq_sender> },
    { S_KEYWD_BLOB_STREAM_MQ_BIPC_SEND_END,
      &Script_interpreter::cmd_blob_stream_mq_peer_send_end<Bipc_mq_sender> },

    { S_KEYWD_CHAN_BUNDLE_POSIX_SEND_END,
      &Script_interpreter::cmd_chan_bundle_send_end<Posix_mqs_socket_stream_channel> },
    { S_KEYWD_CHAN_BUNDLE_BIPC_SEND_END,
      &Script_interpreter::cmd_chan_bundle_send_end<Bipc_mqs_socket_stream_channel> },

    { S_KEYWD_BLOB_STREAM_MQ_POSIX_RECV_BLOB,
      &Script_interpreter::cmd_blob_stream_mq_peer_recv_blob<Posix_mq_receiver> },
    { S_KEYWD_BLOB_STREAM_MQ_BIPC_RECV_BLOB,
      &Script_interpreter::cmd_blob_stream_mq_peer_recv_blob<Bipc_mq_receiver> },

    { S_KEYWD_CHAN_BUNDLE_POSIX_RECV_BLOB,
      &Script_interpreter::cmd_chan_bundle_recv_blob<Posix_mqs_socket_stream_channel> },
    { S_KEYWD_CHAN_BUNDLE_BIPC_RECV_BLOB,
      &Script_interpreter::cmd_chan_bundle_recv_blob<Bipc_mqs_socket_stream_channel> },

    { S_KEYWD_CHAN_BUNDLE_POSIX_RECV,
      &Script_interpreter::cmd_chan_bundle_recv<Posix_mqs_socket_stream_channel> },
    { S_KEYWD_CHAN_BUNDLE_BIPC_RECV,
      &Script_interpreter::cmd_chan_bundle_recv<Bipc_mqs_socket_stream_channel> },

    { S_KEYWD_CHAN_BUNDLE_POSIX_CREATE,
      &Script_interpreter::cmd_chan_bundle_create<Posix_mqs_socket_stream_channel> },
    { S_KEYWD_CHAN_BUNDLE_BIPC_CREATE,
      &Script_interpreter::cmd_chan_bundle_create<Bipc_mqs_socket_stream_channel> }
  };

Script_interpreter::Ptr Script_interpreter::create(flow::log::Logger* logger_ptr, flow::log::Logger* ipc_logger_ptr,
                                                   std::istream& is) // Static.
{
  return Ptr(new Script_interpreter(logger_ptr, ipc_logger_ptr, is));
}

Script_interpreter::Script_interpreter(flow::log::Logger* logger_ptr, flow::log::Logger* ipc_logger_ptr,
                                       std::istream& is) :
  flow::log::Log_context(logger_ptr),
  m_ipc_logger(ipc_logger_ptr),
  m_is(is),
  m_cur_line_idx(0),
  m_test_blob_stream_mq_peers
    ({ { std::type_index(typeid(Posix_mq_sender)), Mq_peer_list<Posix_mq_sender>() },
       { std::type_index(typeid(Bipc_mq_sender)), Mq_peer_list<Bipc_mq_sender>() },
       { std::type_index(typeid(Posix_mq_receiver)), Mq_peer_list<Posix_mq_receiver>() },
       { std::type_index(typeid(Bipc_mq_receiver)), Mq_peer_list<Bipc_mq_receiver>() } }),
  m_test_chan_bundles
    ({ { std::type_index(typeid(Posix_mqs_socket_stream_channel)),
         Chan_bundle_list<Posix_mqs_socket_stream_channel>() },
       { std::type_index(typeid(Bipc_mqs_socket_stream_channel)),
         Chan_bundle_list<Bipc_mqs_socket_stream_channel>() } })
{
  using std::string;
  using boost::algorithm::trim;

  FLOW_LOG_TRACE("Interpreter is about to read all lines without tokenizing.");

  // This can be perf-ier probably by using getline(), etc., but we don't care.
  string line;
  while (is.good())
  {
    const int ch_get = is.get();
    if (!is.good())
    {
      continue;
    }
    // else

    const auto ch = char(ch_get);
    if (ch == '\n')
    {
      trim(line);
      if (line.empty() || line.front() == '#')
      {
        m_lines.push_back("");
        FLOW_LOG_TRACE("Line completed, ignored (blank or comment): [" << line << "], stored as blank.");
        /* Subtlety: We could just not push_back() it at all; but then line counts get messed up resulting in
         * bad error messages.  We could also remember the full line, and the parser would skip them manually;
         * but then in my (ygoldfel) experience the
         * error messages look odd when printing context.  Though, to be honest, I now don't remember the details;
         * I just know I dumbly "fixed" it by not remembering the line at all -- which screwed up line numbers.
         * So then I "re-fixed" it but saving the line but only as blank. */
      }
      else
      {
        m_lines.push_back(line);
        FLOW_LOG_TRACE("Line completed, stored: [" << line << "].");
      }
      line.clear();
    }
    else
    {
      line += ch;
    }
  }
  if (!line.empty())
  {
    m_lines.push_back(line);
    FLOW_LOG_TRACE("Last, unterminated, line completed, stored: [" << line << "].");
  }
  // else { Ignore last empty unterminated line if any. }

  FLOW_LOG_INFO("Interpreter finished reading/storing untokenized lines.");
} // Script_interpreter::Script_interpreter()

bool Script_interpreter::go()
{
  using std::istringstream;
  using std::exception;

  FLOW_LOG_INFO("Interpreting is a go!");

  if (m_lines.empty())
  {
    FLOW_LOG_WARNING("Script is empty.  Bailing out.");
    return false;
  }
  // else

  assert(m_cur_line_idx == 0);
  m_cur_line_is.reset(new istringstream(m_lines.front()));
  m_cur_line_it = m_lines.begin();
  assert(m_cur_line_is->tellg() == 0);

  try
  {
    // Keep executing commands from the command menu, until there are no more in the input.
    while (next_branch(S_CMD_TO_HANDLER_MAP)) {}
  }
  catch (const exception& exc)
  {
    FLOW_LOG_WARNING("Interpreter detected test failure: [" << exc.what() << "].  Bailing out.");
    return false;
  }

  FLOW_LOG_INFO("Interpreting finished without failure.");
  return true;
} // Script_interpreter::go()

bool Script_interpreter::next_token(Token* tok_ptr, bool keyword)
{
  using boost::to_upper;
  using std::string;
  using std::istringstream;

  assert(tok_ptr);
  auto& tok = *tok_ptr;

  if (!m_cur_line_is)
  {
    FLOW_LOG_TRACE("Requested next token, but we are out of lines; last line was [" << user_line_idx() << "].");
    m_last_tok = { string(), m_lines.end(), m_cur_line_idx + 1, 0 }; // For a nicer message in failed().
    return false;
  }
  // else

  do // while (tok.m_str.empty() && m_cur_line_is)
  {
    /* Before parsing a token record in-line position (column) of "cursor."  This can include white-space.
     *   - If we just moved to this line which is "TOKEN ...", then it'll be 0, pointing to the T.
     *   - If we had just parsed PRETOKEN in "...PRETOKEN    TOKEN", then it'll point to the space right after
     *     PRETOKEN.
     *     - Same if the line is "...PRETOKEN    " or "...PRETOKEN ". */
    tok.m_line_pos = m_cur_line_is->tellg();
    assert(tok.m_line_pos != size_t(-1)); // Pre-condition (our own post-condition from previous call if any).

    tok.m_line = m_cur_line_it;
    tok.m_line_idx = m_cur_line_idx;

    // From that position, skip any white-space; then read everything up to (not including) the next white-space char.
    (*m_cur_line_is) >> tok.m_str;
    // A token is by definition non-empty; so if couldn't read one on this line, then must've hit end of line.
    assert((!tok.m_str.empty()) || (!m_cur_line_is->good()));

    if (tok.m_str.empty())
    {
      FLOW_LOG_TRACE("Requested next token; got nothing on this line (will go to next line if possible).");
    }
    else
    {
      if (keyword)
      {
        to_upper(tok.m_str); // Normalize keywords.
      }
      FLOW_LOG_TRACE("Requested next token; got [" << tok.m_str << "].");
    } // if (!tok.m_str.empty())

    // Advance to next real line, if in reading the next token we've hit the end of the line.
    if (!m_cur_line_is->good())
    {
      // Note: Just b/c we've hit end of line doesn't mean tok_str.empty().  Even so, get ready for next token.

      FLOW_LOG_TRACE("After attempting to read next token, reached end of line [" << user_line_idx() << "]; "
                     "going to find next line if possible.");
      bool found_real_line = false;
      while ((m_cur_line_idx < m_lines.size() - 1) && (!found_real_line))
      {
        ++m_cur_line_it;
        const auto& line = m_lines[++m_cur_line_idx];
        if (!(found_real_line = (!line.empty())))
        {
          FLOW_LOG_TRACE("Skipping blank/comment line [" << user_line_idx() << "].");
        }
      }
      if (found_real_line)
      {
        m_cur_line_is.reset(new istringstream(m_lines[m_cur_line_idx]));
        assert(m_cur_line_is->tellg() == 0); // See up above where we state our post-condition.
      }
      else
      {
        FLOW_LOG_TRACE("There are no more non-blank/comment lines.");
        m_cur_line_is.reset();
      }
    } // if (!m_cur_line_is->good())
  }
  while (tok.m_str.empty() && m_cur_line_is);

  /* Save this for failed() basically (as of this writing anyway).  Then even if (which is typical) the ultimate
   * caller didn't care about the actual Token, and didn't get it returned up the stack, but then invoked failed(),
   * then failed() can still point to the last token parsed as the one causing the problem -- even if it's not a parsing
   * problem but an actual test failure. */
  m_last_tok = tok;

  return !tok.m_str.empty();
} // Script_interpreter::next_token()

bool Script_interpreter::next_branch(const Keyword_to_interpret_func_map& keyword_to_interpret_func_map)
{
  using flow::util::ostream_op_string;

  Token key_tok;
  if (!next_token(&key_tok, true)) // Read a keyword token.
  {
    return false; // No more tokens.
  }
  // else

  // Got a token; see which keyword it is (or illegal).

  const auto keys_str = map_keys_str(keyword_to_interpret_func_map);

  const auto key_table_it = keyword_to_interpret_func_map.find(key_tok.m_str);
  if (key_table_it == keyword_to_interpret_func_map.end())
  {
    failed(true, ostream_op_string("Expected one of [", keys_str, "]."));
  }
  // else
  FLOW_LOG_TRACE("Keyword [" << key_tok.m_str << "]: Recognized from choice [" << keys_str << "].  Executing handler.");

  (key_table_it->second)(this);

  return true;
} // Script_interpreter::next_branch()

void Script_interpreter::next_required_branch(const Keyword_to_interpret_func_map& keyword_to_interpret_func_map)
{
  using flow::util::ostream_op_string;

  if (!next_branch(keyword_to_interpret_func_map))
  {
    failed(true, ostream_op_string("Expected one of [", map_keys_str(keyword_to_interpret_func_map), "]."));
  }
  // else { It executed handler for key_tok. }
} // Script_interpreter::next_required_branch()

Script_interpreter::Token Script_interpreter::next_required_token(bool keyword, util::String_view exp_token_description)
{
  using flow::util::ostream_op_string;

  Token tok;
  const bool got_token = next_token(&tok, keyword);

  if (!got_token)
  {
    failed(true, ostream_op_string("Expected token encoding [", exp_token_description, "]; but ran out of input."));
  }
  // else
  return tok;
}

template<typename Value>
Value Script_interpreter::next_required_typed_value()
{
  using flow::util::ostream_op_string;
  using boost::lexical_cast;
  using std::exception;

  const Token tok = next_required_token(false, ostream_op_string("value of type [", typeid(Value).name(), ']'));

  // If Value=std::string, this is a no-op, but we don't care about such perf; good enough for test code.
  Value value;
  try
  {
    value = lexical_cast<Value>(tok.m_str);
  }
  catch (const exception& exc)
  {
    failed(true, ostream_op_string("Expected token encoding value of type [", typeid(Value).name(), "]; "
                                     "but could not convert from string [", tok.m_str, "]; cast error = [",
                                   exc.what(), "]."));
  }

  return value;
} // Script_interpreter::next_required_typed_value()

util::Fine_duration Script_interpreter::next_required_dur()
{
  using flow::util::ostream_op_string;
  using util::Fine_duration;
  using boost::lexical_cast;
  using std::string;
  using std::exception;

  // If Fine_duration represents as-fine-units-as-possible, then its rep type is big enough for any usable units.
  using raw_ticks_t = Fine_duration::rep;

  /* So get two tokens: <number> <units>.
   * Then combine them, with a space between; then use boost.chrono to interpret this into a Fine_duration.
   * Why get `count` as a number?  Just so if it's not a number, we catch it early for an easier-to-read error. */
  const auto count = next_required_typed_value<raw_ticks_t>();
  const auto unit_spec = next_required_typed_value<string>();

  // The only problem possible now, I think, is if <units> is something illegal (not one of: s, seconds, ms, ....).
  Fine_duration value;
  const auto value_str = ostream_op_string(count, ' ', unit_spec);
  try
  {
    value = lexical_cast<Fine_duration>(value_str);
  }
  catch (const exception& exc)
  {
    failed(true, ostream_op_string("Expected 2 tokens encoding value of duration type (<num> <units>); "
                                     "but could not convert from string [", value_str, "]; cast error = [",
                                   exc.what(), "]."));
  }

  return value;
} // Script_interpreter::next_required_dur()

void Script_interpreter::failed(bool parse_error, util::String_view description, const Error_code& err_code)
{
  using flow::error::Runtime_error;
  using flow::util::ostream_op_string;
  using util::String_view;

  const auto& tok = m_last_tok;
  const auto msg = parse_error ? "Parse error" : "Test failed";

  if (tok.m_line == m_lines.end())
  {
    throw Runtime_error(err_code,
                        ostream_op_string(msg, " at <end-of-input>.  Problem: [", description, "]."));

  }
  // else

  const auto line_idx = tok.m_line_idx;
  const auto& line = *tok.m_line;
  const auto col_idx = tok.m_line_pos;
  String_view before_in_line(line);
  String_view rest_of_line(line);
  rest_of_line.remove_prefix(col_idx);
  before_in_line.remove_suffix(before_in_line.size() - col_idx);

  throw Runtime_error(err_code,
                      ostream_op_string(msg,
                                        " at [", user_line_idx(line_idx), ':', user_col_idx(col_idx),
                                        "]: line is [[", before_in_line, "]-fail-point->[",
                                        rest_of_line, "]].  Problem: [", description, "]."));
} // Script_interpreter::failed()

void Script_interpreter::test_with_timeout(util::Fine_duration timeout,
                                           const Async_task& async_do_and_set_promise_func,
                                           const Task& on_done_func)
{
  using flow::util::ostream_op_string;
  using flow::Fine_clock;
  using boost::chrono::round;
  using boost::chrono::milliseconds;
  using boost::chrono::microseconds;

  Done_promise done_promise(new Done_promise_t);

  FLOW_LOG_INFO("Beginning timed async task with timeout ~[" << round<milliseconds>(timeout) << "].");

  async_do_and_set_promise_func(done_promise);

  auto future = done_promise->get_future();
  const auto started_at = Fine_clock::now();
  const auto wait_result = future.wait_for(timeout);

  if (wait_result == Future_status::timeout)
  {
    failed(false, "Operation did not complete before timeout."); // Throws!
    /* async_do_and_set_promise_func() may still asynchronously do *only* the following:
     *   done_promise->set_value(),
     * which is fine in and of itself, since done_promise is a shared_ptr.  It won't matter now.
     * However async_do_and_set_promise_func() must indeed ensure any thread it starts is *gracefully* stopped
     * stopped due to the exception we just threw as opposed to just running indefinitely; otherwise
     * there's a race between program exit and the above statement executing (is not ideal/is chaotic).
     * Long story short:
     *   - If it creates an ipc::transport I/O object (e.g., Native_socket_stream_acceptor), then stack unwinding
     *     shall gracefully destroy that object, whose destructor shall join its internal worker thread(s) before
     *     returning.
     *   - If it creates an explicit flow::async *_loop, then stack unwinding shall gracefully destroy that _loop,
     *     whose destructor shall join thread(s) in the _loop before returning. */
  }
  // else

  const auto dur = Fine_clock::now() - started_at;
  assert(wait_result == Future_status::ready);
  FLOW_LOG_INFO("Operation completed before timeout.  Duration = ~[" << round<microseconds>(dur) << "] "
                "= ~[" << round<milliseconds>(dur) << "].  Now may check whether it was successful.");

  on_done_func(); // This can analyze result of async_do_and_set_promise_func() and still invoke failed() on failure.
  FLOW_LOG_INFO("No problem: operation (that was subject to timeout) finished without throwing failure.");
} // Script_interpreter::test_with_timeout()

void Script_interpreter::cmd_socket_stream_connect()
{
  using flow::util::ostream_op_string;
  using std::string;

  const auto abs_name = next_required_typed_value<Shared_name>();

  FLOW_LOG_INFO("Connecting a Native_socket_stream to [" << abs_name << "].");

  Native_socket_stream peer(m_ipc_logger, ostream_op_string("=>", abs_name));
  Error_code err_code;
  peer.sync_connect(abs_name, &err_code);

  if (err_code)
  {
    failed(false, "Connect failed.", err_code);
  }
  // else

  m_test_sock_streams.emplace_back(new Native_socket_stream(std::move(peer)));
  // Test Native_socket_stream move ctor, while we're at it. --^
} // Script_interpreter::cmd_socket_stream_connect()

void Script_interpreter::cmd_sleep()
{
  using boost::chrono::round;
  using boost::chrono::milliseconds;
  using flow::util::this_thread::sleep_for;

  const auto dur = next_required_dur();

  FLOW_LOG_INFO("Sleeping for ~[" << round<milliseconds>(dur) << "].");
  sleep_for(dur);
} // Script_interpreter::cmd_socket_stream_connect()

void Script_interpreter::cmd_handle_connect_pair()
{
  using Asio_local_peer_socket = asio_local_stream_socket::Peer_socket;
  namespace asio_local = asio_local_stream_socket::local_ns;
  using flow::util::Task_engine;
  using flow::util::ostream_op_string;
  using boost::array;

  /* It's not really used for anything -- we do non-blocking socketpair() thing, and that's it.  Still need one
   * to be able to make Asio_local_peer_socket at all. */
  Task_engine asio_engine;
  Asio_local_peer_socket asio_sock1(asio_engine);
  Asio_local_peer_socket asio_sock2(asio_engine);

  Error_code sys_err_code;
  asio_local::connect_pair(asio_sock1, asio_sock2, sys_err_code);

  if (sys_err_code)
  {
    failed(false, "asio_local::connect_pair() failed.", sys_err_code);
  }
  // else

  array<Native_handle, 2> socks = { Native_handle(asio_sock1.release()), Native_handle(asio_sock2.release()) };
  // asio_* are now irrelevant (and conceptually empty).

  FLOW_LOG_INFO("Created 2 pre-mutually-connected local native handles:");
  for (auto& sock : socks)
  {
    FLOW_LOG_INFO("Pre-connected native local socket handle [" << sock << "]: "
                  "saved at index [" << m_test_native_hndls.size() << "].");
    assert(!sock.null());
    m_test_native_hndls.emplace_back(std::move(sock));

    // Opportunistically check behavior of Native_handle move ctor.  (assert() could do it but let's be nice.)
    if (!sock.null())
    {
      failed(false, ostream_op_string("Native_handle move ctor should have nullified source object but did not."));
    }
    // else
    assert(!m_test_native_hndls.back().null());
  }
} // Script_interpreter::cmd_handle_connect_pair()

void Script_interpreter::cmd_socket_stream_create_from_hndl()
{
  using flow::util::ostream_op_string;

  const auto hndl_idx = next_required_typed_value<size_t>();
  if (hndl_idx >= m_test_native_hndls.size())
  {
    failed(true,
           ostream_op_string("There is no Native_handle at index [", hndl_idx, "]; "
                             "cannot use it to attempt creating a Native_socket_stream."));
  }
  // else

  const auto& table_hndl = m_test_native_hndls[hndl_idx];
  const size_t sock_stream_idx = m_test_sock_streams.size();

  assert(!table_hndl.null());

  m_test_sock_streams.emplace_back
    (new Native_socket_stream
           (m_ipc_logger, ostream_op_string("namelessTestSock", sock_stream_idx),
            Native_handle(table_hndl)));

  // Opportunistically check behavior of Native_handle copy ctor.  (assert() could do it but let's be nice.)
  if (table_hndl.null())
  {
    failed(false, ostream_op_string("Native_handle copy ctor should not have modified source object but did."));
  }
  // else

  FLOW_LOG_INFO("Creation of Native_socket_stream from Native_handle at index [" << hndl_idx << "] succeeded "
                "yielding peer stream [" << *(m_test_sock_streams.back()) << "]; remote-peer Process_credentials "
                "[" << m_test_sock_streams.back()->remote_peer_process_credentials() << "]; "
                "handle saved at index [" << sock_stream_idx << "].  Source Native_handle: [" << table_hndl << "].");
} // Script_interpreter::cmd_socket_stream_create_from_hndl()

void Script_interpreter::cmd_socket_stream_send()
{
  cmd_socket_stream_sender_send_impl<decltype(m_test_sock_streams)>(m_test_sock_streams, "Native_socket_stream");
}

template<typename Peer_list>
void Script_interpreter::cmd_socket_stream_sender_send_impl(const Peer_list& peers, util::String_view peer_type)
{
  using flow::util::Blob;
  using flow::util::ostream_op_string;
  using flow::util::in_closed_open_range;
  using Buf = util::Blob_const;

  constexpr int NO_HNDL_IDX = -1;

  const auto peer = next_required_peer(peers, peer_type);

  const auto hndl_idx = next_required_typed_value<int>();
  if ((!in_closed_open_range(size_t(0), size_t(hndl_idx), m_test_native_hndls.size()))
      && (hndl_idx != NO_HNDL_IDX))
  {
    failed(true,
           ostream_op_string("Native_handle index [", hndl_idx, "] must be [", NO_HNDL_IDX, "] or "
                             "in range of registered Native_handle indices "
                             "[0, ", m_test_sock_streams.size(), ")."));
  }
  // else

  const auto blob_n = next_required_typed_value<size_t>();

  if ((blob_n == 0) && (hndl_idx == NO_HNDL_IDX))
  {
    failed(true, "Must specify a blob or native handle to send.");
  }
  // else

  const auto exp_err_code_or_success = next_required_err_code();

  // Parsed/validated; further scan the validated values in the same order.

  Native_handle hndl_or_null;
  assert(hndl_or_null.null());
  if (hndl_idx != NO_HNDL_IDX)
  {
    assert(!m_test_native_hndls[hndl_idx].null());
    hndl_or_null = m_test_native_hndls[hndl_idx];

    // Opportunistically check behavior of Native_handle copy assignment.  (assert() could do it but let's be nice.)
    if (m_test_native_hndls[hndl_idx].null())
    {
      failed(false, ostream_op_string("Native_handle copy= should not have modified source object but did."));
    }
  }

  Blob blob_or_null; // Init = null.
  Buf buf_or_null; // Init = 0 size.
  if (blob_n != 0)
  {
    blob_or_null = test_blob(blob_n);
    buf_or_null = blob_or_null.const_buffer();
  }

  FLOW_LOG_INFO("Attempting send via peer; payload: Native_handle [" << hndl_or_null << "], "
                "blob sized [" << blob_or_null.size() << "]; "
                "expected result = [" << exp_err_code_or_success << "] [" << exp_err_code_or_success.message() << "].");

  Error_code err_code;
  peer->send_native_handle(hndl_or_null, buf_or_null, &err_code);

  if (err_code != exp_err_code_or_success)
  {
    failed(false,
           // Subtlety: err_code is printed by failed()... but omitted if it == success; so print it here just in case.
           ostream_op_string("Op result [", err_code, "] was not as expected (see just above)."),
           err_code);
  }
  // else

  FLOW_LOG_INFO("Op behaved as expected.");
} // Script_interpreter::cmd_socket_stream_sender_send_impl()

void Script_interpreter::cmd_socket_stream_send_blob()
{
  cmd_blob_sender_send_blob_impl<decltype(m_test_sock_streams)>(m_test_sock_streams, "Native_socket_stream");
} // Script_interpreter::cmd_blob_stream_mq_peer_send()

template<typename Peer_list>
void Script_interpreter::cmd_blob_sender_send_end_impl(const Peer_list& peers, util::String_view peer_type)
{
  using flow::util::ostream_op_string;

  const auto peer = next_required_peer(peers, peer_type);
  const auto dupe_call_or_not = next_required_typed_value<bool>();
  const auto exp_err_code_or_success = next_required_err_code();
  const auto timeout = next_required_dur();

  if (dupe_call_or_not)
  {
    FLOW_LOG_INFO("Attempting dupe send-graceful-close via peer; expected result = instant failure; variation: "
                  "with (dummy) callback.");
    bool ok = peer->async_end_sending([](const Error_code&)
    {
      assert(false && "This callback should never be executed on dupe end_sending(); bug in the latter; "
                      "but we did not set up proper test-failure detection due to the whole halting-problem thing.");
    });
    if (ok)
    {
      failed(false,
             "Peer::async_end_sending(cb) should have instantly returned fail but did not.");
    }
    // else
    FLOW_LOG_INFO("Same but: variation: without callback.");
    if (peer->end_sending())
    {
      failed(false,
             "Peer::end_sending(void) should have instantly returned fail but did not.");
    }
    // else
    return;
  }
  // else if (!dupe_call_or_not)

  FLOW_LOG_INFO("Attempting send-graceful-close via peer; "
                "expected result = [" << exp_err_code_or_success << "] [" << exp_err_code_or_success.message() << "].");

  bool insta_fail;
  struct State
  {
    Error_code m_err_code;
  };
  boost::shared_ptr<State> state(new State);
  auto& err_code = state->m_err_code;

  // Do this (potentially) async first.
  auto async_task = [&](const Done_promise& done_promise)
  {
    insta_fail = (!peer->async_end_sending([state, done_promise](const Error_code& async_err_code)
    {
      state->m_err_code = async_err_code;
      done_promise->set_value();
    }));
    if (insta_fail)
    {
      done_promise->set_value(); // Completion handler won't run; we are immediately done.
    }
  };

  // If it [a]sync-completes before timeout, then the following will (synchronously) run.
  test_with_timeout(timeout, async_task, [&]()
  {
    if (insta_fail)
    {
      failed(false, "async_end_sending() should have instantly returned initial success, "
                      "meaning not a wrong-state call; but returned failure instead.");
    }
    // else

    if (err_code != exp_err_code_or_success)
    {
      // Subtlety: err_code is printed by failed()... but omitted if it == success; so print it here just in case.
      failed(false, ostream_op_string("Op result [", err_code, "] [", err_code.message(), "] "
                                        "was not as expected (see just above)."),
             err_code);
    }
  }); // test_with_timeout()
  /* Otherwise it'll throw which shall at some point (assuming our owner is cool) gracefully destroy *this
   * which shall destroy `peers` which shall shall destroy *peer (hence join its internal thread(s)). */
} // Script_interpreter::cmd_blob_sender_send_end_impl()

void Script_interpreter::cmd_socket_stream_send_end()
{
  cmd_blob_sender_send_end_impl<decltype(m_test_sock_streams)>(m_test_sock_streams, "Native_socket_stream");
}

template<typename Peer_list>
void Script_interpreter::cmd_blob_sender_send_blob_impl(const Peer_list& peers, util::String_view peer_type)
{
  using flow::util::Blob;
  using flow::util::ostream_op_string;
  using Buf = util::Blob_const;

  const auto peer = next_required_peer(peers, peer_type);

  const auto blob_n = next_required_typed_value<size_t>();
  if (blob_n == 0)
  {
    failed(true, "Must specify a positive blob size for blob to send.");
  }
  // else

  const auto exp_err_code_or_success = next_required_err_code();

  // Parsed/validated.
  const Blob blob = test_blob(blob_n);
  const Buf buf(blob.const_buffer());

  FLOW_LOG_INFO("Attempting send via peer; payload blob sized [" << blob.size() << "]; "
                "expected result = [" << exp_err_code_or_success << "] [" << exp_err_code_or_success.message() << "].");

  Error_code err_code;
  peer->send_blob(buf, &err_code);

  if (err_code != exp_err_code_or_success)
  {
    failed(false,
           // Subtlety: err_code is printed by failed()... but omitted if it == success; so print it here just in case.
           ostream_op_string("Op result [", err_code, "] was not as expected (see just above)."),
           err_code);
  }
  // else

  FLOW_LOG_INFO("Op behaved as expected.");
} // Script_interpreter::cmd_blob_sender_send_blob_impl()

void Script_interpreter::cmd_socket_stream_recv()
{
  cmd_socket_stream_receiver_recv_impl<decltype(m_test_sock_streams)>(m_test_sock_streams, "Native_socket_stream");
}

template<typename Peer_list>
void Script_interpreter::cmd_socket_stream_receiver_recv_impl(const Peer_list& peers, util::String_view peer_type)
{
  using flow::util::Blob;
  using flow::util::ostream_op_string;
  using boost::shared_ptr;

  const auto peer = next_required_peer(peers, peer_type);
  const auto blob_n = next_required_typed_value<size_t>();
  const auto exp_err_code_or_success = next_required_err_code();
  const auto exp_hndl_or_not = next_required_typed_value<bool>();
  const auto exp_blob_n = next_required_typed_value<size_t>();

  if (exp_blob_n > blob_n)
  {
    failed(true, ostream_op_string("Expected blob size [", exp_blob_n, "] must not exceed "
                                   "buffer space [", blob_n, "] but does."));
  }
  // else

  const auto timeout = next_required_dur();

  // Parsed/validated.

  FLOW_LOG_INFO("Attempting receive via peer; expected payload: Native_handle? = [" << exp_hndl_or_not << "], "
                "blob sized [" << exp_blob_n << "] in buffer sized [" << blob_n << "]; "
                "expected result = [" << exp_err_code_or_success << "] [" << exp_err_code_or_success.message() << "].");

  bool insta_fail;
  struct State
  {
    Native_handle m_hndl;
    Blob m_blob;
    size_t m_n_rcvd;
    Error_code m_err_code;
  };
  shared_ptr<State> state(new State);
  auto& hndl = state->m_hndl;
  auto& blob = state->m_blob;
  auto& n_rcvd = state->m_n_rcvd;
  auto& err_code = state->m_err_code;

  blob = Blob(m_ipc_logger, blob_n); // .zero() iff blob_n is zero.

  // Do this (potentially) async first.
  auto async_task = [&](const Done_promise& done_promise)
  {
    insta_fail
      = (!peer->async_receive_native_handle(&hndl, blob.mutable_buffer(),
                                            [state, done_promise]
                                              (const Error_code& async_err_code, size_t async_n_rcvd)
    {
      state->m_err_code = async_err_code;
      state->m_n_rcvd = async_n_rcvd;
      done_promise->set_value();
    }));
    if (insta_fail)
    {
      done_promise->set_value(); // Completion handler won't run; we are immediately done.
    }
  }; // async_task =

  // If it [a]sync-completes before timeout, then the following will (synchronously) run.
  test_with_timeout(timeout, async_task, [&]()
  {
    if (insta_fail)
    {
      failed(false, "async_receive_native_handle() "
                      "should have instantly returned initial success, "
                      "meaning not a wrong-state call; but returned failure instead.");
    }
    // else

    if (err_code != exp_err_code_or_success)
    {
      // Subtlety: err_code is printed by failed()... but omitted if it == success; so print it here just in case.
      failed(false, ostream_op_string("Op result [", err_code, "] [", err_code.message(), "] "
                                        "was not as expected (see just above)."),
             err_code);
    }
    // else

    if (err_code)
    {
      if (n_rcvd != 0)
      {
        failed(false, ostream_op_string("Op returned (expected) failure code with n-rcvd [", n_rcvd, "], "
                                          "but it must be zero on failure.",
               err_code));
      }
      // else
      return; // Fail (expected) => nothing more to do.
    }
    // else if (!err_code)

    blob.resize(n_rcvd);

    validate_rcvd_blob_contents(blob, exp_blob_n); // Throw on error.

    if ((!hndl.null()) != exp_hndl_or_not)
    {
      failed(false, "Op returned success (as expected), with expected meta-blob contents (if any), "
                      "but native handle was received but not expected, or vice versa.");
    }
    // else

    if (exp_hndl_or_not)
    {
      FLOW_LOG_INFO("Received native local handle [" << hndl << "]: "
                    "saved at index [" << m_test_native_hndls.size() << "].");
      m_test_native_hndls.emplace_back(std::move(hndl));
      assert(hndl.null()); // We check this with failed() in another cmd/test; assert() here is fine.
    }
  }); // test_with_timeout()
  /* Otherwise it'll throw which shall at some point (assuming our owner is cool) gracefully destroy *this
   * which shall destroy m_test_* which shall shall destroy *peer (hence join its internal thread(s)). */
} // Script_interpreter::cmd_socket_stream_receiver_recv_impl()

void Script_interpreter::cmd_socket_stream_recv_blob()
{
  cmd_blob_receiver_recv_blob_impl<decltype(m_test_sock_streams)>(m_test_sock_streams, "Native_socket_stream");
}

template<typename Peer_list>
void Script_interpreter::cmd_blob_receiver_recv_blob_impl(const Peer_list& peers, util::String_view peer_type)
{
  using flow::util::Blob;
  using flow::util::ostream_op_string;
  using boost::shared_ptr;

  const auto peer = next_required_peer(peers, peer_type);
  const auto blob_n = next_required_typed_value<size_t>();
  const auto exp_err_code_or_success = next_required_err_code();

  if ((!exp_err_code_or_success) && (blob_n == 0))
  {
    failed(true, "Empty blobs not allowed for Blob_receiver/sender concepts; must specify a positive blob size for "
                 "blob to receive.");
  }
  // else

  const auto exp_blob_n = next_required_typed_value<size_t>();

  if (exp_blob_n > blob_n)
  {
    failed(true, ostream_op_string("Expected blob size [", exp_blob_n, "] must not exceed "
                                   "buffer space [", blob_n, "] but does."));
  }
  // else

  const auto timeout = next_required_dur();

  // Parsed/validated.

  FLOW_LOG_INFO("Attempting receive via peer; expected payload blob sized [" << exp_blob_n << "] "
                "in buffer sized [" << blob_n << "]; expected result = "
                "[" << exp_err_code_or_success << "] [" << exp_err_code_or_success.message() << "].");

  struct State
  {
    Blob m_blob;
    size_t m_n_rcvd;
    Error_code m_err_code;
  };
  shared_ptr<State> state(new State);
  auto& blob = state->m_blob;
  auto& n_rcvd = state->m_n_rcvd;
  auto& err_code = state->m_err_code;

  blob = Blob(m_ipc_logger, blob_n);

  // Do this (potentially) async first.
  auto async_task = [&](const Done_promise& done_promise)
  {
    peer->async_receive_blob(blob.mutable_buffer(),
                             [state, done_promise]
                               (const Error_code& async_err_code, size_t async_n_rcvd)
    {
      state->m_err_code = async_err_code;
      state->m_n_rcvd = async_n_rcvd;
      done_promise->set_value();
    });
  }; // async_task =

  // If it [a]sync-completes before timeout, then the following will (synchronously) run.
  test_with_timeout(timeout, async_task, [&]()
  {
    if (err_code != exp_err_code_or_success)
    {
      // Subtlety: err_code is printed by failed()... but omitted if it == success; so print it here just in case.
      failed(false, ostream_op_string("Op result [", err_code, "] [", err_code.message(), "] "
                                        "was not as expected (see just above)."),
             err_code);
    }
    // else

    if (err_code)
    {
      if (n_rcvd != 0)
      {
        failed(false, ostream_op_string("Op returned (expected) failure code with n-rcvd [", n_rcvd, "], "
                                          "but it must be zero on failure.",
               err_code));
      }
      // else
      return; // Fail (expected) => nothing more to do.
    }
    // else if (!err_code)

    blob.resize(n_rcvd);

    validate_rcvd_blob_contents(blob, exp_blob_n); // Throw on error.
  }); // test_with_timeout()
  /* Otherwise it'll throw which shall at some point (assuming our owner is cool) gracefully destroy *this
   * which shall destroy m_test_sock_streams which shall shall destroy *sock_stm (hence join its internal thread(s)). */
} // Script_interpreter::cmd_blob_receiver_recv_blob_impl()

flow::util::Blob Script_interpreter::test_blob(size_t n) const
{
  flow::util::Blob blob(m_ipc_logger, n); // size() = capacity() = blob_n.
  for (size_t idx = 0; idx != n; ++idx)
  {
    (blob.data())[idx] = S_TEST_BLOB_CONTENTS[idx % S_TEST_BLOB_CONTENTS.size()];
  }
  return blob;
}

void Script_interpreter::validate_rcvd_blob_contents(const flow::util::Blob& blob, size_t exp_blob_n)
{
  using flow::util::buffers_dump_string;
  using flow::util::ostream_op_string;

  if (blob.size() != exp_blob_n)
  {
    failed(false, ostream_op_string("Op returned success (as expected), but the blob is of unexpected "
                                    "size [", blob.size(), "]; blob contents = "
                                    "[\n", buffers_dump_string(blob.const_buffer(), "  "), "]."));
  }
  // else

  for (size_t idx = 0; idx != exp_blob_n; ++idx)
  {
    if ((blob.const_data())[idx] != S_TEST_BLOB_CONTENTS[idx % S_TEST_BLOB_CONTENTS.size()])
    {
      failed(false, ostream_op_string("Op returned success (as expected) and blob of expected "
                                      "size; but the contents do not match the test-generated expected repeated "
                                      "pattern [", S_TEST_BLOB_CONTENTS, "]; blob contents = "
                                      "[\n", buffers_dump_string(blob.const_buffer(), "  "), "]."));
    }
    // else continue.
  }
  // Got here.
} // Script_interpreter::validate_rcvd_blob_contents()

template<typename Peer_list>
typename Peer_list::value_type&
  Script_interpreter::next_required_peer_ref(Peer_list& peers, util::String_view peer_type)
{
  const auto peer_idx = next_required_typed_value<size_t>();
  if (peer_idx >= peers.size())
  {
    failed(true,
           ostream_op_string("There is no peer (type [", peer_type, "]) at index [", peer_idx, "]."));
  }
  // else
  auto& peer = peers[peer_idx];

  FLOW_LOG_INFO("Looked up peer (type [" << peer_type << "]) at index [" << peer_idx << "]: "
                "[" << *peer << "].");
  return peer;
}

template<typename Peer_list>
const typename Peer_list::value_type&
  Script_interpreter::next_required_peer(const Peer_list& peers, util::String_view peer_type) const
{
  // This is well-known OK use of const_cast<>.
  return const_cast<Script_interpreter*>(this)->next_required_peer_ref(const_cast<Peer_list&>(peers), peer_type);
}


Error_code Script_interpreter::next_required_err_code()
{
  using error::Code;

  const auto code = next_required_typed_value<Code>();
  Error_code exp_err_code_or_success; // Init = success.
  if (code != Code::S_END_SENTINEL)
  {
    exp_err_code_or_success = code; // Overwrite with a transport::error Error_code if so specified.
  }
  return exp_err_code_or_success;
}

void Script_interpreter::cmd_socket_stream_acceptor_accept()
{
  using std::string;
  using flow::util::ostream_op_string;

  const auto sock_stm_acc_idx = next_required_typed_value<size_t>();
  auto n = next_required_typed_value<unsigned int>();
  const auto timeout = next_required_dur();

  if (n < 1)
  {
    failed(true,
           ostream_op_string("The n-calls arg value [", n, "] to [", S_KEYWD_SOCK_STREAM_ACCEPTOR_ACCEPT, "] "
                             "must be positive."));
  }
  // else

  if (sock_stm_acc_idx >= m_test_sock_stm_acceptors.size())
  {
    failed(true,
           ostream_op_string("There is no Native_socket_stream_acceptor at index [", sock_stm_acc_idx, "]; "
                             "cannot use it to attempt accepting a Native_socket_stream."));
  }
  // else

  auto& acceptor = *m_test_sock_stm_acceptors[sock_stm_acc_idx];
  FLOW_LOG_INFO("Accepting Native_socket_stream [" << n << "] times at Native_socket_stream_acceptor at index "
                "[" << sock_stm_acc_idx << "] [" << acceptor << "].");

  /* Ensures it won't get re-allocated mid-loop below.  The on-accept handlers "could" execute concurrently,
   * and a re-alloc of the vector could be bad, since the handlers access disjoint elements in it.
   * In fact by _acceptor contract they must be serial, but there's no need to create more entropy; and we won't
   * be checking the non-concurrency as part of the test.  @todo Should we? */
  m_test_sock_streams.reserve(m_test_sock_streams.size() + n);

  struct State
  {
    // Final result.
    bool m_ok;
    // Track how many remain before async op finished.
    decltype(n) m_n_left;
    // Ensure they're accepted/rejected in FIFO order.
    size_t m_next_stream_idx;
    // The resulting peers.
    std::vector<sync_io::Native_socket_stream> m_peers;
  };
  boost::shared_ptr<State> state(new State{ true, n, 0, {} });
  auto& ok = state->m_ok;
  auto& peers = state->m_peers;
  peers.resize(n);

  // Do this (potentially) async first.
  auto async_task = [&](const Done_promise& done_promise)
  {
    for (decltype(n) idx = 0; idx != n; ++idx)
    {
      const auto this_streams_idx = idx;

      acceptor.async_accept(&(peers[idx]),
                            [this, state, done_promise, this_streams_idx, sock_stm_acc_idx]
                              (const Error_code& async_err_code)
      {
        auto& ok = state->m_ok;
        auto& n_left = state->m_n_left;
        auto& next_streams_idx = state->m_next_stream_idx;
        auto& peers = state->m_peers;

        if (async_err_code)
        {
          FLOW_LOG_WARNING("Accept at Native_socket_stream_acceptor index [" << sock_stm_acc_idx << "] failed.  "
                           "Details follow; outstanding accepts = [" << (n_left - 1) << "].  Since 1+ requested "
                           "accept(s) failed, the test will fail upon completion even if done before timeout.");

          const auto sys_err_code = async_err_code;
          FLOW_ERROR_SYS_ERROR_LOG_WARNING();

          ok = false;
          // Keep going though... finish it out.
        }
        else if (this_streams_idx != (next_streams_idx++))
        {
          FLOW_LOG_WARNING("Accept at Native_socket_stream_acceptor index [" << sock_stm_acc_idx << "] "
                           "arrived out of order; handle index [" << (next_streams_idx - 1) << "] expected; yet "
                           "index [" << this_streams_idx << "] request was satisfied instead. "
                           "The test will fail upon completion even if done before timeout with all successful "
                           "accepts.");
          ok = false;
          // Keep going though... finish it out.
        }
        else
        {
          const auto& peer = peers[this_streams_idx];
          FLOW_LOG_INFO("Accept at Native_socket_stream_acceptor index [" << sock_stm_acc_idx << "] succeeded yielding "
                        "peer stream [" << peer << "]; remote-peer Process_credentials "
                        "[" << peer.remote_peer_process_credentials() << "]; "
                        "handle saved at index [" << this_streams_idx << "]; outstanding accepts = "
                        "[" << (n_left - 1) << "].");
        }

        if ((--n_left) == 0)
        {
          done_promise->set_value(); // No more async work left!
        }
      }); // acceptor.async_accept()
    } // for (idx in [0, n))
  }; // async_task =

  // If it async-completes before timeout, then the following will (synchronously) run.
  test_with_timeout(timeout, async_task, [&]()
  {
    if (!ok)
    {
      failed(false, "Something went wrong with 1 or more of the attempted accepts (see above).");
    }
    // else

    for (auto& peer : peers)
    {
      FLOW_LOG_INFO("Saving peer stream [" << peer << "] handle at index [" << m_test_sock_streams.size() << "].");
      // Exercise NSS sync_io-core-adopting ctor = nice.
      m_test_sock_streams.emplace_back(new Native_socket_stream(std::move(peer)));
    }
  });
  /* Otherwise it'll throw which shall at some point (assuming our owner is cool) gracefully destroy *this
   * which shall destroy m_test_sock_stm_acceptors and m_test_sock_streams which shall shall destroy any
   * added streams and `acceptor` (hence join their internal threads). */
} // Script_interpreter::cmd_socket_stream_acceptor_accept()

void Script_interpreter::cmd_socket_stream_acceptor_listen()
{
  using std::string;
  using flow::util::ostream_op_string;
  using boost::shared_ptr;

  const auto abs_name = next_required_typed_value<Shared_name>();

  m_test_sock_stm_acceptors.push_back(shared_ptr<Native_socket_stream_acceptor>());
  const size_t sock_stm_acc_idx = m_test_sock_stm_acceptors.size() - 1;

  FLOW_LOG_INFO("Setting up a Native_socket_stream_acceptor at name [" << abs_name << "].");

  Error_code sys_err_code;
  m_test_sock_stm_acceptors[sock_stm_acc_idx]
    .reset(new Native_socket_stream_acceptor(m_ipc_logger, abs_name, &sys_err_code));

  if (sys_err_code)
  {
    failed(false,
           ostream_op_string("Acceptor-listen at name [",  abs_name, "] failed."), sys_err_code);
  }
  // else

  FLOW_LOG_INFO("Acceptor-listen at name [" << abs_name << "] succeeded yielding "
                "acceptor [" << *(m_test_sock_stm_acceptors.back()) << "]; "
                "handle saved at index [" << sock_stm_acc_idx << "].");
} // Script_interpreter::cmd_socket_stream_acceptor_listen()

template<typename Mq_peer>
void Script_interpreter::cmd_blob_stream_mq_peer_create()
{
  using flow::util::ostream_op_string;
  using boost::movelib::unique_ptr;
  using boost::system::system_category;
  using std::type_index;
  using std::get;
  using Mq = typename Mq_peer::Mq;

  const auto abs_name = next_required_typed_value<Shared_name>();
  const auto create_else_open = next_required_typed_value<bool>();
  const auto max_n_msg = next_required_typed_value<size_t>();
  const auto max_msg_sz = next_required_typed_value<size_t>();
  const auto exp_create_fail = next_required_typed_value<bool>();
  const auto exp_err_code_or_success = next_required_err_code();

  unique_ptr<Mq> mq;
  Error_code err_code;
  if (create_else_open)
  {
    if (max_n_msg == 0)
    {
      failed(true, ostream_op_string("max_n_msg argument must be positive."));
    }
    if (max_msg_sz == 0)
    {
      failed(true, ostream_op_string("max_msg_sz argument must be positive."));
    }

    FLOW_LOG_INFO("Creating handle/MQ at [" << abs_name << "] (type [" << typeid(Mq_peer).name() << "]) "
                  "at max [" << max_n_msg << "] msgs with "
                  "size limit [" << max_msg_sz << "] for each; expecting already-exists? = "
                  "[" << exp_create_fail << "].");

    mq.reset(new Mq(m_ipc_logger, abs_name, util::CREATE_ONLY, max_n_msg, max_msg_sz, util::Permissions(),
                    &err_code));
  }
  else
  {
    FLOW_LOG_INFO("Creating handle/opening MQ at [" << abs_name << "] (type [" << typeid(Mq_peer).name() << "]); "
                  "expecting name-not-found? = [" << exp_create_fail << "].");
    mq.reset(new Mq(m_ipc_logger, abs_name, util::OPEN_ONLY, &err_code));
  }

  if (exp_create_fail)
  {
    const Error_code exp_err_code(create_else_open ? EEXIST : ENOENT, system_category());
    if (err_code != exp_err_code)
    {
      // Subtlety: err_code is printed by failed()... but omitted if it == success; so print it here just in case.
      failed(false, ostream_op_string("When creating MQ handle expected error [", exp_err_code, "] [",
                                      exp_err_code.message(), "] but got [", err_code, "] [", err_code.message(), "] "
                                      "instead."),
             err_code);
    }
    // else:
    return; // Nothing more to do.
  }
  // else
  if (err_code) // && (!exp_create_fail)
  {
    failed(false, "When creating MQ handle expected success but got something else instead.",
           err_code);
  }
  // else: Expected no error and got no error.  Continue.

  FLOW_LOG_INFO("Wrapping that by a peer blob-stream; expecting result [" << exp_err_code_or_success << "] "
                "[" << exp_err_code_or_success.message() << "].");

  // Got handle (and ensured MQ exists thereby).  Now create blob stream peer and move handle into it.
  auto mq_peer = boost::movelib::make_unique<Mq_peer>
                   (m_ipc_logger, abs_name.str(), // <-- nickname for logging only.
                    std::move(*mq), &err_code);
  if (err_code != exp_err_code_or_success)
  {
    failed(false,
           // Subtlety: err_code is printed by failed()... but omitted if it == success; so print it here just in case.
           ostream_op_string("Op result [", err_code, "] was not as expected (see just above)."),
           err_code);
  }
  // else

  if (err_code)
  {
    FLOW_LOG_INFO("Op behaved as expected.");
    return; // Nothing more to do.
  }
  // else

  auto& mq_peer_list = get<Mq_peer_list<Mq_peer>>(m_test_blob_stream_mq_peers[type_index(typeid(Mq_peer))]);
  mq_peer_list.emplace_back(std::move(mq_peer));
  FLOW_LOG_INFO("Op behaved as expected; saved MQ blob stream (type [" << typeid(Mq_peer).name() << "]) at index "
                "[" << (mq_peer_list.size() - 1) << "] for that type's list.");
} // Script_interpreter::cmd_blob_stream_mq_peer_create()

template<typename Mq_peer>
void Script_interpreter::cmd_blob_stream_mq_peer_destroy()
{
  using flow::async::Single_thread_task_loop;
  using flow::async::Synchronicity;
  using std::type_index;
  using std::get;

  auto& mq_peer_list = get<Mq_peer_list<Mq_peer>>(m_test_blob_stream_mq_peers[type_index(typeid(Mq_peer))]);
  auto& mq_peer = next_required_peer_ref(mq_peer_list, typeid(Mq_peer).name());

  const auto timeout = next_required_dur();

  FLOW_LOG_INFO("Destroying MQ blob stream.  This can slightly block.");

  /* We just want to do `mq_peer.reset();` which can slightly block; hence the timeout.
   * Naturally-ish there's no async API for that; so we have to start a thread ourselves to comply with
   * test_with_timeout().  Technically, it could run indefinitely (if there's a mega-bug); in that case
   * the timeout will be detected and shown to the user... but the present method will then sit there waiting
   * for the .reset() task to complete before that thread can be joined.  Not great -- but nothing we can do by
   * definition short of aborting process/thread/whatever. */

  Single_thread_task_loop thread(nullptr, // Skip the spam; null logger.
                                 "mq_peer_destroy");
  thread.start();

  auto async_task = [&](const Done_promise& done_promise)
  {
    thread.post([mq_peer = std::move(mq_peer), done_promise]() mutable
    {
      mq_peer.reset(); // <-- !!!
      done_promise->set_value();
    });
  };

  test_with_timeout(timeout, async_task, [](){});
  // Whether that succeeded in time out threw, `thread` will be joined here gracefully.
} // cmd_blob_stream_mq_peer_destroy()

template<typename Chan_bundle>
void Script_interpreter::cmd_chan_bundle_create()
{
  using flow::util::ostream_op_string;
  using flow::error::Runtime_error;
  using boost::movelib::unique_ptr;
  using std::type_index;
  using std::get;
  using std::swap;
  using Mq = typename Chan_bundle::Mq;

  const auto mq_abs_name_prefix = next_required_typed_value<Shared_name>();
  const auto mq_create_else_open = next_required_typed_value<bool>();

  const auto sock_stm = next_required_peer(m_test_sock_streams, "Native_socket_stream");

  constexpr size_t MAX_N_MSG = 10;
  constexpr size_t MAX_MSG_SZ = 8 * 1024;

  /* Create the sender Mq and receiver Mq objects (creating or opening underlying MQ depending on mq_create_else_open).
   * Errors are not OK; we test that stuff in other commands; here we are bundling things together and leave those
   * details be.  While we're at it, test throwing-mode error reporting (null Error_code*); typically we use
   * the other mode (non-null Error_code*).  Well -- sort of test; the test will fail if it's thrown; but at least
   * it's possible to exercise.  @todo Test throwing mode outright at some point somewhere. */

  /* Just to not get confused... there are 2 sides, our side and their side.  Cosmically speaking, though, they are
   * "equal" peers, as the connection is established from the start by definition.  The socket stream is by definition
   * a bidirectional pipe (or 2 unidirectional pipes in other words).  But the MQs are each used as unidirectional
   * pipes.  So we name one side A, the other B; hence one MQ will be called <...>/a2b, the other <...>/b2a.
   * Now we are just one side, so we are either A or B.  The way we choose to decide which one we are is whether
   * we were told to create the MQs or merely open it.  The other side will be told to do the opposite.  So we just
   * choose the convention that we're side A if creating MQ, side B otherwise.  Could've been the opposite, or we
   * could've made them specify this explicitly; but who cares; this is a reasonable convention for the test.
   *
   * So therefore our sender MQ handle (Mq) will create <...>/a2b (since we're A), receiver will open <...>/b2a;
   * unless we're opening the existing MQs; then the opposite symmetrically. */
  unique_ptr<Mq> mq_out;
  unique_ptr<Mq> mq_in;
  auto mq_out_abs_name = mq_abs_name_prefix / "a2b";
  auto mq_in_abs_name = mq_abs_name_prefix / "b2a";
  if (!mq_create_else_open)
  {
    swap(mq_out_abs_name, mq_in_abs_name);
  }

  try
  {
    if (mq_create_else_open)
    {
      FLOW_LOG_INFO("Creating out-pipe handle/MQ at [" << mq_out_abs_name << "] (type [" << typeid(Mq).name() << "]) "
                    "at generous limit config value; expecting success; else exception thrown => test fails.");
      mq_out.reset(new Mq(m_ipc_logger, mq_out_abs_name,
                          util::CREATE_ONLY, MAX_N_MSG, MAX_MSG_SZ, util::Permissions())); // Can throw.
      FLOW_LOG_INFO("Done; creating in-pipe handle/MQ at [" << mq_in_abs_name << "]; same deal.");
      mq_in.reset(new Mq(m_ipc_logger, mq_in_abs_name,
                         util::CREATE_ONLY, MAX_N_MSG, MAX_MSG_SZ, util::Permissions())); // Can throw.
    }
    else
    {
      FLOW_LOG_INFO("Creating out-pipe handle/opening MQ at [" << mq_out_abs_name << "] "
                    "(type [" << typeid(Mq).name() << "]); expecting success; else exception thrown => test fails.");
      mq_out.reset(new Mq(m_ipc_logger, mq_out_abs_name, util::OPEN_ONLY));
      FLOW_LOG_INFO("Done; creating in-pipe handle/opening MQ at [" << mq_in_abs_name << "]; same deal.");
      mq_in.reset(new Mq(m_ipc_logger, mq_in_abs_name, util::OPEN_ONLY));
    }

    /* Got here: mq_out, mq_in are ready.  We have sock_stm also.  We can now bundle them all into
     * Mqs_socket_stream_channel.
     * As advertised, as we do so, the sock_stm in the slot gets "eaten" (becomes a NULL-state Native_socket_stream).
     * This happens via move-semantics. So the sock_stm in the slot becomes unusable by itself. */

    FLOW_LOG_INFO("Wrapping all 3 peers [" << *mq_in << "] [" << *mq_out << "] [" << *sock_stm << "] in a new "
                  "Channel (type [" << typeid(Chan_bundle).name() << "]).");

    auto& chan_bundle_list = get<Chan_bundle_list<Chan_bundle>>(m_test_chan_bundles[type_index(typeid(Chan_bundle))]);
    const auto chan_idx = chan_bundle_list.size();

    // See Mqs_socket_stream_channel ctor docs in channel.hpp.
    auto chan_bundle = boost::movelib::make_unique<Chan_bundle>
                         (m_ipc_logger,
                          ostream_op_string("chan-idx", chan_idx), // <-- nickname for logging only.
                          std::move(*mq_out), std::move(*mq_in), std::move(*sock_stm)); // Can throw.

    chan_bundle_list.emplace_back(std::move(chan_bundle));
    FLOW_LOG_INFO("Done; saved Channel (type [" << typeid(Chan_bundle).name() << "]) at index "
                  "[" << chan_idx << "] for that type's list.");
  }
  catch (const Runtime_error& exc)
  {
    failed(false,
           ostream_op_string("Caught Runtime_error while creating an MQ handle; exc-what = [", exc.what(), "]."),
           exc.code());
  }
  catch (const std::exception& exc)
  {
    failed(false,
           ostream_op_string("Caught exception that was not Runtime_error; please investigate; might be a bug; "
                               "exc-what = [", exc.what(), "]."));
  }
} // Script_interpreter::cmd_chan_bundle_create()

/* @todo I haven't checked but intuitively these 2 (and maybe the NSS counterpart) can all be written
 * as one function template.  Same for _send_end(), _recv_blob() (maybe _send_native_handle(), _recv_native_handle()).
 * Maybe even ALL of these as one template *total*, if the function to call can be a tparam. */
template<typename Mq_peer>
void Script_interpreter::cmd_blob_stream_mq_peer_send_blob()
{
  using std::type_index;
  using std::get;

  const auto& mq_peer_list = get<Mq_peer_list<Mq_peer>>(m_test_blob_stream_mq_peers[type_index(typeid(Mq_peer))]);
  cmd_blob_sender_send_blob_impl<Mq_peer_list<Mq_peer>>(mq_peer_list, typeid(Mq_peer).name());
}

template<typename Chan_bundle>
void Script_interpreter::cmd_chan_bundle_send_blob()
{
  using std::type_index;
  using std::get;

  const auto& chan_bundle_list
    = get<Chan_bundle_list<Chan_bundle>>(m_test_chan_bundles[type_index(typeid(Chan_bundle))]);
  cmd_blob_sender_send_blob_impl<Chan_bundle_list<Chan_bundle>>(chan_bundle_list, typeid(Chan_bundle).name());
}

template<typename Chan_bundle>
void Script_interpreter::cmd_chan_bundle_send()
{
  using std::type_index;
  using std::get;

  const auto& chan_bundle_list
    = get<Chan_bundle_list<Chan_bundle>>(m_test_chan_bundles[type_index(typeid(Chan_bundle))]);
  cmd_socket_stream_sender_send_impl<Chan_bundle_list<Chan_bundle>>(chan_bundle_list, typeid(Chan_bundle).name());
}

template<typename Mq_peer>
void Script_interpreter::cmd_blob_stream_mq_peer_send_end()
{
  using std::type_index;
  using std::get;

  const auto& mq_peer_list = get<Mq_peer_list<Mq_peer>>(m_test_blob_stream_mq_peers[type_index(typeid(Mq_peer))]);
  cmd_blob_sender_send_end_impl<Mq_peer_list<Mq_peer>>(mq_peer_list, typeid(Mq_peer).name());
}

template<typename Chan_bundle>
void Script_interpreter::cmd_chan_bundle_send_end()
{
  using std::type_index;
  using std::get;

  const auto& chan_bundle_list
    = get<Chan_bundle_list<Chan_bundle>>(m_test_chan_bundles[type_index(typeid(Chan_bundle))]);
  cmd_blob_sender_send_end_impl<Chan_bundle_list<Chan_bundle>>(chan_bundle_list, typeid(Chan_bundle).name());
}

template<typename Mq_peer>
void Script_interpreter::cmd_blob_stream_mq_peer_recv_blob()
{
  using std::type_index;
  using std::get;

  const auto& mq_peer_list = get<Mq_peer_list<Mq_peer>>(m_test_blob_stream_mq_peers[type_index(typeid(Mq_peer))]);
  cmd_blob_receiver_recv_blob_impl<Mq_peer_list<Mq_peer>>(mq_peer_list, typeid(Mq_peer).name());
}

template<typename Chan_bundle>
void Script_interpreter::cmd_chan_bundle_recv_blob()
{
  using std::type_index;
  using std::get;

  const auto& chan_bundle_list
    = get<Chan_bundle_list<Chan_bundle>>(m_test_chan_bundles[type_index(typeid(Chan_bundle))]);
  cmd_blob_receiver_recv_blob_impl<Chan_bundle_list<Chan_bundle>>(chan_bundle_list, typeid(Chan_bundle).name());
}

template<typename Chan_bundle>
void Script_interpreter::cmd_chan_bundle_recv()
{
  using std::type_index;
  using std::get;

  const auto& chan_bundle_list
    = get<Chan_bundle_list<Chan_bundle>>(m_test_chan_bundles[type_index(typeid(Chan_bundle))]);
  cmd_socket_stream_receiver_recv_impl<Chan_bundle_list<Chan_bundle>>(chan_bundle_list, typeid(Chan_bundle).name());
}

template<typename Strings_map>
std::string Script_interpreter::map_keys_str(const Strings_map& map) // Static.
{
  using util::String_view;
  using boost::algorithm::join;
  using std::vector;
  using std::string;
  using std::transform;

  vector<string> keys(map.size());
  transform(map.begin(), map.end(),
            keys.begin(), [&](const Keyword_to_interpret_func_map::value_type& key_and_func) -> string
                            { return string(key_and_func.first); });
  return string(join(keys, ", "));
}

unsigned int Script_interpreter::user_line_idx() const
{
  return user_line_idx(m_cur_line_idx);
}

unsigned int Script_interpreter::user_line_idx(size_t line_idx) // Static.
{
  return line_idx + 1; // 1-based.
}

unsigned int Script_interpreter::user_col_idx(size_t col_idx) // Static.
{
  return col_idx + 1; // 1-based.
}

} // namespace ipc::transport::test
