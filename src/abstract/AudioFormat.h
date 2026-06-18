#pragma once

namespace abstract {

// PCM sample layout passed to ISoundSystem::CreateBuffer().
//
// The number suffix denotes bits per sample; the prefix denotes channel count.
enum class AudioFormat {
  kMono8,     // 8-bit unsigned PCM, 1 channel
  kMono16,    // 16-bit signed PCM, 1 channel (most common)
  kStereo8,   // 8-bit unsigned PCM, 2 channels (interleaved L/R)
  kStereo16,  // 16-bit signed PCM, 2 channels (interleaved L/R)
};

}  // namespace abstract
