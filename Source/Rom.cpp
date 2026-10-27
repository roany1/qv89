#include "Rom.h"
#include "Sysex.h"
#include <cstring>

namespace qv::rom
{
static uint32_t reverseLow8 (uint32_t a)
{
    uint32_t lo = 0;
    for (int i = 0; i < 8; ++i) if (a & (1u << i)) lo |= 1u << (7 - i);
    return (a & ~0xffu) | lo;
}

std::vector<uint8_t> descramble (const std::vector<uint8_t>& raw)
{
    std::vector<uint8_t> out (raw.size());
    for (size_t a = 0; a < raw.size(); ++a) out[a] = raw[reverseLow8 ((uint32_t) a)];
    return out;
}

static bool looksLikeProgram (const uint8_t* r)
{
    if (r[0x44] > 7) return false;                    // configuration
    int letters = 0;
    for (int i = 0; i < 14; ++i)
    {
        const uint8_t c = r[0x6A + i];
        if (c < 32 || c > 126) return false;
        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')) ++letters;
    }
    return letters >= 3 && r[0x32] <= 4 && r[0x27] <= 3 && r[0x3A] <= 8;
}

std::vector<Program> extractPrograms (const std::vector<uint8_t>& raw, std::string& info)
{
    std::vector<Program> out;
    if (raw.size() != 65536) { info = "ROM image must be 64 KB (27C512)"; return out; }

    auto img = descramble (raw);
    const char* sig = "QUADRAVERB";
    bool signature = false;
    for (size_t i = 0; i + 10 < img.size() && ! signature; ++i) signature = std::memcmp (&img[i], sig, 10) == 0;
    if (! signature) { info = "not a QuadraVerb EPROM (signature not found after descrambling)"; return out; }

    // version string, e.g. "SOFTWARE VERSIONNUMBER: 1.07"
    const char* vs = "NUMBER: ";
    for (size_t i = 0; i + 12 < img.size(); ++i)
        if (std::memcmp (&img[i], vs, 8) == 0) { info = "QuadraVerb ROM v" + std::string ((const char*) &img[i + 8], 4); break; }

    // find the longest run of consecutive 128-byte program records
    size_t bestStart = 0, bestLen = 0;
    for (size_t s = 0; s + 128 <= img.size(); ++s)
    {
        if (! looksLikeProgram (&img[s])) continue;
        if (s >= 128 && looksLikeProgram (&img[s - 128])) continue;   // not the start of a run
        size_t n = 0;
        while (s + (n + 1) * 128 <= img.size() && looksLikeProgram (&img[s + n * 128])) ++n;
        if (n > bestLen) { bestLen = n; bestStart = s; }
    }
    if (bestLen < 10) { info += ": program table not found"; return out; }

    for (size_t k = 0; k < bestLen && k < 100; ++k)
    {
        Program p; std::string w;
        sysex::decodeProgram (&img[bestStart + k * 128], p, w);
        out.push_back (p);
    }
    info += ", " + std::to_string (out.size()) + " factory programs";
    return out;
}
} // namespace qv::rom
