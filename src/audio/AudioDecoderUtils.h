#pragma once

#include <filesystem>
#include <vector>

#include "abstract/AudioFormat.h"

namespace audio {

// PCM audio decoded from a file, ready for upload to an ISoundSystem backend.
struct DecodedAudio {
  // Interleaved signed 16-bit PCM samples (L, R, L, R, … for stereo).
  // cppcheck-suppress unusedStructMember
  std::vector<int16_t> pcm;
  // cppcheck-suppress unusedStructMember
  int channels    = 0;
  // cppcheck-suppress unusedStructMember
  int sample_rate = 0;
};

// Decodes an audio file at path to signed 16-bit interleaved PCM.
//
// Supported formats determined at compile time:
//   WAV  — always via dr_wav
//   MP3  — always via dr_mp3
//   FLAC — always via dr_flac
//   OGG  — always via stb_vorbis
//   OPUS — only when built with HAVE_OPUSFILE
//
// Returns an empty DecodedAudio (channels == 0) on any error.
DecodedAudio DecodeAudioFile(const std::filesystem::path& path);

// Returns the 16-bit AudioFormat matching the given channel count.
// Any value other than 2 maps to kMono16.
abstract::AudioFormat AudioFormatFromChannels(int channels);

}  // namespace audio
