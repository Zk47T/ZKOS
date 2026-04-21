# ZKOS vs VinixOS: Architecture Comparison

This document provides a technical comparison between **ZKOS** (the kernel we just built) and **VinixOS** (the reference kernel). It highlights how ZKOS was adapted to serve as an educational stepping stone while drawing inspiration from VinixOS principles, without being a 1:1 copy.

## 1. Exception Levels & Privilege Boundaries

### VinixOS (AArch32/AArch64 Production Model)
- **Design:** Strict separation of privilege. The Kernel runs in Supervisor mode (SVC / EL1), while user applications run in User mode (USR / EL0).
- **Complexity:** Requires managing separate stack pointers (`SP_svc` vs `SP_usr` in AArch32, or `SP_EL1` vs `SP_EL0` in AArch64). Context switching involves jumping across privilege boundaries using `ERET` and complex exception frame setups (as seen in `task_stack_init()` pushing `SPSR_usr` and `LR_usr`).
- **Security:** True sandboxing. User code cannot touch kernel memory without triggering a fault.

### ZKOS (Educational Model)
- **Design:** Flat privilege level. Both the Kernel and "User" tasks run at **EL2** (Hypervisor level on i.MX93, chosen due to U-Boot/TF-A boot flow).
- **Simplification:** All tasks share the same privilege. Context switching is dramatically simpler. We don't need to juggle `SP_EL0` and `SP_EL2`; we just save/restore callee-saved registers (`x19-x30`) and the regular `SP` in `switch.S`.
- **Inspiration:** ZKOS introduces the *concept* of system calls via the `SVC` instruction (trap to vector table), identical to how user programs talk to the kernel, but skips the complex MMU isolation and EL dropping for now.

## 2. Memory Management (MMU)

### VinixOS
- Full virtual memory management with user-space memory isolation.
- Validates user pointers during system calls to prevent kernel panic exploits (`validate_user_pointer`).

### ZKOS
- **Identity Mapping:** We implemented a 2-level page table using 1GB block descriptors. Virtual Address = Physical Address.
- **Simplification:** This enables caching (`SCTLR_EL2.C/.I`) for performance without the complexity of managing per-process page tables (TTBR0/TTBR1 swapping).
- **Allocators:** ZKOS features a custom bitmap-based physical page allocator (similar concept to VinixOS) and a simple bump+free-list heap (`kmalloc`), essential for dynamically creating RAMFS files and tasks.

## 3. Scheduler & Multitasking

### VinixOS
- Preemptive scheduler with complex task states, wait queues (for blocking I/O), and process exit cleanup.

### ZKOS
- **Progression:** We built this in two steps to understand the evolution:
  1. **Cooperative:** Tasks must voluntarily call `yield()` (Lesson 9).
  2. **Preemptive:** Timer IRQ triggers `scheduler_tick()`, which forces a `context_switch()` (Lesson 10).
- **Inspiration:** The core `task_struct` and `schedule()` logic (round-robin) is heavily inspired by VinixOS, showcasing how timer interrupts hijack the CPU to enforce timeslices.
- **Simplification:** Non-blocking I/O in the shell. Instead of wait queues, the shell continuously polls UART and yields, avoiding complex sleep/wakeup kernel mechanics.

## 4. Syscalls & Interrupts (GIC)

### VinixOS
- Uses `INTC` (Interrupt Controller).
- Syscall dispatcher decodes complex arguments and relies on strict ABI definitions.

### ZKOS
- Uses modern **GICv3** via System Registers (`ICC_SRE_EL2`, etc.), highly specific to the Cortex-A55 on the i.MX93.
- **Inspiration:** The exception vector table (`vectors.S`) structure and the concept of decoding the Exception Syndrome Register (`ESR_EL2`) to intercept `SVC` commands (EC `0x15`).
- **Implementation:** ZKOS implements exactly 5 syscalls (write, read, yield, getpid, exit) to demonstrate the transition from direct driver access to API-driven OS access.

## 5. Filesystem (VFS)

### VinixOS
- Complex VFS supporting executable loading via ELF parsing (`sys_exec`).

### ZKOS
- **VFS Abstraction:** Implements a clean struct of function pointers (`struct vfs_operations`) inspired by VinixOS but simplified (16 global file descriptors).
- **RAMFS:** An embedded in-memory filesystem that allows `kmalloc`-based dynamic file creation and reading of static strings. No ELF loading yet, focusing strictly on I/O streams.

---
## Summary
**ZKOS is a brilliant middle-ground.** It sacrifices strict security boundaries (EL0 vs EL2) and advanced memory isolation to focus entirely on the *mechanics* of an OS: how exceptions route execution, how a timer steals CPU time, how a context switch actually manipulates registers, and how to abstract hardware via Syscalls and VFS. It is a perfect foundational step before tackling a production system like VinixOS or Linux structure.
