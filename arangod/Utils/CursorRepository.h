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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_UTILS_CURSOR_REPOSITORY_H
#define ARANGOD_UTILS_CURSOR_REPOSITORY_H 1

#include "Basics/Common.h"
#include "Basics/Mutex.h"
#include "Utils/Cursor.h"
#include "VocBase/voc-types.h"

struct TRI_json_t;
struct TRI_vocbase_t;

namespace arangodb {

class CollectionExport;

class CursorRepository {
  
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief create a cursors repository
  //////////////////////////////////////////////////////////////////////////////

  explicit CursorRepository(TRI_vocbase_t*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief destroy a cursors repository
  //////////////////////////////////////////////////////////////////////////////

  ~CursorRepository();

  
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates a cursor and stores it in the registry
  /// the cursor will be returned with the usage flag set to true. it must be
  /// returned later using release()
  /// the cursor will take ownership of both json and extra
  //////////////////////////////////////////////////////////////////////////////

  JsonCursor* createFromJson(struct TRI_json_t*, size_t, struct TRI_json_t*,
                             double, bool, bool);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates a cursor and stores it in the registry
  //////////////////////////////////////////////////////////////////////////////

  ExportCursor* createFromExport(arangodb::CollectionExport*, size_t,
                                 double, bool);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief remove a cursor by id
  //////////////////////////////////////////////////////////////////////////////

  bool remove(CursorId);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief find an existing cursor by id
  /// if found, the cursor will be returned with the usage flag set to true.
  /// it must be returned later using release()
  //////////////////////////////////////////////////////////////////////////////

  Cursor* find(CursorId, bool&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return a cursor
  //////////////////////////////////////////////////////////////////////////////

  void release(Cursor*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the repository contains a used cursor
  //////////////////////////////////////////////////////////////////////////////

  bool containsUsedCursor();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief run a garbage collection on the cursors
  //////////////////////////////////////////////////////////////////////////////

  bool garbageCollect(bool);

  
 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief vocbase
  //////////////////////////////////////////////////////////////////////////////

  TRI_vocbase_t* _vocbase;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief mutex for the cursors repository
  //////////////////////////////////////////////////////////////////////////////

  arangodb::basics::Mutex _lock;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief list of current cursors
  //////////////////////////////////////////////////////////////////////////////

  std::unordered_map<CursorId, Cursor*> _cursors;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief maximum number of cursors to garbage-collect in one go
  //////////////////////////////////////////////////////////////////////////////

  static size_t const MaxCollectCount;
};
}

#endif


