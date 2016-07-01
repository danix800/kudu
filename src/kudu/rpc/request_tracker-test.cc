// Licensed to the Apache Software Foundation (ASF) under one
// or more contributor license agreements.  See the NOTICE file
// distributed with this work for additional information
// regarding copyright ownership.  The ASF licenses this file
// to you under the Apache License, Version 2.0 (the
// "License"); you may not use this file except in compliance
// with the License.  You may obtain a copy of the License at
//
//   http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing,
// software distributed under the License is distributed on an
// "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
// KIND, either express or implied.  See the License for the
// specific language governing permissions and limitations
// under the License.

#include <gtest/gtest.h>
#include <vector>

#include "kudu/rpc/request_tracker.h"
#include "kudu/util/test_util.h"

using std::vector;

namespace kudu {
namespace rpc {

TEST(RequestTrackerTest, TestSequenceNumberGeneration) {
  const int MAX = 10;

  scoped_refptr<RequestTracker> tracker_(new RequestTracker("test_client"));

  // A new tracker should have no incomplete RPCs
  ASSERT_TRUE(tracker_->FirstIncomplete(nullptr).IsNotFound());

  vector<RequestTracker::SequenceNumber> generated_seq_nos;

  // Generate MAX in flight RPCs, making sure they are correctly returned.
  for (int i = 0; i < MAX; i++) {
    RequestTracker::SequenceNumber seq_no;
    ASSERT_OK(tracker_->NewSeqNo(&seq_no));
    generated_seq_nos.push_back(seq_no);
  }

  // Now we should get a first incomplete.
  RequestTracker::SequenceNumber first_incomplete;
  ASSERT_OK(tracker_->FirstIncomplete(&first_incomplete));
  ASSERT_EQ(generated_seq_nos[0], first_incomplete);

  // Marking 'first_incomplete' as done, should advance the first incomplete.
  tracker_->RpcCompleted(first_incomplete);

  ASSERT_OK(tracker_->FirstIncomplete(&first_incomplete));
  ASSERT_EQ(generated_seq_nos[1], first_incomplete);

  // Marking a 'middle' rpc, should not advance 'first_incomplete'.
  tracker_->RpcCompleted(generated_seq_nos[5]);
  ASSERT_OK(tracker_->FirstIncomplete(&first_incomplete));
  ASSERT_EQ(generated_seq_nos[1], first_incomplete);

  // Marking half the rpc as complete should advance FirstIncomplete.
  // Note that this also tests that RequestTracker::RpcCompleted() is idempotent, i.e. that
  // marking the same sequence number as complete twice is a no-op.
  for (int i = 0; i < MAX / 2; i++) {
    tracker_->RpcCompleted(generated_seq_nos[i]);
  }

  ASSERT_OK(tracker_->FirstIncomplete(&first_incomplete));
  ASSERT_EQ(generated_seq_nos[6], first_incomplete);

  for (int i = MAX / 2; i <= MAX; i++) {
    RequestTracker::SequenceNumber seq_no;
    ASSERT_OK(tracker_->NewSeqNo(&seq_no));
    generated_seq_nos.push_back(seq_no);
  }

  // Marking them all as completed should cause RequestTracker::FirstIncomplete() to return
  // Status::NotFound() again.
  for (auto seq_no : generated_seq_nos) {
    tracker_->RpcCompleted(seq_no);
  }

  ASSERT_TRUE(tracker_->FirstIncomplete(nullptr).IsNotFound());
}

} // namespace rpc
} // namespace kudu
