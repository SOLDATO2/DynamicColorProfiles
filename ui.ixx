module;

#include "ui.h"

#include <algorithm>
#include <cmath>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

export module ui;

import dcp.factory;
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

        const int decimals = std::abs(scaled % 100);

        if (decimals < 10)
            output << '0';

        output << decimals;
        return {output.str()};
    }

    int profileIndex(const std::vector<std::string>& profiles, std::string_view selectedProfile)
    {
        for (std::size_t i = 0; i < profiles.size(); i++) {
            if (profiles[i] == selectedProfile)
                return static_cast<int>(i);
        }

        return -1;
    }

    std::vector<std::string> profilesForBackend(dcp::Backend backend)
    {
        return backend == dcp::Backend::Nvidia
            ? dcp::profile::Loader::listNvidiaProfiles()
            : dcp::profile::Loader::listAmdProfiles();
    }

    std::string availableProfileOrDefault(
        std::string_view profileName,
        dcp::Backend backend)
    {
        const std::vector<std::string> profiles =
            profilesForBackend(backend);

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
              m_tray(AppTray::create()),
              m_dcp(dcp::DCPFactory::create())
        {
            m_preferences =
                dcp::preferences::load();
            m_preferences.openOnStartup =
                dcp::windows::isRunAtStartupEnabled();

            setDefaultProfileName();
            connectCallbacks();

            if (m_dcp) {
                const dcp::Backend backend = m_dcp->backend();
                std::string& initialProfileName =
                    backend == dcp::Backend::Nvidia
                        ? m_nvidiaProfileName
                        : m_amdProfileName;
                initialProfileName = availableProfileOrDefault(
                    m_preferences.lastProfileName,
                    backend);
                m_preferences.exitProfileName = availableProfileOrDefault(
                    m_preferences.exitProfileName,
                    backend);
                selectBackend(backend, false);
            } else {
                m_window->set_selected_backend(-1);
                refreshProfiles({});
            }

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
        std::unique_ptr<dcp::DCP> m_dcp;
        std::optional<dcp::Backend> m_selectedBackend;
        dcp::profile::NvidiaColorSettings m_nvidiaSettings;
        dcp::profile::AmdColorSettings m_amdSettings;
        std::string m_nvidiaProfileName =
            dcp::profile::Loader::defaultProfileName();
        std::string m_amdProfileName =
            dcp::profile::Loader::defaultProfileName();
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

            m_window->on_nvidia_brightness_changed([this](float value) {
                setSetting(dcp::Setting::NvidiaBrightness, value);
            });

            m_window->on_nvidia_contrast_changed([this](float value) {
                setSetting(dcp::Setting::NvidiaContrast, value);
            });

            m_window->on_nvidia_gamma_changed([this](float value) {
                setSetting(dcp::Setting::NvidiaGamma, value);
            });

            m_window->on_nvidia_vibrance_changed([this](float value) {
                setSetting(dcp::Setting::NvidiaVibrance, value);
            });

            m_window->on_nvidia_hue_changed([this](float value) {
                setSetting(dcp::Setting::NvidiaHue, value);
            });

            m_window->on_amd_brightness_changed([this](float value) {
                setSetting(dcp::Setting::AmdBrightness, value);
            });

            m_window->on_amd_hue_changed([this](float value) {
                setSetting(dcp::Setting::AmdHue, value);
            });

            m_window->on_amd_contrast_changed([this](float value) {
                setSetting(dcp::Setting::AmdContrast, value);
            });

            m_window->on_amd_saturation_changed([this](float value) {
                setSetting(dcp::Setting::AmdSaturation, value);
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

        bool selectBackend(
            dcp::Backend backend,
            bool remember = true)
        {
            if (!m_dcp || m_dcp->backend() != backend)
                return false;

            m_selectedBackend = backend;
            m_window->set_selected_backend(
                backend == dcp::Backend::Nvidia ? 0 : 1);

            const std::string& profileName =
                backend == dcp::Backend::Nvidia
                    ? m_nvidiaProfileName
                    : m_amdProfileName;

            const std::string selectedProfile =
                availableProfileOrDefault(profileName, backend);

            return loadProfile(selectedProfile, remember);
        }

        void refreshProfiles(std::string_view selectedProfile)
        {
            m_profiles = m_selectedBackend
                ? profilesForBackend(*m_selectedBackend)
                : std::vector<std::string>{};

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

        void updateSettingsFromDcp()
        {
            if (!m_dcp)
                return;

            const dcp::Settings settings = m_dcp->settings();

            if (const auto* nvidiaSettings =
                    std::get_if<dcp::profile::NvidiaColorSettings>(&settings)) {
                m_nvidiaSettings = *nvidiaSettings;
                return;
            }

            m_amdSettings =
                std::get<dcp::profile::AmdColorSettings>(settings);
        }

        void syncSettingsToUi()
        {
            if (m_selectedBackend == dcp::Backend::Nvidia) {
                m_window->set_nvidia_brightness(
                    static_cast<float>(m_nvidiaSettings.brightness));
                m_window->set_nvidia_contrast(
                    static_cast<float>(m_nvidiaSettings.contrast));
                m_window->set_nvidia_gamma(
                    static_cast<float>(m_nvidiaSettings.gamma));
                m_window->set_nvidia_vibrance(
                    static_cast<float>(m_nvidiaSettings.vibrance));
                m_window->set_nvidia_hue(
                    static_cast<float>(m_nvidiaSettings.hue));
            } else if (m_selectedBackend == dcp::Backend::Amd) {
                m_window->set_amd_brightness(
                    static_cast<float>(m_amdSettings.brightness));
                m_window->set_amd_hue(
                    static_cast<float>(m_amdSettings.hue));
                m_window->set_amd_contrast(
                    static_cast<float>(m_amdSettings.contrast));
                m_window->set_amd_saturation(
                    static_cast<float>(m_amdSettings.saturation));
            }

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
            m_window->set_nvidia_brightness_text(
                intText(m_nvidiaSettings.brightness));
            m_window->set_nvidia_contrast_text(
                intText(m_nvidiaSettings.contrast));
            m_window->set_nvidia_gamma_text(
                gammaText(m_nvidiaSettings.gamma));
            m_window->set_nvidia_vibrance_text(
                intText(m_nvidiaSettings.vibrance));
            m_window->set_nvidia_hue_text(
                intText(m_nvidiaSettings.hue));
            m_window->set_amd_brightness_text(
                intText(m_amdSettings.brightness));
            m_window->set_amd_hue_text(
                intText(m_amdSettings.hue));
            m_window->set_amd_contrast_text(
                intText(m_amdSettings.contrast));
            m_window->set_amd_saturation_text(
                intText(m_amdSettings.saturation));
        }

        void resetToDefault()
        {
            loadProfile(dcp::profile::Loader::defaultProfileName());
        }

        void setSetting(dcp::Setting setting, double value)
        {
            if (!m_dcp || !m_dcp->supports(setting))
                return;

            m_dcp->set(setting, value);
            updateSettingsFromDcp();
            updateValueLabels();
        }

        void saveCurrentProfile(const std::string& profileName)
        {
            if (!m_selectedBackend)
                return;

            const bool saved =
                *m_selectedBackend == dcp::Backend::Nvidia
                    ? dcp::profile::Loader::saveNvidiaSettings(
                        profileName,
                        m_nvidiaSettings)
                    : dcp::profile::Loader::saveAmdSettings(
                        profileName,
                        m_amdSettings);

            if (!saved)
                return;

            loadProfile(profileName);
        }

        bool loadProfile(
            const std::string& requestedProfileName,
            bool remember = true)
        {
            if (requestedProfileName.empty())
                return false;

            if (!m_selectedBackend || !m_dcp)
                return false;

            std::string loadedProfileName;

            if (*m_selectedBackend == dcp::Backend::Nvidia) {
                const std::optional<dcp::profile::NvidiaProfile> profile =
                    dcp::profile::Loader::loadNvidiaProfile(
                        requestedProfileName);

                if (!profile || profile->settings.empty())
                    return false;

                m_dcp->applySettings(profile->settings.back());
                updateSettingsFromDcp();
                m_nvidiaProfileName = profile->name;
                loadedProfileName = profile->name;
            } else {
                const std::optional<dcp::profile::AmdProfile> profile =
                    dcp::profile::Loader::loadAmdProfile(requestedProfileName);

                if (!profile || profile->settings.empty())
                    return false;

                m_dcp->applySettings(profile->settings.back());
                updateSettingsFromDcp();
                m_amdProfileName = profile->name;
                loadedProfileName = profile->name;
            }

            m_window->set_profile_name(toSharedString(loadedProfileName));
            refreshProfiles(loadedProfileName);
            syncSettingsToUi();

            if (remember) {
                m_preferences.lastProfileName = loadedProfileName;
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
            if (!m_selectedBackend)
                return;

            if (*m_selectedBackend == dcp::Backend::Nvidia) {
                const std::optional<dcp::profile::NvidiaProfile> profile =
                    dcp::profile::Loader::loadNvidiaProfile(profileName);

                if (!profile)
                    return;

                m_preferences.exitProfileName = profile->name;
            } else {
                const std::optional<dcp::profile::AmdProfile> profile =
                    dcp::profile::Loader::loadAmdProfile(profileName);

                if (!profile)
                    return;

                m_preferences.exitProfileName = profile->name;
            }

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
    if (!windows::acquireSingleInstance())
        return SUCCESS;

    App app;
    return app.run();
}
