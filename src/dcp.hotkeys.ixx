module;

#include <Windows.h>

#include <atomic>
#include <charconv>
#include <filesystem>
#include <fstream>
#include <functional>
#include <future>
#include <iomanip>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>

export module dcp.hotkeys;

import dcp.profile;

export namespace dcp::hotkeys {
    struct Binding
    {
        std::string profileName;
        unsigned int virtualKey = 0;
    };

    class Manager {
        std::thread m_thread;
        std::atomic<DWORD> m_threadId = 0;

    public:
        using Handler = std::function<void(const std::string& profileName)>;

        Manager() = default;
        Manager(const Manager&) = delete;
        Manager& operator=(const Manager&) = delete;

        ~Manager()
        {
            stop();
        }

        static unsigned int keyCode(std::string_view name)
        {
            if (name.size() > 1 && name.front() == 'F') {
                unsigned int number = 0;
                std::from_chars(
                    name.data() + 1,
                    name.data() + name.size(),
                    number);
                return VK_F1 + number - 1;
            }

            if (name.size() == 1) {
                char key = name.front();

                if (key >= 'a' && key <= 'z')
                    key -= 'a' - 'A';

                return static_cast<unsigned int>(key);
            }

            return 0;
        }

        static std::string keyName(unsigned int virtualKey)
        {
            if (virtualKey >= VK_F1 && virtualKey <= VK_F24)
                return "F" + std::to_string(virtualKey - VK_F1 + 1);

            if ((virtualKey >= '0' && virtualKey <= '9') ||
                (virtualKey >= 'A' && virtualKey <= 'Z')) {
                return std::string(1, static_cast<char>(virtualKey));
            }

            return {};
        }

        static std::vector<Binding> load()
        {
            std::vector<Binding> bindings;
            std::ifstream input(path());
            Binding binding;

            while (input >> std::quoted(binding.profileName) >> binding.virtualKey) {
                bindings.push_back(binding);
            }

            return bindings;
        }

        static void save(const std::vector<Binding>& bindings)
        {
            profile::Loader::init();
            std::ofstream output(path(), std::ios::trunc);

            for (const Binding& binding : bindings) {
                output
                    << std::quoted(binding.profileName) << ' '
                    << binding.virtualKey << '\n';
            }
        }

        void setBindings(std::vector<Binding> bindings, Handler handler)
        {
            stop();

            std::promise<void> started;
            std::future<void> ready = started.get_future();

            m_thread = std::thread(
                [this,
                 bindings = std::move(bindings),
                 handler = std::move(handler),
                 started = std::move(started)]() mutable {
                    run(
                        std::move(bindings),
                        std::move(handler),
                        std::move(started));
                });

            ready.get();
        }

        void stop()
        {
            if (!m_thread.joinable())
                return;

            PostThreadMessageW(m_threadId.load(), WM_QUIT, 0, 0);
            m_thread.join();
        }

    private:
        static std::filesystem::path path()
        {
            return std::filesystem::path(profile::Loader::storageDirectory()) / "hotkeys.ini";
        }

        void run(const std::vector<Binding> &bindings, const Handler &handler, std::promise<void> started)
        {
            MSG message{};
            PeekMessageW(&message, nullptr, 0, 0, PM_NOREMOVE);
            m_threadId.store(GetCurrentThreadId());

            for (std::size_t index = 0; index < bindings.size(); ++index) {
                RegisterHotKey(
                    nullptr,
                    static_cast<int>(index + 1),
                    MOD_NOREPEAT,
                    bindings[index].virtualKey);
            }

            started.set_value();

            while (GetMessageW(&message, nullptr, 0, 0) > 0) {
                if (message.message == WM_HOTKEY) {
                    const std::size_t index = message.wParam - 1;
                    handler(bindings[index].profileName);
                }
            }

            for (std::size_t index = 0; index < bindings.size(); ++index) {
                UnregisterHotKey(nullptr, static_cast<int>(index + 1));
            }
        }
    };
}
