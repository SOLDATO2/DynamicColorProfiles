//
// Created by sQuAde on 14/09/2026.
//

module;

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <stdexcept>
#include <string>
#include <variant>
#include <Windows.h>

export module dcp.amd;
import dcp;
import dcp.profile;

namespace dcp::amd {

    constexpr int ADL_OK = 0;

    //https://github.com/GPUOpen-LibrariesAndSDKs/display-library/blob/master/include/adl_defines.h

    constexpr int ADL_DISPLAY_COLOR_BRIGHTNESS = 1 << 0;
    constexpr int ADL_DISPLAY_COLOR_CONTRAST = 1 << 1;
    constexpr int ADL_DISPLAY_COLOR_SATURATION = 1 << 2;
    constexpr int ADL_DISPLAY_COLOR_HUE = 1 << 3;


    using MemoryAllocFn =
        void* (__stdcall*)(
            int size);

    using MainControlCreateFn =
        int (*)(
            MemoryAllocFn callback,
            int enumConnectedAdapters);

    using MainControlDestroyFn =
        int (*)();

    using DisplayColorGetFn =
        int (*)(
            int adapterIndex,
            int displayIndex,
            int colorType,
            int* current,
            int* defaultValue,
            int* min,
            int* max,
            int* step);

    using DisplayColorSetFn =
        int (*)(
            int adapterIndex,
            int displayIndex,
            int colorType,
            int current);


    export class DCP final : public dcp::DCP {
        HMODULE m_amdApi = nullptr;

        MainControlCreateFn m_mainControlCreate = nullptr;
        MainControlDestroyFn m_mainControlDestroy = nullptr;
        DisplayColorGetFn m_displayColorGet = nullptr;
        DisplayColorSetFn m_displayColorSet = nullptr;

        int m_adapterIndex = 0;
        int m_displayIndex = 0;
        bool m_controlCreated = false;
        profile::AmdColorSettings m_settings;

    public:
        DCP() {
            m_amdApi = LoadLibraryW(L"atiadlxx.dll");

            if (!m_amdApi)
                throw std::runtime_error("Could not load atiadlxx.dll");

            try {
                m_mainControlCreate = resolve<MainControlCreateFn>("ADL_Main_Control_Create");

                m_mainControlDestroy = resolve<MainControlDestroyFn>("ADL_Main_Control_Destroy");

                m_displayColorGet = resolve<DisplayColorGetFn>("ADL_Display_Color_Get");

                m_displayColorSet = resolve<DisplayColorSetFn>("ADL_Display_Color_Set");

                if (m_mainControlCreate(memoryAlloc, 1) != ADL_OK)
                    throw std::runtime_error("ADL_Main_Control_Create failed");

                m_controlCreated = true;
                m_settings = readSettings();
            } catch (...) {
                cleanup();
                throw;
            }
        }

        ~DCP() override {
            cleanup();
        }

        DCP(const DCP&) = delete;
        DCP& operator=(const DCP&) = delete;

        [[nodiscard]] Backend backend() const noexcept override {
            return Backend::Amd;
        }

        [[nodiscard]] Settings settings() const override {
            return m_settings;
        }

        [[nodiscard]] bool supports(Setting setting) const noexcept override {
            switch (setting) {
                case Setting::AmdBrightness:
                case Setting::AmdContrast:
                case Setting::AmdHue:
                case Setting::AmdSaturation:
                    return true;
                case Setting::NvidiaBrightness:
                case Setting::NvidiaContrast:
                case Setting::NvidiaGamma:
                case Setting::NvidiaVibrance:
                case Setting::NvidiaHue:
                    return false;
            }

            return false;
        }

        void applySettings(const Settings& settings) override {
            const auto* amdSettings =
                std::get_if<profile::AmdColorSettings>(&settings);

            if (!amdSettings)
                throw std::invalid_argument("Invalid settings for AMD backend");

            setBrightness(amdSettings->brightness);
            setHue(amdSettings->hue);
            setContrast(amdSettings->contrast);
            setSaturation(amdSettings->saturation);
        }

        void set(Setting setting, double value) override {
            const int integerValue =
                static_cast<int>(std::lround(value));

            switch (setting) {
                case Setting::AmdBrightness:
                    setBrightness(integerValue);
                    return;
                case Setting::AmdContrast:
                    setContrast(integerValue);
                    return;
                case Setting::AmdHue:
                    setHue(integerValue);
                    return;
                case Setting::AmdSaturation:
                    setSaturation(integerValue);
                    return;
                case Setting::NvidiaBrightness:
                case Setting::NvidiaContrast:
                case Setting::NvidiaGamma:
                case Setting::NvidiaVibrance:
                case Setting::NvidiaHue:
                    break;
            }

            throw std::invalid_argument("Setting is not supported by AMD backend");
        }

        void setBrightness(int value) {
            value = std::clamp(value, -100, 100);
            setColor(
                ADL_DISPLAY_COLOR_BRIGHTNESS,
                value);
            m_settings.brightness = value;
        }

        void setContrast(int value) {
            value = std::clamp(value, 0, 200);
            setColor(
                ADL_DISPLAY_COLOR_CONTRAST,
                value);
            m_settings.contrast = value;
        }

        void setSaturation(int value) {
            value = std::clamp(value, 0, 200);
            setColor(
                ADL_DISPLAY_COLOR_SATURATION,
                value);
            m_settings.saturation = value;
        }

        void setHue(int value) {
            value = std::clamp(value, -30, 30);
            setColor(
                ADL_DISPLAY_COLOR_HUE,
                value);
            m_settings.hue = value;
        }


    private:
        static void* __stdcall memoryAlloc(int size) {
            return std::malloc(size);
        }

        void cleanup() noexcept {
            if (m_controlCreated && m_mainControlDestroy)
                m_mainControlDestroy();

            m_controlCreated = false;

            if (m_amdApi)
                FreeLibrary(m_amdApi);

            m_amdApi = nullptr;
        }

        [[nodiscard]] profile::AmdColorSettings readSettings() {
            return {
                getColor(ADL_DISPLAY_COLOR_BRIGHTNESS),
                getColor(ADL_DISPLAY_COLOR_HUE),
                getColor(ADL_DISPLAY_COLOR_CONTRAST),
                getColor(ADL_DISPLAY_COLOR_SATURATION)
            };
        }

        [[nodiscard]] int getColor(int colorType) const {
            int current = 0;
            int defaultValue = 0;
            int min = 0;
            int max = 0;
            int step = 0;

            if (m_displayColorGet(
                    m_adapterIndex,
                    m_displayIndex,
                    colorType,
                    &current,
                    &defaultValue,
                    &min,
                    &max,
                    &step) != ADL_OK) {
                throw std::runtime_error(
                    "ADL_Display_Color_Get failed");
            }

            return current;
        }


        void setColor(int colorType, int value)
        {
            int current;
            int defaultValue;
            int min;
            int max;
            int step;

            const int status =
                m_displayColorGet(
                    m_adapterIndex,
                    m_displayIndex,
                    colorType,
                    &current,
                    &defaultValue,
                    &min,
                    &max,
                    &step);

            if (status != ADL_OK)
                throw std::runtime_error(
                    "ADL_Display_Color_Get failed");

            value =
                std::clamp(
                    value,
                    min,
                    max);

            if (
                m_displayColorSet(
                    m_adapterIndex,
                    m_displayIndex,
                    colorType,
                    value) != ADL_OK)
            {
                throw std::runtime_error(
                    "ADL_Display_Color_Set failed");
            }
        }


        template<typename T>
        T resolve(const char* functionName) {
            auto address = GetProcAddress(m_amdApi, functionName);

            return reinterpret_cast<T>(address);
        }
    };
}
