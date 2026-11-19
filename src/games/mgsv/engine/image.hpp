// SPDX-License-Identifier: MIT
#ifndef RENODX_GAMES_MGSV_ENGINE_IMAGE_HPP_
#define RENODX_GAMES_MGSV_ENGINE_IMAGE_HPP_

#include <windows.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <format>
#include <fstream>
#include <ios>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include <include/reshade.hpp>

#include "build.hpp"

// Verifies every entry of engine/build.hpp once and hands out the addresses
// that verified. A feature asks for the entries it needs; if any is missing,
// that feature stays off and the others keep running.
namespace mgsv::engine {

struct Image {
  uint8_t* base = nullptr;
  bool known_build = false;
  // nullptr where the entry did not verify.
  std::array<uint8_t*, ENTRY_COUNT> addresses = {};
};

inline const Image& GetImage() {
  // The first caller verifies; any concurrent caller waits for that result.
  static const Image RUNNING_IMAGE = [] {
    Image result = {.base = reinterpret_cast<uint8_t*>(GetModuleHandleW(nullptr))};
    // Entries are checked in the executable file, not in the loaded image:
    // hooks this or another add-on has already installed change the code in
    // memory, and the probe add-on verifies after the mod has hooked.
    std::array<wchar_t, 32768> path = {};
    const DWORD path_length = GetModuleFileNameW(nullptr, path.data(), static_cast<DWORD>(path.size()));
    std::ifstream file(std::filesystem::path(std::wstring_view(path.data(), path_length)), std::ios::binary);
    const auto read = [&file](uint64_t offset, void* output, size_t size) {
      file.clear();
      file.seekg(static_cast<std::streamoff>(offset));
      file.read(static_cast<char*>(output), static_cast<std::streamsize>(size));
      return file.good();
    };
    IMAGE_DOS_HEADER dos = {};
    IMAGE_NT_HEADERS64 nt = {};
    if (result.base == nullptr || path_length == 0u || !read(0u, &dos, sizeof(dos)) || dos.e_magic != IMAGE_DOS_SIGNATURE
        || !read(static_cast<uint64_t>(dos.e_lfanew), &nt, sizeof(nt)) || nt.Signature != IMAGE_NT_SIGNATURE) {
      reshade::log::message(reshade::log::level::warning, "MgsvEngine: the game executable could not be read; features that hook the engine stay off.");
      return result;
    }
    result.known_build = nt.FileHeader.TimeDateStamp == IMAGE_TIMESTAMP && nt.OptionalHeader.SizeOfImage == IMAGE_SIZE;
    if (!result.known_build) {
      reshade::log::message(
          reshade::log::level::warning,
          std::format("MgsvEngine: this game executable (time stamp {:#x}, size {:#x}) is not the supported build; features that hook the engine stay off.",
                      nt.FileHeader.TimeDateStamp, nt.OptionalHeader.SizeOfImage)
              .c_str());
      return result;
    }
    std::vector<IMAGE_SECTION_HEADER> sections(nt.FileHeader.NumberOfSections);
    const auto section_table = static_cast<uint64_t>(dos.e_lfanew) + offsetof(IMAGE_NT_HEADERS64, OptionalHeader) + nt.FileHeader.SizeOfOptionalHeader;
    if (!read(section_table, sections.data(), sections.size() * sizeof(IMAGE_SECTION_HEADER))) {
      reshade::log::message(reshade::log::level::warning, "MgsvEngine: the game executable's sections could not be read; features that hook the engine stay off.");
      return result;
    }
    const auto read_at = [&](uint32_t rva, void* output, size_t size) {
      const auto section = std::ranges::find_if(sections, [rva, size](const IMAGE_SECTION_HEADER& candidate) {
        return rva >= candidate.VirtualAddress && rva + size <= candidate.VirtualAddress + candidate.SizeOfRawData;
      });
      return section != sections.end() && read(section->PointerToRawData + (rva - section->VirtualAddress), output, size);
    };

    std::array<bool, ENTRY_COUNT> verified = {};
    std::array<std::string_view, ENTRY_COUNT> failure = {};
    const auto index_of = [](Entry entry) { return static_cast<size_t>(entry); };
    for (const auto& info : ENTRIES) {
      std::vector<uint8_t> bytes((info.bytes.size() + 1u) / 3u);
      // A global in uninitialized data has no bytes in the file; a reference identifies it.
      bool matches = info.bytes.empty() || read_at(info.rva - info.bytes_before, bytes.data(), bytes.size());
      for (size_t position = 0u; position < bytes.size() && matches; ++position) {
        const auto token = info.bytes.substr(position * 3u, 2u);
        if (token == "??") continue;
        const auto digit = [](char value) { return static_cast<uint8_t>(value <= '9' ? value - '0' : value - 'A' + 10); };
        matches = bytes[position] == static_cast<uint8_t>((digit(token[0]) << 4u) | digit(token[1]));
      }
      verified[index_of(info.entry)] = matches;
      if (!matches) failure[index_of(info.entry)] = "its bytes differ";
    }

    // An entry without bytes of its own is identified only by a verified
    // entry that refers to it.
    std::array<bool, ENTRY_COUNT> confirmed = {};
    for (const auto& reference : REFERENCES) {
      const auto source = index_of(reference.source);
      const auto target = index_of(reference.target);
      const uint32_t instruction = ENTRIES[source].rva + reference.instruction;
      int32_t displacement = 0;
      if (verified[source] && read_at(instruction + reference.displacement, &displacement, sizeof(displacement))
          && instruction + reference.length + displacement == ENTRIES[target].rva) {
        confirmed[target] = true;
        continue;
      }
      for (const auto entry : {source, target}) {
        if (verified[entry]) failure[entry] = "a reference between entries does not hold";
        verified[entry] = false;
      }
    }
    // A slot in the file holds its target's address at the preferred base.
    for (const auto& slot : SLOTS) {
      const auto table = index_of(slot.slot);
      const auto target = index_of(slot.target);
      uint64_t value = 0u;
      if (verified[target] && read_at(ENTRIES[table].rva, &value, sizeof(value))
          && value == nt.OptionalHeader.ImageBase + ENTRIES[target].rva) {
        confirmed[table] = true;
        continue;
      }
      for (const auto entry : {table, target}) {
        if (verified[entry]) failure[entry] = "its virtual table slot does not hold";
        verified[entry] = false;
      }
    }
    for (const auto& info : ENTRIES) {
      const auto entry = index_of(info.entry);
      if (info.bytes.empty() && verified[entry] && !confirmed[entry]) {
        verified[entry] = false;
        failure[entry] = "no verified entry refers to it";
      }
    }

    size_t verified_count = 0u;
    for (const auto& info : ENTRIES) {
      const auto entry = index_of(info.entry);
      if (verified[entry]) {
        result.addresses[entry] = result.base + info.rva;
        ++verified_count;
        continue;
      }
      reshade::log::message(reshade::log::level::warning,
                            std::format("MgsvEngine: {} (RVA {:#x}) failed verification: {}.", info.name, info.rva, failure[entry]).c_str());
    }
    reshade::log::message(
        verified_count == ENTRY_COUNT ? reshade::log::level::info : reshade::log::level::warning,
        std::format("MgsvEngine: verified {} of {} engine entries in the supported executable.", verified_count, ENTRY_COUNT).c_str());
    return result;
  }();
  return RUNNING_IMAGE;
}

// nullptr if the entry did not verify.
inline uint8_t* Address(Entry entry) {
  return GetImage().addresses[static_cast<size_t>(entry)];
}

inline bool Verified(std::span<const Entry> entries) {
  return std::ranges::all_of(entries, [](Entry entry) { return Address(entry) != nullptr; });
}

}  // namespace mgsv::engine

#endif  // RENODX_GAMES_MGSV_ENGINE_IMAGE_HPP_
