// G3 step 1: pin the Present fbp==0 fallback heuristic (G0 §9 item 4).
// For each present-fbp0-* capture: replay the recorded Present request
// through a fresh GSCpuBackend initialized with the recorded VRAM,
// require the live frame to match the recorded reference byte-exact
// (pixels + sourceFbp), and require the recorded sourceFbp and spot
// pixels to equal the pinned expectations below. Fails if the
// heuristic changes. Usage: test_present_heuristic <synth-dir>

#include "gscap.h"
#include "runtime/gs/gs_cpu_backend.h"

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace
{

constexpr uint32_t kHostStride = 640u; // Present output row stride (pixels)

struct Expectation
{
    const char *name;
    uint32_t sourceFbp;
    uint8_t r, g, b; // expected RGB at every spot pixel
};

void pixelAt(const std::vector<uint8_t> &px, uint32_t x, uint32_t y,
             uint8_t &r, uint8_t &gg, uint8_t &b, uint8_t &a)
{
    const size_t o = (static_cast<size_t>(y) * kHostStride + x) * 4;
    r = px[o];
    gg = px[o + 1];
    b = px[o + 2];
    a = px[o + 3];
}

int checkCapture(const std::string &dir, const Expectation &exp)
{
    const std::string path = dir + "/" + exp.name + ".gscap";
    gscap::Reader reader(path);
    int presents = 0;
    int failures = 0;
    for (size_t i = 0; i < reader.recordCount(); ++i)
    {
        if (reader.recordAt(i).tag != gscap::kPresent)
            continue;
        ++presents;
        GSPresentationRequest request{};
        PresentationFrame ref{};
        std::vector<uint8_t> vram;
        reader.getPresent(i, request, ref, vram);

        // Live replay through a fresh cpu backend on the recorded VRAM.
        GSCpuBackend backend;
        backend.Initialize(vram.data(), static_cast<uint32_t>(vram.size()));
        const PresentationFrame got = backend.Present(request);

        bool ok = true;
        if (!static_cast<bool>(got) || !static_cast<bool>(ref) ||
            got.width != ref.width || got.height != ref.height ||
            got.pixels.size() != ref.pixels.size())
        {
            std::printf("FAIL %s: live frame shape diverged\n", exp.name);
            return 1;
        }
        int maxDiff = 0;
        for (size_t k = 0; k < ref.pixels.size(); ++k)
        {
            const int d = std::abs(static_cast<int>(got.pixels[k]) -
                                   static_cast<int>(ref.pixels[k]));
            if (d > maxDiff)
                maxDiff = d;
        }
        if (maxDiff != 0)
        {
            std::printf("FAIL %s: live-vs-recorded max=%d\n", exp.name, maxDiff);
            ok = false;
        }
        if (got.sourceFbp != ref.sourceFbp)
        {
            std::printf("FAIL %s: live sourceFbp=%u recorded=%u\n", exp.name,
                        got.sourceFbp, ref.sourceFbp);
            ok = false;
        }
        if (ref.sourceFbp != exp.sourceFbp)
        {
            std::printf("FAIL %s: recorded sourceFbp=%u pinned=%u\n", exp.name,
                        ref.sourceFbp, exp.sourceFbp);
            ok = false;
        }
        static const uint32_t kSpots[][2] = {{0, 0}, {63, 0}, {0, 63}, {63, 63}, {32, 32}};
        for (const auto &spot : kSpots)
        {
            uint8_t r, g, b, a;
            pixelAt(ref.pixels, spot[0], spot[1], r, g, b, a);
            if (r != exp.r || g != exp.g || b != exp.b || a != 255u)
            {
                std::printf("FAIL %s: pixel(%u,%u)=(%u,%u,%u,%u) pinned=(%u,%u,%u,255)\n",
                            exp.name, spot[0], spot[1], r, g, b, a, exp.r, exp.g, exp.b);
                ok = false;
            }
        }
        if (ok)
            std::printf("PASS %s: sourceFbp=%u rgb=(%u,%u,%u) max=%d\n", exp.name,
                        ref.sourceFbp, exp.r, exp.g, exp.b, maxDiff);
        else
            failures = 1;
    }
    if (presents != 1)
    {
        std::printf("FAIL %s: presents=%d (expected 1)\n", exp.name, presents);
        return 1;
    }
    return failures;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::fprintf(stderr, "usage: test_present_heuristic <synth-dir>\n");
        return 2;
    }
    const std::string dir = argv[1];
    // Pinned observations (see docs/reports/G3.md §1):
    // black -> falls back to contextFrames[0] (fbp 8, red);
    // nonblack -> no fallback (fbp 0, blue);
    // multi -> first non-black candidate wins ([0], fbp 8, red);
    // empty -> black frame, sourceFbp 0.
    static const Expectation kExpects[] = {
        {"present-fbp0-black", 8, 255, 0, 0},
        {"present-fbp0-nonblack", 0, 0, 0, 255},
        {"present-fbp0-multi", 8, 255, 0, 0},
        {"present-fbp0-empty", 0, 0, 0, 0},
    };
    int failures = 0;
    for (const Expectation &exp : kExpects)
        failures |= checkCapture(dir, exp);
    if (failures == 0)
        std::printf("test_present_heuristic: 4/4 pinned behaviors hold\n");
    return failures;
}
