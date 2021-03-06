// Copyright 2017 The Fuchsia Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ZIRCON_SYSTEM_DEV_MISC_PTY_PTY_CORE_H_
#define ZIRCON_SYSTEM_DEV_MISC_PTY_PTY_CORE_H_

#include <stdint.h>
#include <threads.h>

#include <ddk/device.h>
#include <fuchsia/hardware/pty/c/fidl.h>

#include <zircon/compiler.h>
#include <zircon/listnode.h>
#include <fuchsia/hardware/pty/c/fidl.h>

typedef struct pty_server pty_server_t;
typedef struct pty_client pty_client_t;

struct pty_server {
    zx_device_t* zxdev;

    // lock covers server and all its clients
    mtx_t lock;

    // track server lifetime
    int32_t refcount;

    // pending OOB events
    uint32_t events;

    // list of all clients
    list_node_t clients;

    // active client receives inbound data
    pty_client_t* active;

    // control client receives events
    pty_client_t* control;

    // called when data is written by active client
    // pty_server's lock is held across this call
    // (it is not legal to call back into any pty_server_*() functions)
    zx_status_t (*recv)(pty_server_t* ps, const void* data, size_t len, size_t* actual);

    // if non-null, called for unhandled client message ops
    // no lock is held across this call
    zx_status_t (*set_window_size)(void* ctx, const fuchsia_hardware_pty_WindowSize* size,
                                   fidl_txn_t* txn);

    // called when pty_server_t should be deleted
    // if NULL, free(ps) is called instead
    void (*release)(pty_server_t* ps);

    // window size in character cells
    uint32_t width;
    uint32_t height;
};

// this initializes everything *except* the embedded zx_device_t
void pty_server_init(pty_server_t* ps);

// write data through to active client
// if atomic is true, the send will be all-or-nothing
// if atomic is true ^c, etc processing is not done
zx_status_t pty_server_send(pty_server_t* ps, const void* data, size_t len, bool atomic,
                            size_t* actual);

// If the recv callback returns ZX_ERR_SHOULD_WAIT, pty_server_resume()
// must be called when it is possible to call it successfully again.
// ps->lock must be held.
void pty_server_resume_locked(pty_server_t* ps);

void pty_server_set_window_size(pty_server_t* ps, uint32_t w, uint32_t h);

// Device ops for pty_server. The |ctx| here must be the zx_device_t embedded in pty_server_t.
zx_status_t pty_server_fidl_OpenClient(void* ctx, uint32_t id, zx_handle_t handle, fidl_txn_t* txn);
void pty_server_release(void* ctx);

#endif // ZIRCON_SYSTEM_DEV_MISC_PTY_PTY_CORE_H_
