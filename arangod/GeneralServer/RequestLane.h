////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_GENERAL_SERVER_LANE_H
#define ARANGOD_GENERAL_SERVER_LANE_H 1

#include "Basics/Common.h"

namespace arangodb {
enum class RequestLane {
  CLIENT_FAST,
  CLIENT_AQL,
  CLIENT_V8,
  CLIENT_SLOW,
  AGENCY_INTERNAL,
  AGENCY_CLUSTER,
  CLUSTER_INTERNAL,
  CLUSTER_V8,
  CLUSTER_ADMIN,
  SERVER_REPLICATION
};
}

#endif
