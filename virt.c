#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

#include <assert.h>

#include <fcntl.h>
#include <unistd.h>

#include <linux/kvm.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>


#define KVM_API_VERSION 12

int main(int argc, char* argv[])
{
    int kvm_fd = 0;
    unsigned int version = 0;
    int vm_fd = 0;
    int vcpu_fd = 0;
    int run_size = 0;
    struct kvm_run *run = 0;

    const char *guest_file_path = argv[1];
    int guest_file_fd = 0;
    struct stat guest_file_info = {0};

    void *ram = NULL;
    constexpr unsigned int ram_size = 2 * 1024 * 1024;

    if (argc < 2) {
        fprintf(stderr, "Specify guest code to run\n");
        exit(1);
    }

    fprintf(stdout, "Path of guest is %s\n", guest_file_path);

    kvm_fd = open("/dev/kvm", O_RDONLY);
    version = ioctl(kvm_fd, KVM_GET_API_VERSION);
    assert(version == KVM_API_VERSION);

    vm_fd = ioctl(kvm_fd, KVM_CREATE_VM, 0);
    assert(vm_fd >= 0);

    ram = mmap(NULL, ram_size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (ram == MAP_FAILED) {
        fprintf(stderr, "Can`t mmap ram of size %u\n", ram_size);
        exit(1);
    }

    struct kvm_userspace_memory_region ram_slot = {
        .slot = 0,
        .guest_phys_addr = 0,
        .memory_size = ram_size,
        .userspace_addr = (uintptr_t) ram
    };

    if (ioctl(vm_fd, KVM_SET_USER_MEMORY_REGION, &ram_slot) == -1) {
        fprintf(stderr, "Can`t set ram for virtual machine\n");
        exit(1);
    }

    if (stat(guest_file_path, &guest_file_info)) {
        fprintf(stderr, "Can`t stat guest file %s\n", guest_file_path);
        exit(1);
    }

    guest_file_fd = open(guest_file_path, O_RDONLY);
    read(guest_file_fd, ((unsigned char*) ram) + 0x1000, guest_file_info.st_size);

    vcpu_fd = ioctl(vm_fd, KVM_CREATE_VCPU, 0);
    if (vcpu_fd == -1) {
        fprintf(stderr, "Can`t create vcpu\n");
        exit(1);
    }

    run_size = ioctl(kvm_fd, KVM_GET_VCPU_MMAP_SIZE, 0);
    if (run_size == -1) {
        fprintf(stderr, "Can`t get vcpu run size\n");
        exit(1);
    }

    run = mmap(NULL, run_size, PROT_READ | PROT_WRITE, MAP_SHARED, vcpu_fd, 0);

    if (run == MAP_FAILED) {
        fprintf(stderr, "Can`t mmap vcpu run for size %d\n", run_size);
        exit(1);
    }

    struct kvm_sregs guest_sregs;
    if (ioctl(vcpu_fd, KVM_GET_SREGS, &guest_sregs) == -1) {
        fprintf(stderr, "Can`t get sregs for cpu 0\n");
        exit(1);
    }

    guest_sregs.cs.base = 0;
    guest_sregs.cs.selector = 0;

    if (ioctl(vcpu_fd, KVM_SET_SREGS, &guest_sregs) == -1) {
        fprintf(stderr, "Can`t set sregs for cpu 0\n");
        exit(1);
    }

    struct kvm_regs vcpu_regs;
    if (ioctl(vcpu_fd, KVM_GET_REGS, &vcpu_regs) == -1) {
        fprintf(stderr, "Can`t get regs for cpu 0\n");
        exit(1);
    }

    vcpu_regs.rip = 0x1000;
    vcpu_regs.rflags = 0x2;

    if (ioctl(vcpu_fd, KVM_SET_REGS, &vcpu_regs) == -1) {
        fprintf(stderr, "Can`t set regs for cpu 0\n");
        exit(1);
    }

    bool should_exit = false;

    while(!should_exit) {
        int run_res = ioctl(vcpu_fd, KVM_RUN, 0);
        if (run_res == -1) {
            fprintf(stderr, "Can`t run vcpu 0\n");
            exit(1);
        }

        switch(run->exit_reason) {
            case KVM_EXIT_HLT:
                puts("Guest halted");
                should_exit = true;
                break;
            
            case KVM_EXIT_IO:
                if (run->io.direction != KVM_EXIT_IO_OUT || run->io.port != 0xE9 || run->io.size != 1) {
                    fprintf(stderr, "IO not from 0xE9\n");
                    break;
                }
                unsigned char *data = (unsigned char*) run + run->io.data_offset;  

                for (size_t i = 0; i < run->io.count; ++i) {
                    putchar(data[i]);
                }
                putchar('\n');
                fflush(stdout);
                break;
            default:
                fprintf(stderr, "Unexpected kvm exit\n");
                exit(1);
        }
    }

    munmap(run, run_size);
    close(vcpu_fd);
    close(vm_fd);
    munmap(ram, ram_size);
    close(kvm_fd);
}