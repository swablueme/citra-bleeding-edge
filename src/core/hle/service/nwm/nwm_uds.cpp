// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <unordered_map>
#include <vector>
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/core_timing.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/result.h"
#include "core/hle/service/nwm/nwm_uds.h"
#include "core/memory.h"

namespace Service {
namespace NWM {

// Event that is signaled every time the connection status changes.
static Kernel::SharedPtr<Kernel::Event> connection_status_event;

// Shared memory provided by the application to store the receive buffer.
// This is not currently used.
static Kernel::SharedPtr<Kernel::SharedMemory> recv_buffer_memory;

// Connection status of this 3DS.
static ConnectionStatus connection_status{};

// Node information about the current 3DS.
// TODO(Subv): Keep an array of all nodes connected to the network,
// that data has to be retransmitted in every beacon frame.
static NodeInfo node_info;

// Mapping of bind node ids to their respective events.
static std::unordered_map<u32, Kernel::SharedPtr<Kernel::Event>> bind_node_events;

// The wifi network channel that the network is currently on.
// Since we're not actually interacting with physical radio waves, this is just a dummy value.
static u8 network_channel = DefaultNetworkChannel;

// Information about the network that we're currently connected to.
static NetworkInfo network_info;

// Event that will generate and send the 802.11 beacon frames.
static int beacon_broadcast_event;

/**
 * NWM_UDS::Shutdown service function
 *  Inputs:
 *      1 : None
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void Shutdown(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    // TODO(purpasmart): Verify return header on HW

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_NWM, "(STUBBED) called");
}

/**
 * NWM_UDS::RecvBeaconBroadcastData service function
 * Returns the raw beacon data for nearby networks that match the supplied WlanCommId.
 *  Inputs:
 *      1 : Output buffer max size
 *    2-3 : Unknown
 *    4-5 : Host MAC address.
 *   6-14 : Unused
 *     15 : WLan Comm ID
 *     16 : Id
 *     17 : Value 0
 *     18 : Input handle
 *     19 : (Size<<4) | 12
 *     20 : Output buffer ptr
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void RecvBeaconBroadcastData(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 out_buffer_size = cmd_buff[1];
    u32 unk1 = cmd_buff[2];
    u32 unk2 = cmd_buff[3];
    u32 mac_address = cmd_buff[4];

    u32 unk3 = cmd_buff[6];

    u32 wlan_comm_id = cmd_buff[15];
    u32 ctr_gen_id = cmd_buff[16];
    u32 value = cmd_buff[17];
    u32 input_handle = cmd_buff[18];
    u32 new_buffer_size = cmd_buff[19];
    u32 out_buffer_ptr = cmd_buff[20];

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_NWM,
                "(STUBBED) called out_buffer_size=0x%08X, unk1=0x%08X, unk2=0x%08X,"
                "mac_address=0x%08X, unk3=0x%08X, wlan_comm_id=0x%08X, ctr_gen_id=0x%08X,"
                "value=%u, input_handle=0x%08X, new_buffer_size=0x%08X, out_buffer_ptr=0x%08X",
                out_buffer_size, unk1, unk2, mac_address, unk3, wlan_comm_id, ctr_gen_id, value,
                input_handle, new_buffer_size, out_buffer_ptr);
}

/**
 * NWM_UDS::Initialize service function
 *  Inputs:
 *      1 : Shared memory size
 *   2-11 : Input NodeInfo Structure
 *     12 : 2-byte Version
 *     13 : Value 0
 *     14 : Shared memory handle
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Value 0
 *      3 : Output event handle
 */
static void InitializeWithVersion(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1B, 12, 2);

    u32 sharedmem_size = rp.Pop<u32>();

    // Update the node information with the data the game gave us.
    rp.PopRaw(node_info);

    u16 version;
    rp.PopRaw(version);
    Kernel::Handle sharedmem_handle = rp.PopHandle();

    recv_buffer_memory = Kernel::g_handle_table.Get<Kernel::SharedMemory>(sharedmem_handle);

    ASSERT_MSG(recv_buffer_memory->size == sharedmem_size, "Invalid shared memory size.");

    // Reset the connection status, it contains all zeros after initialization,
    // except for the actual status value.
    connection_status = {};
    connection_status.status = static_cast<u32>(NetworkStatus::NotConnected);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyHandles(Kernel::g_handle_table.Create(connection_status_event).MoveFrom());

    LOG_DEBUG(Service_NWM, "called sharedmem_size=0x%08X, version=0x%08X, sharedmem_handle=0x%08X",
              sharedmem_size, version, sharedmem_handle);
}

/**
 * NWM_UDS::GetConnectionStatus service function.
 * Returns the connection status structure for the currently open network connection.
 * This structure contains information about the connection,
 * like the number of connected nodes, etc.
 *  Inputs:
 *      0 : Command header.
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 *      2-13 : Channel of the current WiFi network connection.
 */
static void GetConnectionStatus(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0xB, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(13, 0);

    rb.Push(RESULT_SUCCESS);
    rb.PushRaw(connection_status);

    LOG_DEBUG(Service_NWM, "called");
}

/**
 * NWM_UDS::Bind service function.
 * Binds a BindNodeId to a data channel and retrieves a data event.
 *  Inputs:
 *      1 : BindNodeId
 *      2 : Receive buffer size.
 *      3 : u8 Data channel to bind to.
 *      4 : Network node id.
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Copy handle descriptor.
 *      3 : Data available event handle.
 */
static void Bind(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x12, 4, 0);

    u32 bind_node_id = rp.Pop<u32>();
    u32 recv_buffer_size = rp.Pop<u32>();
    u8 data_channel;
    rp.PopRaw(data_channel);
    u16 network_node_id;
    rp.PopRaw(network_node_id);

    // TODO(Subv): Store the data channel and verify it when receiving data frames.

    LOG_DEBUG(Service_NWM, "called");

    if (data_channel == 0) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ResultCode(ErrorDescription::NotAuthorized, ErrorModule::UDS,
                           ErrorSummary::WrongArgument, ErrorLevel::Usage));
        return;
    }

    // Create a new event for this bind node.
    // TODO(Subv): Signal this event when new data is received on this data channel.
    auto event = Kernel::Event::Create(Kernel::ResetType::OneShot,
                                       "NWM::BindNodeEvent" + std::to_string(bind_node_id));
    bind_node_events[bind_node_id] = event;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);

    rb.Push(RESULT_SUCCESS);
    rb.PushCopyHandles(Kernel::g_handle_table.Create(event).MoveFrom());
}

/**
 * NWM_UDS::BeginHostingNetwork service function.
 * Creates a network and starts broadcasting its presence.
 *  Inputs:
 *      1 : Passphrase buffer size.
 *      3 : VAddr of the NetworkInfo structure.
 *      5 : VAddr of the passphrase.
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void BeginHostingNetwork(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1D, 1, 4);

    const u32 passphrase_size = rp.Pop<u32>();

    size_t desc_size;
    const VAddr network_info_address = rp.PopStaticBuffer(&desc_size, false);
    ASSERT(desc_size == sizeof(NetworkInfo));
    const VAddr passphrase_address = rp.PopStaticBuffer(&desc_size, false);
    ASSERT(desc_size == passphrase_size);

    // TODO(Subv): Store the passphrase and verify it when attempting a connection.

    LOG_DEBUG(Service_NWM, "called");

    Memory::ReadBlock(network_info_address, &network_info, sizeof(NetworkInfo));

    connection_status.status = static_cast<u32>(NetworkStatus::ConnectedAsHost);
    connection_status.max_nodes = network_info.max_nodes;

    // There's currently only one node in the network (the host).
    connection_status.total_nodes = 1;
    // The host is always the first node
    connection_status.network_node_id = 1;
    node_info.network_node_id = 1;
    // Set the bit 0 in the nodes bitmask to indicate that node 1 is already taken.
    connection_status.node_bitmask |= 1;

    // If the game has a preferred channel, use that instead.
    if (network_info.channel != 0)
        network_channel = network_info.channel;

    connection_status_event->Signal();

    // Start broadcasting the network, send a beacon frame every 102.4ms.
    CoreTiming::ScheduleEvent(msToCycles(DefaultBeaconInterval * MillisecondsPerTU),
                              beacon_broadcast_event, 0);

    LOG_WARNING(Service_NWM,
                "An UDS network has been created, but broadcasting it is unimplemented.");

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

/**
 * NWM_UDS::DestroyNetwork service function.
 * Closes the network that we're currently hosting.
 *  Inputs:
 *      0 : Command header.
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void DestroyNetwork(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x08, 0, 0);

    // TODO(Subv): Find out what happens if this is called while
    // no network is being hosted.

    // Unschedule the beacon broadcast event.
    CoreTiming::UnscheduleEvent(beacon_broadcast_event, 0);

    connection_status.status = static_cast<u8>(NetworkStatus::NotConnected);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_NWM, "called");
}

/**
 * NWM_UDS::GetChannel service function.
 * Returns the WiFi channel in which the network we're connected to is transmitting.
 *  Inputs:
 *      0 : Command header.
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Channel of the current WiFi network connection.
 */
static void GetChannel(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1A, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);

    u8 channel = network_channel;

    if (connection_status.status == static_cast<u32>(NetworkStatus::NotConnected))
        channel = 0;

    rb.Push(RESULT_SUCCESS);
    rb.PushRaw(channel);

    LOG_DEBUG(Service_NWM, "called");
}

/**
 * NWM_UDS::SetApplicationData service function.
 * Updates the application data that is being broadcast in the beacon frames
 * for the network that we're hosting.
 *  Inputs:
 *      1 : Data size.
 *      3 : VAddr of the data.
 *  Outputs:
 *      0 : Return header
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Channel of the current WiFi network connection.
 */
static void SetApplicationData(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1A, 1, 2);

    u32 size = rp.Pop<u32>();

    size_t desc_size;
    const VAddr address = rp.PopStaticBuffer(&desc_size, false);
    ASSERT(desc_size == size);

    LOG_DEBUG(Service_NWM, "called");

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    if (size > ApplicationDataSize) {
        rb.Push(ResultCode(ErrorDescription::TooLarge, ErrorModule::UDS,
                           ErrorSummary::WrongArgument, ErrorLevel::Usage));
        return;
    }

    network_info.application_data_size = size;
    Memory::ReadBlock(address, network_info.application_data.data(), size);

    rb.Push(RESULT_SUCCESS);
}

// Sends a 802.11 beacon frame with information about the current network.
static void BeaconBroadcastCallback(u64 userdata, int cycles_late) {
    // Don't do anything if we're not actually hosting a network
    if (connection_status.status != static_cast<u32>(NetworkStatus::ConnectedAsHost))
        return;

    // TODO(Subv): Actually generate the beacon and send it.

    // Start broadcasting the network, send a beacon frame every 102.4ms.
    CoreTiming::ScheduleEvent(msToCycles(DefaultBeaconInterval * MillisecondsPerTU) - cycles_late,
                              beacon_broadcast_event, 0);
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010442, nullptr, "Initialize (deprecated)"},
    {0x00020000, nullptr, "Scrap"},
    {0x00030000, Shutdown, "Shutdown"},
    {0x00040402, nullptr, "CreateNetwork (deprecated)"},
    {0x00050040, nullptr, "EjectClient"},
    {0x00060000, nullptr, "EjectSpectator"},
    {0x00070080, nullptr, "UpdateNetworkAttribute"},
    {0x00080000, DestroyNetwork, "DestroyNetwork"},
    {0x00090442, nullptr, "ConnectNetwork (deprecated)"},
    {0x000A0000, nullptr, "DisconnectNetwork"},
    {0x000B0000, GetConnectionStatus, "GetConnectionStatus"},
    {0x000D0040, nullptr, "GetNodeInformation"},
    {0x000E0006, nullptr, "DecryptBeaconData (deprecated)"},
    {0x000F0404, RecvBeaconBroadcastData, "RecvBeaconBroadcastData"},
    {0x00100042, SetApplicationData, "SetApplicationData"},
    {0x00110040, nullptr, "GetApplicationData"},
    {0x00120100, Bind, "Bind"},
    {0x00130040, nullptr, "Unbind"},
    {0x001400C0, nullptr, "PullPacket"},
    {0x00150080, nullptr, "SetMaxSendDelay"},
    {0x00170182, nullptr, "SendTo"},
    {0x001A0000, GetChannel, "GetChannel"},
    {0x001B0302, InitializeWithVersion, "InitializeWithVersion"},
    {0x001D0044, BeginHostingNetwork, "BeginHostingNetwork"},
    {0x001E0084, nullptr, "ConnectToNetwork"},
    {0x001F0006, nullptr, "DecryptBeaconData"},
    {0x00200040, nullptr, "Flush"},
    {0x00210080, nullptr, "SetProbeResponseParam"},
    {0x00220402, nullptr, "ScanOnConnection"},
};

NWM_UDS::NWM_UDS() {
    connection_status_event =
        Kernel::Event::Create(Kernel::ResetType::OneShot, "NWM::connection_status_event");

    Register(FunctionTable);

    beacon_broadcast_event =
        CoreTiming::RegisterEvent("UDS::BeaconBroadcastCallback", BeaconBroadcastCallback);
}

NWM_UDS::~NWM_UDS() {
    network_info = {};
    bind_node_events.clear();
    connection_status_event = nullptr;
    recv_buffer_memory = nullptr;

    connection_status = {};
    connection_status.status = static_cast<u32>(NetworkStatus::NotConnected);

    CoreTiming::UnscheduleEvent(beacon_broadcast_event, 0);
}

} // namespace NWM
} // namespace Service
