#include <msquic.h>

#include <wrappers.hpp>

class MOQT;

class MOQTFactory {
    rvn::unique_QUIC_API_TABLE tbl;
    rvn::unique_registration reg;
    rvn::unique_configuration configuration;
    rvn::unique_listener listener;

    std::uint8_t counter = 0;

    // Every time a value is setup update counter
    // TODO : change to constexpr map
    enum Indexes {
        tbl_idx = 0,
        reg_idx = 1,
        configuration_idx = 2,
        listener_idx = 3
    };

    constexpr std::uint8_t get_filled_counter() {
        return (1 << tbl_idx) | (1 << reg_idx) |
               (1 << configuration_idx) | (1 << listener_idx);
    }

   public:
    MOQTFactory() : tbl(rvn::make_unique_quic_table()) {}

    MOQTFactory& setup_registration(
        const QUIC_REGISTRATION_CONFIG* RegConfig) {
        assert(counter >= (1 << tbl_idx));
        reg = rvn::unique_registration(tbl.get(), RegConfig);
        counter |= (1 << reg_idx);
        return *this;
    }

    MOQTFactory& setup_configuration(
        rvn::unique_configuration::ConfigurationOpenParams
            openParams,
        rvn::unique_configuration::ConfigurationStartParams
            startParams) {
        assert(counter >= (1 << reg_idx));
        configuration = rvn::unique_configuration(
            tbl.get(), openParams, startParams);
        counter |= (1 << configuration_idx);
        return *this;
    }

    MOQTFactory& setup_unique_listener(
        rvn::unique_listener::ListenerOpenParams openParams,
        rvn::unique_listener::ListenerStartParams startParams) {
        assert(counter >= (1 << reg_idx));
        listener = rvn::unique_listener(tbl.get(), openParams,
                                        startParams);
        counter |= (1 << listener_idx);
        return *this;
    }

    std::unique_ptr<MOQT> get() {
        if (counter != get_filled_counter()) {
            // TODO : print what all have been set
            throw std::runtime_error(
                "Not all required Parameters are set");
        }

        return std::make_unique<MOQT>(
            std::move(tbl), std::move(reg),
            std::move(configuration), std::move(listener));
    }
};
