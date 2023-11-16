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

@0xa780a4869d13f307;

using Cxx = import "/capnp/c++.capnp";
using Common = import "/ipc/transport/struc/schema/common.capnp";
using ShmCommon = import "/ipc/transport/struc/shm/schema/common.capnp";
using ShmHandle = ShmCommon.ShmHandle;
using Size = Common.Size;

$Cxx.namespace("ipc::transport::test::capnp");

### Metadata applicable to entire session.

struct ExMdt
{
  struct MdtOne
  {
    onePayload @0 :Text;
  }

  struct MdtTwo
  {
    twoPayload @0 :Int64;
  }

  union
  {
    mdtOne @0 :MdtOne;
    mdtTwo @1 :MdtTwo;
  }

  numChansYouWant @2 :Size;
  # Guy (cli or srv) filling-out the metadata, say, sets this to # of init-channels the *recepient* will want opened
  # on its behalf.  This is contrived and only for demo purposes; so in our apps we just, like, assert this value is
  # consistent with that.  E.g., client will say "you want 3 init-channels" and then server will go,
  # "ah yes, indeed I was gonna open 3 init-channels."


  autoPing @3: Bool;
  # Causes the passive-opener, if applicable, to auto_ping(), otherwise not.
  # The active opener may or may not set up an idle timer and test that feature.
}

### struc::Channel type A schema.

struct ExBodyA
{
  union
  {
    msg @0 :MultiPurposeMsgA;

    msgTwo @1 :Text;

    msgHandle @2 :Int32;
    # Here we will expect a message with a user-generated order ID (so we can verify reassembly/some bug doesn't
    # mess up ordering); plus optionally a Native_handle which is the write-end of a pipe into which recipient
    # will write the ID to be checked by the sender.
  }

  description @3 :Text;
  # Demonstrates that, if desired, every message (despite the union requirement) can come with a common thing too.
  # Thanks, capnp!  Anyway we can stuff a description of the test here; self-documentation of sorts.
}

struct MultiPurposeMsgA
{
  numVal @0 :Int32;

  stringList @1 :List(Text);
  cumulativeSz @2 :Size;
  # Length sum in stringList must equal this.  Sender can send small or large payloads;
  # a large payload (e.g., a million short strings, at least) won't fit into a non-SHM-backed message.
}

### struc::Channel type B schema.
# It's similar but slightly different, just to show it can be different -- it's not working by coincidence.
# It's arbitrary how to differentiate them, but for fun we'll replace the possibly-big List(Text) (capnp-native
# string list) with a possibly-big-thing-pointing ShmHandle (C++ string list).

struct ExBodyB
{
  union
  {
    msg @0 :MultiPurposeMsgB;
    msgTwo @1 :UInt16;
  }

  description @2 :Text;
}

struct MultiPurposeMsgB
{
  numVal @0 :UInt64;
  stringBlobListDirect @1 :ShmHandle;
  # Points to list<X>, where X = struct { string; Basic_blob<true>; Basic_blob<false>; } in SHM.
  # The 2 blob types are sharing-capable and sharing-incapable respectively.  Sharing = ability to use a
  # blob as a pool of sub-blobs.
  cumulativeSz @2 :Size;
  # Length sum in stringBlobListDirect must equal this.  Sender will send large payloads;
  # any size will "fit" into a message, since only a handle is being sent (but without SHM backing one can't, like,
  # send a SHM handle).

  sessionLendElseArenaLend @3 :Bool;
  # When invoking lend_object() and borrow_object(), one can do it via
  # Session::lend_object()/borrow_object() or via Arena::lend_object()/borrow_object();
  # these must be mutually consistent.  (Internally in the former case (which is arguably easier to use, depending),
  # to make it work, Session must determine which Arena the lent handle is from -- app_shm() or session_shm() --
  # and encode that into the serialization blob, so that internally Session::borrow_object() can figure out which
  # internally-stored Arena::borrow_object() it must forward-to.  That's the case for SHM-classic.
  # With SHM-jemalloc it's a direct forward on both sides: to Jemalloc Shm_session lend_object() and borrow_object()
  # respectively.  Though we can still invoke the wrapper in Session or directly via Shm_session.)
  #
  # This Bool indicates to the receiver which borrow/lend_object() API is being tested; true means the Session
  # one; otherwise the Arena (SHM-classic) or Session (SHM-jemalloc).

  omitHandleRead @4 :Bool;
  # Iff true, *ignore* stringBlobListDirect (which is <whatever>, but no lend_object() was called) -- instead
  # check the STL data structure saved from the last message; its contents nevertheless should match
  # cumulativeSz.  It tests in-place modification without re-borrow/lend_object().
}
