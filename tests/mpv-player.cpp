#include "../src/playback/mpv-player.hpp"
#include "../src/playback/controller.hpp"
#include "../src/utils/thread-pool.hpp"
#include "../src/youtube/stream.hpp"
#include "../src/youtube/schema/player.hpp"
#include "../src/youtube/session.hpp"

#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <thread>

template <typename Predicate>
static bool wait_for_player(MpvPlayer& player, Predicate ready) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    do {
        player.update();
        if (ready()) return true;
        if (!player.state().error.empty()) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    } while (std::chrono::steady_clock::now() < deadline);
    return false;
}

TEST_CASE("libmpv loads pauses seeks and reports natural completion", "[playback][mpv]") {
    constexpr std::uint32_t data_size = 48000 * 2 * 2;
    std::string wave(44 + data_size, '\0');
    const auto integer = [&wave](std::size_t offset, std::uint32_t value, int bytes) {
        for (int index = 0; index < bytes; ++index) {
            wave[offset + index] = static_cast<char>(value >> (index * 8));
        }
    };
    std::memcpy(wave.data(), "RIFF", 4);
    integer(4, 36 + data_size, 4);
    std::memcpy(wave.data() + 8, "WAVEfmt ", 8);
    integer(16, 16, 4);
    integer(20, 1, 2);
    integer(22, 1, 2);
    integer(24, 48000, 4);
    integer(28, 96000, 4);
    integer(32, 2, 2);
    integer(34, 16, 2);
    std::memcpy(wave.data() + 36, "data", 4);
    integer(40, data_size, 4);
    const auto path = std::filesystem::temp_directory_path() /
                      ("musiq-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".wav");
    {
        std::ofstream output(path, std::ios::binary);
        output.write(wave.data(), wave.size());
    }

    ThreadPool workers;
    MpvPlayer player(true);
    PlaybackController playback(workers, player);
    player.load_file(path.string());
    REQUIRE(wait_for_player(player, [&] { return player.state().loaded && player.state().duration > 0; }));
    player.pause(true);
    REQUIRE(wait_for_player(player, [&] { return player.state().paused; }));
    player.seek(1.5);
    REQUIRE(wait_for_player(player, [&] { return player.state().position >= 1.4; }));
    const auto position = player.state().position;
    playback.set_session(YoutubeMusicSession{});
    REQUIRE(player.state().loaded);
    REQUIRE(player.state().paused);
    REQUIRE(player.state().position == position);
    player.pause(false);
    playback.set_session(YoutubeMusicSession{});
    REQUIRE(player.state().loaded);
    REQUIRE_FALSE(player.state().paused);
    REQUIRE(wait_for_player(player, [&] { return player.state().ended; }));
    player.stop();
    player.update();
    REQUIRE_FALSE(player.state().ended);
    REQUIRE(player.state().error.empty());
    std::filesystem::remove(path);
}

TEST_CASE("audio selection ignores DRM ciphered streams and alternate languages", "[youtube][stream]") {
    MusicStreamingData data;
    MusicFormat original;
    original.url = "https://example.invalid/audio";
    original.mime_type = "audio/webm";
    original.bitrate = 128000;
    original.content_length = 100;
    auto dubbed = original;
    dubbed.bitrate = 256000;
    dubbed.audio_track = MusicAudioTrack{.is_auto_dubbed = true};
    auto drm = original;
    drm.drm_families = {"WIDEVINE"};
    auto cipher = original;
    cipher.url.clear();
    cipher.signature_cipher = "s=signature";
    data.adaptive_formats = {dubbed, drm, cipher, original};
    REQUIRE(YoutubeMediaStream::select_audio_format(data) == &data.adaptive_formats.back());
    data.adaptive_formats.pop_back();
    REQUIRE(YoutubeMediaStream::select_audio_format(data) == nullptr);
}
