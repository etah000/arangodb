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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_CLUSTER_AGENCY_COMM_H
#define ARANGOD_CLUSTER_AGENCY_COMM_H 1

#include "Basics/Common.h"
#include "Basics/ReadWriteLock.h"
#include "Basics/json.h"
#include "Rest/HttpRequest.h"
#include "velocypack/Slice.h"

#include <list>

namespace arangodb {
namespace httpclient {
class GeneralClientConnection;
}

namespace rest {
class Endpoint;
}

class AgencyComm;

struct AgencyEndpoint {
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates an agency endpoint
  //////////////////////////////////////////////////////////////////////////////

  AgencyEndpoint(arangodb::rest::Endpoint*,
                 arangodb::httpclient::GeneralClientConnection*);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief destroys an agency endpoint
  //////////////////////////////////////////////////////////////////////////////

  ~AgencyEndpoint();

  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief the endpoint
  //////////////////////////////////////////////////////////////////////////////

  arangodb::rest::Endpoint* _endpoint;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the connection
  //////////////////////////////////////////////////////////////////////////////

  arangodb::httpclient::GeneralClientConnection* _connection;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the endpoint is busy
  //////////////////////////////////////////////////////////////////////////////

  bool _busy;
};


struct AgencyConnectionOptions {
  double _connectTimeout;
  double _requestTimeout;
  double _lockTimeout;
  size_t _connectRetries;
};


struct AgencyCommResultEntry {
  uint64_t _index;
  std::shared_ptr<VPackBuilder> _vpack;
  bool _isDir;
};


struct AgencyCommResult {
  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructs a communication result
  //////////////////////////////////////////////////////////////////////////////

  AgencyCommResult();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief destroys a communication result
  //////////////////////////////////////////////////////////////////////////////

  ~AgencyCommResult();

  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns whether the last request was successful
  //////////////////////////////////////////////////////////////////////////////

  inline bool successful() const {
    return (_statusCode >= 200 && _statusCode <= 299);
  }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief extract the connected flag from the result
  //////////////////////////////////////////////////////////////////////////////

  bool connected() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief extract the http code from the result
  //////////////////////////////////////////////////////////////////////////////

  int httpCode() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief extract the "index" attribute from the result
  //////////////////////////////////////////////////////////////////////////////

  uint64_t index() const { return _index; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief extract the error code from the result
  //////////////////////////////////////////////////////////////////////////////

  int errorCode() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief extract the error message from the result
  /// if there is no error, an empty string will be returned
  //////////////////////////////////////////////////////////////////////////////

  std::string errorMessage() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief extract the error details from the result
  /// if there is no error, an empty string will be returned
  //////////////////////////////////////////////////////////////////////////////

  std::string errorDetails() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the location header (might be empty)
  //////////////////////////////////////////////////////////////////////////////

  std::string const location() const { return _location; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief return the body (might be empty)
  //////////////////////////////////////////////////////////////////////////////

  std::string const body() const { return _body; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief flush the internal result buffer
  //////////////////////////////////////////////////////////////////////////////

  void clear();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief recursively flatten the VelocyPack response into a map
  ///
  /// stripKeyPrefix is decoded, as is the _globalPrefix
  //////////////////////////////////////////////////////////////////////////////

  bool parseVelocyPackNode(VPackSlice const&, std::string const&, bool);

  //////////////////////////////////////////////////////////////////////////////
  /// parse an agency result
  /// note that stripKeyPrefix is a decoded, normal key!
  //////////////////////////////////////////////////////////////////////////////

  bool parse(std::string const&, bool);

  
 public:
  std::string _location;
  std::string _message;
  std::string _body;

  std::map<std::string, AgencyCommResultEntry> _values;
  uint64_t _index;
  int _statusCode;
  bool _connected;
};


class AgencyCommLocker {
  
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructs an agency comm locker
  ///
  /// The keys mentioned in this class are all not yet encoded.
  //////////////////////////////////////////////////////////////////////////////

  AgencyCommLocker(std::string const&, std::string const&, double = 0.0);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief destroys an agency comm locker
  //////////////////////////////////////////////////////////////////////////////

  ~AgencyCommLocker();

  
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief return whether the locking was successful
  //////////////////////////////////////////////////////////////////////////////

  bool successful() const { return _isLocked; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief unlocks the lock
  //////////////////////////////////////////////////////////////////////////////

  void unlock();

  
 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief fetch a lock version from the agency
  //////////////////////////////////////////////////////////////////////////////

  bool fetchVersion(AgencyComm&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief update a lock version in the agency
  //////////////////////////////////////////////////////////////////////////////

  bool updateVersion(AgencyComm&);

  
 private:
  std::string const _key;
  std::string const _type;
  std::shared_ptr<VPackBuilder> _vpack;
  uint64_t _version;
  bool _isLocked;
};


class AgencyComm {
  friend struct AgencyCommResult;
  friend class AgencyCommLocker;

  
 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates a communication channel
  //////////////////////////////////////////////////////////////////////////////

  AgencyComm(bool = true);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief destroys a communication channel
  //////////////////////////////////////////////////////////////////////////////

  ~AgencyComm();

  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief cleans up all connections
  //////////////////////////////////////////////////////////////////////////////

  static void cleanup();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief tries to establish a communication channel
  //////////////////////////////////////////////////////////////////////////////

  static bool tryConnect();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief disconnects all communication channels
  //////////////////////////////////////////////////////////////////////////////

  static void disconnect();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief adds an endpoint to the agents list
  //////////////////////////////////////////////////////////////////////////////

  static bool addEndpoint(std::string const&, bool = false);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks if an endpoint is present
  //////////////////////////////////////////////////////////////////////////////

  static bool hasEndpoint(std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get a stringified version of the endpoints
  //////////////////////////////////////////////////////////////////////////////

  static std::vector<std::string> getEndpoints();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get a stringified version of the endpoints
  //////////////////////////////////////////////////////////////////////////////

  static std::string getEndpointsString();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets the global prefix for all operations
  //////////////////////////////////////////////////////////////////////////////

  static bool setPrefix(std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the global prefix for all operations
  //////////////////////////////////////////////////////////////////////////////

  static std::string prefix();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief generate a timestamp
  //////////////////////////////////////////////////////////////////////////////

  static std::string generateStamp();

  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates a new agency endpoint
  //////////////////////////////////////////////////////////////////////////////

  static AgencyEndpoint* createAgencyEndpoint(std::string const&);

  
  //////////////////////////////////////////////////////////////////////////////
  /// @brief sends the current server state to the agency
  //////////////////////////////////////////////////////////////////////////////

  AgencyCommResult sendServerState(double ttl);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief gets the backend version
  //////////////////////////////////////////////////////////////////////////////

  std::string getVersion();

  //////////////////////////////////////////////////////////////////////////////
  /// @brief update a version number in the agency
  //////////////////////////////////////////////////////////////////////////////

  bool increaseVersion(std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief update a version number in the agency, retry until it works
  //////////////////////////////////////////////////////////////////////////////

  void increaseVersionRepeated(std::string const& key);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief creates a directory in the backend
  //////////////////////////////////////////////////////////////////////////////

  AgencyCommResult createDirectory(std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets a value in the back end
  //////////////////////////////////////////////////////////////////////////////

  AgencyCommResult setValue(std::string const&, TRI_json_t const*, double);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sets a value in the back end, velocypack variant
  //////////////////////////////////////////////////////////////////////////////

  AgencyCommResult setValue(std::string const&,
                            arangodb::velocypack::Slice const, double);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief checks whether a key exists
  //////////////////////////////////////////////////////////////////////////////

  bool exists(std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief gets one or multiple values from the back end
  //////////////////////////////////////////////////////////////////////////////

  AgencyCommResult getValues(std::string const&, bool);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief removes one or multiple values from the back end
  //////////////////////////////////////////////////////////////////////////////

  AgencyCommResult removeValues(std::string const&, bool);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief compares and swaps a single value in the backend
  /// the CAS condition is whether or not a previous value existed for the key
  //////////////////////////////////////////////////////////////////////////////

  AgencyCommResult casValue(std::string const&, TRI_json_t const*, bool, double,
                            double) = delete;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief compares and swaps a single value in the backend
  /// the CAS condition is whether or not a previous value existed for the key
  /// velocypack variant
  //////////////////////////////////////////////////////////////////////////////

  AgencyCommResult casValue(std::string const&,
                            arangodb::velocypack::Slice const, bool, double,
                            double);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief compares and swaps a single value in the back end
  /// the CAS condition is whether or not the previous value for the key was
  /// identical to `oldValue`
  //////////////////////////////////////////////////////////////////////////////

  AgencyCommResult casValue(std::string const&, TRI_json_t const*,
                            TRI_json_t const*, double, double) = delete;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief compares and swaps a single value in the back end
  /// the CAS condition is whether or not the previous value for the key was
  /// identical to `oldValue`
  /// velocypack variant
  //////////////////////////////////////////////////////////////////////////////

  AgencyCommResult casValue(std::string const&, VPackSlice const&,
                            VPackSlice const&, double, double);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get unique id
  //////////////////////////////////////////////////////////////////////////////

  AgencyCommResult uniqid(std::string const&, uint64_t, double);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief blocks on a change of a single value in the back end
  //////////////////////////////////////////////////////////////////////////////

  AgencyCommResult watchValue(std::string const&, uint64_t, double, bool);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief acquire a read lock
  //////////////////////////////////////////////////////////////////////////////

  bool lockRead(std::string const&, double, double);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief acquire a write lock
  //////////////////////////////////////////////////////////////////////////////

  bool lockWrite(std::string const&, double, double);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief release a read lock
  //////////////////////////////////////////////////////////////////////////////

  bool unlockRead(std::string const&, double);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief release a write lock
  //////////////////////////////////////////////////////////////////////////////

  bool unlockWrite(std::string const&, double);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief encode a key for etcd
  ///
  /// We need the following properties: The encoding of a concatenation
  /// of two strings is the concatenation of the two encodings. Thus the
  /// empty string is encoded to the empty string.
  ///
  /// Here is an overview of where encoded keys and where decoded keys are
  /// used. The user gives normal, decoded keys. On the way "into" etcd,
  /// keys are encoded only in buildURL. This means in particular that all
  /// arguments to methods that take keys all get decoded, normal keys.
  /// AgencyCommLockers also completely work with unencoded keys.
  ///
  /// On the way out, the JSON answers of etcd of course contain encoded
  /// keys and the response is only stored as a big string containing JSON.
  /// Therefore things stored in AgencyCommResult have encoded keys.
  /// We parse the JSON and when we recursively work on it in processJsonNode
  /// we decode the key when we see it.
  //////////////////////////////////////////////////////////////////////////////

  static std::string encodeKey(std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief decode a key for etcd
  //////////////////////////////////////////////////////////////////////////////

  static std::string decodeKey(std::string const&);

  
 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief create a query parameter for a TTL value
  //////////////////////////////////////////////////////////////////////////////

  std::string ttlParam(double, bool);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief acquire a lock
  //////////////////////////////////////////////////////////////////////////////

  bool lock(std::string const&, double, double, VPackSlice const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief release a lock
  //////////////////////////////////////////////////////////////////////////////

  bool unlock(std::string const&, VPackSlice const&, double);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief pop an endpoint from the queue
  //////////////////////////////////////////////////////////////////////////////

  AgencyEndpoint* popEndpoint(std::string const&);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief reinsert an endpoint into the queue
  //////////////////////////////////////////////////////////////////////////////

  void requeueEndpoint(AgencyEndpoint*, bool);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief construct a URL
  //////////////////////////////////////////////////////////////////////////////

  std::string buildUrl(std::string const&) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief construct a URL, without a key
  //////////////////////////////////////////////////////////////////////////////

  std::string buildUrl() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sends an HTTP request to the agency, handling failover
  //////////////////////////////////////////////////////////////////////////////

  bool sendWithFailover(arangodb::rest::HttpRequest::HttpRequestType, double,
                        AgencyCommResult&, std::string const&,
                        std::string const&, bool);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief sends data to the URL
  //////////////////////////////////////////////////////////////////////////////

  bool send(arangodb::httpclient::GeneralClientConnection*,
            arangodb::rest::HttpRequest::HttpRequestType, double,
            AgencyCommResult&, std::string const&, std::string const&);

  
 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief automatically add unknown endpoints if redirected to by agency?
  //////////////////////////////////////////////////////////////////////////////

  bool _addNewEndpoints;

  
 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief the static global URL prefix
  //////////////////////////////////////////////////////////////////////////////

  static std::string const AGENCY_URL_PREFIX;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief the (variable) global prefix
  //////////////////////////////////////////////////////////////////////////////

  static std::string _globalPrefix;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief endpoints lock
  //////////////////////////////////////////////////////////////////////////////

  static arangodb::basics::ReadWriteLock _globalLock;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief all endpoints
  //////////////////////////////////////////////////////////////////////////////

  static std::list<AgencyEndpoint*> _globalEndpoints;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief global connection options
  //////////////////////////////////////////////////////////////////////////////

  static AgencyConnectionOptions _globalConnectionOptions;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief number of connections per endpoint
  //////////////////////////////////////////////////////////////////////////////

  static size_t const NumConnections = 3;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief initial sleep time
  //////////////////////////////////////////////////////////////////////////////

  static unsigned long const InitialSleepTime = 5000;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief maximum sleep time
  //////////////////////////////////////////////////////////////////////////////

  static unsigned long const MaxSleepTime = 50000;
};
}

#endif

