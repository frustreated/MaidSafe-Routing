/*******************************************************************************
 *  Copyright 2012 maidsafe.net limited                                        *
 *                                                                             *
 *  The following source code is property of maidsafe.net limited and is not   *
 *  meant for external use.  The use of this code is governed by the licence   *
 *  file licence.txt found in the root of this directory and also on           *
 *  www.maidsafe.net.                                                          *
 *                                                                             *
 *  You are not free to copy, amend or otherwise use this source code without  *
 *  the explicit written permission of the board of directors of maidsafe.net. *
 ******************************************************************************/

#ifndef MAIDSAFE_ROUTING_NETWORK_UTILS_H_
#define MAIDSAFE_ROUTING_NETWORK_UTILS_H_

#include <memory>
#include <string>
#include <vector>

#include "boost/asio/ip/udp.hpp"
#include "boost/thread/shared_mutex.hpp"

#include "maidsafe/rudp/managed_connections.h"

#include "maidsafe/routing/node_info.h"
#include "maidsafe/routing/timer.h"


namespace maidsafe {

namespace routing {

namespace protobuf { class Message; }

class NonRoutingTable;
class RoutingTable;

namespace test { class GenericNode; }

class NetworkUtils {
 public:
  NetworkUtils(RoutingTable& routing_table, NonRoutingTable& non_routing_table,
                Timer& timer);
  void Stop();
  int Bootstrap(const std::vector<boost::asio::ip::udp::endpoint> &bootstrap_endpoints,
                rudp::MessageReceivedFunctor message_received_functor,
                rudp::ConnectionLostFunctor connection_lost_functor,
                boost::asio::ip::udp::endpoint local_endpoint = boost::asio::ip::udp::endpoint());
  int GetAvailableEndpoint(const boost::asio::ip::udp::endpoint& peer_endpoint,
                           rudp::EndpointPair& this_endpoint_pair,
                           rudp::NatType& this_nat_type);
  int Add(const boost::asio::ip::udp::endpoint& this_endpoint,
          const boost::asio::ip::udp::endpoint& peer_endpoint,
          const std::string& validation_data);
  void Remove(const boost::asio::ip::udp::endpoint& peer_endpoint);
  // For sending relay requests, message with empty source ID may be provided, along with
  // direct endpoint.
  void SendToDirectEndpoint(const protobuf::Message& message,
                            boost::asio::ip::udp::endpoint direct_endpoint,
                            rudp::MessageSentFunctor message_sent_functor);
  void SendToDirectEndpoint(const protobuf::Message& message,
                            boost::asio::ip::udp::endpoint direct_endpoint);
  // Handles relay response messages.  Also leave destination ID empty if needs to send as a relay
  // response message
  void SendToClosestNode(const protobuf::Message& message);
  boost::asio::ip::udp::endpoint bootstrap_endpoint() const;
  boost::asio::ip::udp::endpoint this_node_relay_endpoint() const;
  rudp::NatType nat_type();
  Timer& timer();
  friend class test::GenericNode;
#ifdef LOCAL_TEST
friend class Routing;
#endif

 private:
  NetworkUtils(const NetworkUtils&);
  NetworkUtils(const NetworkUtils&&);
  NetworkUtils& operator=(const NetworkUtils&);

  void OnConnectionLost(const boost::asio::ip::udp::endpoint& endpoint);
  void RudpSend(const protobuf::Message& message,
                boost::asio::ip::udp::endpoint endpoint,
                rudp::MessageSentFunctor message_sent_functor);
  void SendTo(const protobuf::Message& message,
              const NodeId& node_id,
              const boost::asio::ip::udp::endpoint& endpoint);
  void RecursiveSendOn(protobuf::Message message,
                       NodeInfo last_node_attempted = NodeInfo(),
                       int attempt_count = 0);
  void AdjustRouteHistory(protobuf::Message& message);
//  void SignMessage(protobuf::Message& message);

  boost::asio::ip::udp::endpoint bootstrap_endpoint_, this_node_relay_endpoint_;
  rudp::ConnectionLostFunctor connection_lost_functor_;
  RoutingTable& routing_table_;
  NonRoutingTable& non_routing_table_;
  Timer& timer_;
  std::unique_ptr<rudp::ManagedConnections> rudp_;
  boost::shared_mutex shared_mutex_;
  bool stopped_;
  rudp::NatType nat_type_;
};

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_NETWORK_UTILS_H_
