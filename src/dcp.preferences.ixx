module;

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>

export module dcp.preferences;

import dcp.profile;

export namespace dcp::preferences {
    enum class CloseAction {
        KeepRunningInTray,
        Quit
    };

    struct AppPreferences
    {
        bool openOnStartup = false;
        CloseAction closeAction = CloseAction::KeepRunningInTray;
        bool applyProfileOnExit = false;
        std::string exitProfileName = dcp::profile::Loader::defaultProfileName();
        std::string lastProfileName = dcp::profile::Loader::defaultProfileName();
    };

    AppPreferences load();
    bool save(const AppPreferences& preferences);
}

namespace {
    constexpr std::string_view SETTINGS_HEADER = "DCP_SETTINGS_V1";

    std::filesystem::path preferencesPath()
    {
        return std::filesystem::path(dcp::profile::Loader::storageDirectory()) /
            "settings.ini";
    }

    std::string trim(std::string_view value)
    {
        const auto first =
            std::find_if_not(
                value.begin(),
                value.end(),
                [](const unsigned char character) {
                    return std::isspace(character) != 0;
                });

        if (first == value.end())
            return {};

        const auto last =
            std::find_if_not(
                value.rbegin(),
                value.rend(),
                [](const unsigned char character) {
                    return std::isspace(character) != 0;
                }).base();

        return std::string(first, last);
    }

    std::string lowerCopy(std::string value)
    {
        for (char& character : value) {
            character = static_cast<char>(
                std::tolower(static_cast<unsigned char>(character)));
        }

        return value;
    }

    std::optional<bool> parseBool(std::string_view value)
    {
        const std::string normalized =
            lowerCopy(trim(value));

        if (normalized == "1" ||
            normalized == "true" ||
            normalized == "yes") {
            return true;
        }

        if (normalized == "0" ||
            normalized == "false" ||
            normalized == "no") {
            return false;
        }

        return std::nullopt;
    }

    std::optional<dcp::preferences::CloseAction> parseCloseAction(
        std::string_view value)
    {
        const std::string normalized =
            lowerCopy(trim(value));

        if (normalized == "quit")
            return dcp::preferences::CloseAction::Quit;

        if (normalized == "keep_running_in_tray" ||
            normalized == "minimize_to_taskbar") {
            return dcp::preferences::CloseAction::KeepRunningInTray;
        }

        return std::nullopt;
    }

    std::string boolText(bool value)
    {
        return value ? "1" : "0";
    }

    std::string closeActionText(dcp::preferences::CloseAction action)
    {
        if (action == dcp::preferences::CloseAction::Quit)
            return "quit";

        return "keep_running_in_tray";
    }
}

dcp::preferences::AppPreferences dcp::preferences::load()
{
    AppPreferences preferences;

    if (!dcp::profile::Loader::init())
        return preferences;

    std::ifstream input(preferencesPath());

    if (!input)
        return preferences;

    std::string line;

    if (!std::getline(input, line) || line != SETTINGS_HEADER)
        return preferences;

    while (std::getline(input, line)) {
        const std::size_t separator = line.find('=');

        if (separator == std::string::npos)
            continue;

        const std::string key =
            trim(std::string_view(line).substr(0, separator));
        const std::string value =
            trim(std::string_view(line).substr(separator + 1));

        if (key == "openOnStartup") {
            if (const std::optional<bool> parsed = parseBool(value))
                preferences.openOnStartup = *parsed;
            continue;
        }

        if (key == "closeAction") {
            if (const std::optional<CloseAction> parsed =
                    parseCloseAction(value)) {
                preferences.closeAction = *parsed;
            }
            continue;
        }

        if (key == "applyProfileOnExit") {
            if (const std::optional<bool> parsed = parseBool(value))
                preferences.applyProfileOnExit = *parsed;
            continue;
        }

        if (key == "exitProfileName") {
            preferences.exitProfileName = value;
            continue;
        }

        if (key == "lastProfileName")
            preferences.lastProfileName = value;
    }

    return preferences;
}

bool dcp::preferences::save(const AppPreferences& preferences)
{
    if (!dcp::profile::Loader::init())
        return false;

    std::ofstream output(preferencesPath(), std::ios::trunc);

    if (!output) {
        std::cerr << "Could not write preferences file" << std::endl;
        return false;
    }

    output
        << SETTINGS_HEADER << '\n'
        << "openOnStartup=" << boolText(preferences.openOnStartup) << '\n'
        << "closeAction=" << closeActionText(preferences.closeAction) << '\n'
        << "applyProfileOnExit="
        << boolText(preferences.applyProfileOnExit) << '\n'
        << "exitProfileName=" << preferences.exitProfileName << '\n'
        << "lastProfileName=" << preferences.lastProfileName << '\n';

    return true;
}
