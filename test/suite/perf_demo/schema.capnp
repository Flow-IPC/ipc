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

using Hash = UInt64;

struct Body
{
  # We simulate a simple scenario of a cache client and cache server processes, where:
  #   - server has memory-cached various files in memory, some quite large (e.g., between 100k and 1G bytes);
  #   - client process requests an object via small GetCacheReq message;
  #   - server responds with (potentially) large GetCacheRsp message including the file's contents.
  union
  {
    getCacheReq @0 :GetCacheReq;
    getCacheRsp @1 :GetCacheRsp;
  }
}

struct GetCacheReq
{
  fileName @0 :Text;
  # The file whose memory-cached contents server shall fetch.  For now this is just for show.
  # TODO: Perhaps based on the file name server could return files of various sizes.
  # At the moment the server program just takes the benchmarked file size as a command-line arg;
  # instead we can give it to the client, or it can auto-run a benchmark with various sizes.
}

struct GetCacheRsp
{
  # We simulate the server returning files in multiple equally-sized chunks, each sized at its discretion.
  # A decent estimate of the serialized size of a given GetCacheRsp is `data.size()` summed over all `fileParts`.
  struct FilePart
  {
    data @0 :Data;
    dataSizeToVerify @1 :UInt64; # Recipient can verify that `data` blob's size is indeed this.
    dataHashToVerify @2 :Hash; # Recipient can hash `data` and verify it is indeed this.
  }

  fileParts @0 :List(FilePart);
}
