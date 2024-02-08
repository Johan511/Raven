#include <msquic.h>

#include <functional>
#include <utilities.hpp>
#include <wrappers.hpp>

class MOQTClient : public MOQT {
    rvn::unique_connection connection;

   public:
    MOQTClient() : MOQT(){};

    void start_connection(QUIC_ADDRESS_FAMILY Family,
                          const char* ServerName,
                          uint16_t ServerPort) {
        assert(secondaryCounter == full_sec_counter_value());
        reg = rvn::unique_registration(tbl.get(), regConfig);
        configuration = rvn::unique_configuration(
            tbl.get(),
            {reg.get(), AlpnBuffers, AlpnBufferCount, Settings,
             SettingsSize, this},
            {CredConfig});
        connection = rvn::unique_connection(
            tbl.get(),
            {reg.get(), MOQT::connection_cb_wrapper, this},
            {configuration.get(), Family, ServerName,
             ServerPort});
    }
};
