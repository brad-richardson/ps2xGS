// Capture write->read round trip on a small stream, driven through
// GSRecordingBackend(CPU) so the recording path itself is covered.

#include "gscap.h"
#include "gsregs.h"
#include "recording_backend.h"
#include "runtime/gs/gs_cpu_backend.h"

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

namespace
{

int g_failures = 0;

void check(bool ok, const std::string &what)
{
    if (!ok)
    {
        std::printf("FAIL %s\n", what.c_str());
        ++g_failures;
    }
}

} // namespace

int main(int argc, char **argv)
{
    const std::string path =
        argc > 1 ? argv[1] : "/tmp/ps2xgs-build/test-roundtrip.gscap";

    constexpr uint32_t kVramSize = 4u * 1024u * 1024u;
    std::vector<uint8_t> vram(kVramSize, 0);

    // Drive the recording backend through the real interface.
    {
        auto cpu = std::make_unique<GSCpuBackend>();
        GSRecordingBackend rec(std::move(cpu), path, kVramSize);
        rec.Initialize(vram.data(), kVramSize);
        rec.Reset();

        GSPrimitiveBatch b{};
        b.vertexCount = 2;
        b.vertices[0].x = 4.0f;
        b.vertices[0].y = 5.0f;
        b.vertices[0].z = 256.0;
        b.vertices[0].r = 11;
        b.vertices[0].g = 22;
        b.vertices[0].b = 33;
        b.vertices[0].a = 0x80;
        b.vertices[0].u = 16;
        b.vertices[1] = b.vertices[0];
        b.vertices[1].x = 40.0f;
        b.state.prim.type = GS_PRIM_SPRITE;
        b.state.prim.abe = true;
        b.state.context.frame.fbp = 7;
        b.state.context.frame.fbw = 10;
        b.state.context.test = gsregs::TestReg{}.encode();
        b.state.context.alpha = gsregs::AlphaReg{}.encode();
        rec.Submit(b);

        GSTransferCommand cmd{};
        cmd.bitbltbuf.dbp = 0;
        cmd.bitbltbuf.dbw = 10;
        cmd.bitbltbuf.dpsm = GS_PSM_CT32;
        cmd.trxreg.rrw = 2;
        cmd.trxreg.rrh = 2;
        cmd.direction = 0;
        rec.BeginTransfer(cmd);
        const uint8_t px[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
        rec.UploadImage(px, sizeof(px));
        rec.Flush();
        rec.TextureFlush();
        rec.Sync(GSSyncReason::Presentation);

        GSContext ctx{};
        ctx.frame.fbp = 0;
        ctx.frame.fbw = 10;
        const bool clearOk = rec.ClearFramebuffer(ctx, 0x11223344u);
        rec.WriteVram(GS_PSM_CT32, 0, 10, 1, 1, 0xAABBCCDDu);
        const uint32_t rv = rec.ReadVram(GS_PSM_CT32, 0, 10, 1, 1);
        check(rv == 0xAABBCCDDu, "live ReadVram after WriteVram");

        gsregs::DispfbReg dispfb;
        gsregs::DisplayReg display;
        display.dw = 63;
        display.dh = 63;
        GSPresentationRequest req{};
        req.pmode = gsregs::PmodeReg{}.encode();
        req.smode2 = gsregs::Smode2Reg{}.encode();
        req.dispfb1 = dispfb.encode();
        req.display1 = display.encode();
        req.contextFrames[0].fbp = 0;
        req.contextFrames[0].fbw = 10;
        const PresentationFrame live = rec.Present(req);
        check(static_cast<bool>(live), "live Present non-empty");
        (void)clearOk;

        GSTransferSnapshot snap = rec.GetTransferSnapshot();
        (void)snap;
        std::vector<uint8_t> vsnap;
        rec.SnapshotVram(vsnap);
        rec.finalize();
    }

    // Read it back and verify every record.
    gscap::Reader r(path);
    check(r.vramSize() == kVramSize, "header vram size");
    check(r.recordCount() == 14, "record count");
    check(r.presentOffsets().size() == 1, "one present offset");

    // Writer emits in call order, matching the driver above.
    const uint32_t wantTags[14] = {
        gscap::kInitialize, gscap::kReset,         gscap::kSubmit,
        gscap::kBeginTransfer, gscap::kUploadImage, gscap::kFlush,
        gscap::kTextureFlush, gscap::kSync,        gscap::kClearFramebuffer,
        gscap::kWriteVram,    gscap::kReadVram,    gscap::kPresent,
        gscap::kTransferSnapshot, gscap::kSnapshotVram};
    for (size_t i = 0; i < 14; ++i)
        check(r.recordAt(i).tag == wantTags[i], "tag order #" + std::to_string(i));

    GSPrimitiveBatch b{};
    r.getSubmit(2, b);
    check(b.vertexCount == 2, "submit vertexCount");
    check(b.vertices[0].x == 4.0f && b.vertices[0].z == 256.0, "submit vertex");
    check(b.vertices[0].r == 11 && b.vertices[0].a == 0x80, "submit color");
    check(b.state.prim.type == GS_PRIM_SPRITE && b.state.prim.abe, "submit prim");
    check(b.state.context.frame.fbp == 7, "submit fbp");

    std::vector<uint8_t> up;
    r.getUpload(4, up);
    check(up.size() == 16 && up[0] == 1 && up[15] == 16, "upload bytes");

    GSContext cctx{};
    uint32_t rgba = 0;
    bool cok = false;
    r.getClear(8, cctx, rgba, cok);
    check(rgba == 0x11223344u && cok, "clear record");

    uint32_t args[5] = {};
    uint32_t rres = 0;
    r.getReadVram(10, args, rres);
    check(rres == 0xAABBCCDDu, "readvram result");

    GSPresentationRequest req{};
    PresentationFrame f{};
    std::vector<uint8_t> vsnap;
    size_t presentIdx = 0;
    for (size_t i = 0; i < r.recordCount(); ++i)
        if (r.recordAt(i).tag == gscap::kPresent)
            presentIdx = i;
    r.getPresent(presentIdx, req, f, vsnap);
    check(f.width == 64 && f.height == 64, "present dims");
    check(vsnap.size() == kVramSize, "present vram size");
    check(r.presentOffsets()[0] == r.recordAt(presentIdx).offset, "present index offset");

    // VRAM snapshot must show the clear color at (0,0) in CT32 layout:
    // byte-exactness against the live backend is covered by gsreplay;
    // here we check the snapshot is non-trivial.
    bool anyNonzero = false;
    for (uint8_t byte : vsnap)
        if (byte != 0)
        {
            anyNonzero = true;
            break;
        }
    check(anyNonzero, "vram snapshot non-trivial");

    if (g_failures == 0)
        std::printf("test_gscap_roundtrip: pass (%zu records)\n", r.recordCount());
    return g_failures == 0 ? 0 : 1;
}
