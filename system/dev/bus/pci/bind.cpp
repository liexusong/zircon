// Copyright 2019 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT
//
#include "bus.h"

#include <ddk/binding.h>
#include <ddk/device.h>
#include <ddk/platform-defs.h>

static zx_status_t pci_bus_bind(void* ctx, zx_device_t* parent) {
    return pci::Bus::Create(parent);
}

static zx_driver_ops_t pci_driver_ops = {
    .version = DRIVER_OPS_VERSION,
    .init = nullptr,
    .bind = pci_bus_bind,
    .create = nullptr,
    .release = nullptr,
};

// clang-format off
ZIRCON_DRIVER_BEGIN(pci, pci_driver_ops, "zircon", "0.1", 5)
    BI_MATCH_IF(EQ, BIND_PROTOCOL, ZX_PROTOCOL_PCIROOT),
    BI_ABORT_IF(NE, BIND_PROTOCOL, ZX_PROTOCOL_PDEV),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_VID, PDEV_VID_GENERIC),
    BI_ABORT_IF(NE, BIND_PLATFORM_DEV_PID, PDEV_PID_GENERIC),
    BI_MATCH_IF(EQ, BIND_PLATFORM_DEV_DID, PDEV_DID_KPCI),
ZIRCON_DRIVER_END(pci)
