//
// Created by sQuAde on 11/09/2026.
//
module;
#include <filesystem>
#include <iostream>
#include <windows.h>
#include <string>
#include <shlobj.h>
export module dcp.profile;

#pragma comment(lib, "shell32.lib")

export namespace dcp::profile{
    struct ColorSettings
    {
        int brightness; //def 50
        int contrast; // 50
        double gamma; //1.0

        int vibrance; //50
        int hue; //0
    };

    class Loader {

        public:
        static void init() {
            CreateAppSpecificFolder(L"DynamicColorProfiles");
        }

    private:
        static bool CreateAppSpecificFolder(const std::wstring& appName) {
            PWSTR path = nullptr;

            HRESULT hr = SHGetKnownFolderPath(FOLDERID_LocalAppData, 0, nullptr, &path);
            if (FAILED(hr)) {
                return false;
            }

            std::wstring fullPath = std::wstring(path) + L"\\" + appName;
            CoTaskMemFree(path);

            try {
                std::filesystem::create_directories(fullPath);
                std::wcout << L"Created path: " << fullPath << std::endl;
                return true;
            } catch (const std::exception& e) {
                std::cerr << "Filesystem error: " << e.what() << std::endl;
                return false;
            }
        }

    };



}
