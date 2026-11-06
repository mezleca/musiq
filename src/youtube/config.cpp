#include "config.hpp"
#include "schema/request.hpp"

#include <stdexcept>

const music_config::MusicClientProfile& music_config::client_profile(MusicClient client) {
    static constexpr MusicClientProfile web{
        "WEB_REMIX",
        "67",
        CLIENT_VERSION,
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64; rv:140.0) Gecko/20100101 Firefox/140.0",
        true,
        {},
        {},
        {},
        {}
    };
    static constexpr MusicClientProfile vision{
        "VISIONOS",
        "101",
        "0.1",
        "Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/18.0 Safari/605.1.15",
        false,
        "visionOS",
        "1.3.21O771",
        "Apple",
        "RealityDevice14,1"
    };
    switch (client) {
        case MusicClient::WebRemix:
            return web;
        case MusicClient::VisionOs:
            return vision;
    }
    throw std::invalid_argument("unknown YouTube client");
}
