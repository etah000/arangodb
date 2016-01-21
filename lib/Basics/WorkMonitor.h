////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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

#ifndef ARANGODB_UTILS_WORK_MONITOR
#define ARANGODB_UTILS_WORK_MONITOR 1

#include "Basics/Thread.h"

#include "Basics/WorkItem.h"

namespace arangodb {
namespace rest {
class HttpHandler;
}
}

namespace arangodb {
namespace velocypack {
class Builder;
}

////////////////////////////////////////////////////////////////////////////////
/// @brief type of the current work
////////////////////////////////////////////////////////////////////////////////

enum class WorkType { THREAD, HANDLER, CUSTOM };

////////////////////////////////////////////////////////////////////////////////
/// @brief description of the current work
////////////////////////////////////////////////////////////////////////////////

struct WorkDescription {
  WorkDescription(WorkType, WorkDescription*);

  WorkType _type;
  bool _destroy;

  char _customType[16];

  union data {
    data() {}
    ~data() {}

    char text[256];
    arangodb::basics::Thread* thread;
    arangodb::rest::HttpHandler* handler;
  } _data;

  WorkDescription* _prev;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief work monitor class
////////////////////////////////////////////////////////////////////////////////

class WorkMonitor : public arangodb::basics::Thread {
 public:

  WorkMonitor();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates an empty WorkDescription
  //////////////////////////////////////////////////////////////////////////////

  static WorkDescription* createWorkDescription(WorkType);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief activates a WorkDescription
  //////////////////////////////////////////////////////////////////////////////

  static void activateWorkDescription(WorkDescription*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief deactivates a WorkDescription
  //////////////////////////////////////////////////////////////////////////////

  static WorkDescription* deactivateWorkDescription();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief frees an WorkDescription
  //////////////////////////////////////////////////////////////////////////////

  static void freeWorkDescription(WorkDescription* desc);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief pushes a threads
  //////////////////////////////////////////////////////////////////////////////

  static void pushThread(arangodb::basics::Thread* thread);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief pops a threads
  //////////////////////////////////////////////////////////////////////////////

  static void popThread(arangodb::basics::Thread* thread);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief pushes a custom task
  //////////////////////////////////////////////////////////////////////////////

  static void pushCustom(char const* type, char const* text, size_t length);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief pushes a custom task
  //////////////////////////////////////////////////////////////////////////////

  static void pushCustom(char const* type, uint64_t id);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief pops a custom task
  //////////////////////////////////////////////////////////////////////////////

  static void popCustom();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief pushes a handler
  //////////////////////////////////////////////////////////////////////////////

  static void pushHandler(arangodb::rest::HttpHandler*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief pops and releases a handler
  //////////////////////////////////////////////////////////////////////////////

  static WorkDescription* popHandler(arangodb::rest::HttpHandler*, bool free);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief handler deleter
  //////////////////////////////////////////////////////////////////////////////

  static void DELETE_HANDLER(WorkDescription* desc);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief requests a work overview
  //////////////////////////////////////////////////////////////////////////////

  static void requestWorkOverview(uint64_t taskId);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief handler description
  //////////////////////////////////////////////////////////////////////////////

  static void VPACK_HANDLER(arangodb::velocypack::Builder*,
                            WorkDescription* desc);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sends the overview
  //////////////////////////////////////////////////////////////////////////////

  static void SEND_WORK_OVERVIEW(uint64_t, std::string const&);

 protected:

  void run() override;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief initiate shutdown
  //////////////////////////////////////////////////////////////////////////////

  void shutdown() { _stopping = true; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief stopping flag
  //////////////////////////////////////////////////////////////////////////////

  std::atomic<bool> _stopping;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief auto push and pop for HttpHandler
////////////////////////////////////////////////////////////////////////////////

class HandlerWorkStack {
  HandlerWorkStack(const HandlerWorkStack&) = delete;
  HandlerWorkStack& operator=(const HandlerWorkStack&) = delete;

 public:

  explicit HandlerWorkStack(arangodb::rest::HttpHandler*);


  explicit HandlerWorkStack(WorkItem::uptr<arangodb::rest::HttpHandler>&);


  ~HandlerWorkStack();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the handler
  //////////////////////////////////////////////////////////////////////////////

  arangodb::rest::HttpHandler* handler() const { return _handler; }

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief handler
  //////////////////////////////////////////////////////////////////////////////

  arangodb::rest::HttpHandler* _handler;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief auto push and pop for Custom
////////////////////////////////////////////////////////////////////////////////

class CustomWorkStack {
  CustomWorkStack(const CustomWorkStack&) = delete;
  CustomWorkStack& operator=(const CustomWorkStack&) = delete;

 public:

  CustomWorkStack(char const* type, char const* text, size_t length);


  CustomWorkStack(char const* type, uint64_t id);


  ~CustomWorkStack();
};

////////////////////////////////////////////////////////////////////////////////
/// @brief starts the work monitor
////////////////////////////////////////////////////////////////////////////////

void InitializeWorkMonitor();

////////////////////////////////////////////////////////////////////////////////
/// @brief stops the work monitor
////////////////////////////////////////////////////////////////////////////////

void ShutdownWorkMonitor();
}

#endif
