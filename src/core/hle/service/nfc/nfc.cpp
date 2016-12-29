// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/kernel/event.h"
#include "core/hle/service/nfc/nfc.h"
#include "core/hle/service/nfc/nfc_m.h"
#include "core/hle/service/nfc/nfc_u.h"

namespace Service {
namespace NFC {

static Kernel::SharedPtr<Kernel::Event> tag_in_range_event;

void GetTagInRangeEvent(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[3] = Kernel::g_handle_table.Create(tag_in_range_event).MoveFrom();
    LOG_WARNING(Service_NFC, "(STUBBED) called");
}

void Init() {
    AddService(new NFC_M());
    AddService(new NFC_U());

    tag_in_range_event =
        Kernel::Event::Create(Kernel::ResetType::OneShot, "NFC::tag_in_range_event");
}

void Shutdown() {
    tag_in_range_event = nullptr;
}

} // namespace NFC
} // namespace Service
