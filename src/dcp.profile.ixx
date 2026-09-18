//
// Created by sQuAde on 11/09/2026.
//
module;

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

export module dcp.profile;

export namespace dcp::profile {
    struct NvidiaColorSettings
    {
        int brightness = 50;
        int contrast = 50;
        double gamma = 1.0;
        int vibrance = 50;
        int hue = 0;
    };

    struct NvidiaProfile
    {
        std::string name;
        std::vector<NvidiaColorSettings> settings;
    };

    struct AmdColorSettings
    {
        int brightness = 0;
        int hue = 0;
        int contrast = 100;
        int saturation = 100;
    };

    struct AmdProfile
    {
        std::string name;
        std::vector<AmdColorSettings> settings;
    };

    class Loader {
    public:
        static std::string defaultProfileName() {
            return "Default";
        }

        static NvidiaColorSettings defaultNvidiaSettings() {
            return {};
        }

        static AmdColorSettings defaultAmdSettings() {
            return {};
        }

        static bool isDefaultProfileName(const std::string& name) {
            return equalsIgnoreCase( normalizeName(name), defaultProfileName());
        }

        static bool init() {
            return ensureAppDirectory();
        }

        static bool createNvidiaProfile(const std::string& name) {
            if (!init())
                return false;

            const std::string profileName = normalizeName(name);

            if (profileName.empty() || isDefaultProfileName(profileName))
                return false;

            const std::filesystem::path path = getNvidiaProfilePath(profileName);

            if (std::filesystem::exists(path)) {
                return false;
            }

            return saveNvidiaProfile({profileName, {}});
        }

        static bool saveNvidiaSettings(
            const std::string& name,
            const NvidiaColorSettings& settings)
        {
            if (!init())
                return false;

            const std::string profileName = normalizeName(name);

            if (profileName.empty() || isDefaultProfileName(profileName))
                return false;

            NvidiaProfile profile;

            if (const std::optional<NvidiaProfile> existing =
                    loadNvidiaProfile(profileName)) {
                profile = *existing;
            } else {
                profile.name = profileName;
            }

            profile.settings.push_back(clampNvidiaSettings(settings));

            return saveNvidiaProfile(profile);
        }

        static bool saveAmdSettings(
            const std::string& name,
            const AmdColorSettings& settings)
        {
            if (!init())
                return false;

            const std::string profileName = normalizeName(name);

            if (profileName.empty() || isDefaultProfileName(profileName))
                return false;

            AmdProfile profile;

            if (const std::optional<AmdProfile> existing =
                    loadAmdProfile(profileName)) {
                profile = *existing;
            } else {
                profile.name = profileName;
            }

            profile.settings.push_back(clampAmdSettings(settings));
            return saveAmdProfile(profile);
        }

        static bool saveNvidiaProfile(NvidiaProfile profile) {
            if (!init())
                return false;

            profile.name = normalizeName(profile.name);

            if (profile.name.empty() || isDefaultProfileName(profile.name))
                return false;

            for (NvidiaColorSettings& settings : profile.settings) {
                settings = clampNvidiaSettings(settings);
            }

            std::ofstream output(
                getNvidiaProfilePath(profile.name),
                std::ios::trunc);

            if (!output)
                return false;

            output << "DCP_PROFILE_V1\n" << "name=" << profile.name << '\n';

            for (const NvidiaColorSettings& settings : profile.settings) {
                output
                    << "settings "
                    << settings.brightness << ' '
                    << settings.contrast << ' '
                    << settings.gamma << ' '
                    << settings.vibrance << ' '
                    << settings.hue << '\n';
            }

            return true;
        }

        static std::optional<NvidiaProfile> loadNvidiaProfile(
            const std::string& name)
        {
            const std::string profileName = normalizeName(name);

            if (profileName.empty())
                return std::nullopt;

            if (isDefaultProfileName(profileName)) {
                return NvidiaProfile{
                    defaultProfileName(),
                    {defaultNvidiaSettings()}
                };
            }

            if (!init())
                return std::nullopt;

            return readNvidiaProfile(getNvidiaProfilePath(profileName));
        }

        static std::optional<AmdProfile> loadAmdProfile(
            const std::string& name)
        {
            const std::string profileName = normalizeName(name);

            if (profileName.empty())
                return std::nullopt;

            if (isDefaultProfileName(profileName)) {
                return AmdProfile{
                    defaultProfileName(),
                    {defaultAmdSettings()}
                };
            }

            if (!init())
                return std::nullopt;

            return readAmdProfile(getAmdProfilePath(profileName));
        }

        static std::vector<std::string> listNvidiaProfiles() {
            std::vector<std::string> userProfiles;

            if (init()) {
                try {
                    for (const std::filesystem::directory_entry& entry :
                         std::filesystem::directory_iterator(getAppDirectory())) {
                        if (!entry.is_regular_file() ||
                            entry.path().extension() != ".dcp") {
                            continue;
                        }

                        if (const std::optional<NvidiaProfile> profile =
                                readNvidiaProfile(entry.path());
                            profile &&
                            !isDefaultProfileName(profile->name)) {
                            userProfiles.push_back(profile->name);
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Filesystem error: " << e.what() << std::endl;
                }
            }

            std::ranges::sort(userProfiles);

            std::vector<std::string> profiles;
            profiles.reserve(userProfiles.size() + 1);
            profiles.push_back(defaultProfileName());
            profiles.insert(profiles.end(), userProfiles.begin(), userProfiles.end());
            return profiles;
        }

        static std::vector<std::string> listAmdProfiles() {
            std::vector<std::string> userProfiles;

            if (init()) {
                try {
                    for (const std::filesystem::directory_entry& entry :
                         std::filesystem::directory_iterator(getAppDirectory())) {
                        if (!entry.is_regular_file() ||
                            entry.path().extension() != ".dcp") {
                            continue;
                        }

                        if (const std::optional<AmdProfile> profile =
                                readAmdProfile(entry.path());
                            profile &&
                            !isDefaultProfileName(profile->name)) {
                            userProfiles.push_back(profile->name);
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Filesystem error: " << e.what() << std::endl;
                }
            }

            std::ranges::sort(userProfiles);
            userProfiles.erase(
                std::ranges::unique(userProfiles).begin(),
                userProfiles.end());
            userProfiles.erase(
                std::ranges::unique(userProfiles).begin(),
                userProfiles.end());

            std::vector<std::string> profiles;
            profiles.reserve(userProfiles.size() + 1);
            profiles.push_back(defaultProfileName());
            profiles.insert(profiles.end(), userProfiles.begin(), userProfiles.end());
            return profiles;
        }

        static std::optional<NvidiaColorSettings> loadLatestNvidiaSettings(
            const std::string& name) {
            if (isDefaultProfileName(name))
                return defaultNvidiaSettings();

            const std::optional<NvidiaProfile> profile = loadNvidiaProfile(name);

            if (!profile || profile->settings.empty())
                return std::nullopt;

            return profile->settings.back();
        }

        static std::optional<AmdColorSettings> loadLatestAmdSettings(
            const std::string& name)
        {
            if (isDefaultProfileName(name))
                return defaultAmdSettings();

            const std::optional<AmdProfile> profile =
                loadAmdProfile(name);

            if (!profile || profile->settings.empty())
                return std::nullopt;

            return profile->settings.back();
        }

        static std::string storageDirectory() {
            return getAppDirectory().string();
        }

    private:
        static bool equalsIgnoreCase( std::string_view left, std::string_view right) {
            if (left.size() != right.size())
                return false;

            for (std::size_t i = 0; i < left.size(); i++) {
                const auto leftChar =
                    static_cast<unsigned char>(left[i]);
                const auto rightChar =
                    static_cast<unsigned char>(right[i]);

                if (std::tolower(leftChar) !=
                    std::tolower(rightChar)) {
                    return false;
                }
            }

            return true;
        }

        static std::filesystem::path getAppDirectory() {
            const char* localAppData = std::getenv("LOCALAPPDATA");

            if (localAppData && localAppData[0] != '\0') {
                return std::filesystem::path(localAppData) /
                    "DynamicColorProfiles";
            }

            const char* appData = std::getenv("APPDATA");

            if (appData && appData[0] != '\0') {
                return std::filesystem::path(appData) /
                    "DynamicColorProfiles";
            }

            return std::filesystem::current_path() /
                "DynamicColorProfiles";
        }

        static bool ensureAppDirectory() {
            try {
                std::filesystem::create_directories(getAppDirectory());
                return true;
            } catch (const std::exception& e) {
                std::cerr << "Filesystem error: " << e.what() << std::endl;
                return false;
            }
        }

        static std::filesystem::path getNvidiaProfilePath(
            const std::string& profileName)
        {
            return getAppDirectory() /
                (sanitizeFileName(profileName) + ".dcp");
        }

        static std::filesystem::path getAmdProfilePath(
            const std::string& profileName)
        {
            return getAppDirectory() /
                (sanitizeFileName(profileName) + ".amd.dcp");
        }

        static bool saveAmdProfile(AmdProfile profile) {
            profile.name = normalizeName(profile.name);

            if (profile.name.empty() || isDefaultProfileName(profile.name))
                return false;

            for (AmdColorSettings& settings : profile.settings)
                settings = clampAmdSettings(settings);

            std::ofstream output(getAmdProfilePath(profile.name), std::ios::trunc);

            if (!output)
                return false;

            output << "DCP_AMD_PROFILE_V1\n" << "name=" << profile.name << '\n';

            for (const AmdColorSettings& settings : profile.settings) {
                output
                    << "settings "
                    << settings.brightness << ' '
                    << settings.hue << ' '
                    << settings.contrast << ' '
                    << settings.saturation << '\n';
            }

            return true;
        }

        static std::optional<NvidiaProfile> readNvidiaProfile(
            const std::filesystem::path& path)
        {
            std::ifstream input(path);

            if (!input)
                return std::nullopt;

            std::string line;

            if (!std::getline(input, line) ||
                line != "DCP_PROFILE_V1") {
                return std::nullopt;
            }

            NvidiaProfile profile;

            while (std::getline(input, line)) {
                if (line.rfind("name=", 0) == 0) {
                    profile.name = normalizeName(line.substr(5));
                    continue;
                }

                if (line.rfind("settings ", 0) == 0) {
                    if (const std::optional<NvidiaColorSettings> settings =
                            parseNvidiaSettingsLine(line)) {
                        profile.settings.push_back(*settings);
                    }
                }
            }

            if (profile.name.empty())
                profile.name = path.stem().string();

            return profile;
        }

        static std::optional<AmdProfile> readAmdProfile(
            const std::filesystem::path& path)
        {
            std::ifstream input(path);

            if (!input)
                return std::nullopt;

            std::string line;

            if (!std::getline(input, line) ||
                line != "DCP_AMD_PROFILE_V1") {
                return std::nullopt;
            }

            AmdProfile profile;

            while (std::getline(input, line)) {
                if (line.rfind("name=", 0) == 0) {
                    profile.name = normalizeName(line.substr(5));
                    continue;
                }

                if (line.rfind("settings ", 0) == 0) {
                    if (const std::optional<AmdColorSettings> settings =
                            parseAmdSettingsLine(line)) {
                        profile.settings.push_back(*settings);
                    }
                }
            }

            if (profile.name.empty()) {
                std::string fileName = path.stem().string();
                constexpr std::string_view AMD_SUFFIX = ".amd";

                if (fileName.ends_with(AMD_SUFFIX))
                    fileName.resize(fileName.size() - AMD_SUFFIX.size());

                profile.name = fileName;
            }

            return profile;
        }

        static std::optional<NvidiaColorSettings> parseNvidiaSettingsLine(
            const std::string& line)
        {
            std::istringstream input(line);
            std::string keyword;
            NvidiaColorSettings settings;

            input
                >> keyword
                >> settings.brightness
                >> settings.contrast
                >> settings.gamma
                >> settings.vibrance
                >> settings.hue;

            if (!input || keyword != "settings")
                return std::nullopt;

            return clampNvidiaSettings(settings);
        }

        static std::optional<AmdColorSettings> parseAmdSettingsLine(
            const std::string& line)
        {
            std::istringstream input(line);
            std::string keyword;
            AmdColorSettings settings;

            input
                >> keyword
                >> settings.brightness
                >> settings.hue
                >> settings.contrast
                >> settings.saturation;

            if (!input || keyword != "settings")
                return std::nullopt;

            return clampAmdSettings(settings);
        }

        static NvidiaColorSettings clampNvidiaSettings(
            NvidiaColorSettings settings)
        {
            settings.brightness =
                std::clamp(settings.brightness, 0, 100);

            settings.contrast =
                std::clamp(settings.contrast, 0, 100);

            settings.gamma =
                std::clamp(settings.gamma, 0.4, 2.8);

            settings.vibrance =
                std::clamp(settings.vibrance, 0, 100);

            settings.hue =
                ((settings.hue % 360) + 360) % 360;

            return settings;
        }

        static AmdColorSettings clampAmdSettings(AmdColorSettings settings) {
            settings.brightness =
                std::clamp(settings.brightness, -100, 100);
            settings.hue =
                std::clamp(settings.hue, -30, 30);
            settings.contrast =
                std::clamp(settings.contrast, 0, 200);
            settings.saturation =
                std::clamp(settings.saturation, 0, 200);
            return settings;
        }

        static std::string normalizeName(std::string name) {
            const auto first =
                std::ranges::find_if_not(name,
                 [](const unsigned char character) {
                     return std::isspace(character) != 0;
                 });

            if (first == name.end())
                return {};

            //ranges
            const auto last = std::ranges::find_if_not(name.rbegin(), name.rend(),
                    [](const unsigned char character) {
                        return std::isspace(character) != 0;
                    }).base();

            return std::string(first, last);
        }

        static std::string sanitizeFileName(const std::string& profileName) {
            std::string fileName;
            fileName.reserve(profileName.size());

            for (const unsigned char character : profileName) {
                const bool invalid =
                    character < 32 ||
                    character == '<' ||
                    character == '>' ||
                    character == ':' ||
                    character == '"' ||
                    character == '/' ||
                    character == '\\' ||
                    character == '|' ||
                    character == '?' ||
                    character == '*';

                fileName.push_back(
                    invalid ? '_' : static_cast<char>(character));
            }

            while (!fileName.empty() &&
                   (fileName.back() == '.' ||
                    fileName.back() == ' ')) {
                fileName.pop_back();
            }

            if (fileName.empty())
                fileName = "profile";

            return fileName;
        }
    };
}
