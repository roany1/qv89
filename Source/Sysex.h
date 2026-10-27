// QuadraVerb SysEx (F0 00 00 0E 02 ...) import/export, following the
// "MIDI Implementation / System Exclusive" chapter of the QuadraVerb service manual.
#pragma once
#include "Params.h"
#include <cstdint>

namespace qv::sysex
{
std::vector<uint8_t> unpack7 (const uint8_t* d, size_t n);        // 8 MIDI bytes -> 7 data bytes (bitstream)
std::vector<uint8_t> pack7 (const std::vector<uint8_t>& bytes);

bool decodeProgram (const uint8_t* b /*128 bytes*/, Program& out, std::string& warnings);
std::vector<uint8_t> encodeProgram (const Program& p);           // 128 bytes

// Parse a whole .syx file (single dumps, edit-buffer dumps or 100-program banks).
std::vector<Program> parseFile (const std::vector<uint8_t>& file, std::string& log);
// Build a "MIDI Data Dump" message for one program (100 = edit buffer).
std::vector<uint8_t> makeDump (const Program& p, int programNumber = 100);
} // namespace qv::sysex
