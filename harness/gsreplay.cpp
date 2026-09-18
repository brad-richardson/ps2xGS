// gsreplay: drive a backend from a .gscap capture and diff every
// Present against the recorded reference (pixels per-channel, VRAM
// byte-exact). Prints one row per present; exits non-zero on any
// failing row.
//
// Usage:
//   gsreplay <capture.gscap> --backend cpu [--repeat N] [--threshold 4]
//       [--max-bad-pct 1.0] [--json out.json]

#include "backend_factory.h"
#include "gscap.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

struct Options
{
    std::string capture;
    std::string backend = "cpu";
    int repeat = 1;
    int threshold = 4;
    double maxBadPct = 1.0;
    std::string jsonPath;
};

Options parseArgs(int argc, char **argv)
{
    Options opts;
    if (argc < 2)
        throw std::runtime_error("usage: gsreplay <capture.gscap> --backend cpu "
                                 "[--repeat N] [--threshold 4] [--max-bad-pct 1.0] "
                                 "[--json out.json]");
    opts.capture = argv[1];
    for (int i = 2; i < argc; ++i)
    {
        const std::string arg = argv[i];
        auto needValue = [&](const char *flag) -> std::string
        {
            if (i + 1 >= argc)
                throw std::runtime_error(std::string("missing value for ") + flag);
            return argv[++i];
        };
        if (arg == "--backend")
            opts.backend = needValue("--backend");
        else if (arg == "--repeat")
            opts.repeat = std::stoi(needValue("--repeat"));
        else if (arg == "--threshold")
            opts.threshold = std::stoi(needValue("--threshold"));
        else if (arg == "--max-bad-pct")
            opts.maxBadPct = std::stod(needValue("--max-bad-pct"));
        else if (arg == "--json")
            opts.jsonPath = needValue("--json");
        else
            throw std::runtime_error("unknown flag '" + arg + "'");
    }
    if (opts.repeat < 1)
        throw std::runtime_error("--repeat must be >= 1");
    return opts;
}

struct PresentRow
{
    int index = 0;
    uint32_t width = 0;
    uint32_t height = 0;
    int maxDiff = 0;
    double meanDiff = 0.0;
    double badPct = 0.0;
    bool vramExact = false;
    double medianMs = 0.0;
    bool pass = false;
};

double medianOf(std::vector<double> v)
{
    if (v.empty())
        return 0.0;
    std::sort(v.begin(), v.end());
    const size_t n = v.size();
    if (n % 2 == 1)
        return v[n / 2];
    return 0.5 * (v[n / 2 - 1] + v[n / 2]);
}

void compareFrames(const PresentationFrame &got, const PresentationFrame &ref, int threshold,
                   int &maxDiff, double &meanDiff, double &badPct)
{
    maxDiff = 0;
    meanDiff = 0.0;
    badPct = 0.0;
    if (got.pixels.size() != ref.pixels.size() || got.pixels.empty())
        return;
    long long sum = 0;
    long long bad = 0;
    const size_t n = ref.pixels.size();
    for (size_t k = 0; k < n; ++k)
    {
        const int d = std::abs(static_cast<int>(got.pixels[k]) - static_cast<int>(ref.pixels[k]));
        maxDiff = std::max(maxDiff, d);
        sum += d;
        if (d > threshold)
            ++bad;
    }
    meanDiff = static_cast<double>(sum) / static_cast<double>(n);
    badPct = 100.0 * static_cast<double>(bad) / static_cast<double>(n);
}

[[noreturn]] void replayFail(const std::string &msg)
{
    throw std::runtime_error("replay mismatch: " + msg);
}

} // namespace

int main(int argc, char **argv)
{
    try
    {
        const Options opts = parseArgs(argc, argv);
        gscap::Reader reader(opts.capture);
        std::unique_ptr<GSRasterBackend> backend = createRasterBackend(opts.backend);

        std::vector<uint8_t> vram(reader.vramSize(), 0);
        bool initialized = false;
        std::vector<PresentRow> rows;
        int presentIndex = 0;
        int failures = 0;

        for (size_t i = 0; i < reader.recordCount(); ++i)
        {
            const uint32_t tag = reader.recordAt(i).tag;
            switch (tag)
            {
            case gscap::kInitialize:
            {
                uint32_t size = 0;
                reader.getInitialize(i, size);
                vram.assign(size, 0);
                backend->Initialize(vram.data(), size);
                initialized = true;
                break;
            }
            case gscap::kReset:
                backend->Reset();
                break;
            case gscap::kSubmit:
            {
                GSPrimitiveBatch batch{};
                reader.getSubmit(i, batch);
                backend->Submit(batch);
                break;
            }
            case gscap::kBeginTransfer:
            {
                GSTransferCommand cmd{};
                reader.getBeginTransfer(i, cmd);
                backend->BeginTransfer(cmd);
                break;
            }
            case gscap::kUploadImage:
            {
                std::vector<uint8_t> data;
                reader.getUpload(i, data);
                backend->UploadImage(data.data(), static_cast<uint32_t>(data.size()));
                break;
            }
            case gscap::kFlush:
                backend->Flush();
                break;
            case gscap::kTextureFlush:
                backend->TextureFlush();
                break;
            case gscap::kSync:
            {
                GSSyncReason reason = GSSyncReason::Finish;
                reader.getSync(i, reason);
                backend->Sync(reason);
                break;
            }
            case gscap::kPresent:
            {
                if (!initialized)
                    replayFail("Present before Initialize");
                GSPresentationRequest request{};
                PresentationFrame ref{};
                std::vector<uint8_t> refVram;
                reader.getPresent(i, request, ref, refVram);

                std::vector<double> timings;
                PresentationFrame got;
                for (int r = 0; r < opts.repeat; ++r)
                {
                    const auto t0 = std::chrono::steady_clock::now();
                    got = backend->Present(request);
                    const auto t1 = std::chrono::steady_clock::now();
                    timings.push_back(
                        std::chrono::duration<double, std::milli>(t1 - t0).count());
                }
                std::vector<uint8_t> gotVram;
                backend->SnapshotVram(gotVram);

                PresentRow row;
                row.index = presentIndex++;
                row.medianMs = medianOf(timings);
                row.vramExact = (gotVram == refVram);

                const bool gotEmpty = !static_cast<bool>(got);
                const bool refEmpty = !static_cast<bool>(ref);
                if (gotEmpty && refEmpty)
                {
                    row.pass = row.vramExact;
                }
                else if (gotEmpty != refEmpty || got.width != ref.width ||
                         got.height != ref.height || got.pixels.size() != ref.pixels.size())
                {
                    row.width = got.width;
                    row.height = got.height;
                    row.maxDiff = 255;
                    row.pass = false;
                }
                else
                {
                    row.width = got.width;
                    row.height = got.height;
                    compareFrames(got, ref, opts.threshold, row.maxDiff, row.meanDiff,
                                  row.badPct);
                    row.pass = (row.badPct <= opts.maxBadPct) && row.vramExact;
                }
                if (!row.pass)
                    ++failures;
                rows.push_back(row);
                std::printf("present %d %ux%u max=%d mean=%.4f bad=%.4f%% vram=%s "
                            "median_ms=%.3f %s\n",
                            row.index, row.width, row.height, row.maxDiff, row.meanDiff,
                            row.badPct, row.vramExact ? "yes" : "NO", row.medianMs,
                            row.pass ? "PASS" : "FAIL");
                break;
            }
            case gscap::kClearFramebuffer:
            {
                GSContext ctx{};
                uint32_t rgba = 0;
                bool expected = false;
                reader.getClear(i, ctx, rgba, expected);
                if (backend->ClearFramebuffer(ctx, rgba) != expected)
                    replayFail("ClearFramebuffer result diverged");
                break;
            }
            case gscap::kConsumeLocalToHost:
            {
                uint32_t maxBytes = 0;
                std::vector<uint8_t> expected;
                reader.getConsume(i, maxBytes, expected);
                std::vector<uint8_t> tmp(maxBytes);
                const uint32_t count = backend->ConsumeLocalToHostBytes(
                    tmp.data(), maxBytes);
                tmp.resize(count);
                if (tmp != expected)
                    replayFail("ConsumeLocalToHostBytes diverged");
                break;
            }
            case gscap::kReadVram:
            {
                uint32_t args[5] = {};
                uint32_t expected = 0;
                reader.getReadVram(i, args, expected);
                if (backend->ReadVram(args[0], args[1], args[2], args[3], args[4]) !=
                    expected)
                    replayFail("ReadVram result diverged");
                break;
            }
            case gscap::kWriteVram:
            {
                uint32_t args[6] = {};
                reader.getWriteVram(i, args);
                backend->WriteVram(args[0], args[1], args[2], args[3], args[4], args[5]);
                break;
            }
            case gscap::kSnapshotVram:
            {
                std::vector<uint8_t> expected;
                reader.getSnapshot(i, expected);
                std::vector<uint8_t> got;
                backend->SnapshotVram(got);
                if (got != expected)
                    replayFail("SnapshotVram diverged");
                break;
            }
            case gscap::kTransferSnapshot:
            {
                GSTransferSnapshot expected{};
                reader.getTransferSnapshot(i, expected);
                const GSTransferSnapshot got = backend->GetTransferSnapshot();
                if (got.x != expected.x || got.y != expected.y ||
                    got.totalPixels != expected.totalPixels ||
                    got.copiedPixels != expected.copiedPixels ||
                    got.direction != expected.direction ||
                    got.localToHostPendingBytes != expected.localToHostPendingBytes)
                    replayFail("GetTransferSnapshot diverged");
                break;
            }
            default:
                replayFail("unknown record tag " + std::to_string(tag));
            }
        }

        if (!opts.jsonPath.empty())
        {
            std::ostringstream json;
            json << "{\"capture\":\"" << opts.capture << "\",\"backend\":\"" << opts.backend
                 << "\",\"threshold\":" << opts.threshold << ",\"max_bad_pct\":" << opts.maxBadPct
                 << ",\"presents\":[";
            for (size_t k = 0; k < rows.size(); ++k)
            {
                const PresentRow &r = rows[k];
                if (k > 0)
                    json << ",";
                json << "{\"index\":" << r.index << ",\"width\":" << r.width
                     << ",\"height\":" << r.height << ",\"max_diff\":" << r.maxDiff
                     << ",\"mean_diff\":" << r.meanDiff << ",\"bad_pct\":" << r.badPct
                     << ",\"vram_exact\":" << (r.vramExact ? "true" : "false")
                     << ",\"median_ms\":" << r.medianMs
                     << ",\"pass\":" << (r.pass ? "true" : "false") << "}";
            }
            json << "],\"failures\":" << failures << "}";
            std::ofstream os(opts.jsonPath, std::ios::binary | std::ios::trunc);
            if (!os)
                throw std::runtime_error("cannot open --json path");
            os << json.str() << "\n";
        }

        std::printf("gsreplay: %zu presents, %d failures\n", rows.size(), failures);
        return failures == 0 ? 0 : 1;
    }
    catch (const std::exception &e)
    {
        std::fprintf(stderr, "gsreplay: error: %s\n", e.what());
        return 2;
    }
}
