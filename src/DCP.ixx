module;

#include <variant>

export module dcp;

import dcp.profile;

export namespace dcp {
    enum class Backend {
        Nvidia,
        Amd
    };

    enum class Setting {
        NvidiaBrightness,
        NvidiaContrast,
        NvidiaGamma,
        NvidiaVibrance,
        NvidiaHue,
        AmdBrightness,
        AmdContrast,
        AmdHue,
        AmdSaturation
    };

    using Settings = std::variant<profile::NvidiaColorSettings, profile::AmdColorSettings>;

    class DCP {
    public:
        virtual ~DCP() = default;

        [[nodiscard]] virtual Backend backend() const noexcept = 0;
        [[nodiscard]] virtual Settings settings() const = 0;
        [[nodiscard]] virtual bool supports(Setting setting) const noexcept = 0;

        virtual void applySettings(const Settings& settings) = 0;
        virtual void set(Setting setting, double value) = 0;
    };
}
