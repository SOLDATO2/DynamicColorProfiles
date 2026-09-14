module;

#include <cwctype>
#include <string>
#include <string_view>
#include <vector>
#include <Windows.h>

export module dcp.windows;

export namespace dcp::windows {
    bool acquireSingleInstance();
    bool isRunAtStartupEnabled();
    bool setRunAtStartup(bool enabled);
}

namespace {
    constexpr wchar_t SINGLE_INSTANCE_MUTEX[] =
        L"Local\\DynamicColorProfiles.SingleInstance";
    constexpr wchar_t RUN_KEY[] =
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
    constexpr wchar_t RUN_VALUE_NAME[] =
        L"DynamicColorProfiles";

    HANDLE& singleInstanceMutex()
    {
        static HANDLE mutex = nullptr;
        return mutex;
    }

    std::wstring executablePath()
    {
        std::wstring path(MAX_PATH, L'\0');

        for (;;) {
            const DWORD copied =
                GetModuleFileNameW(nullptr, path.data(),
                    static_cast<DWORD>(path.size()));

            if (copied == 0)
                return {};

            if (copied < path.size()) {
                path.resize(copied);
                return path;
            }

            path.resize(path.size() * 2);
        }
    }

    std::wstring startupCommand()
    {
        const std::wstring path = executablePath();

        if (path.empty())
            return {};

        return L"\"" + path + L"\"";
    }

    std::wstring lowerCopy(std::wstring value)
    {
        for (wchar_t& character : value)
            character = static_cast<wchar_t>(std::towlower(character));

        return value;
    }

    bool equalsIgnoreCase(std::wstring_view left, std::wstring_view right)
    {
        return lowerCopy(std::wstring(left)) == lowerCopy(std::wstring(right));
    }

    std::wstring readStartupCommand()
    {
        HKEY key = nullptr;

        if (RegOpenKeyExW(
                HKEY_CURRENT_USER,
                RUN_KEY,
                0,
                KEY_QUERY_VALUE,
                &key) != ERROR_SUCCESS) {
            return {};
        }

        DWORD type = 0;
        DWORD bytes = 0;

        LONG result =
            RegQueryValueExW(
                key,
                RUN_VALUE_NAME,
                nullptr,
                &type,
                nullptr,
                &bytes);

        if (result != ERROR_SUCCESS ||
            (type != REG_SZ && type != REG_EXPAND_SZ)) {
            RegCloseKey(key);
            return {};
        }

        std::vector<wchar_t> buffer(bytes / sizeof(wchar_t) + 1, L'\0');

        result =
            RegQueryValueExW(
                key,
                RUN_VALUE_NAME,
                nullptr,
                &type,
                reinterpret_cast<LPBYTE>(buffer.data()),
                &bytes);

        RegCloseKey(key);

        if (result != ERROR_SUCCESS)
            return {};

        return std::wstring(buffer.data());
    }
}

bool dcp::windows::acquireSingleInstance()
{
    if (singleInstanceMutex())
        return true;

    SetLastError(ERROR_SUCCESS);

    HANDLE mutex =
        CreateMutexW(nullptr, TRUE, SINGLE_INSTANCE_MUTEX);

    if (!mutex)
        return true;

    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        return false;
    }

    singleInstanceMutex() = mutex;
    return true;
}

bool dcp::windows::isRunAtStartupEnabled()
{
    const std::wstring configuredCommand =
        readStartupCommand();
    const std::wstring currentCommand =
        startupCommand();

    return !configuredCommand.empty() &&
        !currentCommand.empty() &&
        equalsIgnoreCase(configuredCommand, currentCommand);
}

bool dcp::windows::setRunAtStartup(bool enabled)
{
    if (!enabled) {
        HKEY key = nullptr;

        if (RegOpenKeyExW(
                HKEY_CURRENT_USER,
                RUN_KEY,
                0,
                KEY_SET_VALUE,
                &key) != ERROR_SUCCESS) {
            return true;
        }

        const LONG result =
            RegDeleteValueW(key, RUN_VALUE_NAME);

        RegCloseKey(key);
        return result == ERROR_SUCCESS || result == ERROR_FILE_NOT_FOUND;
    }

    const std::wstring command =
        startupCommand();

    if (command.empty())
        return false;

    HKEY key = nullptr;

    if (RegCreateKeyExW(
            HKEY_CURRENT_USER,
            RUN_KEY,
            0,
            nullptr,
            REG_OPTION_NON_VOLATILE,
            KEY_SET_VALUE,
            nullptr,
            &key,
            nullptr) != ERROR_SUCCESS) {
        return false;
    }

    const DWORD byteCount = static_cast<DWORD>((command.size() + 1) * sizeof(wchar_t));

    const LONG result =
        RegSetValueExW(
            key,
            RUN_VALUE_NAME,
            0,
            REG_SZ,
            reinterpret_cast<const BYTE*>(command.c_str()),
            byteCount);

    RegCloseKey(key);
    return result == ERROR_SUCCESS;
}
