module;

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <Windows.h>

export module dcp.nvidia;
import dcp.profile;

namespace dcp::nvidia {
    using NvStatus = int;
    using NvU32 = std::uint32_t;
    using NvS32 = std::int32_t;
    using NvDisplayHandle = void*;

    constexpr NvStatus NVAPI_OK = 0;

    //https://github.com/NVIDIA/nvapi/blob/main/nvapi_interface.h

    // NvAPI_Initialize                    -> 0x0150E828
    // NvAPI_Unload                        -> 0xD22BDD7E
    // NvAPI_EnumNvidiaDisplayHandle       -> 0x9ABDD40D
    // NvAPI_GetAssociatedNvidiaDisplayName-> 0x22A78B05
    // NvAPI_DISP_GetDisplayIdByDisplayName-> 0xAE457190

    constexpr NvU32 NVAPI_INITIALIZE = 0x0150E828;
    constexpr NvU32 NVAPI_UNLOAD = 0xD22BDD7E;
    constexpr NvU32 NVAPI_ENUM_NVIDIA_DISPLAY_HANDLE = 0x9ABDD40D;
    constexpr NvU32 NVAPI_GET_ASSOCIATED_DISPLAY_NAME = 0x22A78B05;
    constexpr NvU32 NVAPI_GET_DISPLAY_ID_BY_NAME = 0xAE457190;

    //non public

    //https://github.com/jNizM/NVIDIA_NvAPI/blob/master/
    //https://github.com/falahati/NvAPIWrapper/blob/master/NvAPIWrapper/Native/Helpers/FunctionId.cs

    //Brightness + Contrast + Gamma
    constexpr NvU32 NVAPI_DISP_SET_TARGET_GAMMA_CORRECTION = 0x7082A053;

    //Digital Vibrance
    constexpr NvU32 NVAPI_GET_DIGITAL_VIBRANCE_CONTROL_INFO_EX = 0x0E45002D;
    constexpr NvU32 NVAPI_SET_DIGITAL_VIBRANCE_CONTROL_LEVEL_EX = 0x4A82C2B1;

    //Hue
    constexpr NvU32 NVAPI_GET_HUE_INFO = 0x95B64341;
    constexpr NvU32 NVAPI_SET_HUE_ANGLE = 0xF5A0F22C;

    //Output Id
    constexpr NvU32 DEFAULT_OUTPUT_ID = 0;
    constexpr std::size_t NVAPI_SHORT_STRING_MAX = 64;


    //--------struct version
    // https://github.com/NVIDIA/nvapi/blob/main/nvapi.h

    //it is necessary to specify the version so the functions called in the nvidia api know how to interpret the parameters
    constexpr NvU32 makeVersion(std::size_t size, NvU32 version) {
        return static_cast<NvU32>(size) | version << 16;
    }
    //----------------------------------------------------------
    struct DigitalVibranceControlInfoEx
    {
        NvU32 version;

        NvS32 currentLevel;
        NvS32 minimumLevel;
        NvS32 maximumLevel;
        NvS32 defaultLevel;
    };

    struct HueInfo
    {
        NvU32 version;

        NvS32 currentAngle;
        NvS32 defaultAngle;
    };

    struct GammaCorrectionEx
    {
        NvU32 version;
        float gammaRamp[1024 * 3];
        NvU32 unknown;
    };

    //declaring functions that will store adresses to functions in the nvidia api
    //i.e pointer to a function that recieves id as a param and does not return anything
    using QueryInterfaceFn = void* (*)(NvU32 id);

    //i.e pointer to a function that doesn't receive params and returns NvStatus
    using InitializeFn = NvStatus (*)();

    using UnloadFn = NvStatus (*)();

    using EnumDisplayFn = NvStatus (*)(
        NvU32 index,
        NvDisplayHandle* display);

    using GetAssociatedDisplayNameFn = NvStatus (*)(
        NvDisplayHandle display,
        char* displayName);

    using GetDisplayIdByNameFn = NvStatus (*)(
        const char* displayName,
        NvU32* displayId);

    using SetTargetGammaCorrectionFn = NvStatus (*)(
        NvU32 displayId,
        GammaCorrectionEx* data);

    using GetDigitalVibranceControlInfoExFn = NvStatus (*)(
        NvDisplayHandle display,
        NvU32 outputId,
        DigitalVibranceControlInfoEx* data);

    using SetDigitalVibranceControlLevelExFn = NvStatus (*)(
        NvDisplayHandle display,
        NvU32 outputId,
        DigitalVibranceControlInfoEx* data);

    using GetHueInfoFn = NvStatus (*)(
        NvDisplayHandle display,
        NvU32 outputId,
        HueInfo* data);

    using SetHueAngleFn = NvStatus (*)(
        NvDisplayHandle display,
        NvU32 outputId,
        NvS32 angle);

//------------------------- PUBLIC USE -------------------------//
    //DISPLAY
    struct Display {
        NvDisplayHandle handle = nullptr;
        NvU32 displayId = 0;
        std::string name;
    };

    //settings, default

    //main class
    export class DCP {
        HMODULE m_nvidiaApi = nullptr;

        profile::ColorSettings m_settings{50, 50, 1.0, 50, 0};
        Display m_display;

        //QueryInterface
        //uses the functions Ids previously defined to resolve real pointers to the functions
        QueryInterfaceFn m_queryInterface = nullptr;

        InitializeFn m_initialize = nullptr;
        UnloadFn m_unload = nullptr;
        EnumDisplayFn m_enumDisplay = nullptr;
        GetAssociatedDisplayNameFn m_getAssociatedDisplayName = nullptr;
        GetDisplayIdByNameFn m_getDisplayIdByName = nullptr;
        SetTargetGammaCorrectionFn m_setTargetGammaCorrectionFn = nullptr;
        GetDigitalVibranceControlInfoExFn m_getDigitalVibranceControlInfoEx = nullptr;
        SetDigitalVibranceControlLevelExFn m_setDigitalVibranceControlLevelEx = nullptr;
        GetHueInfoFn m_getHueInfo = nullptr;
        SetHueAngleFn m_setHueAngle = nullptr;

    public:
        DCP() {

            profile::Loader::init();

            m_nvidiaApi = LoadLibrary("nvapi64.dll");

            if (!m_nvidiaApi)
                throw std::runtime_error("Could not load nvapi64.dll");

            m_queryInterface = reinterpret_cast<QueryInterfaceFn>(
                GetProcAddress(m_nvidiaApi, "nvapi_QueryInterface"));

            if (!m_queryInterface)
                throw std::runtime_error("nvapi_QueryInterface not found");

            m_initialize = resolve<InitializeFn>(NVAPI_INITIALIZE);
            m_unload = resolve<UnloadFn>(NVAPI_UNLOAD);
            m_enumDisplay = resolve<EnumDisplayFn>(NVAPI_ENUM_NVIDIA_DISPLAY_HANDLE);
            m_getAssociatedDisplayName = resolve<GetAssociatedDisplayNameFn>(NVAPI_GET_ASSOCIATED_DISPLAY_NAME);
            m_getDisplayIdByName = resolve<GetDisplayIdByNameFn>(NVAPI_GET_DISPLAY_ID_BY_NAME);
            m_setTargetGammaCorrectionFn = resolve<SetTargetGammaCorrectionFn>(NVAPI_DISP_SET_TARGET_GAMMA_CORRECTION);
            m_getDigitalVibranceControlInfoEx = resolve<GetDigitalVibranceControlInfoExFn>(NVAPI_GET_DIGITAL_VIBRANCE_CONTROL_INFO_EX);
            m_setDigitalVibranceControlLevelEx = resolve<SetDigitalVibranceControlLevelExFn>(NVAPI_SET_DIGITAL_VIBRANCE_CONTROL_LEVEL_EX);
            m_getHueInfo = resolve<GetHueInfoFn>(NVAPI_GET_HUE_INFO);
            m_setHueAngle = resolve<SetHueAngleFn>(NVAPI_SET_HUE_ANGLE);

            const NvStatus status = m_initialize();

            if (status != NVAPI_OK)
                throw std::runtime_error("NvAPI_Initialize failed: " + std::to_string(status));

            m_display = getDisplay();
        }

        ~DCP() {
            if (m_unload)
                m_unload();

            if (m_nvidiaApi)
                FreeLibrary(m_nvidiaApi);
        }

        profile::ColorSettings settings() const {
            return m_settings;
        }

        void applySettings(profile::ColorSettings settings) {
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

            m_settings.brightness = settings.brightness;
            m_settings.contrast = settings.contrast;
            m_settings.gamma = settings.gamma;
            applyGammaCorrection();

            setDigitalVibrance(settings.vibrance);
            setHue(settings.hue);
        }

        void setDigitalVibrance(int percent) {
            percent = std::clamp(percent, 0, 100);

            DigitalVibranceControlInfoEx info{};
            info.version = makeVersion(sizeof(DigitalVibranceControlInfoEx), 1);
            m_getDigitalVibranceControlInfoEx(m_display.handle, DEFAULT_OUTPUT_ID, &info);

            //     -1.0 --- 0 --- +1.0
            //
            //
            //
            // normalized = (percent - 50) / 50
            //
            // 0    -> -1
            // 50   ->  0
            // 100  -> +1

            const double normalized = (static_cast<double>(percent) - 50.0) / 50.0;
            double nativeLevel = 0.0;

            if (normalized >= 0.0)
            {
                nativeLevel = info.defaultLevel + normalized * (info.maximumLevel - info.defaultLevel);
            } else{
                nativeLevel = info.defaultLevel + normalized * (info.defaultLevel - info.minimumLevel);
            }

            info.currentLevel = static_cast<NvS32>(std::lround(nativeLevel));
            m_setDigitalVibranceControlLevelEx(m_display.handle, DEFAULT_OUTPUT_ID, &info);

            m_settings.vibrance = percent;
        }

        void setHue(int angle) {
            angle = ((angle % 360) + 360) % 360;
            m_setHueAngle(m_display.handle, DEFAULT_OUTPUT_ID, angle);
            m_settings.hue = angle;
        }

        void setBrightness(int percent) {
            m_settings.brightness = std::clamp(percent, 0, 100);
            applyGammaCorrection();
        }

        void setContrast(int percent) {
            m_settings.contrast = std::clamp(percent, 0, 100);
            applyGammaCorrection();
        }

        void setGamma(double gamma) {
            m_settings.gamma = std::clamp(gamma, 0.4, 2.8);
            applyGammaCorrection();
        }

    private:
        Display getDisplay(NvU32 index = 0) {
            Display result;
            char displayName[NVAPI_SHORT_STRING_MAX]{};

            if (m_enumDisplay(index, &result.handle) != NVAPI_OK)
                throw std::runtime_error("Could not get NVIDIA display");

            if (m_getAssociatedDisplayName(result.handle, displayName) != NVAPI_OK)
                throw std::runtime_error("Could not get NVIDIA display name");

            result.name = displayName;

            if (m_getDisplayIdByName(result.name.c_str(), &result.displayId) != NVAPI_OK)
                throw std::runtime_error("Could not get NVIDIA display id");

            return result;
        }

        void applyGammaCorrection() {
            GammaCorrectionEx data{};
            data.version = makeVersion(sizeof(GammaCorrectionEx), 1);
            data.unknown = 1;

            const double brightness = (80.0 + m_settings.brightness / 100.0 * 40.0 - 100.0) / 100.0;
            const double contrast = (80.0 + m_settings.contrast / 100.0 * 40.0 - 100.0) / 100.0;
            const double gamma = 1.0 / m_settings.gamma;

            for (int i = 0; i < 1024; i++) {
                const double x = static_cast<double>(i) / 1023.0;

                double value = contrast <= 0.0
                    ? (contrast + 1.0) * (x - 0.5)
                    : (x - 0.5) / (1.0 - contrast);

                value = std::clamp(value + brightness + 0.5, 0.0, 1.0);
                value = std::pow(value, gamma);

                data.gammaRamp[i * 3] = static_cast<float>(value);
                data.gammaRamp[i * 3 + 1] = static_cast<float>(value);
                data.gammaRamp[i * 3 + 2] = static_cast<float>(value);
            }

            const NvStatus status =
                m_setTargetGammaCorrectionFn(m_display.displayId, &data);

            if (status != NVAPI_OK)
                throw std::runtime_error(
                    "NvAPI_DISP_SetTargetGammaCorrection failed: " +
                    std::to_string(status));
        }

        template<typename T>
        T resolve(NvU32 functionId) {
            if (!m_queryInterface)
                throw std::runtime_error("nvapi_QueryInterface is null");

            void* address = m_queryInterface(functionId);

            if (!address)
                throw std::runtime_error("NVAPI function not found: " + std::to_string(functionId));

            return reinterpret_cast<T>(address);
        }
    };
}
