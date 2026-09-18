#pragma once

// Small encoder/decoder helpers for the GS registers that cross the
// GSRasterBackend seam as raw uint64_t (GSDrawState.context.tex1 /
// clamp / alpha / test / fba, GSDrawState.scanmsk / dimx / dthe /
// colclamp, GSPresentationRequest.pmode / smode2 / dispfb* / display*).
// Bit layouts match the CPU backend's decoders in
// upstream/ps2xRuntime/src/lib/gs/gs_cpu_backend.cpp, which is the
// reference for G0. Where the CPU backend ignores a register (TEX1,
// DIMX, DTHE, COLCLAMP, SCANMSK), the helpers still round-trip so the
// generator can emit distinct values and the census can report them.

#include <array>
#include <cstdint>

namespace gsregs
{

// ---- TEST ----
// ATE bit0, ATST bits1-3, AREF bits4-11, AFAIL bits12-13, DATE bit14,
// DATM bit15, ZTE bit16, ZTST bits17-18.
struct TestReg
{
    bool ate = false;
    uint8_t atst = 1; // 0 NEVER,1 ALWAYS,2 LESS,3 LEQUAL,4 EQUAL,5 GEQUAL,6 GREATER,7 NOTEQUAL
    uint8_t aref = 0;
    uint8_t afail = 0; // 0 KEEP,1 FB_ONLY,2 ZB_ONLY,3 RGB_ONLY
    bool date = false;
    bool datm = false;
    bool zte = false;
    uint8_t ztst = 1; // 0 NEVER,1 ALWAYS,2 GEQUAL,3 GREATER

    uint64_t encode() const
    {
        return (ate ? 1ull : 0ull) | (uint64_t(atst & 7u) << 1) | (uint64_t(aref) << 4) |
               (uint64_t(afail & 3u) << 12) | (date ? (1ull << 14) : 0ull) |
               (datm ? (1ull << 15) : 0ull) | (zte ? (1ull << 16) : 0ull) |
               (uint64_t(ztst & 3u) << 17);
    }
    static TestReg decode(uint64_t v)
    {
        TestReg r;
        r.ate = (v & 1ull) != 0;
        r.atst = (v >> 1) & 7u;
        r.aref = (v >> 4) & 0xFFu;
        r.afail = (v >> 12) & 3u;
        r.date = ((v >> 14) & 1ull) != 0;
        r.datm = ((v >> 15) & 1ull) != 0;
        r.zte = ((v >> 16) & 1ull) != 0;
        r.ztst = (v >> 17) & 3u;
        return r;
    }
    bool operator==(const TestReg &o) const
    {
        return encode() == o.encode();
    }
};

inline constexpr uint8_t kAtstNever = 0, kAtstAlways = 1, kAtstLess = 2, kAtstLequal = 3,
                         kAtstEqual = 4, kAtstGequal = 5, kAtstGreater = 6, kAtstNotequal = 7;
inline constexpr uint8_t kAfailKeep = 0, kAfailFbOnly = 1, kAfailZbOnly = 2, kAfailRgbOnly = 3;
inline constexpr uint8_t kZtstNever = 0, kZtstAlways = 1, kZtstGequal = 2, kZtstGreater = 3;

// ---- ALPHA ----
// A bits0-1, B bits2-3, C bits4-5, D bits6-7, FIX bits32-39.
// A/B/D: 0 Cs, 1 Cd, 2 zero. C: 0 As, 1 Ad, 2 FIX.
struct AlphaReg
{
    uint8_t a = 0;
    uint8_t b = 1;
    uint8_t c = 0;
    uint8_t d = 1;
    uint8_t fix = 0x80;

    uint64_t encode() const
    {
        return uint64_t(a & 3u) | (uint64_t(b & 3u) << 2) | (uint64_t(c & 3u) << 4) |
               (uint64_t(d & 3u) << 6) | (uint64_t(fix) << 32);
    }
    static AlphaReg decode(uint64_t v)
    {
        AlphaReg r;
        r.a = v & 3u;
        r.b = (v >> 2) & 3u;
        r.c = (v >> 4) & 3u;
        r.d = (v >> 6) & 3u;
        r.fix = (v >> 32) & 0xFFu;
        return r;
    }
    bool operator==(const AlphaReg &o) const
    {
        return encode() == o.encode();
    }
};

// ---- CLAMP ----
// WMS bits0-1, WMT bits2-3, MINU bits4-13, MAXU bits14-23,
// MINV bits24-33, MAXV bits34-43. Wrap modes: 0 REPEAT, 1 CLAMP,
// 2 REGION_CLAMP, 3 REGION_REPEAT.
struct ClampReg
{
    uint8_t wms = 0;
    uint8_t wmt = 0;
    uint16_t minu = 0;
    uint16_t maxu = 0;
    uint16_t minv = 0;
    uint16_t maxv = 0;

    uint64_t encode() const
    {
        return uint64_t(wms & 3u) | (uint64_t(wmt & 3u) << 2) | (uint64_t(minu & 0x3FFu) << 4) |
               (uint64_t(maxu & 0x3FFu) << 14) | (uint64_t(minv & 0x3FFu) << 24) |
               (uint64_t(maxv & 0x3FFu) << 34);
    }
    static ClampReg decode(uint64_t v)
    {
        ClampReg r;
        r.wms = v & 3u;
        r.wmt = (v >> 2) & 3u;
        r.minu = (v >> 4) & 0x3FFu;
        r.maxu = (v >> 14) & 0x3FFu;
        r.minv = (v >> 24) & 0x3FFu;
        r.maxv = (v >> 34) & 0x3FFu;
        return r;
    }
    bool operator==(const ClampReg &o) const
    {
        return encode() == o.encode();
    }
};

inline constexpr uint8_t kWrapRepeat = 0, kWrapClamp = 1, kWrapRegionClamp = 2,
                         kWrapRegionRepeat = 3;

// ---- TEX1 ----
// Carried verbatim; the CPU backend ignores it. Layout: LCM bit0,
// MXL bits2-4, MMAG bit5, MMIN bit6, MTBA bit9, L bits10-11,
// K bits12-23.
struct Tex1Reg
{
    bool lcm = false;
    uint8_t mxl = 0;
    bool mmag = false;
    bool mmin = false;
    bool mtba = false;
    uint8_t l = 0;
    uint16_t k = 0;

    uint64_t encode() const
    {
        return (lcm ? 1ull : 0ull) | (uint64_t(mxl & 7u) << 2) | (mmag ? (1ull << 5) : 0ull) |
               (mmin ? (1ull << 6) : 0ull) | (mtba ? (1ull << 9) : 0ull) |
               (uint64_t(l & 3u) << 10) | (uint64_t(k & 0xFFFu) << 12);
    }
    static Tex1Reg decode(uint64_t v)
    {
        Tex1Reg r;
        r.lcm = (v & 1ull) != 0;
        r.mxl = (v >> 2) & 7u;
        r.mmag = ((v >> 5) & 1ull) != 0;
        r.mmin = ((v >> 6) & 1ull) != 0;
        r.mtba = ((v >> 9) & 1ull) != 0;
        r.l = (v >> 10) & 3u;
        r.k = (v >> 12) & 0xFFFu;
        return r;
    }
    bool operator==(const Tex1Reg &o) const
    {
        return encode() == o.encode();
    }
};

// ---- TEXA ----
// TA0 bits0-7, AEM bit15, TA1 bits32-39. (The draw state carries the
// decoded struct; this helper covers raw-word usage and the census.)
struct TexaReg
{
    uint8_t ta0 = 0;
    bool aem = false;
    uint8_t ta1 = 0;

    uint64_t encode() const
    {
        return uint64_t(ta0) | (aem ? (1ull << 15) : 0ull) | (uint64_t(ta1) << 32);
    }
    static TexaReg decode(uint64_t v)
    {
        TexaReg r;
        r.ta0 = v & 0xFFu;
        r.aem = ((v >> 15) & 1ull) != 0;
        r.ta1 = (v >> 32) & 0xFFu;
        return r;
    }
    bool operator==(const TexaReg &o) const
    {
        return encode() == o.encode();
    }
};

// ---- FOGCOL ----
// FCR bits0-7, FCG bits8-15, FCB bits16-23. (Draw state carries the
// decoded bytes; helper is for raw words and the census.)
struct FogColReg
{
    uint8_t fcr = 0;
    uint8_t fcg = 0;
    uint8_t fcb = 0;

    uint64_t encode() const
    {
        return uint64_t(fcr) | (uint64_t(fcg) << 8) | (uint64_t(fcb) << 16);
    }
    static FogColReg decode(uint64_t v)
    {
        FogColReg r;
        r.fcr = v & 0xFFu;
        r.fcg = (v >> 8) & 0xFFu;
        r.fcb = (v >> 16) & 0xFFu;
        return r;
    }
    bool operator==(const FogColReg &o) const
    {
        return encode() == o.encode();
    }
};

// ---- DIMX ----
// 4x4 dither matrix, 3 bits per entry at bit (y*4+x)*3.
// Carried verbatim; the CPU backend ignores it.
struct DimxReg
{
    std::array<std::array<uint8_t, 4>, 4> m{};

    uint64_t encode() const
    {
        uint64_t v = 0;
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x)
                v |= uint64_t(m[y][x] & 7u) << ((y * 4 + x) * 3);
        return v;
    }
    static DimxReg decode(uint64_t v)
    {
        DimxReg r;
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x)
                r.m[y][x] = (v >> ((y * 4 + x) * 3)) & 7u;
        return r;
    }
    bool operator==(const DimxReg &o) const
    {
        return encode() == o.encode();
    }
};

// ---- PMODE ----
// EN1 bit0, EN2 bit1, MMOD bit5, AMOD bit6, SLBG bit7, ALP bits8-15.
struct PmodeReg
{
    bool en1 = true;
    bool en2 = false;
    bool mmod = false;
    bool amod = false;
    bool slbg = false;
    uint8_t alp = 0;

    uint64_t encode() const
    {
        return (en1 ? 1ull : 0ull) | (en2 ? 2ull : 0ull) | (mmod ? (1ull << 5) : 0ull) |
               (amod ? (1ull << 6) : 0ull) | (slbg ? (1ull << 7) : 0ull) |
               (uint64_t(alp) << 8);
    }
    static PmodeReg decode(uint64_t v)
    {
        PmodeReg r;
        r.en1 = (v & 1ull) != 0;
        r.en2 = ((v >> 1) & 1ull) != 0;
        r.mmod = ((v >> 5) & 1ull) != 0;
        r.amod = ((v >> 6) & 1ull) != 0;
        r.slbg = ((v >> 7) & 1ull) != 0;
        r.alp = (v >> 8) & 0xFFu;
        return r;
    }
    bool operator==(const PmodeReg &o) const
    {
        return encode() == o.encode();
    }
};

// ---- SMODE2 ----
// INT bit0 (interlaced), FFMD bit1 (frame mode when set).
struct Smode2Reg
{
    bool interlaced = false;
    bool frameMode = true;

    uint64_t encode() const
    {
        return (interlaced ? 1ull : 0ull) | (frameMode ? 2ull : 0ull);
    }
    static Smode2Reg decode(uint64_t v)
    {
        Smode2Reg r;
        r.interlaced = (v & 1ull) != 0;
        r.frameMode = ((v >> 1) & 1ull) != 0;
        return r;
    }
    bool operator==(const Smode2Reg &o) const
    {
        return encode() == o.encode();
    }
};

// ---- DISPFB ----
// FBP bits0-8, FBW bits9-14, PSM bits15-19, DBX bits32-42, DBY bits43-53.
struct DispfbReg
{
    uint16_t fbp = 0;
    uint8_t fbw = 10;
    uint8_t psm = 0;
    uint16_t dbx = 0;
    uint16_t dby = 0;

    uint64_t encode() const
    {
        return uint64_t(fbp & 0x1FFu) | (uint64_t(fbw & 0x3Fu) << 9) |
               (uint64_t(psm & 0x1Fu) << 15) | (uint64_t(dbx & 0x7FFu) << 32) |
               (uint64_t(dby & 0x7FFu) << 43);
    }
    static DispfbReg decode(uint64_t v)
    {
        DispfbReg r;
        r.fbp = v & 0x1FFu;
        r.fbw = (v >> 9) & 0x3Fu;
        r.psm = (v >> 15) & 0x1Fu;
        r.dbx = (v >> 32) & 0x7FFu;
        r.dby = (v >> 43) & 0x7FFu;
        return r;
    }
    bool operator==(const DispfbReg &o) const
    {
        return encode() == o.encode();
    }
};

// ---- DISPLAY ----
// DW bits32-43, DH bits44-54, MAGH bits23-26. Decoded width is
// (DW+1)/(MAGH+1), height DH+1.
struct DisplayReg
{
    uint16_t dw = 63;
    uint16_t dh = 63;
    uint8_t magh = 0;

    uint64_t encode() const
    {
        return (uint64_t(magh & 0xFu) << 23) | (uint64_t(dw & 0xFFFu) << 32) |
               (uint64_t(dh & 0x7FFu) << 44);
    }
    static DisplayReg decode(uint64_t v)
    {
        DisplayReg r;
        r.magh = (v >> 23) & 0xFu;
        r.dw = (v >> 32) & 0xFFFu;
        r.dh = (v >> 44) & 0x7FFu;
        return r;
    }
    bool operator==(const DisplayReg &o) const
    {
        return encode() == o.encode();
    }
};

} // namespace gsregs
