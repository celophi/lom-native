#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "diagnostics_internal.h"

#include "lom_native_session.h"

#include "lom_native_fs.h"

#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <execinfo.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/utsname.h>
#include <unistd.h>
#ifdef __APPLE__
#include <sys/sysctl.h>
#include <sys/ucontext.h>
#else
#include <ucontext.h>
#endif

static char s_crash_path[PATH_CAPACITY + 32];
static uintptr_t s_image_base;
static volatile sig_atomic_t s_crashing;

static char s_alternate_stack[ALTERNATE_STACK_BYTES];

/**
 * @brief Name a crash signal.
 * @param sig The signal.
 * @return Its name, like "SIGSEGV".
 */
static const char* signal_name(int sig)
{
    switch (sig)
    {
    case SIGSEGV:
        return "SIGSEGV";
    case SIGBUS:
        return "SIGBUS";
    case SIGILL:
        return "SIGILL";
    case SIGFPE:
        return "SIGFPE";
    case SIGABRT:
        return "SIGABRT";
    default:
        return "signal";
    }
}

static void out_registers(int fd, const ucontext_t* uc)
{
    crash_out_text(fd, "\nregisters\n");
#if defined(__linux__) && defined(__x86_64__)
    static const struct
    {
        const char* name;
        int index;
    } registers[] = {{"  rip", REG_RIP}, {"  rsp", REG_RSP}, {"  rbp", REG_RBP}, {"  rax", REG_RAX}, {"  rbx", REG_RBX}, {"  rcx", REG_RCX},
                     {"  rdx", REG_RDX}, {"  rsi", REG_RSI}, {"  rdi", REG_RDI}, {"  r8", REG_R8},   {"  r9", REG_R9},   {"  r10", REG_R10},
                     {"  r11", REG_R11}, {"  r12", REG_R12}, {"  r13", REG_R13}, {"  r14", REG_R14}, {"  r15", REG_R15}};

    for (size_t i = 0; i < sizeof(registers) / sizeof(*registers); i++)
    {
        crash_out_field_hex(fd, registers[i].name, (uintptr_t)uc->uc_mcontext.gregs[registers[i].index]);
    }
#elif defined(__linux__) && defined(__i386__)
    crash_out_field_hex(fd, "  eip", (uintptr_t)uc->uc_mcontext.gregs[REG_EIP]);
    crash_out_field_hex(fd, "  esp", (uintptr_t)uc->uc_mcontext.gregs[REG_ESP]);
    crash_out_field_hex(fd, "  ebp", (uintptr_t)uc->uc_mcontext.gregs[REG_EBP]);
    crash_out_field_hex(fd, "  eax", (uintptr_t)uc->uc_mcontext.gregs[REG_EAX]);
#elif defined(__linux__) && defined(__aarch64__)
    crash_out_field_hex(fd, "  pc", (uintptr_t)uc->uc_mcontext.pc);
    crash_out_field_hex(fd, "  sp", (uintptr_t)uc->uc_mcontext.sp);
    crash_out_field_hex(fd, "  fp", (uintptr_t)uc->uc_mcontext.regs[29]);
    crash_out_field_hex(fd, "  lr", (uintptr_t)uc->uc_mcontext.regs[30]);
#elif defined(__APPLE__) && defined(__aarch64__)
    crash_out_field_hex(fd, "  pc", (uintptr_t)__darwin_arm_thread_state64_get_pc(uc->uc_mcontext->__ss));
    crash_out_field_hex(fd, "  sp", (uintptr_t)__darwin_arm_thread_state64_get_sp(uc->uc_mcontext->__ss));
    crash_out_field_hex(fd, "  fp", (uintptr_t)__darwin_arm_thread_state64_get_fp(uc->uc_mcontext->__ss));
    crash_out_field_hex(fd, "  lr", (uintptr_t)__darwin_arm_thread_state64_get_lr(uc->uc_mcontext->__ss));
#else
    (void)uc;
    crash_out_text(fd, "  not recorded on this platform\n");
#endif
}

static void write_crash_report(int sig, const siginfo_t* info, const ucontext_t* uc)
{
    int fd = crash_open_file(s_crash_path);

    if (fd < 0)
    {
        return;
    }

    crash_out_header(fd, signal_name(sig));
    crash_out_text(fd, "signal: ");
    crash_out_decimal(fd, sig);
    crash_out_text(fd, "\ncode: ");
    crash_out_decimal(fd, info ? info->si_code : 0);
    crash_out_text(fd, "\n");
    crash_out_field_hex(fd, "fault address", info ? (uintptr_t)info->si_addr : 0);
    out_registers(fd, uc);

    void* frames[MAX_FRAMES];
    int count = backtrace(frames, MAX_FRAMES);
    crash_out_text(fd, "\nbacktrace (address, then offset into the game's image)\n");

    for (int i = 0; i < count; i++)
    {
        crash_out_text(fd, "  #");
        crash_out_decimal(fd, i);
        crash_out_text(fd, " ");
        crash_out_hex(fd, (uintptr_t)frames[i]);
        crash_out_text(fd, " +");
        crash_out_hex(fd, (uintptr_t)frames[i] - s_image_base);
        crash_out_text(fd, "\n");
    }

    crash_out_text(fd, "\nsymbols\n");
    backtrace_symbols_fd(frames, count, fd);
    crash_out_breadcrumbs(fd);
    crash_close_file(fd);
}

/**
 * @brief Handle a crash signal once: write the report, end the session
 *        recording with the signal, then let the signal kill the program as it
 *        would have.
 * @param sig The signal.
 * @param info What it says about the fault.
 * @param context The context at the crash.
 */
static void crash_signal(int sig, siginfo_t* info, void* context)
{
    if (!s_crashing)
    {
        s_crashing = 1;
        write_crash_report(sig, info, (const ucontext_t*)context);

        static char reason[] = "signal 00";
        reason[7] = (char)('0' + sig / 10 % 10);
        reason[8] = (char)('0' + sig % 10);
        lom_native_session_crashed(reason);
    }

    signal(sig, SIG_DFL);
    raise(sig);
}

/**
 * @brief Catch crash signals on their own stack, so even a stack overflow
 *        gets a report, after warming up backtrace so it doesn't allocate in a
 *        crash.
 * @param prefix Where the crash files go, without their suffix.
 * @param crash_path The crash report.
 * @return Where the program was loaded, for the report's offsets.
 */
uintptr_t crash_install_handlers(const char* prefix, const char* crash_path)
{
    (void)prefix;
    snprintf(s_crash_path, sizeof(s_crash_path), "%s", crash_path);
    void* warm_up[1];
    backtrace(warm_up, 1);

    Dl_info self;

    if (dladdr((void*)crash_install_handlers, &self))
    {
        s_image_base = (uintptr_t)self.dli_fbase;
    }

    stack_t stack = {0};
    stack.ss_sp = s_alternate_stack;
    stack.ss_size = sizeof(s_alternate_stack);
    sigaltstack(&stack, NULL);

    static const int fatal[] = {SIGSEGV, SIGBUS, SIGILL, SIGFPE, SIGABRT};
    struct sigaction action = {0};
    action.sa_sigaction = crash_signal;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK;
    sigemptyset(&action.sa_mask);

    for (size_t i = 0; i < sizeof(fatal) / sizeof(*fatal); i++)
    {
        sigaction(fatal[i], &action, NULL);
    }

    return s_image_base;
}

void crash_out_bytes(int fd, const char* data, size_t size)
{
    while (size > 0)
    {
        ssize_t n = write(fd, data, size);

        if (n < 0 && errno == EINTR)
        {
            continue;
        }

        if (n <= 0)
        {
            return;
        }

        data += n;
        size -= (size_t)n;
    }
}

int crash_open_file(const char* path)
{
    return open(path, O_WRONLY | O_CREAT | O_TRUNC | LOM_NATIVE_O_PRIVATE, 0600);
}

void crash_close_file(int fd)
{
    close(fd);
}

#ifndef __APPLE__
/**
 * @brief Read the rest of the first line of a system file that starts with a
 *        key, like PRETTY_NAME= in /etc/os-release, without its quotes.
 * @param path The file.
 * @param key What the line starts with, its separator included.
 * @param value Where the rest of the line goes.
 * @param size Bytes at @p value.
 * @return 1 when it found the line, otherwise 0.
 */
static int read_keyed_line(const char* path, const char* key, char* value, size_t size)
{
    FILE* file = fopen(path, "r");
    char line[TEXT_LINE_BYTES];
    size_t key_length = strlen(key);
    int found = 0;

    if (!file)
    {
        return 0;
    }

    while (!found && fgets(line, sizeof(line), file))
    {
        if (strncmp(line, key, key_length) == 0)
        {
            const char* start = line + key_length;
            size_t length = strcspn(start, "\r\n");

            if (*start == '"' && length >= 2)
            {
                start++;
                length -= 2;
            }

            snprintf(value, size, "%.*s", (int)length, start);
            found = 1;
        }
    }

    fclose(file);
    return found;
}
#endif

void diagnostics_describe_os(FILE* report)
{
    char text[TEXT_LINE_BYTES] = "";
#ifdef __APPLE__
    size_t size = sizeof(text);

    if (sysctlbyname("kern.osproductversion", text, &size, NULL, 0) == 0)
    {
        fprintf(report, "os: macOS %s\n", text);
    }

    size = sizeof(text);

    if (sysctlbyname("hw.model", text, &size, NULL, 0) == 0)
    {
        fprintf(report, "model: %s\n", text);
    }

    size = sizeof(text);

    if (sysctlbyname("machdep.cpu.brand_string", text, &size, NULL, 0) == 0)
    {
        fprintf(report, "cpu: %s\n", text);
    }
#else
    if (read_keyed_line("/run/host/os-release", "PRETTY_NAME=", text, sizeof(text)) || read_keyed_line("/etc/os-release", "PRETTY_NAME=", text, sizeof(text)))
    {
        fprintf(report, "os: %s\n", text);
    }

    if (read_keyed_line("/proc/cpuinfo", "model name\t: ", text, sizeof(text)) || read_keyed_line("/proc/cpuinfo", "Model\t\t: ", text, sizeof(text)))
    {
        fprintf(report, "cpu: %s\n", text);
    }

    const char* desktop = getenv("XDG_CURRENT_DESKTOP");
    const char* session = getenv("XDG_SESSION_TYPE");
    fprintf(report, "desktop: %s (%s)\n", desktop ? desktop : "unknown", session ? session : "unknown session type");
#endif
    struct utsname name;

    if (uname(&name) == 0)
    {
        fprintf(report, "kernel: %s %s %s\n", name.sysname, name.release, name.machine);
    }
}
