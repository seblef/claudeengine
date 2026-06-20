#include "audio/AudioDecoderUtils.h"

#include <string>

#include <loguru.hpp>

// dr_libs header-only implementations — define exactly once per binary.
#define DR_WAV_IMPLEMENTATION
#include <dr_wav.h>  // NOLINT(build/include_order)

#define DR_MP3_IMPLEMENTATION
#include <dr_mp3.h>  // NOLINT(build/include_order)

#define DR_FLAC_IMPLEMENTATION
#include <dr_flac.h>  // NOLINT(build/include_order)

// stb_vorbis: single-file OGG/Vorbis decoder; included as an implementation
// unit (not a header), so this is the only translation unit that must see it.
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#pragma GCC diagnostic ignored "-Wunused-value"
#pragma GCC diagnostic ignored "-Wsign-compare"
#endif
#include <stb_vorbis.c>  // NOLINT(build/include)
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#ifdef HAVE_OPUSFILE
#include <opusfile.h>
#endif

namespace audio {

namespace {

DecodedAudio DecodeWav(const std::filesystem::path& path) {
    unsigned int channels    = 0;
    unsigned int sample_rate = 0;
    drwav_uint64 frame_count = 0;

    drwav_int16* data = drwav_open_file_and_read_pcm_frames_s16(
        path.c_str(), &channels, &sample_rate, &frame_count, nullptr);
    if (!data) {
        LOG_F(ERROR, "AudioDecoder: WAV decode failed: %s", path.c_str());
        return {};
    }

    DecodedAudio result;
    result.channels    = static_cast<int>(channels);
    result.sample_rate = static_cast<int>(sample_rate);
    const auto total   = static_cast<size_t>(frame_count) * channels;
    result.pcm.assign(data, data + total);
    drwav_free(data, nullptr);
    return result;
}

DecodedAudio DecodeMp3(const std::filesystem::path& path) {
    drmp3_config config{};
    drmp3_uint64 frame_count = 0;

    drmp3_int16* data = drmp3_open_file_and_read_pcm_frames_s16(
        path.c_str(), &config, &frame_count, nullptr);
    if (!data) {
        LOG_F(ERROR, "AudioDecoder: MP3 decode failed: %s", path.c_str());
        return {};
    }

    DecodedAudio result;
    result.channels    = static_cast<int>(config.channels);
    result.sample_rate = static_cast<int>(config.sampleRate);
    const auto total   = static_cast<size_t>(frame_count) * config.channels;
    result.pcm.assign(data, data + total);
    drmp3_free(data, nullptr);
    return result;
}

DecodedAudio DecodeFlac(const std::filesystem::path& path) {
    unsigned int channels    = 0;
    unsigned int sample_rate = 0;
    drflac_uint64 frame_count = 0;

    drflac_int16* data = drflac_open_file_and_read_pcm_frames_s16(
        path.c_str(), &channels, &sample_rate, &frame_count, nullptr);
    if (!data) {
        LOG_F(ERROR, "AudioDecoder: FLAC decode failed: %s", path.c_str());
        return {};
    }

    DecodedAudio result;
    result.channels    = static_cast<int>(channels);
    result.sample_rate = static_cast<int>(sample_rate);
    const auto total   = static_cast<size_t>(frame_count) * channels;
    result.pcm.assign(data, data + total);
    drflac_free(data, nullptr);
    return result;
}

DecodedAudio DecodeOgg(const std::filesystem::path& path) {
    int channels    = 0;
    int sample_rate = 0;
    int16_t* raw    = nullptr;

    const int sample_count = stb_vorbis_decode_filename(
        path.c_str(), &channels, &sample_rate,
        reinterpret_cast<short**>(&raw));  // NOLINT(runtime/int)
    if (sample_count < 0 || !raw) {
        LOG_F(ERROR, "AudioDecoder: OGG decode failed: %s", path.c_str());
        free(raw);  // NOLINT(cppcoreguidelines-no-malloc)
        return {};
    }

    DecodedAudio result;
    result.channels    = channels;
    result.sample_rate = sample_rate;
    result.pcm.assign(raw, raw + sample_count);
    free(raw);  // NOLINT(cppcoreguidelines-no-malloc)
    return result;
}

#ifdef HAVE_OPUSFILE
DecodedAudio DecodeOpus(const std::filesystem::path& path) {
    int error = 0;
    OggOpusFile* of = op_open_file(path.c_str(), &error);
    if (!of) {
        LOG_F(ERROR, "AudioDecoder: OPUS open failed (%d): %s",
              error, path.c_str());
        return {};
    }

    const int channels  = op_channel_count(of, -1);
    const int frequency = 48000;  // opusfile always outputs 48 kHz
    std::vector<int16_t> pcm;

    static constexpr int kReadSamples = 4096;
    opus_int16 buf[kReadSamples];

    int n;
    while ((n = op_read(of, buf, kReadSamples, nullptr)) > 0) {
        pcm.insert(pcm.end(), buf, buf + n * channels);
    }
    op_free(of);

    if (n < 0) {
        LOG_F(ERROR, "AudioDecoder: OPUS read error (%d): %s",
              n, path.c_str());
        return {};
    }

    DecodedAudio result;
    result.channels    = channels;
    result.sample_rate = frequency;
    result.pcm         = std::move(pcm);
    return result;
}
#endif

// Downmixes interleaved multi-channel PCM to mono by averaging all channels.
// OpenAL only applies 3D distance attenuation to mono sources; stereo buffers
// are rendered flat regardless of position.
void DownmixToMono(DecodedAudio& audio) {
    const size_t frames = audio.pcm.size() / static_cast<size_t>(audio.channels);
    std::vector<int16_t> mono(frames);
    for (size_t i = 0; i < frames; ++i) {
        int32_t sum = 0;
        for (int c = 0; c < audio.channels; ++c)
            sum += audio.pcm[i * static_cast<size_t>(audio.channels) + c];
        mono[i] = static_cast<int16_t>(sum / audio.channels);
    }
    audio.pcm      = std::move(mono);
    audio.channels = 1;
}

}  // namespace

DecodedAudio DecodeAudioFile(const std::filesystem::path& path) {
    const std::string ext = path.extension().string();

    DecodedAudio result;
    if (ext == ".wav") {
        result = DecodeWav(path);
    } else if (ext == ".mp3") {
        result = DecodeMp3(path);
    } else if (ext == ".flac") {
        result = DecodeFlac(path);
    } else if (ext == ".ogg") {
        result = DecodeOgg(path);
#ifdef HAVE_OPUSFILE
    } else if (ext == ".opus") {
        result = DecodeOpus(path);
#endif
    } else {
        LOG_F(ERROR, "AudioDecoder: unsupported format '%s': %s",
              ext.c_str(), path.c_str());
        return {};
    }

    if (result.channels > 1) {
        LOG_F(INFO, "AudioDecoder: downmixing %d-ch to mono: %s",
              result.channels, path.c_str());
        DownmixToMono(result);
    }
    return result;
}

abstract::AudioFormat AudioFormatFromChannels(int channels) {
    return (channels == 2) ? abstract::AudioFormat::kStereo16
                           : abstract::AudioFormat::kMono16;
}

}  // namespace audio
