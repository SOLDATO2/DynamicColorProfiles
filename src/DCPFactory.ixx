module;

#include <memory>

export module dcp.factory;

export import dcp;
import dcp.amd;
import dcp.nvidia;

export namespace dcp {
    class DCPFactory {
    public:
        [[nodiscard]] static std::unique_ptr<DCP> create() noexcept
        {
            if (std::unique_ptr<DCP> dcp = createNvidia())
                return dcp;

            return createAmd();
        }

    private:
        [[nodiscard]] static std::unique_ptr<DCP> createNvidia() noexcept
        {
            try {
                return std::make_unique<nvidia::DCP>();
            } catch (...) {
                return nullptr;
            }
        }

        [[nodiscard]] static std::unique_ptr<DCP> createAmd() noexcept
        {
            try {
                return std::make_unique<amd::DCP>();
            } catch (...) {
                return nullptr;
            }
        }
    };
}
