// CLI tool: convert PNG/JPEG/TGA/BMP to BCn-compressed DDS files with pre-baked
// mipmaps.  Optionally patches material YAML files referencing the converted
// textures.
//
// Usage:
//   texture_compressor [--format <bc1|bc3|bc4|bc5|bc7>] [--delete] <file> [...]
//
// Format auto-detection (when --format is omitted):
//   *_normal* / *_nrm*  -> BC5  (dual-channel normal map)
//   single-channel image -> BC4
//   image has alpha      -> BC3
//   default              -> BC1

// --- Single-file library implementations -----------------------------------
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_RESIZE_IMPLEMENTATION
#define STB_DXT_IMPLEMENTATION

#include <stb_image.h>
#include <stb_image_resize2.h>
#include <stb_dxt.h>

// GLI / GLM — suppress the benign enum-conversion deprecation warning from GLI.
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-enum-enum-conversion"
#define GLM_ENABLE_EXPERIMENTAL
#include <gli/gli.hpp>
#pragma GCC diagnostic pop

// BC7 encoder (bc7enc library).
#include <bc7enc.h>

// YAML-cpp for material-file patching.
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <cassert>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace fs = std::filesystem;

// ---------------------------------------------------------------------------
// BCFormat
// ---------------------------------------------------------------------------
enum class BCFormat { kBC1, kBC3, kBC4, kBC5, kBC7 };

static const char* FormatName(BCFormat f) {
  switch (f) {
    case BCFormat::kBC1: return "BC1";
    case BCFormat::kBC3: return "BC3";
    case BCFormat::kBC4: return "BC4";
    case BCFormat::kBC5: return "BC5";
    case BCFormat::kBC7: return "BC7";
  }
  return "?";
}

static int BytesPerBlock(BCFormat f) {
  return (f == BCFormat::kBC1 || f == BCFormat::kBC4) ? 8 : 16;
}

static gli::format GliFormat(BCFormat f) {
  switch (f) {
    case BCFormat::kBC1: return gli::FORMAT_RGBA_DXT1_UNORM_BLOCK8;
    case BCFormat::kBC3: return gli::FORMAT_RGBA_DXT5_UNORM_BLOCK16;
    case BCFormat::kBC4: return gli::FORMAT_R_ATI1N_UNORM_BLOCK8;
    case BCFormat::kBC5: return gli::FORMAT_RG_ATI2N_UNORM_BLOCK16;
    case BCFormat::kBC7: return gli::FORMAT_RGBA_BP_UNORM_BLOCK16;
  }
  return gli::FORMAT_RGBA_DXT1_UNORM_BLOCK8;
}

// ---------------------------------------------------------------------------
// Format auto-detection
// ---------------------------------------------------------------------------
static BCFormat AutoDetect(const std::string& filename, int orig_channels,
                           const uint8_t* rgba, int w, int h) {
  auto lower = filename;
  std::transform(lower.begin(), lower.end(), lower.begin(),
                 [](unsigned char c){ return std::tolower(c); });

  if (lower.find("_normal") != std::string::npos ||
      lower.find("_nrm")    != std::string::npos) {
    return BCFormat::kBC5;
  }

  if (orig_channels == 1) return BCFormat::kBC4;

  if (orig_channels >= 4) {
    for (int i = 0; i < w * h; ++i) {
      if (rgba[i * 4 + 3] < 255) return BCFormat::kBC3;
    }
  }

  return BCFormat::kBC1;
}

// ---------------------------------------------------------------------------
// Mip-level count
// ---------------------------------------------------------------------------
static int MipCount(int w, int h) {
  int lvl = 1;
  while (w > 1 || h > 1) {
    w = std::max(1, w / 2);
    h = std::max(1, h / 2);
    ++lvl;
  }
  return lvl;
}

// ---------------------------------------------------------------------------
// Compress one 4×4 RGBA block
// ---------------------------------------------------------------------------
static void CompressBlock(uint8_t* dst, const uint8_t* rgba_block, BCFormat fmt,
                          const bc7enc_compress_block_params& bc7_params) {
  switch (fmt) {
    case BCFormat::kBC1:
      stb_compress_dxt_block(dst, rgba_block, 0, STB_DXT_NORMAL);
      break;

    case BCFormat::kBC3:
      stb_compress_dxt_block(dst, rgba_block, 1, STB_DXT_NORMAL);
      break;

    case BCFormat::kBC4: {
      // BC4 takes one byte per pixel (R channel only).
      uint8_t r[16];
      for (int i = 0; i < 16; ++i) r[i] = rgba_block[i * 4];
      stb_compress_bc4_block(dst, r);
      break;
    }

    case BCFormat::kBC5: {
      // BC5 takes two bytes per pixel (R then G), interleaved.
      uint8_t rg[32];
      for (int i = 0; i < 16; ++i) {
        rg[i * 2 + 0] = rgba_block[i * 4 + 0];
        rg[i * 2 + 1] = rgba_block[i * 4 + 1];
      }
      stb_compress_bc5_block(dst, rg);
      break;
    }

    case BCFormat::kBC7:
      bc7enc_compress_block(dst, rgba_block, &bc7_params);
      break;
  }
}

// ---------------------------------------------------------------------------
// Compress a full mip level
// ---------------------------------------------------------------------------
static std::vector<uint8_t> CompressMip(const uint8_t* pixels, int w, int h,
                                        BCFormat fmt,
                                        const bc7enc_compress_block_params& bc7p) {
  int bx  = (w + 3) / 4;
  int by  = (h + 3) / 4;
  int bpb = BytesPerBlock(fmt);
  std::vector<uint8_t> out(static_cast<size_t>(bx * by) * bpb);
  uint8_t* dst = out.data();

  for (int row = 0; row < by; ++row) {
    for (int col = 0; col < bx; ++col) {
      uint8_t block[64] = {};
      for (int py = 0; py < 4; ++py) {
        for (int px = 0; px < 4; ++px) {
          int sx = std::min(col * 4 + px, w - 1);
          int sy = std::min(row * 4 + py, h - 1);
          int bi = (py * 4 + px) * 4;
          int si = (sy * w + sx) * 4;
          block[bi + 0] = pixels[si + 0];
          block[bi + 1] = pixels[si + 1];
          block[bi + 2] = pixels[si + 2];
          block[bi + 3] = pixels[si + 3];
        }
      }
      CompressBlock(dst, block, fmt, bc7p);
      dst += bpb;
    }
  }
  return out;
}

// ---------------------------------------------------------------------------
// Walk the path upward to find 'textures/' and return (data_root, tex_rel).
// Returns false if the path is not under any 'textures/' directory.
// ---------------------------------------------------------------------------
static bool FindDataRoot(const fs::path& abs, fs::path& data_root,
                         fs::path& tex_rel) {
  fs::path rel;
  fs::path cur = abs;
  while (cur.has_parent_path() && cur != cur.parent_path()) {
    if (cur.filename() == "textures") {
      data_root = cur.parent_path();
      tex_rel   = rel;
      return true;
    }
    rel = (rel.empty() ? cur.filename() : cur.filename() / rel);
    cur = cur.parent_path();
  }
  return false;
}

// ---------------------------------------------------------------------------
// Validate a texture path found in a material file.
// Returns false (and prints a warning) if the path looks unsafe.
// ---------------------------------------------------------------------------
static bool ValidateTexPath(const std::string& path) {
  if (path.empty()) return false;
  // Reject absolute paths.
  if (fs::path(path).is_absolute()) {
    std::printf("  WARNING: skipping absolute texture path '%s'\n", path.c_str());
    return false;
  }
  // Reject paths with '..'.
  if (path.find("..") != std::string::npos) {
    std::printf("  WARNING: skipping unsafe texture path '%s'\n", path.c_str());
    return false;
  }
  return true;
}

// ---------------------------------------------------------------------------
// Patch one YAML material file: replace old_rel with new_rel under
// rendering.textures.*.
// ---------------------------------------------------------------------------
static bool PatchYaml(const fs::path& yaml_path, const std::string& old_rel,
                      const std::string& new_rel) {
  YAML::Node root;
  try {
    root = YAML::LoadFile(yaml_path.string());
  } catch (const YAML::Exception& e) {
    std::fprintf(stderr, "  WARNING: failed to parse '%s': %s\n",
                 yaml_path.string().c_str(), e.what());
    return false;
  }

  if (!root["rendering"] || !root["rendering"]["textures"]) return false;

  bool changed = false;
  YAML::Node textures = root["rendering"]["textures"];
  for (auto it = textures.begin(); it != textures.end(); ++it) {
    if (!it->second.IsScalar()) continue;
    const std::string val = it->second.as<std::string>();
    if (!ValidateTexPath(val)) continue;
    if (val == old_rel) {
      it->second = new_rel;
      changed = true;
    }
  }

  if (changed) {
    std::ofstream fout(yaml_path.string());
    if (!fout) {
      std::fprintf(stderr, "  WARNING: cannot write '%s'\n",
                   yaml_path.string().c_str());
      return false;
    }
    fout << root;
  }
  return changed;
}

// ---------------------------------------------------------------------------
// Scan data/materials/**/*.yaml and patch all references.
// ---------------------------------------------------------------------------
static void PatchMaterials(const fs::path& data_root, const fs::path& old_rel,
                           const fs::path& new_rel) {
  const fs::path mat_dir = data_root / "materials";
  if (!fs::is_directory(mat_dir)) return;

  const std::string old_str = old_rel.generic_string();
  const std::string new_str = new_rel.generic_string();

  for (const auto& entry : fs::recursive_directory_iterator(mat_dir)) {
    if (!entry.is_regular_file()) continue;
    if (entry.path().extension() != ".yaml") continue;
    if (PatchYaml(entry.path(), old_str, new_str)) {
      std::printf("  patched material: %s\n",
                  entry.path().filename().string().c_str());
    }
  }
}

// ---------------------------------------------------------------------------
// Convert one source image to a BCn-compressed DDS file.
// pixels: 4-channel RGBA data (loaded by caller).
// ---------------------------------------------------------------------------
static bool ConvertToDDS(const fs::path& src, const uint8_t* pixels, int w,
                         int h, BCFormat fmt, bool do_delete,
                         const bc7enc_compress_block_params& bc7p) {
  std::printf("[texture_compressor] %s -> %s  (%dx%d)\n",
              src.filename().string().c_str(), FormatName(fmt), w, h);

  // Build the GLI texture with the correct number of mip levels.
  int levels = MipCount(w, h);
  gli::texture2d tex(GliFormat(fmt),
                     gli::texture2d::extent_type(w, h),
                     static_cast<gli::texture2d::size_type>(levels));

  // Copy base level pixels into a working buffer (we resize in-place).
  std::vector<uint8_t> mip(static_cast<size_t>(w * h) * 4);
  std::memcpy(mip.data(), pixels, mip.size());

  int mw = w, mh = h;
  for (int lvl = 0; lvl < levels; ++lvl) {
    if (lvl > 0) {
      int nw = std::max(1, mw / 2);
      int nh = std::max(1, mh / 2);
      std::vector<uint8_t> resized(static_cast<size_t>(nw * nh) * 4);
      stbir_resize_uint8_linear(mip.data(), mw, mh, mw * 4,
                                resized.data(), nw, nh, nw * 4,
                                STBIR_RGBA);
      mip = std::move(resized);
      mw = nw;
      mh = nh;
    }

    auto compressed = CompressMip(mip.data(), mw, mh, fmt, bc7p);
    assert(compressed.size() == tex.size(lvl));
    std::memcpy(tex.data(0, 0, lvl), compressed.data(), compressed.size());
  }

  // Save DDS alongside the source (same directory, .dds extension).
  fs::path dst = src;
  dst.replace_extension(".dds");

  if (!gli::save_dds(tex, dst.string())) {
    std::fprintf(stderr, "  ERROR: GLI failed to save '%s'\n",
                 dst.string().c_str());
    return false;
  }
  std::printf("  -> %s  (%d mips)\n", dst.filename().string().c_str(), levels);

  // Patch material YAML files.
  const fs::path abs_src = fs::absolute(src);
  fs::path data_root, tex_rel;
  if (FindDataRoot(abs_src, data_root, tex_rel)) {
    fs::path new_rel = tex_rel;
    new_rel.replace_extension(".dds");
    PatchMaterials(data_root, tex_rel, new_rel);
  } else {
    std::printf("  (not under data/textures/ — skipping material patch)\n");
  }

  // Delete source on request.
  if (do_delete) {
    std::error_code ec;
    fs::remove(src, ec);
    if (ec) {
      std::fprintf(stderr, "  WARNING: could not delete '%s': %s\n",
                   src.string().c_str(), ec.message().c_str());
    } else {
      std::printf("  deleted source\n");
    }
  }

  return true;
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------
int main(int argc, char** argv) {
  if (argc < 2) {
    std::printf(
        "Usage: texture_compressor [--format <bc1|bc3|bc4|bc5|bc7>]"
        " [--delete] <file> [<file> ...]\n");
    return 1;
  }

  // Initialize the BC7 encoder once.
  bc7enc_compress_block_init();
  bc7enc_compress_block_params bc7p;
  bc7enc_compress_block_params_init(&bc7p);

  bool format_override = false;
  BCFormat forced_fmt  = BCFormat::kBC1;
  bool do_delete       = false;
  std::vector<std::string> files;

  for (int i = 1; i < argc; ++i) {
    std::string arg = argv[i];
    if (arg == "--format") {
      if (i + 1 >= argc) {
        std::fprintf(stderr, "ERROR: --format requires an argument\n");
        return 1;
      }
      ++i;
      std::string f = argv[i];
      if (f == "bc1") {
        forced_fmt = BCFormat::kBC1;
      } else if (f == "bc3") {
        forced_fmt = BCFormat::kBC3;
      } else if (f == "bc4") {
        forced_fmt = BCFormat::kBC4;
      } else if (f == "bc5") {
        forced_fmt = BCFormat::kBC5;
      } else if (f == "bc7") {
        forced_fmt = BCFormat::kBC7;
      } else {
        std::fprintf(stderr, "ERROR: unknown format '%s'\n", f.c_str());
        return 1;
      }
      format_override = true;
    } else if (arg == "--delete") {
      do_delete = true;
    } else {
      files.push_back(arg);
    }
  }

  if (files.empty()) {
    std::fprintf(stderr, "ERROR: no input files specified\n");
    return 1;
  }

  int ok = 0, failed = 0;
  for (const auto& file : files) {
    const fs::path src(file);
    if (!fs::is_regular_file(src)) {
      std::fprintf(stderr, "ERROR: '%s' is not a file\n", file.c_str());
      ++failed;
      continue;
    }

    // Load image (always as RGBA so all compressors get 4-channel input).
    int w = 0, h = 0, orig_channels = 0;
    stbi_set_flip_vertically_on_load(1);
    uint8_t* pixels = stbi_load(file.c_str(), &w, &h, &orig_channels, 4);
    if (!pixels) {
      std::fprintf(stderr, "ERROR: stb_image failed to load '%s'\n",
                   file.c_str());
      ++failed;
      continue;
    }

    BCFormat fmt = format_override
        ? forced_fmt
        : AutoDetect(src.filename().string(), orig_channels, pixels, w, h);

    bool success = ConvertToDDS(src, pixels, w, h, fmt, do_delete, bc7p);
    stbi_image_free(pixels);

    if (success) {
      ++ok;
    } else {
      ++failed;
    }
  }

  std::printf("\nDone: %d converted, %d failed.\n", ok, failed);
  return failed > 0 ? 1 : 0;
}
