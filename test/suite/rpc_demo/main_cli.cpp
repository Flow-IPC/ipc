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

// Attention: Please see Addendum at the end of this source code file.

#include "common.hpp"
#include "schema.capnp.h"
#include <ipc/transport/struc/shm/rpc/ez_rpc.hpp>
#include <flow/perf/checkpt_timer.hpp>
#include <flow/util/string_view.hpp>
#include <kj/debug.h>
#include <cmath>

using Session = Client_session;
void calc_test(flow::log::Logger* logger_ptr, flow::log::Logger* std_logger_ptr, bool no_zero_copy);

/* ./$0 <log output file> <verbosity: none|warning|info|trace|data> --no-zc
 * Be sure to execute the server program also.
 * All args optional.  --no-zc will cause capnp-RPC to be performed in vanilla mode (no zero-copy).
 * Default verbosity is `info`; anything more-verbose *will* affect perf.
 * Server will handle either mode, so no need to synchronize its command-line. */
int main(int argc, char const * const * argv)
{
  using flow::log::Simple_ostream_logger;
  using flow::log::Async_file_logger;
  using flow::Flow_log_component;
  using flow::util::String_view;
  using flow::error::Runtime_error;
  using std::exception;
  using std::optional;

  /* Set up logging within this function.  We could easily just use `cout` and `cerr` instead, but this
   * Flow stuff will give us time stamps and such for free, so why not?  Normally, one derives from
   * Log_context to do this very trivially, but we just have the one function, main(), so far so: */
  optional<Simple_ostream_logger> std_logger;
  optional<Async_file_logger> log_logger;
  setup_logging(&std_logger, &log_logger, argc, argv, false);
  FLOW_LOG_SET_CONTEXT(&(*std_logger), Flow_log_component::S_UNCAT);

#if JEM_ELSE_CLASSIC
  ipc::session::shm::arena_lend::Borrower_shm_pool_collection_repository_singleton::get_instance()
    .set_logger(&(*log_logger));
#endif

  try
  {
    ensure_run_env(argv[0], false);

    const bool no_zc = argc > 3;
    if (no_zc && (String_view("--no-zc") != argv[3]))
    {
      throw Runtime_error("Arg 3, if present, must be: --no-zc.");
    }

    calc_test(&(*log_logger), &(*std_logger), no_zc);

    FLOW_LOG_INFO("Exiting.");
  } // try
  catch (const exception& exc)
  {
    FLOW_LOG_WARNING("Caught exception: [" << exc.what() << "].");
    FLOW_LOG_WARNING("(Perhaps you did not execute session-server executable in parallel, or "
                     "you executed one or both of us oddly?)");
    return 1;
  }

  return 0;
} // main()

class PowerFunction final: public rpc_demo::schema::Calculator::Function::Server {
  // An implementation of the Function interface wrapping pow().  Note that
  // we're implementing this on the client side and will pass a reference to
  // the server.  The server will then be able to make calls back to the client.

public:
  kj::Promise<void> call(CallContext context) {
    auto params = context.getParams().getParams();
    KJ_REQUIRE(params.size() == 2, "Wrong number of parameters.");
    context.getResults().setValue(pow(params[0], params[1]));
    return kj::READY_NOW;
  }
};

void calc_test([[maybe_unused]] flow::log::Logger* logger_ptr, flow::log::Logger* std_logger_ptr, bool no_zero_copy)
{
  using ipc::util::Native_handle;
  using Calculator = rpc_demo::schema::Calculator;
  using Timer = flow::perf::Checkpointing_timer;

  FLOW_LOG_SET_CONTEXT(std_logger_ptr, flow::Flow_log_component::S_UNCAT);

  /* This is analogout to capnp:EzRpcClient.
   * We could intead use the lower-layer rpc::Client_context.
   * Or still lower-layer rpc::Session_vat_network + capnp::Rpc_system (+ ipc::session::* to connect).
   * Ez_rpc_* does exercise those other layers by being implemented using them internally. */
  ipc::transport::struc::shm::rpc::Ez_rpc_client<Client_session> client{&(*logger_ptr),
                                                                        // common.cpp explains this part:
                                                                        CLI_APPS.find(no_zero_copy
                                                                                        ? CLI_NAME_NO_ZC
                                                                                        : CLI_NAME)->second,
                                                                        SRV_APPS.find(SRV_NAME)->second,
                                                                        true,
                                                                        no_zero_copy}; // And this part.
  /* From here, it's just like the original Calculator sample (except we added a little additional test case at th
   * end).  Oh and we time each test case, just for fun. */

  Calculator::Client calculator = client.get_main<Calculator>();
  auto& waitScope = *(client.get_wait_scope());

  Timer tmr{nullptr, "benchiez", Timer::real_clock_types(), 7};

  // Keep an eye on `waitScope`.  Whenever you see it used is a place where we
  // stop and wait for the server to respond.  If a line of code does not use
  // `waitScope`, then it does not block!

  {
    // Make a request that just evaluates the literal value 123.
    //
    // What's interesting here is that evaluate() returns a "Value", which is
    // another interface and therefore points back to an object living on the
    // server.  We then have to call read() on that object to read it.
    // However, even though we are making two RPC's, this block executes in
    // *one* network round trip because of promise pipelining:  we do not wait
    // for the first call to complete before we send the second call to the
    // server.

    FLOW_LOG_INFO("Evaluating a literal....");

    // Set up the request.
    auto request = calculator.evaluateRequest();
    request.getExpression().setLiteral(123);

    // Send it, which returns a promise for the result (without blocking).
    auto evalPromise = request.send();

    // Using the promise, create a pipelined request to call read() on the
    // returned object, and then send that.
    auto readPromise = evalPromise.getValue().readRequest().send();

    // Now that we've sent all the requests, wait for the response.  Until this
    // point, we haven't waited at all!
    auto response = readPromise.wait(waitScope);
    KJ_ASSERT(response.getValue() == 123);

    FLOW_LOG_INFO("PASS");
  }

  tmr.checkpoint("eval literal");

  {
    // Make a request to evaluate 123 + 45 - 67.
    //
    // The Calculator interface requires that we first call getOperator() to
    // get the addition and subtraction functions, then call evaluate() to use
    // them.  But, once again, we can get both functions, call evaluate(), and
    // then read() the result -- four RPCs -- in the time of *one* network
    // round trip, because of promise pipelining.

    FLOW_LOG_INFO("Using add and subtract....");

    Calculator::Function::Client add = nullptr;
    Calculator::Function::Client subtract = nullptr;

    {
      // Get the "add" function from the server.
      auto request = calculator.getOperatorRequest();
      request.setOp(Calculator::Operator::ADD);
      add = request.send().getFunc();
    }

    {
      // Get the "subtract" function from the server.
      auto request = calculator.getOperatorRequest();
      request.setOp(Calculator::Operator::SUBTRACT);
      subtract = request.send().getFunc();
    }

    // Build the request to evaluate 123 + 45 - 67.
    auto request = calculator.evaluateRequest();

    auto subtractCall = request.getExpression().initCall();
    subtractCall.setFunction(subtract);
    auto subtractParams = subtractCall.initParams(2);
    subtractParams[1].setLiteral(67);

    auto addCall = subtractParams[0].initCall();
    addCall.setFunction(add);
    auto addParams = addCall.initParams(2);
    addParams[0].setLiteral(123);
    addParams[1].setLiteral(45);

    // Send the evaluate() request, read() the result, and wait for read() to
    // finish.
    auto evalPromise = request.send();
    auto readPromise = evalPromise.getValue().readRequest().send();

    auto response = readPromise.wait(waitScope);
    KJ_ASSERT(response.getValue() == 101);

    FLOW_LOG_INFO("PASS");
  }

  tmr.checkpoint("add-sub");

  {
    // Make a request to evaluate 4 * 6, then use the result in two more
    // requests that add 3 and 5.
    //
    // Since evaluate() returns its result wrapped in a `Value`, we can pass
    // that `Value` back to the server in subsequent requests before the first
    // `evaluate()` has actually returned.  Thus, this example again does only
    // one network round trip.

    FLOW_LOG_INFO("Pipelining eval() calls....");

    Calculator::Function::Client add = nullptr;
    Calculator::Function::Client multiply = nullptr;

    {
      // Get the "add" function from the server.
      auto request = calculator.getOperatorRequest();
      request.setOp(Calculator::Operator::ADD);
      add = request.send().getFunc();
    }

    {
      // Get the "multiply" function from the server.
      auto request = calculator.getOperatorRequest();
      request.setOp(Calculator::Operator::MULTIPLY);
      multiply = request.send().getFunc();
    }

    // Build the request to evaluate 4 * 6
    auto request = calculator.evaluateRequest();

    auto multiplyCall = request.getExpression().initCall();
    multiplyCall.setFunction(multiply);
    auto multiplyParams = multiplyCall.initParams(2);
    multiplyParams[0].setLiteral(4);
    multiplyParams[1].setLiteral(6);

    auto multiplyResult = request.send().getValue();

    // Use the result in two calls that add 3 and add 5.

    auto add3Request = calculator.evaluateRequest();
    auto add3Call = add3Request.getExpression().initCall();
    add3Call.setFunction(add);
    auto add3Params = add3Call.initParams(2);
    add3Params[0].setPreviousResult(multiplyResult);
    add3Params[1].setLiteral(3);
    auto add3Promise = add3Request.send().getValue().readRequest().send();

    auto add5Request = calculator.evaluateRequest();
    auto add5Call = add5Request.getExpression().initCall();
    add5Call.setFunction(add);
    auto add5Params = add5Call.initParams(2);
    add5Params[0].setPreviousResult(multiplyResult);
    add5Params[1].setLiteral(5);
    auto add5Promise = add5Request.send().getValue().readRequest().send();

    // Now wait for the results.
    KJ_ASSERT(add3Promise.wait(waitScope).getValue() == 27);
    KJ_ASSERT(add5Promise.wait(waitScope).getValue() == 29);

    FLOW_LOG_INFO("PASS");
  }

  tmr.checkpoint("pipeline eval");

  {
    // Our calculator interface supports defining functions.  Here we use it
    // to define two functions and then make calls to them as follows:
    //
    //   f(x, y) = x * 100 + y
    //   g(x) = f(x, x + 1) * 2;
    //   f(12, 34)
    //   g(21)
    //
    // Once again, the whole thing takes only one network round trip.

    FLOW_LOG_INFO("Defining functions....");

    Calculator::Function::Client add = nullptr;
    Calculator::Function::Client multiply = nullptr;
    Calculator::Function::Client f = nullptr;
    Calculator::Function::Client g = nullptr;

    {
      // Get the "add" function from the server.
      auto request = calculator.getOperatorRequest();
      request.setOp(Calculator::Operator::ADD);
      add = request.send().getFunc();
    }

    {
      // Get the "multiply" function from the server.
      auto request = calculator.getOperatorRequest();
      request.setOp(Calculator::Operator::MULTIPLY);
      multiply = request.send().getFunc();
    }

    {
      // Define f.
      auto request = calculator.defFunctionRequest();
      request.setParamCount(2);

      {
        // Build the function body.
        auto addCall = request.getBody().initCall();
        addCall.setFunction(add);
        auto addParams = addCall.initParams(2);
        addParams[1].setParameter(1);  // y

        auto multiplyCall = addParams[0].initCall();
        multiplyCall.setFunction(multiply);
        auto multiplyParams = multiplyCall.initParams(2);
        multiplyParams[0].setParameter(0);  // x
        multiplyParams[1].setLiteral(100);
      }

      f = request.send().getFunc();
    }

    {
      // Define g.
      auto request = calculator.defFunctionRequest();
      request.setParamCount(1);

      {
        // Build the function body.
        auto multiplyCall = request.getBody().initCall();
        multiplyCall.setFunction(multiply);
        auto multiplyParams = multiplyCall.initParams(2);
        multiplyParams[1].setLiteral(2);

        auto fCall = multiplyParams[0].initCall();
        fCall.setFunction(f);
        auto fParams = fCall.initParams(2);
        fParams[0].setParameter(0);

        auto addCall = fParams[1].initCall();
        addCall.setFunction(add);
        auto addParams = addCall.initParams(2);
        addParams[0].setParameter(0);
        addParams[1].setLiteral(1);
      }

      g = request.send().getFunc();
    }

    // OK, we've defined all our functions.  Now create our eval requests.

    // f(12, 34)
    auto fEvalRequest = calculator.evaluateRequest();
    auto fCall = fEvalRequest.initExpression().initCall();
    fCall.setFunction(f);
    auto fParams = fCall.initParams(2);
    fParams[0].setLiteral(12);
    fParams[1].setLiteral(34);
    auto fEvalPromise = fEvalRequest.send().getValue().readRequest().send();

    // g(21)
    auto gEvalRequest = calculator.evaluateRequest();
    auto gCall = gEvalRequest.initExpression().initCall();
    gCall.setFunction(g);
    gCall.initParams(1)[0].setLiteral(21);
    auto gEvalPromise = gEvalRequest.send().getValue().readRequest().send();

    // Wait for the results.
    KJ_ASSERT(fEvalPromise.wait(waitScope).getValue() == 1234);
    KJ_ASSERT(gEvalPromise.wait(waitScope).getValue() == 4244);

    FLOW_LOG_INFO("PASS");
  }

  tmr.checkpoint("define funcs");

  {
    // Make a request that will call back to a function defined locally.
    //
    // Specifically, we will compute 2^(4 + 5).  However, exponent is not
    // defined by the Calculator server.  So, we'll implement the Function
    // interface locally and pass it to the server for it to use when
    // evaluating the expression.
    //
    // This example requires two network round trips to complete, because the
    // server calls back to the client once before finishing.  In this
    // particular case, this could potentially be optimized by using a tail
    // call on the server side -- see CallContext::tailCall().  However, to
    // keep the example simpler, we haven't implemented this optimization in
    // the sample server.

    FLOW_LOG_INFO("Using a callback....");

    Calculator::Function::Client add = nullptr;

    {
      // Get the "add" function from the server.
      auto request = calculator.getOperatorRequest();
      request.setOp(Calculator::Operator::ADD);
      add = request.send().getFunc();
    }

    // Build the eval request for 2^(4+5).
    auto request = calculator.evaluateRequest();

    auto powCall = request.getExpression().initCall();
    powCall.setFunction(kj::heap<PowerFunction>());
    auto powParams = powCall.initParams(2);
    powParams[0].setLiteral(2);

    auto addCall = powParams[1].initCall();
    addCall.setFunction(add);
    auto addParams = addCall.initParams(2);
    addParams[0].setLiteral(4);
    addParams[1].setLiteral(5);

    // Send the request and wait.
    auto response = request.send().getValue().readRequest()
                           .send().wait(waitScope);
    KJ_ASSERT(response.getValue() == 512);

    FLOW_LOG_INFO("PASS");
  }

  tmr.checkpoint("use callback");

  // This next part, we (Flow-IPC people!) added to the original Calculator sample from capnp distro.

  {
    FLOW_LOG_INFO("Preparing big list...."); // Separating this out from the transmission of it back-and-forth.

    // Set up the request.
    auto request = calculator.retListRequest();
    constexpr size_t N = 21001001;
    auto inList = request.initInList(N);
    for (size_t idx = 0; idx != N; ++idx)
    {
      inList.set(idx, idx);
    }

    tmr.checkpoint("big list prep");

    FLOW_LOG_INFO("Bouncing big list back and forth....");

    /* We aren't going for some extensive testing here or anything; just an easy thing.
     * We will send a big list; and they'll send us an equal list as the return value.
     * Without zero-copy, in each direction, the list would be copied into transport; out of transport;
     * with zero-copy, none of those 4 large copy-ops occurs. */

    // Send it, which returns a promise for the result (without blocking).
    auto evalPromise = request.send();
    auto response = evalPromise.wait(waitScope);
    auto list = response.getResult();
    KJ_ASSERT(list.size() == N);
    KJ_ASSERT(list[N / 2] == uint64_t(N / 2)); // Spot-check.

    tmr.checkpoint("big list bounce");

    // Don't time the long part of the verification.
    for (size_t idx = 0; idx != N; ++idx)
    {
      KJ_ASSERT(list[idx] == uint64_t(idx));
    }

    FLOW_LOG_INFO("PASS");
  }

  FLOW_LOG_INFO(tmr);
}

/* Addendum: The source code in this file is based on a small portion of Cap 'n Proto,
 * version 1.0.2, namely samples/calculator-client.c++.  We have made key additions,
 * but largely this remains the same.  The code here is a sample application that
 * uses some features of Cap 'n Proto as well as our project here, Flow-IPC.
 * The license header from the Cap'n Proto source file follows. */

// Copyright (c) 2013-2014 Sandstorm Development Group, Inc. and contributors
// Licensed under the MIT License:
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
