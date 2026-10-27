// Reads an original QuadraVerb EPROM image (27C512, 64 KB) and extracts the factory programs.
// The EPROM is wired with address lines A0..A7 reversed on the PCB; we undo that, then look for
// the table of 128-byte program records (same layout as the SysEx data dump).
#pragma once
#include "Params.h"
#include <cstdint>

namespace qv::rom
{
std::vector<uint8_t> descramble (const std::vector<uint8_t>& raw);
// Returns the factory programs (90 for v1.0x) or empty if the image is not recognised.
std::vector<Program> extractPrograms (const std::vector<uint8_t>& raw, std::string& info);
} // namespace qv::rom
