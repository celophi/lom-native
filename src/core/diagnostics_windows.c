#include "diagnostics_internal.h"

#include "lom_native_fs.h"
#include "lom_native_session.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <errno.h>
#include <fcntl.h>
#include <io.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define DUMP_SUFFIX ".dmp"

static char s_crash_path[PATH_CAPACITY + 32];
static char s_dump_path[PATH_CAPACITY + 32];
static uintptr_t s_image_base;
static volatile sig_atomic_t s_crashing;

/**
 * @brief Write where an address is, as a module's file name and an offset into
 *        it, without allocating, for a crash report.
 * @param fd Where to write.
 * @param address The address.
 */
static void out_module_offset(int fd, uintptr_t address)
{
    HMODULE module = NULL;
    char path[MAX_PATH];

    if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)address, &module) ||
        !GetModuleFileNameA(module, path, sizeof(path)))
    {
        crash_out_text(fd, "?");
        return;
    }

    const char* name = path;

    for (const char* p = path; *p; p++)
    {
        if (*p == '\\' || *p == '/')
        {
            name = p + 1;
        }
    }

    crash_out_text(fd, name);
    crash_out_text(fd, "+");
    crash_out_hex(fd, address - (uintptr_t)module);
}

/**
 * @brief Walk the stack from a context and write each frame's address, module
 *        and offset.
 * @param fd The crash report.
 * @param context The context to start from.
 */
static void out_backtrace(int fd, CONTEXT context)
{
    crash_out_text(fd, "\nbacktrace (address, then module and offset)\n");
#if defined(_M_X64) || defined(__x86_64__)
    for (int i = 0; i < MAX_FRAMES && context.Rip; i++)
    {
        crash_out_text(fd, "  #");
        crash_out_decimal(fd, i);
        crash_out_text(fd, " ");
        crash_out_hex(fd, (uintptr_t)context.Rip);
        crash_out_text(fd, " ");
        out_module_offset(fd, (uintptr_t)context.Rip);
        crash_out_text(fd, "\n");

        DWORD64 image_base = 0;
        PRUNTIME_FUNCTION function = RtlLookupFunctionEntry(context.Rip, &image_base, NULL);

        if (function)
        {
            void* handler_data = NULL;
            DWORD64 establisher = 0;
            RtlVirtualUnwind(UNW_FLAG_NHANDLER, image_base, context.Rip, function, &context, &handler_data, &establisher, NULL);
        }
        else
        {
            context.Rip = *(DWORD64*)context.Rsp;
            context.Rsp += sizeof(DWORD64);
        }
    }
#else
    (void)context;
    crash_out_text(fd, "  not recorded on this platform\n");
#endif
}

/**
 * @brief Write the registers at the crash, where this platform's context has
 *        them.
 * @param fd The crash report.
 * @param uc The signal's context.
 */
static void out_registers(int fd, const CONTEXT* context)
{
    crash_out_text(fd, "\nregisters\n");
#if defined(_M_X64) || defined(__x86_64__)
    crash_out_field_hex(fd, "  rip", (uintptr_t)context->Rip);
    crash_out_field_hex(fd, "  rsp", (uintptr_t)context->Rsp);
    crash_out_field_hex(fd, "  rbp", (uintptr_t)context->Rbp);
    crash_out_field_hex(fd, "  rax", (uintptr_t)context->Rax);
    crash_out_field_hex(fd, "  rbx", (uintptr_t)context->Rbx);
    crash_out_field_hex(fd, "  rcx", (uintptr_t)context->Rcx);
    crash_out_field_hex(fd, "  rdx", (uintptr_t)context->Rdx);
    crash_out_field_hex(fd, "  rsi", (uintptr_t)context->Rsi);
    crash_out_field_hex(fd, "  rdi", (uintptr_t)context->Rdi);
    crash_out_field_hex(fd, "  r8", (uintptr_t)context->R8);
    crash_out_field_hex(fd, "  r9", (uintptr_t)context->R9);
    crash_out_field_hex(fd, "  r10", (uintptr_t)context->R10);
    crash_out_field_hex(fd, "  r11", (uintptr_t)context->R11);
    crash_out_field_hex(fd, "  r12", (uintptr_t)context->R12);
    crash_out_field_hex(fd, "  r13", (uintptr_t)context->R13);
    crash_out_field_hex(fd, "  r14", (uintptr_t)context->R14);
    crash_out_field_hex(fd, "  r15", (uintptr_t)context->R15);
#else
    (void)context;
    crash_out_text(fd, "  not recorded on this platform\n");
#endif
}

/**
 * @brief Write the whole crash report: the header, the registers, a backtrace
 *        and where the game was.
 * @param sig The signal.
 * @param info What the signal says about the fault.
 * @param uc The context at the crash.
 */
static void write_crash_report(const char* what, DWORD code, uintptr_t address, const CONTEXT* context)
{
    int fd = crash_open_file(s_crash_path);

    if (fd < 0)
    {
        return;
    }

    crash_out_header(fd, what);
    crash_out_field_hex(fd, "exception code", (uintptr_t)code);
    crash_out_field_hex(fd, "exception address", address);
    out_registers(fd, context);
    out_backtrace(fd, *context);
    crash_out_breadcrumbs(fd);
    crash_close_file(fd);
}

/**
 * @brief Write a Windows minidump next to the crash report.
 * @param info The exception.
 */
static void write_minidump(EXCEPTION_POINTERS* info)
{
    HANDLE file = CreateFileA(s_dump_path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);

    if (file == INVALID_HANDLE_VALUE)
    {
        return;
    }

    MINIDUMP_EXCEPTION_INFORMATION exception = {GetCurrentThreadId(), info, FALSE};
    MINIDUMP_TYPE type = (MINIDUMP_TYPE)(MiniDumpWithIndirectlyReferencedMemory | MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
    MiniDumpWriteDump(GetCurrentProcess(), GetCurrentProcessId(), file, type, info ? &exception : NULL, NULL, NULL);
    CloseHandle(file);
}

/**
 * @brief Handle an unhandled Windows exception once: write the crash report and
 *        a minidump, and end the session recording.
 * @param info The exception.
 * @return EXCEPTION_CONTINUE_SEARCH, so Windows ends the program.
 */
static LONG WINAPI crash_exception(EXCEPTION_POINTERS* info)
{
    if (!s_crashing)
    {
        s_crashing = 1;
        const EXCEPTION_RECORD* record = info->ExceptionRecord;
        write_crash_report("exception", record->ExceptionCode, (uintptr_t)record->ExceptionAddress, info->ContextRecord);
        write_minidump(info);

        static char reason[] = "exception 00000000";
        static const char digits[] = "0123456789abcdef";

        for (int i = 0; i < 8; i++)
        {
            reason[10 + i] = digits[(record->ExceptionCode >> ((7 - i) * 4)) & 0xf];
        }

        lom_native_session_crashed(reason);
    }

    return EXCEPTION_CONTINUE_SEARCH;
}

/**
 * @brief Handle abort() on Windows: write the crash report from here and end
 *        the session recording.
 * @param sig SIGABRT.
 */
static void crash_abort(int sig)
{
    (void)sig;

    if (!s_crashing)
    {
        s_crashing = 1;
        CONTEXT context;
        RtlCaptureContext(&context);
        write_crash_report("abort", 0, 0, &context);
        write_minidump(NULL);
        lom_native_session_crashed("abort");
    }

    signal(SIGABRT, SIG_DFL);
    raise(SIGABRT);
}

/**
 * @brief Catch crashes: an exception filter writes the report and a
 *        minidump, abort() gets a report too, and the thread keeps enough
 *        stack in reserve to write them after a stack overflow.
 * @param prefix Where the crash files go, without their suffix.
 * @param crash_path The crash report.
 * @return Where the program was loaded, for the report's offsets.
 */
uintptr_t crash_install_handlers(const char* prefix, const char* crash_path)
{
    snprintf(s_crash_path, sizeof(s_crash_path), "%s", crash_path);
    s_image_base = (uintptr_t)GetModuleHandleA(NULL);
    snprintf(s_dump_path, sizeof(s_dump_path), "%s" DUMP_SUFFIX, prefix);
    ULONG guarantee = ALTERNATE_STACK_BYTES;
    SetThreadStackGuarantee(&guarantee);
    SetUnhandledExceptionFilter(crash_exception);
    signal(SIGABRT, crash_abort);
    return s_image_base;
}

void crash_out_bytes(int fd, const char* data, size_t size)
{
    while (size > 0)
    {
        int n = _write(fd, data, (unsigned)size);

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
    return _open(path, _O_WRONLY | _O_CREAT | _O_TRUNC | LOM_NATIVE_O_PRIVATE, _S_IREAD | _S_IWRITE);
}

void crash_close_file(int fd)
{
    _close(fd);
}

void diagnostics_describe_os(FILE* report)
{
    char text[TEXT_LINE_BYTES] = "";
    typedef LONG(WINAPI * RtlGetVersionFunction)(OSVERSIONINFOW*);
    RtlGetVersionFunction get_version = (RtlGetVersionFunction)(void*)GetProcAddress(GetModuleHandleA("ntdll.dll"), "RtlGetVersion");
    OSVERSIONINFOW version = {sizeof(version)};

    if (get_version && get_version(&version) == 0)
    {
        fprintf(report, "os: Windows %lu.%lu build %lu\n", version.dwMajorVersion, version.dwMinorVersion, version.dwBuildNumber);
    }

    DWORD size = sizeof(text);

    if (RegGetValueA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", "ProcessorNameString", RRF_RT_REG_SZ, NULL, text, &size) ==
        ERROR_SUCCESS)
    {
        fprintf(report, "cpu: %s\n", text);
    }
}
