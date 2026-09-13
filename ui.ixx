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
import dcp.profile;

export namespace dcp::ui {
    int run();
}

namespace {
    using dcp_slint::AppTray;
    using dcp_slint::AppWindow;

    constexpr int SUCCESS = 0;
    std::string toStdString(const slint::SharedString& value)
    {
        return std::string(std::string_view(value));
    }

    slint::SharedString toSharedString(const std::string& value)
    {
        return slint::SharedString(std::string_view(value));
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
        return slint::SharedString(output.str());
    }

    class App {
    public:
        App()
            : m_window(AppWindow::create()),
              m_tray(AppTray::create())
        {
            setDefaultProfileName();
            connectCallbacks();
            refreshProfiles(dcp::profile::Loader::defaultProfileName());
            m_window->set_profile_name(
                toSharedString(dcp::profile::Loader::defaultProfileName()));
            m_settings = m_dcp.settings();
            syncSettingsToUi();
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
        dcp::nvidia::DCP m_dcp;
        dcp::profile::ColorSettings m_settings;

        void connectCallbacks()
        {
            auto windowWeak =
                slint::ComponentWeakHandle(m_window);

            m_window->window().on_close_requested([] {
                return slint::CloseRequestResponse::HideWindow;
            });

            /*m_window->on_hide_to_tray([windowWeak] {
                if (auto window = windowWeak.lock())
                    (*window)->hide();
            });*/

            /*m_window->on_exit_requested([] {
                slint::quit_event_loop();
            });*/

            m_window->on_save_profile([this](slint::SharedString name) {
                saveCurrentProfile(toStdString(name));
            });

            m_window->on_reset_settings([this] {
                resetToDefault();
            });

            m_window->on_profile_selected([this](slint::SharedString name) {
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

            m_tray->on_show_window([windowWeak] {
                if (auto window = windowWeak.lock())
                    (*window)->show();
            });

            m_tray->on_load_profile([this](slint::SharedString name) {
                loadProfile(toStdString(name));
            });

            m_tray->on_quit_requested([] {
                slint::quit_event_loop();
            });
        }

        void setDefaultProfileName()
        {
            m_window->set_default_profile_name(
                toSharedString(dcp::profile::Loader::defaultProfileName()));
        }

        void refreshProfiles(std::string_view selectedProfile)
        {
            const std::vector<std::string> profiles =
                dcp::profile::Loader::listProfiles();

            std::vector<slint::SharedString> items;
            items.reserve(profiles.size());

            int selectedIndex = -1;

            for (std::size_t i = 0; i < profiles.size(); i++) {
                if (profiles[i] == selectedProfile)
                    selectedIndex = static_cast<int>(i);

                items.emplace_back(profiles[i]);
            }

            m_profileModel =
                std::make_shared<slint::VectorModel<slint::SharedString>>(
                    std::move(items));

            m_window->set_profiles(m_profileModel);
            m_window->set_selected_profile(selectedIndex);
            m_tray->set_profiles(m_profileModel);
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
            refreshProfiles(profileName);
            m_window->set_profile_name(toSharedString(profileName));
        }

        void loadProfile(const std::string& profileName)
        {
            if (profileName.empty())
                return;

            const std::optional<dcp::profile::ColorSettings> settings =
                dcp::profile::Loader::loadLatestSettings(profileName);

            m_dcp.applySettings(*settings);
            m_settings = m_dcp.settings();
            m_window->set_profile_name(toSharedString(profileName));
            refreshProfiles(profileName);
            syncSettingsToUi();
        }

    };
}

int dcp::ui::run()
{
    App app;
    return app.run();
}
