// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/service/service.h"

// Local-WLAN service

namespace Service {
namespace NWM {

const size_t ApplicationDataSize = 0xC8;
const u8 DefaultNetworkChannel = 11;

// Number of milliseconds in a TU.
const double MillisecondsPerTU = 1.024;
// Interval measured in TU, the default value is 100TU = 102.4ms
const u16 DefaultBeaconInterval = 100;

struct NodeInfo {
    u64_le friend_code_seed;
    std::array<u16_le, 10> username;
    INSERT_PADDING_BYTES(4);
    u16_le network_node_id;
    INSERT_PADDING_BYTES(6);
};

static_assert(sizeof(NodeInfo) == 40, "NodeInfo has incorrect size.");

enum class NetworkStatus {
    NotConnected = 3,
    ConnectedAsHost = 6,
    ConnectedAsClient = 9,
    ConnectedAsSpectator = 10,
};

struct ConnectionStatus {
    u32_le status;
    INSERT_PADDING_WORDS(1);
    u16_le network_node_id;
    INSERT_PADDING_BYTES(2);
    INSERT_PADDING_BYTES(32);
    u8 total_nodes;
    u8 max_nodes;
    u16_le node_bitmask;
};

static_assert(sizeof(ConnectionStatus) == 0x30, "ConnectionStatus has incorrect size.");

struct NetworkInfo {
    std::array<u8, 6> host_mac_address;
    u8 channel;
    INSERT_PADDING_BYTES(1);
    u8 initialized;
    INSERT_PADDING_BYTES(3);
    std::array<u8, 3> oui_value;
    u8 oui_type;
    // This field is received as BigEndian from the game.
    u32_be wlan_comm_id;
    u8 id;
    INSERT_PADDING_BYTES(1);
    u16_be attributes;
    u32_be network_id;
    u8 total_nodes;
    u8 max_nodes;
    INSERT_PADDING_BYTES(2);
    INSERT_PADDING_BYTES(0x1F);
    u8 application_data_size;
    std::array<u8, ApplicationDataSize> application_data;
};

static_assert(sizeof(NetworkInfo) == 0x108, "NetworkInfo has incorrect size.");

class NWM_UDS final : public Interface {
public:
    NWM_UDS();
    ~NWM_UDS() override;

    std::string GetPortName() const override {
        return "nwm::UDS";
    }
};

} // namespace NWM
} // namespace Service
