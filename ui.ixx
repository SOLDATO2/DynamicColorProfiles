module;

#include "ui.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

export module ui;

import dcp.nvidia;
import dcp.preferences;
import dcp.profile;
import dcp.windows;

export namespace dcp::ui {
    int run();
}

namespace {
    using dcp_slint::AppTray;
    using dcp_slint::AppWindow;

    constexpr int SUCCESS = 0;
    constexpr int CLOSE_ACTION_QUIT = 1;

    std::string toStdString(const slint::SharedString& value)
    {
        return std::string(std::string_view(value));
    }

    slint::SharedString toSharedString(const std::string& value)
    {
        return {std::string_view(value)};
    }

    slint::SharedString intText(int value)
    {
        return slint::SharedString::from_number(value);
    }

    slint::SharedString gammaText(double value)
    {
        const int scaled =
            static_cast<int>(std::lround(value * 100.0));

        std::ostringstream output;
        output
            << scaled / 100
            << '.';

        const int decimals =
            std::abs(scaled % 100);

        if (decimals < 10)
            output << '0';

        output << decimals;
        return {output.str()};
    }

    int profileIndex(
        const std::vector<std::string>& profiles,
        std::string_view selectedProfile)
    {
        for (std::size_t i = 0; i < profiles.size(); i++) {
            if (profiles[i] == selectedProfile)
                return static_cast<int>(i);
        }

        return -1;
    }

    std::string availableProfileOrDefault(std::string_view profileName)
    {
        const std::vector<std::string> profiles =
            dcp::profile::Loader::listProfiles();

        const int index =
            profileIndex(profiles, profileName);

        if (index >= 0)
            return profiles[static_cast<std::size_t>(index)];

        return dcp::profile::Loader::defaultProfileName();
    }

    class App {
    public:
        App()
            : m_window(AppWindow::create()),
              m_tray(AppTray::create())
        {
            m_preferences =
                dcp::preferences::load();
            m_preferences.openOnStartup =
                dcp::windows::isRunAtStartupEnabled();
            m_preferences.lastProfileName =
                availableProfileOrDefault(m_preferences.lastProfileName);
            m_preferences.exitProfileName =
                availableProfileOrDefault(m_preferences.exitProfileName);

            setDefaultProfileName();
            connectCallbacks();

            loadProfile(m_preferences.lastProfileName, false);
            syncPreferencesToUi();
            savePreferences();
        }

        int run()
        {
            m_tray->show();
            m_window->show();

            slint::run_event_loop();
            return SUCCESS;
        }

    private:
        slint::ComponentHandle<AppWindow> m_window;
        slint::ComponentHandle<AppTray> m_tray;
        std::shared_ptr<slint::VectorModel<slint::SharedString>> m_profileModel =
            std::make_shared<slint::VectorModel<slint::SharedString>>();
        std::vector<std::string> m_profiles;
        dcp::preferences::AppPreferences m_preferences;
        dcp::nvidia::DCP m_dcp;
        dcp::profile::ColorSettings m_settings;
        bool m_quitting = false;

        void connectCallbacks()
        {
            auto windowWeak =
                slint::ComponentWeakHandle(m_window);

            m_window->window().on_close_requested([this] {
                if (m_preferences.closeAction ==
                    dcp::preferences::CloseAction::Quit) {
                    requestQuit();
                    return slint::CloseRequestResponse::HideWindow;
                }

                return slint::CloseRequestResponse::HideWindow;
            });

            m_window->on_save_profile([this](const slint::SharedString& name) {
                saveCurrentProfile(toStdString(name));
            });

            m_window->on_reset_settings([this] {
                resetToDefault();
            });

            m_window->on_profile_selected([this](const slint::SharedString& name) {
                loadProfile(toStdString(name));
            });

            m_window->on_brightness_changed([this](float value) {
                setBrightness(static_cast<int>(std::lround(value)));
            });

            m_window->on_contrast_changed([this](float value) {
                setContrast(static_cast<int>(std::lround(value)));
            });

            m_window->on_gamma_changed([this](float value) {
                setGamma(value);
            });

            m_window->on_vibrance_changed([this](float value) {
                setVibrance(static_cast<int>(std::lround(value)));
            });

            m_window->on_hue_changed([this](float value) {
                setHue(static_cast<int>(std::lround(value)));
            });

            m_window->on_open_on_startup_changed([this](bool enabled) {
                setOpenOnStartup(enabled);
            });

            m_window->on_close_action_changed([this](int action) {
                setCloseAction(action);
            });

            m_window->on_apply_exit_profile_changed([this](bool enabled) {
                setApplyProfileOnExit(enabled);
            });

            m_window->on_exit_profile_selected(
                [this](const slint::SharedString& name) {
                    setExitProfile(toStdString(name));
                });

            m_tray->on_show_window([windowWeak] {
                if (auto window = windowWeak.lock()) {
                    (*window)->show();
                    (*window)->window().set_minimized(false);
                }
            });

            m_tray->on_load_profile([this](const slint::SharedString& name) {
                loadProfile(toStdString(name));
            });

            m_tray->on_quit_requested([this] {
                requestQuit();
            });
        }

        void setDefaultProfileName()
        {
            m_window->set_default_profile_name(
                toSharedString(dcp::profile::Loader::defaultProfileName()));
        }

        void refreshProfiles(std::string_view selectedProfile)
        {
            m_profiles =
                dcp::profile::Loader::listProfiles();

            std::vector<slint::SharedString> items;
            items.reserve(m_profiles.size());

            const int selectedIndex =
                profileIndex(m_profiles, selectedProfile);

            for (const std::string& profile : m_profiles)
                items.emplace_back(profile);

            m_profileModel =
                std::make_shared<slint::VectorModel<slint::SharedString>>(
                    std::move(items));

            m_window->set_profiles(m_profileModel);
            m_window->set_selected_profile(selectedIndex);
            m_tray->set_profiles(m_profileModel);

            if (profileIndex(m_profiles, m_preferences.exitProfileName) < 0)
                m_preferences.exitProfileName =
                    dcp::profile::Loader::defaultProfileName();

            m_window->set_exit_profile_index(
                profileIndex(m_profiles, m_preferences.exitProfileName));
        }

        void syncSettingsToUi()
        {
            m_window->set_brightness(
                static_cast<float>(m_settings.brightness));
            m_window->set_contrast(
                static_cast<float>(m_settings.contrast));
            m_window->set_gamma(
                static_cast<float>(m_settings.gamma));
            m_window->set_vibrance(
                static_cast<float>(m_settings.vibrance));
            m_window->set_hue(
                static_cast<float>(m_settings.hue));

            updateValueLabels();
        }

        void syncPreferencesToUi()
        {
            const bool closeQuits =
                m_preferences.closeAction ==
                dcp::preferences::CloseAction::Quit;

            m_window->set_open_on_startup(m_preferences.openOnStartup);
            m_window->set_close_quit(closeQuits);
            m_window->set_close_keep_running_in_tray(!closeQuits);
            m_window->set_apply_exit_profile(
                m_preferences.applyProfileOnExit);
            m_window->set_exit_profile_index(
                profileIndex(m_profiles, m_preferences.exitProfileName));
        }

        void savePreferences()
        {
            dcp::preferences::save(m_preferences);
        }

        void updateValueLabels()
        {
            m_window->set_brightness_text(
                intText(m_settings.brightness));
            m_window->set_contrast_text(
                intText(m_settings.contrast));
            m_window->set_gamma_text(
                gammaText(m_settings.gamma));
            m_window->set_vibrance_text(
                intText(m_settings.vibrance));
            m_window->set_hue_text(
                intText(m_settings.hue));
        }

        void resetToDefault()
        {
            loadProfile(dcp::profile::Loader::defaultProfileName());
        }

        void setBrightness(int value)
        {


            m_dcp.setBrightness(value);
            m_settings = m_dcp.settings();
            updateValueLabels();
        }

        void setContrast(int value)
        {


            m_dcp.setContrast(value);
            m_settings = m_dcp.settings();
            updateValueLabels();
        }

        void setGamma(double value)
        {


            m_dcp.setGamma(value);
            m_settings = m_dcp.settings();
            updateValueLabels();
        }

        void setVibrance(int value)
        {


            m_dcp.setDigitalVibrance(value);
            m_settings = m_dcp.settings();
            updateValueLabels();

        }

        void setHue(int value)
        {


            m_dcp.setHue(value);
            m_settings = m_dcp.settings();
            updateValueLabels();

        }

        void saveCurrentProfile(const std::string& profileName)
        {
            if (!dcp::profile::Loader::saveSettings(profileName, m_settings))
                return;

            loadProfile(profileName);
        }

        bool loadProfile(
            const std::string& requestedProfileName,
            bool remember = true)
        {
            if (requestedProfileName.empty())
                return false;

            const std::optional<dcp::profile::Profile> profile =
                dcp::profile::Loader::loadProfile(requestedProfileName);

            if (!profile || profile->settings.empty())
                return false;

            const dcp::profile::ColorSettings settings =
                profile->settings.back();

            m_dcp.applySettings(settings);
            m_settings = m_dcp.settings();
            m_window->set_profile_name(toSharedString(profile->name));
            refreshProfiles(profile->name);
            syncSettingsToUi();

            if (remember) {
                m_preferences.lastProfileName = profile->name;
                savePreferences();
            }

            syncPreferencesToUi();
            return true;
        }

        void setOpenOnStartup(bool enabled)
        {
            if (dcp::windows::setRunAtStartup(enabled))
                m_preferences.openOnStartup = enabled;
            else
                m_preferences.openOnStartup =
                    dcp::windows::isRunAtStartupEnabled();

            savePreferences();
            syncPreferencesToUi();
        }

        void setCloseAction(int action)
        {
            m_preferences.closeAction =
                action == CLOSE_ACTION_QUIT
                    ? dcp::preferences::CloseAction::Quit
                    : dcp::preferences::CloseAction::KeepRunningInTray;

            savePreferences();
            syncPreferencesToUi();
        }

        void setApplyProfileOnExit(bool enabled)
        {
            m_preferences.applyProfileOnExit = enabled;

            savePreferences();
            syncPreferencesToUi();
        }

        void setExitProfile(const std::string& profileName)
        {
            const std::optional<dcp::profile::Profile> profile =
                dcp::profile::Loader::loadProfile(profileName);

            if (!profile)
                return;

            m_preferences.exitProfileName = profile->name;

            savePreferences();
            syncPreferencesToUi();
        }

        void requestQuit()
        {
            if (m_quitting)
                return;

            m_quitting = true;

            if (m_preferences.applyProfileOnExit)
                loadProfile(m_preferences.exitProfileName, false);

            savePreferences();
            slint::quit_event_loop();
        }

    };
}

int dcp::ui::run()
{
    if (!dcp::windows::acquireSingleInstance())
        return SUCCESS;

    App app;
    return app.run();
}
