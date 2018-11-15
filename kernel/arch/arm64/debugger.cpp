// Copyright 2016 The Fuchsia Authors
//
// Use of this source code is governed by a MIT-style
// license that can be found in the LICENSE file or at
// https://opensource.org/licenses/MIT

#include <arch/arm64.h>
#include <arch/arm64/registers.h>
#include <arch/debugger.h>
#include <err.h>
#include <kernel/thread.h>
#include <kernel/thread_lock.h>
#include <string.h>
#include <sys/types.h>
#include <zircon/syscalls/debug.h>
#include <zircon/types.h>

// Only the NZCV flags (bits 31 to 28 respectively) of the CPSR are
// readable and writable by userland on ARM64.
static uint32_t kUserVisibleFlags = 0xf0000000;

// SS (="Single Step") is bit 0 in MDSCR_EL1.
static constexpr uint64_t kMdscrSSMask = 1;

// Single Step for PSTATE, see ARMv8 Manual C5.2.18, enable Single step for Process
static constexpr uint64_t kSSMaskSPSR = (1 << 21);

zx_status_t arch_get_general_regs(struct thread* thread, zx_thread_state_general_regs_t* out) {
    Guard<spin_lock_t, IrqSave> thread_lock_guard{ThreadLock::Get()};

    // Punt if registers aren't available. E.g.,
    // ZX-563 (registers aren't available in synthetic exceptions)
    if (thread->arch.suspended_general_regs == nullptr)
        return ZX_ERR_NOT_SUPPORTED;

    struct arm64_iframe_long* in = thread->arch.suspended_general_regs;
    DEBUG_ASSERT(in);

    static_assert(sizeof(in->r) == sizeof(out->r), "");
    memcpy(&out->r[0], &in->r[0], sizeof(in->r));
    out->lr = in->lr;
    out->sp = in->usp;
    out->pc = in->elr;
    out->cpsr = in->spsr & kUserVisibleFlags;

    return ZX_OK;
}

zx_status_t arch_set_general_regs(struct thread* thread, const zx_thread_state_general_regs_t* in) {
    Guard<spin_lock_t, IrqSave> thread_lock_guard{ThreadLock::Get()};

    // Punt if registers aren't available. E.g.,
    // ZX-563 (registers aren't available in synthetic exceptions)
    if (thread->arch.suspended_general_regs == nullptr)
        return ZX_ERR_NOT_SUPPORTED;

    struct arm64_iframe_long* out = thread->arch.suspended_general_regs;
    DEBUG_ASSERT(out);

    static_assert(sizeof(out->r) == sizeof(in->r), "");
    memcpy(&out->r[0], &in->r[0], sizeof(in->r));
    out->lr = in->lr;
    out->usp = in->sp;
    out->elr = in->pc;
    out->spsr = (out->spsr & ~kUserVisibleFlags) | (in->cpsr & kUserVisibleFlags);

    return ZX_OK;
}

zx_status_t arch_get_single_step(struct thread* thread, bool* single_step) {
    Guard<spin_lock_t, IrqSave> thread_lock_guard{ThreadLock::Get()};

    // Punt if registers aren't available. E.g.,
    // ZX-563 (registers aren't available in synthetic exceptions)
    if (thread->arch.suspended_general_regs == nullptr)
        return ZX_ERR_NOT_SUPPORTED;
    struct arm64_iframe_long* regs = thread->arch.suspended_general_regs;

    const bool mdscr_ss_enable = !!(regs->mdscr & kMdscrSSMask);
    const bool spsr_ss_enable = !!(regs->spsr & kSSMaskSPSR);

    *single_step = mdscr_ss_enable && spsr_ss_enable;
    return ZX_OK;
}

zx_status_t arch_set_single_step(struct thread* thread, bool single_step) {
    Guard<spin_lock_t, IrqSave> thread_lock_guard{ThreadLock::Get()};

    // Punt if registers aren't available. E.g.,
    // ZX-563 (registers aren't available in synthetic exceptions)
    if (thread->arch.suspended_general_regs == nullptr)
        return ZX_ERR_NOT_SUPPORTED;
    struct arm64_iframe_long* regs = thread->arch.suspended_general_regs;
    if (single_step) {
        regs->mdscr |= kMdscrSSMask;
        regs->spsr |= kSSMaskSPSR;
    } else {
        regs->mdscr &= ~kMdscrSSMask;
        regs->spsr &= ~kSSMaskSPSR;
    }
    return ZX_OK;
}

zx_status_t arch_get_fp_regs(struct thread* thread, zx_thread_state_fp_regs* out) {
    // There are no ARM fp regs.
    (void)out;
    return ZX_OK;
}

zx_status_t arch_set_fp_regs(struct thread* thread, const zx_thread_state_fp_regs* in) {
    // There are no ARM fp regs.
    (void)in;
    return ZX_OK;
}

zx_status_t arch_get_vector_regs(struct thread* thread, zx_thread_state_vector_regs* out) {
    Guard<spin_lock_t, IrqSave> thread_lock_guard{ThreadLock::Get()};

    const fpstate* in = &thread->arch.fpstate;
    out->fpcr = in->fpcr;
    out->fpsr = in->fpsr;
    for (int i = 0; i < 32; i++) {
        out->v[i].low = in->regs[i * 2];
        out->v[i].high = in->regs[i * 2 + 1];
    }

    return ZX_OK;
}

zx_status_t arch_set_vector_regs(struct thread* thread, const zx_thread_state_vector_regs* in) {
    Guard<spin_lock_t, IrqSave> thread_lock_guard{ThreadLock::Get()};

    fpstate* out = &thread->arch.fpstate;
    out->fpcr = in->fpcr;
    out->fpsr = in->fpsr;
    for (int i = 0; i < 32; i++) {
        out->regs[i * 2] = in->v[i].low;
        out->regs[i * 2 + 1] = in->v[i].high;
    }

    return ZX_OK;
}

zx_status_t arch_get_debug_regs(struct thread* thread, zx_thread_state_debug_regs* out) {
    arm64_debug_state_t state = {};
    Guard<spin_lock_t, IrqSave> thread_lock_guard{ThreadLock::Get()};

    // TODO(ZX-3025): Change the getter of these values to be the in-memory ones instead of
    //                querying the HW. Currently this behaviour is wrong, because it will return
    //                the debug state of the *current* thread running instead of the one named
    //                by the handle.
    //                This is what the x86 counterpart does.
    arm64_read_hw_debug_regs(&state);

    // Copy over the state from the internal representation.
    out->hw_bps_count = state.hw_bps_count;
    for (size_t i = 0; i < state.hw_bps_count; i++) {
        out->hw_bps[i].dbgbcr = state.hw_bps[i].dbgbcr;
        out->hw_bps[i].dbgbvr = state.hw_bps[i].dbgbvr;
    }

    // Hacked through the last debug registers for now for development.
    // THIS CODE WILL GO AWAY!
    // This normally doesn't affect functionality as normally a CPU implementation has around six
    // debug registers.
    // TODO(donosoc): This should be exposed through a standard interface.
    //                Either the sysinfo fidl, the vDSO info mapping or some other mechanism.
    out->hw_bps[AARCH64_MAX_HW_BREAKPOINTS - 1].dbgbvr = ARM64_READ_SYSREG(id_aa64dfr0_el1);
    out->hw_bps[AARCH64_MAX_HW_BREAKPOINTS - 2].dbgbvr = ARM64_READ_SYSREG(mdscr_el1);

    return ZX_OK;
}

zx_status_t arch_set_debug_regs(struct thread* thread, const zx_thread_state_debug_regs* in) {
    arm64_debug_state_t state = {};
    Guard<spin_lock_t, IrqSave> thread_lock_guard{ThreadLock::Get()};

    // We copy over the state from the input.
    state.hw_bps_count = in->hw_bps_count;
    size_t bp_count = arm64_hw_breakpoint_count();
    for (size_t i = 0; i < bp_count; i++) {
        state.hw_bps[i].dbgbcr = in->hw_bps[i].dbgbcr;
        state.hw_bps[i].dbgbvr = in->hw_bps[i].dbgbvr;
    }

    if (!arm64_validate_debug_state(&state)) {
        return ZX_ERR_INVALID_ARGS;
    }

    arm64_write_hw_debug_regs(&state);

    return ZX_OK;
}

zx_status_t arch_get_x86_register_fs(struct thread* thread, uint64_t* out) {
    // There are no FS register on ARM.
    (void)out;
    return ZX_ERR_NOT_SUPPORTED;
}

zx_status_t arch_set_x86_register_fs(struct thread* thread, const uint64_t* in) {
    // There are no FS register on ARM.
    (void)in;
    return ZX_ERR_NOT_SUPPORTED;
}

zx_status_t arch_get_x86_register_gs(struct thread* thread, uint64_t* out) {
    // There are no GS register on ARM.
    (void)out;
    return ZX_ERR_NOT_SUPPORTED;
}

zx_status_t arch_set_x86_register_gs(struct thread* thread, const uint64_t* in) {
    // There are no GS register on ARM.
    (void)in;
    return ZX_ERR_NOT_SUPPORTED;
}
