/*  Copyright 2012 MaidSafe.net limited

    This MaidSafe Software is licensed to you under (1) the MaidSafe.net Commercial License,
    version 1.0 or later, or (2) The General Public License (GPL), version 3, depending on which
    licence you accepted on initial access to the Software (the "Licences").

    By contributing code to the MaidSafe Software, or to this project generally, you agree to be
    bound by the terms of the MaidSafe Contributor Agreement, version 1.0, found in the root
    directory of this project at LICENSE, COPYING and CONTRIBUTOR respectively and also
    available at: http://www.maidsafe.net/licenses

    Unless required by applicable law or agreed to in writing, the MaidSafe Software distributed
    under the GPL Licence is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS
    OF ANY KIND, either express or implied.

    See the Licences for the specific language governing permissions and limitations relating to
    use of the MaidSafe Software.                                                                 */

/*******************************************************************************
*Guarantees                                                                    *
*______________________________________________________________________________*
*                                                                              *
*1:  Provide NAT traversal techniques where necessary.                         *
*2:  Read and Write configuration file to allow bootstrap from known nodes.    *
*3:  Allow retrieval of bootstrap nodes from known location.                   *
*4:  Remove bad nodes from all routing tables (ban from network).              *
*5:  Inform of changes in data range to be stored and sent to each node        *
*6:  Respond to every send that requires it, either with timeout or reply      *
*******************************************************************************/

#ifndef MAIDSAFE_ROUTING_ROUTING_API_H_
#define MAIDSAFE_ROUTING_ROUTING_API_H_

#include <future>
#include <memory>
#include <string>
#include <vector>

#include "boost/asio/ip/udp.hpp"
#include "boost/date_time/posix_time/posix_time_config.hpp"

#include "maidsafe/common/node_id.h"
#include "maidsafe/common/rsa.h"

#include "maidsafe/passport/types.h"

#include "maidsafe/routing/api_config.h"

namespace maidsafe {

namespace routing {

struct NodeInfo;

namespace test {
class GenericNode;
}

namespace detail {

template <typename FobType>
struct is_client : public std::true_type {};

template <>
struct is_client<passport::Pmid> : public std::false_type {};

template <>
struct is_client<const passport::Pmid> : public std::false_type {};

}  // namespace detail

class Routing {
 public:
  // Providing :
  // pmid as a paramater will create a non client routing object(vault).
  // maid as a paramater will create a mutating client.
  // NodeId as a parameter will create a non-mutating client
  // Non-mutating client means that random keys will be generated by routing for this node.
  template <typename FobType>
  explicit Routing(const FobType& fob)
      : pimpl_() {
    asymm::Keys keys;
    keys.private_key = fob.private_key();
    keys.public_key = fob.public_key();
    InitialisePimpl(detail::is_client<FobType>::value, NodeId(fob.name()->string()), keys);
  }

  // Joins the network. Valid method for requesting public key must be provided by the functor,
  // otherwise no node will be added to the routing table and node will fail to join the network.
  // To force the node to use a specific endpoint for bootstrapping, provide peer_endpoint (i.e.
  // private network).
  void Join(Functors functors, std::vector<boost::asio::ip::udp::endpoint> peer_endpoints =
                                   std::vector<boost::asio::ip::udp::endpoint>());

  // WARNING: THIS FUNCTION SHOULD BE ONLY USED TO JOIN FIRST TWO ZERO STATE NODES.
  int ZeroStateJoin(Functors functors, const boost::asio::ip::udp::endpoint& local_endpoint,
                    const boost::asio::ip::udp::endpoint& peer_endpoint, const NodeInfo& peer_info);

  // Sends message to a known destnation. (Typed Message API)
  // Throws on invalid paramaters
  template <typename T>
  void Send(const T& message);

  // Sends message to a known destnation.
  // If a valid response functor is provided, it will be called when:
  // a) the response is receieved or,
  // b) waiting time (Parameters::default_response_timeout) for receiving the response expires
  // Throws on invalid paramaters
  void SendDirect(const NodeId& destination_id,                       // ID of final destination
                  const std::string& message, const bool& cacheable,  // to cache message content
                  ResponseFunctor response_functor);                  // Called on response

  // Sends message to Parameters::node_group_size most closest nodes to destination_id. The node
  // having id equal to destination id is not considered as part of group and will not receive
  // group message
  // If a valid response functor is provided, it will be called when:
  // a) for each response receieved (Parameters::node_group_size responses expected) or,
  // b) waiting time (Parameters::default_response_timeout) for receiving the response expires
  // Throws on invalid paramaters
  void SendGroup(const NodeId& destination_id,  // ID of final destination or group centre
                 const std::string& message, const bool& cacheable,  // to cache message content
                 ResponseFunctor response_functor);                  // Called on each response

  // Compares own closeness to target against other known nodes' closeness to the target
  bool ClosestToId(const NodeId& target_id);

  // Returns: kInRange : if node_id is in group_id range (Group Range),
  // kInProximalRange : if node_id is in proximity of the group. Node not in range but,
  // (node_id ^ group_id) < (kNodeId_ ^ FurthestCloseNode()),
  // kOutwithRange : (node_id ^ group_id) > (kNodeId_ ^ FurthestCloseNode()).
  // Throws if node_id provided is not this node id & this node is not part of the group (group_id)
  // if kNodeId_ == group_id or node_id == group_id, it returns kOutwithRange
  GroupRangeStatus IsNodeIdInGroupRange(const NodeId& group_id, const NodeId& node_id) const;

  // Returns: kInRange : if this node_id is in group_id range (Group Range),
  // kInProximalRange : if this node_id is in proximity of the group. Node not in range but,
  // (kNodeId_ ^ group_id) < (kNodeId_ ^ FurthestCloseNode())
  // kOutwithRange : (kNodeId_ ^ group_id) > (kNodeId_ ^ FurthestCloseNode())
  // if kNodeId_ == group_id, it returns kOutwithRange
  GroupRangeStatus IsNodeIdInGroupRange(const NodeId& group_id) const;

  // Gets a random connected node from routing table (excluding closest
  // Parameters::closest_nodes_size nodes).
  // Shouldn't be called when routing table is likely to be smaller than closest_nodes_size.
  NodeId RandomConnectedNode();

  // Evaluates whether the sender_id is a legitimate source to send a request for performing
  // an operation on info_id
  bool EstimateInGroup(const NodeId& sender_id, const NodeId& info_id) const;

  // Returns the closest nodes to info_id
  std::future<std::vector<NodeId>> GetGroup(const NodeId& group_id);

  // Returns this node's id.
  NodeId kNodeId() const;

  // Returns a number between 0 to 100 representing % network health w.r.t. number of connections
  int network_status();

  // Returns the group matrix
  std::vector<NodeInfo> ClosestNodes();

  // Checks if routing table or group matrix contains given node id
  bool IsConnectedVault(const NodeId& node_id);

  // Checks if client routing table contains given node id
  bool IsConnectedClient(const NodeId& node_id);

  friend class test::GenericNode;

 private:
  Routing(const Routing&);
  Routing(const Routing&&);
  Routing& operator=(const Routing&);
  void InitialisePimpl(bool client_mode, const NodeId& node_id, const asymm::Keys& keys);

  class Impl;
  std::shared_ptr<Impl> pimpl_;
};

template <>
Routing::Routing(const NodeId& node_id);

template <>
void Routing::Send(const SingleToSingleMessage& message);
template <>
void Routing::Send(const SingleToGroupMessage& message);
template <>
void Routing::Send(const GroupToSingleMessage& message);
template <>
void Routing::Send(const GroupToGroupMessage& message);

template <typename T>
void Routing::Send(const T&) {
  T::message_type_must_be_one_of_the_specialisations_defined_as_typedefs_in_message_dot_h_file;
}

}  // namespace routing

}  // namespace maidsafe

#endif  // MAIDSAFE_ROUTING_ROUTING_API_H_
