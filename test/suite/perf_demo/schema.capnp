# Flow-IPC
# Copyright 2023 Akamai Technologies, Inc.
#
# Licensed under the Apache License, Version 2.0 (the
# "License"); you may not use this file except in
# compliance with the License.  You may obtain a copy
# of the License at
#
#   https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in
# writing, software distributed under the License is
# distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing
# permissions and limitations under the License.

@0xa30343c0b99be6ef;

using Cxx = import "/capnp/c++.capnp";

$Cxx.namespace("perf_demo::schema");

struct Body
{
  union
  {
    getCacheReq @0 :GetCacheReq;
    getCacheRsp @1 :GetCacheRsp;
  }
}

struct GetCacheReq
{
  fileName @0 :Text;
}

struct GetCacheRsp
{
  fileParts @0 :List(FilePart);
}

struct FilePart
{
  data @0 :Data;
  dataSize @1 :UInt64;
  dataHashToVerify @2 :UInt64;
}
