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
#include <kj/debug.h>
#include <capnp/message.h>
#include <iostream>

using Session = Session_server::Server_session_obj;
void calc_test(flow::log::Logger* logger_ptr, flow::log::Logger* std_logger_ptr);

int main(int argc, char const * const * argv)
{
  using flow::log::Simple_ostream_logger;
  using flow::log::Async_file_logger;
  using flow::Flow_log_component;
  using boost::promise;
  using std::exception;
  using std::optional;

  /* Set up logging within this function.  We could easily just use `cout` and `cerr` instead, but this
   * Flow stuff will give us time stamps and such for free, so why not?  Normally, one derives from
   * Log_context to do this very trivially, but we just have the one function, main(), so far so: */
  optional<Simple_ostream_logger> std_logger;
  optional<Async_file_logger> log_logger;
  setup_logging(&std_logger, &log_logger, argc, argv, true);
  FLOW_LOG_SET_CONTEXT(&(*std_logger), Flow_log_component::S_UNCAT);

#if JEM_ELSE_CLASSIC
  /* Instructed to do so by ipc::session::shm::arena_lend public docs (short version: this is basically a global,
   * and it would not be cool for ipc::session non-global objects to impose their individual loggers on it). */
  ipc::session::shm::arena_lend::Borrower_shm_pool_collection_repository_singleton::get_instance()
    .set_logger(&(*log_logger));
#endif

  try
  {
    ensure_run_env(argv[0], true);

    calc_test(&(*log_logger), &(*std_logger));

    FLOW_LOG_WARNING("Exiting.  That is weird!  Should not be possible in this application.");
  } // try
  catch (const exception& exc)
  {
    FLOW_LOG_WARNING("Caught exception: [" << exc.what() << "].");
  }

  return 1;
} // main()

// The next section, up to calc_test() body, is straight from the original Calculator sample.

using uint = unsigned int;
using Calculator = rpc_demo::schema::Calculator;

kj::Promise<double> readValue(Calculator::Value::Client value) {
  // Helper function to asynchronously call read() on a Calculator::Value and
  // return a promise for the result.  (In the future, the generated code might
  // include something like this automatically.)

  return value.readRequest().send()
      .then([](capnp::Response<Calculator::Value::ReadResults> result) {
    return result.getValue();
  });
}

kj::Promise<double> evaluateImpl(
    Calculator::Expression::Reader expression,
    capnp::List<double>::Reader params = capnp::List<double>::Reader()) {
  // Implementation of CalculatorImpl::evaluate(), also shared by
  // FunctionImpl::call().  In the latter case, `params` are the parameter
  // values passed to the function; in the former case, `params` is just an
  // empty list.

  switch (expression.which()) {
    case Calculator::Expression::LITERAL:
      return expression.getLiteral();

    case Calculator::Expression::PREVIOUS_RESULT:
      return readValue(expression.getPreviousResult());

    case Calculator::Expression::PARAMETER: {
      KJ_REQUIRE(expression.getParameter() < params.size(),
                 "Parameter index out-of-range.");
      return params[expression.getParameter()];
    }

    case Calculator::Expression::CALL: {
      auto call = expression.getCall();
      auto func = call.getFunction();

      // Evaluate each parameter.
      kj::Array<kj::Promise<double>> paramPromises =
          KJ_MAP(param, call.getParams()) {
            return evaluateImpl(param, params);
          };

      // Join the array of promises into a promise for an array.
      kj::Promise<kj::Array<double>> joinedParams =
          kj::joinPromises(kj::mv(paramPromises));

      // When the parameters are complete, call the function.
      return joinedParams.then([KJ_CPCAP(func)](kj::Array<double>&& paramValues) mutable {
        auto request = func.callRequest();
        request.setParams(paramValues);
        return request.send().then(
            [](capnp::Response<Calculator::Function::CallResults>&& result) {
          return result.getValue();
        });
      });
    }

    default:
      // Throw an exception.
      KJ_FAIL_REQUIRE("Unknown expression type.");
  }
}

class ValueImpl final: public Calculator::Value::Server {
  // Simple implementation of the Calculator.Value Cap'n Proto interface.

public:
  ValueImpl(double value): value(value) {}

  kj::Promise<void> read(ReadContext context) {
    context.getResults().setValue(value);
    return kj::READY_NOW;
  }

private:
  double value;
};

class FunctionImpl final: public Calculator::Function::Server {
  // Implementation of the Calculator.Function Cap'n Proto interface, where the
  // function is defined by a Calculator.Expression.

public:
  FunctionImpl(uint paramCount, Calculator::Expression::Reader body)
      : paramCount(paramCount) {
    this->body.setRoot(body);
  }

  kj::Promise<void> call(CallContext context) {
    auto params = context.getParams().getParams();
    KJ_REQUIRE(params.size() == paramCount, "Wrong number of parameters.");

    return evaluateImpl(body.getRoot<Calculator::Expression>(), params)
        .then([KJ_CPCAP(context)](double value) mutable {
      context.getResults().setValue(value);
    });
  }

private:
  uint paramCount;
  // The function's arity.

  capnp::MallocMessageBuilder body;
  // Stores a permanent copy of the function body.
};

class OperatorImpl final: public Calculator::Function::Server {
  // Implementation of the Calculator.Function Cap'n Proto interface, wrapping
  // basic binary arithmetic operators.

public:
  OperatorImpl(Calculator::Operator op): op(op) {}

  kj::Promise<void> call(CallContext context) {
    auto params = context.getParams().getParams();
    KJ_REQUIRE(params.size() == 2, "Wrong number of parameters.");

    double result;
    switch (op) {
      case Calculator::Operator::ADD:     result = params[0] + params[1]; break;
      case Calculator::Operator::SUBTRACT:result = params[0] - params[1]; break;
      case Calculator::Operator::MULTIPLY:result = params[0] * params[1]; break;
      case Calculator::Operator::DIVIDE:  result = params[0] / params[1]; break;
      default:
        KJ_FAIL_REQUIRE("Unknown operator.");
    }

    context.getResults().setValue(result);
    return kj::READY_NOW;
  }

private:
  Calculator::Operator op;
};

class CalculatorImpl final: public Calculator::Server {
  // Implementation of the Calculator Cap'n Proto interface.

public:
  kj::Promise<void> evaluate(EvaluateContext context) override {
    return evaluateImpl(context.getParams().getExpression())
        .then([KJ_CPCAP(context)](double value) mutable {
      context.getResults().setValue(kj::heap<ValueImpl>(value));
    });
  }

  kj::Promise<void> defFunction(DefFunctionContext context) override {
    auto params = context.getParams();
    context.getResults().setFunc(kj::heap<FunctionImpl>(
        params.getParamCount(), params.getBody()));
    return kj::READY_NOW;
  }

  kj::Promise<void> getOperator(GetOperatorContext context) override {
    context.getResults().setFunc(kj::heap<OperatorImpl>(
        context.getParams().getOp()));
    return kj::READY_NOW;
  }

  // We (Flow-IPC guys) added this part!  See main_cli.cpp for context.
  kj::Promise<void> retList(RetListContext context) override {
    context.getResults().setResult(context.getParams().getInList());
    return kj::READY_NOW;
  }

};

void calc_test([[maybe_unused]] flow::log::Logger* logger_ptr, flow::log::Logger* std_logger_ptr)
{
  using ipc::util::Native_handle;

  FLOW_LOG_SET_CONTEXT(std_logger_ptr, flow::Flow_log_component::S_UNCAT);

  // Set up a server.
  ipc::transport::struc::shm::rpc::Ez_rpc_server<Session_server>
     server{&(*logger_ptr),
            [](auto&&...) -> auto { return kj::heap<CalculatorImpl>(); },
            SRV_APPS.find(SRV_NAME)->second, CLI_APPS,
            true,
            [](const auto& app) { return app.m_name == CLI_NAME_NO_ZC; }};
  auto& wait_scope = *(server.get_wait_scope());

  FLOW_LOG_INFO("Ez_rpc_server [" << server << "is ready.");
  FLOW_LOG_INFO("Listening....");

  // From here -- and the Impl classes above of course! -- it's just like the original sample.

  // Run forever, accepting connections and handling requests.
  kj::NEVER_DONE.wait(wait_scope);
} // calc_test()


/* Addendum: The source code in this file is based on a small portion of Cap 'n Proto,
 * version 1.0.2, namely samples/calculator-server.c++.  We have made key additions,
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
