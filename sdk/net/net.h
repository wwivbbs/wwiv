/**************************************************************************/
/*                                                                        */
/*                              WWIV Version 5.x                          */
/*             Copyright (C)1998-2021, WWIV Software Services             */
/*                                                                        */
/*    Licensed  under the  Apache License, Version  2.0 (the "License");  */
/*    you may not use this  file  except in compliance with the License.  */
/*    You may obtain a copy of the License at                             */
/*                                                                        */
/*                http://www.apache.org/licenses/LICENSE-2.0              */
/*                                                                        */
/*    Unless  required  by  applicable  law  or agreed to  in  writing,   */
/*    software  distributed  under  the  License  is  distributed on an   */
/*    "AS IS"  BASIS, WITHOUT  WARRANTIES  OR  CONDITIONS OF ANY  KIND,   */
/*    either  express  or implied.  See  the  License for  the specific   */
/*    language governing permissions and limitations under the License.   */
/*                                                                        */
/**************************************************************************/
#ifndef INCLUDED_SDK_NET_NET_H
#define INCLUDED_SDK_NET_NET_H

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "core/uuid.h"
#include "sdk/fido/nodelist.h"

// We don't want anyone else including legacy_net.h
// ReSharper disable once CppUnusedIncludeDirective
#include "sdk/net/legacy_net.h"


namespace wwiv::sdk::net {

/*
 * This data is also read in from a text file.  It tells how much it costs for
 * sysnum to call out to other systems.  It is stored in connect.net.
 * This is never written as binary data.
 */
struct net_interconnect_rec {
  /* outward calling system */
  uint16_t sysnum;
  /* num systems it can call */
  uint16_t numsys;

  net_interconnect_rec() : sysnum(0), numsys(0) {}
  void clear() noexcept {
    numsys = 0;
    sysnum = 0;
    connect.clear();
    cost.clear();
  }
  /* points to an array of numsys integers that tell which
   * other systems sysnum can connect to
   */
  std::vector<uint16_t> connect;
  /*
   * cost[] - points to an array of numsys floating point numbers telling
   *   how much it costs to connect to that system, per minute.  ie, it would
   *   cost (cost[1]) dollars per minute for sysnum to call out to system
   *   number (connect[1]).
   */
  std::vector<float> cost;
};

/**
 * Contains per-node data in callout.net
 */
struct net_call_out_rec {
  net_call_out_rec() = default;
  net_call_out_rec(std::string f, uint16_t sn, uint16_t mn, uint16_t op, uint16_t ca, int8_t nh,
                   int8_t xh, std::string pw, uint8_t, uint16_t nk)
      : ftn_address(std::move(f)), sysnum(sn), macnum(mn), options(op), call_every_x_minutes(ca),
        min_hr(nh), max_hr(xh), session_password(std::move(pw)), min_k(nk) {}
  // FTN Address.
  std::string ftn_address;
  /* system number */
  uint16_t sysnum = 0;
  /* macro/script to use */
  uint16_t macnum = 0;
  /* bit mapped */
  uint16_t options = 0;
  /* hours between callouts */
  uint16_t call_every_x_minutes = 0;
  /* callout min hour */
  int8_t min_hr = -1;
  /* callout max hour */
  int8_t max_hr = -1;
  /* password for system */
  std::string session_password;
  /* minimum # k before callout */
  uint16_t min_k = 0;
};


/**
 * Indicates this is the fake FTN outbound node.  This should
 * not be exposed to users unless required. It's an implementation detail.
 */
static constexpr int16_t FTN_FAKE_OUTBOUND_NODE = 32765;
static constexpr char FTN_FAKE_OUTBOUND_ADDRESS[] = "@32765";

/**
 * Indicates this is the fake FTN outbound node.  This should
 * not be exposed to users unless required. It's an implementation detail.
 */
static constexpr int16_t INTERNET_NEWS_FAKE_OUTBOUND_NODE = 32766;
static constexpr char INTERNET_NEWS_FAKE_OUTBOUND_ADDRESS[] = "@32766";

/**
 * Indicates this is the fake FTN outbound node.  This should
 * not be exposed to users unless required. It's an implementation detail.
 */
static constexpr int16_t INTERNET_EMAIL_FAKE_OUTBOUND_NODE = 32767;
static constexpr char INTERNET_EMAIL_FAKE_OUTBOUND_ADDRESS[] = "@32767";

/**
 * Used to indicate no node number in functions that return -1 when no
 * WWIVnet node number is found.
 */
static constexpr uint16_t WWIVNET_NO_NODE = 65535;

enum class fido_packet_t { unset, type2_plus };
enum class fido_transport_t { unset, directory, binkp };
enum class fido_mailer_t { unset, flo, attach };

enum class fido_bundle_status_t : char {
  normal = 'f',
  crash = 'c',
  direct = 'd',
  hold = 'h',
  immediate = 'i',
  // Do not use for creating a packet.
  unknown = 'x'
};

// Remember to update the serialize function in networks_cereal.h and also
// the code (packet_config_for) in fido_callout.cpp when updating these.
/**
 * Fido specific per-packet settings.
 */
struct fido_packet_config_t {
  // Type of packet to create
  fido_packet_t packet_type = fido_packet_t::unset;
  // File extension to map to type defined in archivers.dat
  std::string compression_type;
  // Password to use in the packet.  It must be <= 8 chars.
  std::string packet_password;
  // Password to use for areafix requests. It must be <= 8 chars.
  std::string tic_password;
  // Password to use for areafix requests. It must be <= 8 chars.
  std::string areafix_password;
  // Maximum size of the bundles in bytes before a new one is created.
  int max_archive_size = 0;
  // Maximum size of the packets in bytes before a new one is created.
  int max_packet_size = 0;
  // Status used for the bundles or packets to send.
  // i.e.: CRASH, IMMEDIATE, NORMAL.
  fido_bundle_status_t netmail_status = fido_bundle_status_t::normal;
};

/**
 * Contains the binkp session specific settings. This can come
 * from a fidonet nodelist, WWIVnet's binkp.net, or overrides
 * specified in the address settings.
 */
struct binkp_session_config_t {
  std::string host;
  int port{0};
  std::string password;
};

/**
 * Minimal callout configuration
 */
struct network_callout_config_t {
  bool auto_callouts{false};
  int call_every_x_minutes{15};
  int min_k{0};
};

/**
 * Specific config for a fido node.
 */
struct fido_node_config_t {
  // Space separated list of nodes that route through this node.
  // i.e.: "1:*" will route all of zone 1 through this node.
  std::string routes;
  // Configuration for packet specific options.
  fido_packet_config_t packet_config;
  // BinkP session options.
  binkp_session_config_t binkp_config;
  // Automatic callout config
  network_callout_config_t callout_config;
};

// ***************************************************************************************
// ****
// **** Remember to update the serialize function in networks_cereal.h when updating these.
// ****
// ***************************************************************************************

/**
 * Fido specific per-network settings.
 */
struct fido_network_config_t {
  // Your FTN network address. (i.e. 1:100/123) [3d-5d accepted].
  std::string fido_address;
  // Your FTN nodelist base name (i.e. NODELiST)
  std::string nodelist_base;
  // Fidonet mailer type {FLO, Attach}.
  // Only FLO is even close to being supported.
  fido_mailer_t mailer_type;
  // Type of transport to use for this packet.
  fido_transport_t transport;
  // Inbound directory for packets
  std::string inbound_dir;
  // Inbound temporary directory.
  std::string temp_inbound_dir;
  // Outbound temporary directory.
  std::string temp_outbound_dir;
  // Outbound directory for packets
  std::string outbound_dir;
  // Location of FidoNet NetMail directory.
  // Note that this usually lives under your mailer's directory
  // if you are using an ATTACH style mailer.
  std::string netmail_dir;
  // Configuration for packet specific options.
  fido_packet_config_t packet_config;
  // Location to move bad packets
  std::string bad_packets_dir;
  // Place tic files here.  based off of net_dir.
  std::string tic_dir;
  // Unknown files go here.  based off of net_dir.
  std::string unknown_dir;
  // Default Origin line to use for this network.
  std::string origin_line;
  
  // Process TIC files
  bool process_tic{false};
  // Convert wwiv heart codes to standard pipes.
  bool wwiv_pipe_color_codes{true};
  // convert wwiv user color pipe codes to standard pipe codes.
  bool wwiv_heart_color_codes{false};
  // Allow any pipe codes, WWIV and standard.
  bool allow_any_pipe_codes{true};
};

/**
 * The types of networks currently or optimistically one day supported by WWIV.
 */
enum class network_type_t : uint8_t { wwivnet = 0, ftn, internet, news, unknown = 255 };

/**
 * Internal class for network defined in networks.json.
 *
 * This class used to be known as net_neworks_rec and is persisted
 * in legacy wwiv format as net_networks_rec_disk.
 */
class Network {
public:
  Network() = default;
  Network(network_type_t t, std::string n) : type(t), name(std::move(n)) {}
  Network(network_type_t t, std::string n, std::filesystem::path d, uint16_t s)
      : type(t), name(std::move(n)), dir(std::move(d)), sysnum(s) {}

  /**
   * Tries to load the Nodelist for this network
   * returns true if the nodelist is found, and parsed successfully.
   */
  [[nodiscard]] bool try_load_nodelist();

  /*
   * The current network number, assigned when loaded.
   */
  [[nodiscard]] int network_number() const noexcept { return network_number_; };

  /* type of network */
  network_type_t type{network_type_t::unknown};
  /* network name. Up to 16 chars*/
  std::string name;
  /* directory for net data */
  std::filesystem::path dir;
  /* system number */
  uint16_t sysnum{0};

  // Used by FTN type nodes.
  fido_network_config_t fido{};
  // uuid_t to identify the network locally.
  core::uuid_t uuid;
  // A parsed nodelist if available.
  std::shared_ptr<fido::Nodelist> nodelist;

// private: TODO make Networks a friend class
  // The current network number, assigned when loaded.
  // ** This is a transient field.
  int network_number_{-1};
};

std::string to_string(const net::Network& n);


}

#endif
