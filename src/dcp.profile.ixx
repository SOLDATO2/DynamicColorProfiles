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
    struct ColorSettings
    {
        int brightness = 50;
        int contrast = 50;
        double gamma = 1.0;
        int vibrance = 50;
        int hue = 0;
    };

    struct Profile
    {
        std::string name;
        std::vector<ColorSettings> settings;
    };

    class Loader {
    public:
        static std::string defaultProfileName() {
            return "Default";
        }

        static ColorSettings defaultSettings() {
            return {};
        }

        static bool isDefaultProfileName(const std::string& name) {
            return equalsIgnoreCase( normalizeName(name), defaultProfileName());
        }

        static bool init() {
            return ensureAppDirectory();
        }

        static bool createProfile(const std::string& name) {
            if (!init())
                return false;

            const std::string profileName = normalizeName(name);

            if (profileName.empty() || isDefaultProfileName(profileName))
                return false;

            const std::filesystem::path path = getProfilePath(profileName);

            if (std::filesystem::exists(path))
                return false;

            return saveProfile({profileName, {}});
        }

        static bool saveSettings( const std::string& name, const ColorSettings& settings) {
            if (!init())
                return false;

            const std::string profileName = normalizeName(name);

            if (profileName.empty() || isDefaultProfileName(profileName))
                return false;

            Profile profile;

            if (const std::optional<Profile> existing = loadProfile(profileName)) {
                profile = *existing;
            } else {
                profile.name = profileName;
            }

            profile.settings.push_back(clampSettings(settings));

            return saveProfile(profile);
        }

        static bool saveProfile(Profile profile) {
            if (!init())
                return false;

            profile.name = normalizeName(profile.name);

            if (profile.name.empty() || isDefaultProfileName(profile.name))
                return false;

            for (ColorSettings& settings : profile.settings) {
                settings = clampSettings(settings);
            }

            std::ofstream output(getProfilePath(profile.name), std::ios::trunc);

            if (!output)
                return false;

            output << "DCP_PROFILE_V1\n" << "name=" << profile.name << '\n';

            for (const ColorSettings& settings : profile.settings) {
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

        static std::optional<Profile> loadProfile(const std::string& name) {
            const std::string profileName = normalizeName(name);

            if (profileName.empty())
                return std::nullopt;

            if (isDefaultProfileName(profileName)) {
                return Profile{
                    defaultProfileName(),
                    {defaultSettings()}
                };
            }

            if (!init())
                return std::nullopt;

            return readProfile(getProfilePath(profileName));
        }

        static std::vector<std::string> listProfiles() {
            std::vector<std::string> userProfiles;

            if (init()) {
                try {
                    for (const std::filesystem::directory_entry& entry :
                         std::filesystem::directory_iterator(getAppDirectory())) {
                        if (!entry.is_regular_file() ||
                            entry.path().extension() != ".dcp") {
                            continue;
                        }

                        if (const std::optional<Profile> profile = readProfile(entry.path());
                            profile &&
                            !isDefaultProfileName(profile->name)) {
                            userProfiles.push_back(profile->name);
                        }
                    }
                } catch (const std::exception& e) {
                    std::cerr << "Filesystem error: " << e.what() << std::endl;
                }
            }

            std::sort(userProfiles.begin(), userProfiles.end());

            std::vector<std::string> profiles;
            profiles.reserve(userProfiles.size() + 1);
            profiles.push_back(defaultProfileName());
            profiles.insert(profiles.end(), userProfiles.begin(), userProfiles.end());
            return profiles;
        }

        static std::optional<ColorSettings> loadLatestSettings(
            const std::string& name) {
            if (isDefaultProfileName(name))
                return defaultSettings();

            const std::optional<Profile> profile =
                loadProfile(name);

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

        static std::filesystem::path getProfilePath( const std::string& profileName) {
            return getAppDirectory() /
                (sanitizeFileName(profileName) + ".dcp");
        }

        static std::optional<Profile> readProfile(const std::filesystem::path& path) {
            std::ifstream input(path);

            if (!input)
                return std::nullopt;

            std::string line;

            if (!std::getline(input, line) ||
                line != "DCP_PROFILE_V1") {
                return std::nullopt;
            }

            Profile profile;

            while (std::getline(input, line)) {
                if (line.rfind("name=", 0) == 0) {
                    profile.name = normalizeName(line.substr(5));
                    continue;
                }

                if (line.rfind("settings ", 0) == 0) {
                    if (const std::optional<ColorSettings> settings =
                            parseSettingsLine(line)) {
                        profile.settings.push_back(*settings);
                    }
                }
            }

            if (profile.name.empty())
                profile.name = path.stem().string();

            return profile;
        }

        static std::optional<ColorSettings> parseSettingsLine(const std::string& line) {
            std::istringstream input(line);
            std::string keyword;
            ColorSettings settings;

            input
                >> keyword
                >> settings.brightness
                >> settings.contrast
                >> settings.gamma
                >> settings.vibrance
                >> settings.hue;

            if (!input || keyword != "settings")
                return std::nullopt;

            return clampSettings(settings);
        }

        static ColorSettings clampSettings(ColorSettings settings) {
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

        static std::string normalizeName(std::string name) {
            const auto first =
                std::find_if_not(
                    name.begin(),
                    name.end(),
                    [](const unsigned char character) {
                        return std::isspace(character) != 0;
                    });

            if (first == name.end())
                return {};

            const auto last = std::find_if_not( name.rbegin(), name.rend(),
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
