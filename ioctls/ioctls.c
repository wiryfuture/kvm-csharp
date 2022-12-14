#include <stdio.h>
#include <sys/ioctl.h>
#include <asm/ioctl.h>
#include <asm/bootparam.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <string.h>

#include "./ioctls.h"

/// @brief Asks kvm what version it is.
/// @param fd Open file descriptor for /dev/kvm
/// @return Usually 12
int KVM_GET_API_VERSION(int fd)
{
  __u_long req = _IO(KVM_ID, KVM_GET_API_VERSION_seq);
  return ioctl(fd, req, 0);
}

/// @brief Creates a new vm in kvm, with no resources assigned to it.
/// @param fd Open file descriptor for /dev/kvm
/// @return File descriptor of new vm, or -1 on error.
int KVM_CREATE_VM(int fd)
{
  __u_long req = _IO(KVM_ID, KVM_CREATE_VM_seq);
  return ioctl(fd, req, 0);
}

/// @brief Assigns a memory region to a virtual machine using the vm file descriptor.
/// @param fd Open file descriptor for a virtual machine.
/// @param region kvm_userspace_memory_region struct that tells kvm how and what memory region to assign to this vm.
/// @return 0 on success, -1 on failure.
int KVM_SET_USER_MEMORY_REGION(int fd, struct kvm_userspace_memory_region region)
{
  __u_long req = _IOW(KVM_ID, KVM_SET_USER_MEMORY_REGION_seq, struct kvm_userspace_memory_region);
  return ioctl(fd, req, &region);
}

/// @brief Asks kvm whether it supports and extension or not.
/// @param fd Open file descriptor for /dev/kvm.
/// @param extension_identifier Sequence number for the ioctl that is being asked about.
/// @return 0 if supported, positive integer to provide additionl information, or -1 if unsupported
unsigned int KVM_CHECK_EXTENSION(int fd, int extension_identifier)
{
  __u_long req = _IO(KVM_ID, KVM_CHECK_EXTENSION_seq);
  return ioctl(fd, req, extension_identifier);
}

/// @brief Creates a vcpu for a vm with the provides vcpu id.
/// @param fd File descriptor for a vm.
/// @param vcpu_id Which vcpu this is creating.
/// @return 0 on success or -1 on failure.
int KVM_CREATE_VCPU(int fd, unsigned int vcpu_id)
{
  __u_long req = _IO(KVM_ID, KVM_CREATE_VCPU_seq);
  return ioctl(fd, req, vcpu_id);
}

/// @brief Assigns special registers and regular registers necessary to start the vm.
/// @param vcpu File descriptor for a virtual cpu.
/// @param arm64 Whether the virtual machine is runnining on the arm64 platform or not.
/// @return 0 on success, -1 on failure
int KVM_GET_and_SET_SREGS(int vcpu, short arm64)
{
  if (arm64 == 0)
  {
    // get sregs
    struct kvm_sregs sregs;
    __u_long req = _IOR(KVM_ID, KVM_GET_SREGS_seq, struct kvm_sregs);
    int ret = ioctl(vcpu, req, &sregs);
    if (ret == -1)
    {
      return ret;
    }
    // init sregs
    sregs.cs.base = 0;
    sregs.cs.limit = ~0;
    sregs.cs.g = 1;

    sregs.ds.base = 0;
    sregs.ds.limit = ~0;
    sregs.ds.g = 1;

    sregs.fs.base = 0;
    sregs.fs.limit = ~0;
    sregs.fs.g = 1;

    sregs.gs.base = 0;
    sregs.gs.limit = ~0;
    sregs.gs.g = 1;

    sregs.es.base = 0;
    sregs.es.limit = ~0;
    sregs.es.g = 1;

    sregs.ss.base = 0;
    sregs.ss.limit = ~0;
    sregs.ss.g = 1;

    sregs.cs.db = 1;
    sregs.ss.db = 1;
    sregs.cr0 |= 1; // enables protected mode

    req = _IOW(KVM_ID, KVM_SET_SREGS_seq, struct kvm_sregs);
    ret = ioctl(vcpu, req, &sregs);
    return ret;
  }
  else
  {
    return -1;
  }
}

/// @brief Assigns registers for a vcpu.
/// @param vcpu File descriptor for a virtual cpu.
/// @return 0 on success, -1 on failure.
int KVM_SET_REGS(int vcpu)
{
  struct kvm_regs regs;
  regs.rflags = 2;
  regs.rip = 0x100000; // start of kernel
  regs.rsi = 0x10000;  // start of boot params
  __u_long req = _IOW(KVM_ID, KVM_SET_REGS_seq, struct kvm_regs);
  return ioctl(vcpu, req, &regs);
}

/// @brief Intel virtualisation quirk awfulness.
/// @param vm_fd
/// @return
int KVM_SET_TSS_ADDR(int vm_fd)
{
  __u_long req = _IO(KVM_ID, KVM_SET_TSS_ADDR_seq);
  return ioctl(vm_fd, req, 0xffffd000 /*using zserge's numbers rn*/ /*0xfffbc000 + 0x1000 numbers from qemu implementation*/);
}

int KVM_SET_IDENTITY_MAP_ADDR(int vm_fd)
{
  __u_long req = _IOW(KVM_ID, KVM_SET_IDENTITY_MAP_ADDR_seq, unsigned long);
  unsigned long identity_base = 0xffffc000 /*0xfeffc000*/;
  return ioctl(vm_fd, req, &identity_base);
}

int KVM_CREATE_IRQCHIP(int vm_fd)
{
  __u_long req = _IO(KVM_ID, KVM_CREATE_IRQCHIP_seq);
  return ioctl(vm_fd, req, 0);
}

int KVM_CREATE_PIT2(int vm_fd)
{
  __u_long req = _IOW(KVM_ID, KVM_CREATE_PIT2_seq, struct kvm_pit_config);
  struct kvm_pit_config pit = {
      .flags = 0};
  return ioctl(vm_fd, req, &pit);
}

int KVM_SET_CPUID2(int kvm_fd, int vcpu_fd)
{
  struct kvm_cpuid cpuid;
  cpuid.nent = sizeof(cpuid.entries) / sizeof(cpuid.entries[0]);
  __u_long req = _IOWR(KVM_ID, KVM_GET_SUPPORTED_CPUID_seq, struct kvm_cpuid2);
  int res = ioctl(kvm_fd, req, &cpuid);

  for (unsigned int i = 0; i < cpuid.nent; i++)
  {
    struct kvm_cpuid_entry2 *entry = &cpuid.entries[i];
    if (entry->function == KVM_CPUID_SIGNATURE)
    {
      entry->eax = KVM_CPUID_FEATURES;
      entry->ebx = 0x4b4d564b; // KVMK
      entry->ecx = 0x564b4d56; // VMKV
      entry->edx = 0x4d;       // M
    }
  }
  req = _IOW(KVM_ID, KVM_SET_CPUID2_seq, struct kvm_cpuid2);
  res = ioctl(vcpu_fd, req, &cpuid);
  return res;
}

/// @brief Load a linux bz2image into memory
/// @param mem_size
/// @param image_data
/// @param image_size
/// @return

int load_guest(void *memory_start, void *image_data, unsigned long image_size)
{
  struct boot_params *boot =
      (struct boot_params *)(((unsigned char *)memory_start) + 0x10000);
  void *cmdline = (void *)(((unsigned char *)memory_start) + 0x20000);
  void *kernel = (void *)(((unsigned char *)memory_start) + 0x100000);
  memset(boot, 0, sizeof(struct boot_params));
  memmove(boot, (void *)image_data, sizeof(struct boot_params));
  unsigned long setup_sectors = boot->hdr.setup_sects;
  unsigned long setupsz = (setup_sectors = 1) * 512;
  boot->hdr.vid_mode = 0xFFFF; // vga
  boot->hdr.type_of_loader = 0xFF;
  boot->hdr.ramdisk_image = 0x0;
  boot->hdr.ram_size = 0x0;
  boot->hdr.loadflags |= CAN_USE_HEAP | 0x01 | KEEP_SEGMENTS;
  boot->hdr.heap_end_ptr = 0xFE00;
  boot->hdr.ext_loader_ver = 0x0;
  boot->hdr.cmd_line_ptr = 0x20000;

  memset(cmdline, 0, boot->hdr.cmdline_size);
  memcpy(cmdline, "console=tty0", 13);
  memmove(kernel, (char *)image_data + setupsz, image_size - setupsz);
  return 0;
}

unsigned int KVM_GET_VCPU_MMAP_SIZE(int kvm_fd)
{
  __u_long req = _IO(KVM_ID, KVM_GET_VCPU_MMAP_SIZE_seq);
  return ioctl(kvm_fd, req, 0);
}

int KVM_RUN(int vcpu_fd)
{
  __u_long req = _IO(KVM_ID, KVM_RUN_seq);
  return ioctl(vcpu_fd, req, 0);
}

int run_vm(int vcpu_fd, int vcpu_map_size, void *mem)
{
  struct kvm_run *run = mmap(0, vcpu_map_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpu_fd, 0);
  run->flags = 65535;
  for (;;)
  {
    int ret = KVM_RUN(vcpu_fd);
    if (ret < 0)
    {
      return -1;
    }

    switch (run->exit_reason)
    {
    case KVM_EXIT_IO:
      if (run->io.port == 0x3f8 && run->io.direction == KVM_EXIT_IO_OUT)
      {
        unsigned int size = run->io.size;
        unsigned long offset = run->io.data_offset;
        printf("%.*s", size * run->io.count, (char *)run + offset);
      }
      else if (run->io.port == 0x3f8 + 5 &&
               run->io.direction == KVM_EXIT_IO_IN)
      {
        char *value = (char *)run + run->io.data_offset;
        *value = 0x20;
      }
      break;
    case KVM_EXIT_SHUTDOWN:
      printf("shutdown\n");
      return 0;
    default:
      printf("reason: %d\n", run->exit_reason);
      return -1;
    }
  }
}
