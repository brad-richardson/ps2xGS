// gsgen: synthetic .gscap generator. Drives a GSRecordingBackend(CPU)
// directly through the GSRasterBackend interface (no frontend), one
// capture per feature. Usage: gsgen <outdir>

#include "gscap.h"
#include "gsregs.h"
#include "recording_backend.h"
#include "runtime/gs/gs_cpu_backend.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace
{

constexpr uint32_t kVramSize = gscap::kVramSize;
constexpr int kFW = 64;
constexpr int kFH = 64;

// VRAM page plan (4 MiB = 512 pages): frame at 0, second frame at 8,
// depth at 64, textures at 128, CLUT palettes at 256.
constexpr uint32_t kFrameFbp = 0;
constexpr uint32_t kFrame2Fbp = 8;
constexpr uint32_t kFrameFbw = 10; // 640 px rows
constexpr uint32_t kZbp = 64;
constexpr uint32_t kTexTbp = 128;
constexpr uint32_t kClutCbp = 256;
// G2 mip levels: same 32x32 CT32 size, distinct solid colors,
// spaced to avoid swizzle overlap (tbw=1).
constexpr uint32_t kMipTbp1 = 136;
constexpr uint32_t kMipTbp2 = 144;

GSContext baseContext(uint8_t framePsm = GS_PSM_CT32)
{
    GSContext c{};
    c.frame.fbp = kFrameFbp;
    c.frame.fbw = kFrameFbw;
    c.frame.psm = framePsm;
    c.frame.fbmsk = 0;
    c.scissor.x0 = 0;
    c.scissor.y0 = 0;
    c.scissor.x1 = kFW - 1;
    c.scissor.y1 = kFH - 1;
    c.zbuf.zbp = kZbp;
    c.zbuf.psm = GS_PSM_Z32;
    c.zbuf.zmask = false;
    gsregs::TestReg test;
    test.zte = true;
    test.ztst = gsregs::kZtstAlways;
    c.test = test.encode();
    gsregs::AlphaReg alpha;
    c.alpha = alpha.encode();
    return c;
}

GSDrawState baseState(GSPrimType type, uint8_t framePsm = GS_PSM_CT32)
{
    GSDrawState s{};
    s.context = baseContext(framePsm);
    s.prim.type = type;
    s.colclamp = 1;
    s.textureWidth = 1;
    s.textureHeight = 1;
    return s;
}

GSVertex vtx(float x, float y, double z, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
    GSVertex v{};
    v.x = x;
    v.y = y;
    v.z = z;
    v.r = r;
    v.g = g;
    v.b = b;
    v.a = a;
    v.q = 1.0f;
    return v;
}

GSPrimitiveBatch spriteBatch(float x0, float y0, float x1, float y1, double z, uint8_t r,
                             uint8_t g, uint8_t b, uint8_t a, const GSDrawState &state)
{
    GSPrimitiveBatch batch{};
    batch.vertices[0] = vtx(x0, y0, z, r, g, b, a);
    batch.vertices[1] = vtx(x1, y1, z, r, g, b, a);
    batch.vertexCount = 2;
    batch.state = state;
    return batch;
}

struct Gen
{
    GSRecordingBackend &be;
    int presents = 0;

    void submit(const GSPrimitiveBatch &b) { be.Submit(b); }
    void flush()
    {
        be.Flush();
        be.TextureFlush();
    }

    void present(uint8_t psm = GS_PSM_CT32, uint64_t vsyncTick = 0,
                gsregs::PmodeReg pmode = gsregs::PmodeReg(),
                gsregs::Smode2Reg smode2 = gsregs::Smode2Reg(),
                uint16_t fbp = kFrameFbp)
    {
        gsregs::DispfbReg dispfb;
        dispfb.fbp = fbp;
        dispfb.fbw = kFrameFbw;
        dispfb.psm = psm;
        gsregs::DisplayReg display;
        display.dw = kFW - 1;
        display.dh = kFH - 1;
        GSPresentationRequest req{};
        req.pmode = pmode.encode();
        req.smode2 = smode2.encode();
        req.dispfb1 = dispfb.encode();
        req.display1 = display.encode();
        req.vsyncTick = vsyncTick;
        req.contextFrames[0].fbp = fbp;
        req.contextFrames[0].fbw = kFrameFbw;
        req.contextFrames[0].psm = psm;
        be.Present(req);
        ++presents;
    }

    void upload(uint32_t dbp, uint32_t dbw, uint8_t dpsm, uint16_t dx, uint16_t dy,
                uint16_t w, uint16_t h, const std::vector<uint8_t> &pixels)
    {
        GSTransferCommand cmd{};
        cmd.bitbltbuf.dbp = dbp;
        cmd.bitbltbuf.dbw = static_cast<uint8_t>(dbw);
        cmd.bitbltbuf.dpsm = dpsm;
        cmd.trxpos.dsax = dx;
        cmd.trxpos.dsay = dy;
        cmd.trxreg.rrw = w;
        cmd.trxreg.rrh = h;
        cmd.direction = 0;
        be.BeginTransfer(cmd);
        be.UploadImage(pixels.data(), static_cast<uint32_t>(pixels.size()));
    }
};

// ---- pixel patterns ----

std::vector<uint8_t> patternCT32(int w, int h)
{
    std::vector<uint8_t> px(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            size_t o = (static_cast<size_t>(y) * w + x) * 4;
            px[o] = static_cast<uint8_t>((x * 8) & 0xFF);
            px[o + 1] = static_cast<uint8_t>((y * 8) & 0xFF);
            px[o + 2] = static_cast<uint8_t>(((x + y) * 4) & 0xFF);
            px[o + 3] = 0x80;
        }
    return px;
}

std::vector<uint8_t> patternCT24(int w, int h)
{
    std::vector<uint8_t> px(static_cast<size_t>(w) * h * 3);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            size_t o = (static_cast<size_t>(y) * w + x) * 3;
            px[o] = static_cast<uint8_t>((x * 8) & 0xFF);
            px[o + 1] = static_cast<uint8_t>((y * 8) & 0xFF);
            px[o + 2] = static_cast<uint8_t>(((x + y) * 4) & 0xFF);
        }
    return px;
}

std::vector<uint8_t> patternCT16(int w, int h)
{
    std::vector<uint8_t> px(static_cast<size_t>(w) * h * 2);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            const uint16_t v = static_cast<uint16_t>(((x & 0x1F)) | ((y & 0x1F) << 5) |
                                                     (((x + y) & 0x1F) << 10) | 0x8000u);
            size_t o = (static_cast<size_t>(y) * w + x) * 2;
            px[o] = v & 0xFFu;
            px[o + 1] = (v >> 8) & 0xFFu;
        }
    return px;
}

std::vector<uint8_t> patternT8(int w, int h)
{
    std::vector<uint8_t> px(static_cast<size_t>(w) * h);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            px[static_cast<size_t>(y) * w + x] = static_cast<uint8_t>((x + y * w) & 0xFF);
    return px;
}

std::vector<uint8_t> patternT4(int w, int h)
{
    std::vector<uint8_t> px(static_cast<size_t>(w) * h / 2);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; x += 2)
        {
            const uint8_t lo = static_cast<uint8_t>((x + y * w) & 0xF);
            const uint8_t hi = static_cast<uint8_t>((x + 1 + y * w) & 0xF);
            px[(static_cast<size_t>(y) * w + x) / 2] = static_cast<uint8_t>(lo | (hi << 4));
        }
    return px;
}

std::vector<uint8_t> paletteCT32(int entries)
{
    std::vector<uint8_t> px(static_cast<size_t>(entries) * 4);
    for (int i = 0; i < entries; ++i)
    {
        px[static_cast<size_t>(i) * 4] = static_cast<uint8_t>(i & 0xFF);
        px[static_cast<size_t>(i) * 4 + 1] = static_cast<uint8_t>((255 - i) & 0xFF);
        px[static_cast<size_t>(i) * 4 + 2] = static_cast<uint8_t>((i * 3) & 0xFF);
        px[static_cast<size_t>(i) * 4 + 3] = (i % 3 == 0) ? 0x80u : 0x00u;
    }
    return px;
}

void uploadPalette(Gen &g, uint8_t cpsm = GS_PSM_CT32)
{
    // 16x32 entries at cbp; covers CSM1 swizzle range and CSM2 cov=16.
    std::vector<uint8_t> pal = paletteCT32(512);
    uint8_t dpsm = cpsm;
    std::vector<uint8_t> bytes = pal;
    if (cpsm == GS_PSM_CT16 || cpsm == GS_PSM_CT16S)
    {
        bytes.clear();
        for (int i = 0; i < 512; ++i)
        {
            const uint16_t v = static_cast<uint16_t>(
                (((pal[size_t(i) * 4] >> 3) & 0x1F)) | (((pal[size_t(i) * 4 + 1] >> 3) & 0x1F) << 5) |
                (((pal[size_t(i) * 4 + 2] >> 3) & 0x1F) << 10) |
                ((pal[size_t(i) * 4 + 3] >= 0x40u) ? 0x8000u : 0u));
            bytes.push_back(v & 0xFFu);
            bytes.push_back((v >> 8) & 0xFFu);
        }
    }
    g.upload(kClutCbp, 1, dpsm, 0, 0, 16, 32, bytes);
}

// ---- clear cases ----

void caseClear(Gen &g, uint8_t psm, uint32_t rgba)
{
    GSContext ctx = baseContext(psm);
    if (psm == GS_PSM_CT32)
    {
        g.be.Reset();
        g.be.Sync(GSSyncReason::Finish);
    }
    g.be.ClearFramebuffer(ctx, rgba);
    g.flush();
    g.present(psm);
}

// ---- transfer cases ----

void caseTransferH2L(Gen &g, uint8_t psm)
{
    std::vector<uint8_t> px;
    switch (psm)
    {
    case GS_PSM_CT24:
        px = patternCT24(16, 16);
        break;
    case GS_PSM_CT16:
    case GS_PSM_CT16S:
        px = patternCT16(16, 16);
        break;
    default:
        px = patternCT32(16, 16);
        break;
    }
    g.upload(kFrameFbp, kFrameFbw, psm, 8, 8, 16, 16, px);
    g.flush();
    g.be.Sync(GSSyncReason::Presentation);
    GSTransferSnapshot snap = g.be.GetTransferSnapshot();
    (void)snap;
    g.present();
}

void drawUploadedTexels(Gen &g, uint8_t psm);

void caseTransferH2LT8(Gen &g)
{
    uploadPalette(g);
    g.upload(kTexTbp, 1, GS_PSM_T8, 0, 0, 32, 32, patternT8(32, 32));
    g.be.WriteVram(GS_PSM_T8, kTexTbp, 1, 31, 31, 0x7Fu);
    const uint32_t v = g.be.ReadVram(GS_PSM_T8, kTexTbp, 1, 31, 31);
    (void)v;
    std::vector<uint8_t> snap;
    g.be.SnapshotVram(snap);
    drawUploadedTexels(g, GS_PSM_T8);
    g.present();
}

void caseTransferH2LT4(Gen &g)
{
    uploadPalette(g);
    g.upload(kTexTbp, 1, GS_PSM_T4, 0, 0, 32, 32, patternT4(32, 32));
    const uint32_t v = g.be.ReadVram(GS_PSM_T4, kTexTbp, 1, 0, 0);
    (void)v;
    drawUploadedTexels(g, GS_PSM_T4);
    g.present();
}

void caseTransferL2L(Gen &g)
{
    g.upload(kFrameFbp, kFrameFbw, GS_PSM_CT32, 0, 0, 16, 16, patternCT32(16, 16));
    GSTransferCommand cmd{};
    cmd.bitbltbuf.sbp = kFrameFbp;
    cmd.bitbltbuf.sbw = kFrameFbw;
    cmd.bitbltbuf.spsm = GS_PSM_CT32;
    cmd.bitbltbuf.dbp = kFrameFbp;
    cmd.bitbltbuf.dbw = kFrameFbw;
    cmd.bitbltbuf.dpsm = GS_PSM_CT32;
    cmd.trxpos.ssax = 0;
    cmd.trxpos.ssay = 0;
    cmd.trxpos.dsax = 32;
    cmd.trxpos.dsay = 32;
    cmd.trxreg.rrw = 16;
    cmd.trxreg.rrh = 16;
    cmd.direction = 2;
    g.be.BeginTransfer(cmd);
    g.be.Sync(GSSyncReason::DebugReadback);
    g.present();
}

void caseTransferL2H(Gen &g)
{
    g.upload(kFrameFbp, kFrameFbw, GS_PSM_CT32, 8, 8, 16, 16, patternCT32(16, 16));
    GSTransferCommand cmd{};
    cmd.bitbltbuf.sbp = kFrameFbp;
    cmd.bitbltbuf.sbw = kFrameFbw;
    cmd.bitbltbuf.spsm = GS_PSM_CT32;
    cmd.trxpos.ssax = 8;
    cmd.trxpos.ssay = 8;
    cmd.trxreg.rrw = 16;
    cmd.trxreg.rrh = 16;
    cmd.direction = 1;
    g.be.BeginTransfer(cmd);
    std::vector<uint8_t> chunk(512);
    uint32_t got = g.be.ConsumeLocalToHostBytes(chunk.data(), 512);
    std::vector<uint8_t> rest(2048);
    got += g.be.ConsumeLocalToHostBytes(rest.data(), 2048);
    (void)got;
    g.be.Sync(GSSyncReason::LocalToHost);
    GSTransferSnapshot snap = g.be.GetTransferSnapshot();
    (void)snap;
    g.present();
}

// ---- primitive cases ----

void casePrimPoint(Gen &g)
{
    GSDrawState s = baseState(GS_PRIM_POINT);
    GSPrimitiveBatch b{};
    b.vertexCount = 1;
    b.state = s;
    b.vertices[0] = vtx(8, 8, 0x100, 255, 0, 0, 0x80);
    g.submit(b);
    b.vertices[0] = vtx(32, 40, 0x100, 0, 255, 0, 0x80);
    g.submit(b);
    b.vertices[0] = vtx(56, 20, 0x100, 0, 0, 255, 0x80);
    g.submit(b);
    g.be.Sync(GSSyncReason::Reset);
    g.present();
}

void casePrimLine(Gen &g, bool gouraud)
{
    GSDrawState s = baseState(GS_PRIM_LINE);
    s.prim.iip = gouraud;
    GSPrimitiveBatch b{};
    b.vertexCount = 2;
    b.state = s;
    b.vertices[0] = vtx(4, 4, 0x100, 255, 0, 0, 0x80);
    b.vertices[1] = vtx(60, 56, 0x100, gouraud ? 0 : 255, gouraud ? 0 : 0, 255, 0x80);
    g.submit(b);
    b.vertices[0] = vtx(60, 8, 0x100, 0, 255, 0, 0x80);
    b.vertices[1] = vtx(8, 60, 0x100, 255, 255, 0, 0x80);
    g.submit(b);
    g.present();
}

void triBatch(Gen &g, GSPrimType type, bool gouraud, bool textured)
{
    GSDrawState s = baseState(type);
    s.prim.iip = gouraud;
    GSPrimitiveBatch b{};
    b.vertexCount = 3;
    b.state = s;
    if (gouraud)
    {
        b.vertices[0] = vtx(8, 56, 0x100, 255, 0, 0, 0x80);
        b.vertices[1] = vtx(56, 56, 0x100, 0, 255, 0, 0x80);
        b.vertices[2] = vtx(32, 8, 0x100, 0, 0, 255, 0x80);
    }
    else
    {
        b.vertices[0] = vtx(8, 56, 0x100, 255, 255, 255, 0x80);
        b.vertices[1] = vtx(56, 56, 0x100, 255, 255, 255, 0x80);
        b.vertices[2] = vtx(32, 8, 0x100, 200, 100, 50, 0x80);
    }
    (void)textured;
    g.submit(b);
}

void casePrimScissor(Gen &g)
{
    GSDrawState s = baseState(GS_PRIM_SPRITE);
    s.context.scissor.x0 = 8;
    s.context.scissor.y0 = 8;
    s.context.scissor.x1 = 24;
    s.context.scissor.y1 = 24;
    g.submit(spriteBatch(0, 0, kFW, kFH, 0x100, 0, 200, 200, 0x80, s));
    g.present();
}

void casePrimXyoffset(Gen &g)
{
    GSDrawState s = baseState(GS_PRIM_SPRITE);
    s.context.xyoffset.ofx = 8 << 4;
    s.context.xyoffset.ofy = 4 << 4;
    // shifts left/up by (8,4): drawn at (8..40, 4..36) -> lands (0..32, 0..32)
    g.submit(spriteBatch(8, 4, 40, 36, 0x100, 200, 150, 50, 0x80, s));
    g.present();
}

void casePrimFbmsk(Gen &g)
{
    GSContext clearCtx = baseContext();
    g.be.ClearFramebuffer(clearCtx, 0xFF804020u);
    GSDrawState s = baseState(GS_PRIM_SPRITE);
    s.context.frame.fbmsk = 0x00FFFFFFu; // keep dest RGB, write source alpha
    g.submit(spriteBatch(8, 8, 56, 56, 0x100, 10, 20, 30, 0xC0, s));
    g.present();
}

// ---- depth cases ----

void ztestSetup(Gen &g, GSDrawState &s)
{
    // background sheet at z=0x100, then a near quad (left, z=0x200)
    // and a far quad (right, z=0x050).
    g.submit(spriteBatch(0, 0, kFW, kFH, 0x100, 100, 100, 100, 0x80, s));
    g.submit(spriteBatch(0, 0, 32, kFH, 0x200, 200, 50, 50, 0x80, s));
    g.submit(spriteBatch(32, 0, kFW, kFH, 0x050, 50, 50, 200, 0x80, s));
}

void caseZtest(Gen &g, bool zte, uint8_t ztst, uint8_t zpsm, bool zmsk, bool preClear = false)
{
    if (preClear)
    {
        // NEVER must draw nothing: pre-fill so the check is non-vacuous.
        GSContext ctx = baseContext();
        g.be.ClearFramebuffer(ctx, 0xFF606060u);
    }
    GSDrawState s = baseState(GS_PRIM_SPRITE);
    s.context.zbuf.psm = zpsm;
    s.context.zbuf.zmask = zmsk;
    gsregs::TestReg test;
    test.zte = zte;
    test.ztst = ztst;
    s.context.test = test.encode();
    ztestSetup(g, s);
    g.present();
}

// ---- texture cases ----

struct TexSetup
{
    uint8_t psm = GS_PSM_CT32;
    uint8_t cpsm = GS_PSM_CT32;
    uint8_t csm = 0;
    uint8_t csa = 0;
    uint8_t cou = 0;
    uint8_t cov = 0;
    bool linear = false;
};

void uploadTexels(Gen &g, const TexSetup &t)
{
    switch (t.psm)
    {
    case GS_PSM_CT32:
        g.upload(kTexTbp, 1, GS_PSM_CT32, 0, 0, 32, 32, patternCT32(32, 32));
        break;
    case GS_PSM_CT24:
        g.upload(kTexTbp, 1, GS_PSM_CT24, 0, 0, 32, 32, patternCT24(32, 32));
        break;
    case GS_PSM_CT16:
    case GS_PSM_CT16S:
        g.upload(kTexTbp, 1, t.psm, 0, 0, 32, 32, patternCT16(32, 32));
        break;
    case GS_PSM_T8:
    case GS_PSM_T8H:
        g.upload(kTexTbp, 1, t.psm, 0, 0, 32, 32, patternT8(32, 32));
        uploadPalette(g, t.cpsm);
        break;
    case GS_PSM_T4:
        g.upload(kTexTbp, 1, GS_PSM_T4, 0, 0, 32, 32, patternT4(32, 32));
        uploadPalette(g, t.cpsm);
        break;
    default:
        break;
    }
}

GSDrawState texturedSpriteState(const TexSetup &t, bool fst, gsregs::ClampReg clamp,
                                uint8_t tfx = 0)
{
    GSDrawState s = baseState(GS_PRIM_SPRITE);
    s.prim.tme = true;
    s.prim.fst = fst;
    s.context.tex0.tbp0 = kTexTbp;
    s.context.tex0.tbw = 1;
    s.context.tex0.psm = t.psm;
    s.context.tex0.tw = 5;
    s.context.tex0.th = 5;
    s.context.tex0.tcc = 1;
    s.context.tex0.tfx = tfx;
    s.context.tex0.cbp = kClutCbp;
    s.context.tex0.cpsm = t.cpsm;
    s.context.tex0.csm = t.csm;
    s.context.tex0.csa = t.csa;
    s.texclut.cbw = 1;
    s.texclut.cou = t.cou;
    s.texclut.cov = t.cov;
    s.context.clamp = clamp.encode();
    gsregs::Tex1Reg tex1;
    tex1.mmin = t.linear;
    tex1.mmag = t.linear;
    s.context.tex1 = tex1.encode();
    s.textureWidth = 32;
    s.textureHeight = 32;
    s.linearFilter = t.linear;
    return s;
}

void drawUvSprite(Gen &g, const GSDrawState &s, float x0, float y0, float x1, float y1,
                  uint16_t u0, uint16_t v0, uint16_t u1, uint16_t v1)
{
    GSPrimitiveBatch b{};
    b.vertexCount = 2;
    b.state = s;
    b.vertices[0] = vtx(x0, y0, 0x100, 255, 255, 255, 0x80);
    b.vertices[1] = vtx(x1, y1, 0x100, 255, 255, 255, 0x80);
    b.vertices[0].u = u0;
    b.vertices[0].v = v0;
    b.vertices[1].u = u1;
    b.vertices[1].v = v1;
    g.submit(b);
}

void drawUploadedTexels(Gen &g, uint8_t psm)
{
    // Render the just-uploaded texels through the palette so the
    // transfer -> texture -> render chain is pixel-checked too.
    TexSetup t;
    t.psm = psm;
    gsregs::ClampReg clamp;
    GSDrawState s = texturedSpriteState(t, true, clamp);
    drawUvSprite(g, s, 16, 16, 48, 48, 0, 0, 512, 512);
}

void caseTexBasic(Gen &g, const TexSetup &t)
{
    uploadTexels(g, t);
    gsregs::ClampReg clamp;
    GSDrawState s = texturedSpriteState(t, true, clamp);
    drawUvSprite(g, s, 16, 16, 48, 48, 0, 0, 512, 512);
    g.present();
}

void caseTexStq(Gen &g)
{
    TexSetup t;
    uploadTexels(g, t);
    gsregs::ClampReg clamp;
    GSDrawState s = texturedSpriteState(t, false, clamp);
    s.prim.type = GS_PRIM_TRIANGLE;
    GSPrimitiveBatch b{};
    b.vertexCount = 3;
    b.state = s;
    b.vertices[0] = vtx(8, 56, 0x100, 255, 255, 255, 0x80);
    b.vertices[1] = vtx(56, 56, 0x100, 255, 255, 255, 0x80);
    b.vertices[2] = vtx(32, 8, 0x100, 255, 255, 255, 0x80);
    b.vertices[0].s = 0.0f;
    b.vertices[0].t = 0.0f;
    b.vertices[1].s = 1.0f;
    b.vertices[1].t = 0.0f;
    b.vertices[2].s = 0.5f;
    b.vertices[2].t = 1.0f;
    g.submit(b);
    g.present();
}

void caseTexUvTri(Gen &g)
{
    TexSetup t;
    uploadTexels(g, t);
    gsregs::ClampReg clamp;
    GSDrawState s = texturedSpriteState(t, true, clamp);
    s.prim.type = GS_PRIM_TRIANGLE;
    GSPrimitiveBatch b{};
    b.vertexCount = 3;
    b.state = s;
    b.vertices[0] = vtx(8, 56, 0x100, 255, 255, 255, 0x80);
    b.vertices[1] = vtx(56, 56, 0x100, 255, 255, 255, 0x80);
    b.vertices[2] = vtx(32, 8, 0x100, 255, 255, 255, 0x80);
    b.vertices[0].u = 0;
    b.vertices[0].v = 0;
    b.vertices[1].u = 512;
    b.vertices[1].v = 0;
    b.vertices[2].u = 256;
    b.vertices[2].v = 512;
    g.submit(b);
    g.present();
}

void caseTexClamp(Gen &g, uint8_t mode, uint16_t minu, uint16_t maxu, uint16_t minv,
                  uint16_t maxv)
{
    TexSetup t;
    uploadTexels(g, t);
    gsregs::ClampReg clamp;
    clamp.wms = mode;
    clamp.wmt = mode;
    clamp.minu = minu;
    clamp.maxu = maxu;
    clamp.minv = minv;
    clamp.maxv = maxv;
    GSDrawState s = texturedSpriteState(t, true, clamp);
    // UVs run 2x past the texture to exercise wrap/clamp paths.
    drawUvSprite(g, s, 8, 8, 56, 56, 0, 0, 1024, 1024);
    g.present();
}

void caseTexFilter(Gen &g, bool linear)
{
    TexSetup t;
    t.linear = linear;
    uploadTexels(g, t);
    gsregs::ClampReg clamp;
    GSDrawState s = texturedSpriteState(t, true, clamp);
    drawUvSprite(g, s, 16, 16, 48, 48, 0, 0, 512, 512);
    g.present();
}

void caseTexTexa(Gen &g)
{
    TexSetup t;
    t.psm = GS_PSM_CT16;
    uploadTexels(g, t);
    gsregs::ClampReg clamp;
    GSDrawState s = texturedSpriteState(t, true, clamp);
    s.texa.ta0 = 0x20;
    s.texa.aem = true;
    s.texa.ta1 = 0xB0;
    drawUvSprite(g, s, 16, 16, 48, 48, 0, 0, 512, 512);
    g.present();
}

// ---- G2: TEX1 filtering + mip chains ----

std::vector<uint8_t> patternSolidCT32(int w, int h, uint8_t r, uint8_t g, uint8_t b,
                                      uint8_t a)
{
    std::vector<uint8_t> px(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
        {
            size_t o = (static_cast<size_t>(y) * w + x) * 4;
            px[o] = r;
            px[o + 1] = g;
            px[o + 2] = b;
            px[o + 3] = a;
        }
    return px;
}

void uploadMipChain(Gen &g)
{
    // L0 red, L1 green, L2 blue: wrong-LOD selection shows as
    // full-sprite color diffs. CPU samples L0; strict picks L1/L2.
    g.upload(kTexTbp, 1, GS_PSM_CT32, 0, 0, 32, 32,
             patternSolidCT32(32, 32, 255, 0, 0, 0x80));
    g.upload(kMipTbp1, 1, GS_PSM_CT32, 0, 0, 32, 32,
             patternSolidCT32(32, 32, 0, 255, 0, 0x80));
    g.upload(kMipTbp2, 1, GS_PSM_CT32, 0, 0, 32, 32,
             patternSolidCT32(32, 32, 0, 0, 255, 0x80));
}

GSDrawState mipSpriteState(bool mmin, bool mmag, uint8_t mxl, bool linear, bool fst)
{
    GSDrawState s = baseState(GS_PRIM_SPRITE);
    s.prim.tme = true;
    s.prim.fst = fst;
    s.context.tex0.tbp0 = kTexTbp;
    s.context.tex0.tbw = 1;
    s.context.tex0.psm = GS_PSM_CT32;
    s.context.tex0.tw = 5;
    s.context.tex0.th = 5;
    s.context.tex0.tcc = 1;
    gsregs::ClampReg clamp;
    s.context.clamp = clamp.encode();
    gsregs::Tex1Reg tex1;
    tex1.mmin = mmin;
    tex1.mmag = mmag;
    tex1.mxl = mxl;
    s.context.tex1 = tex1.encode();
    gsregs::Miptbp1Reg miptbp1;
    miptbp1.tbp1 = kMipTbp1;
    miptbp1.tbw1 = 1;
    miptbp1.tbp2 = kMipTbp2;
    miptbp1.tbw2 = 1;
    s.context.miptbp1 = miptbp1.encode();
    s.context.miptbp2 = 0;
    s.textureWidth = 32;
    s.textureHeight = 32;
    s.linearFilter = linear;
    return s;
}

void caseTex1Filter(Gen &g, bool linear, bool mmin, bool mmag)
{
    // Gradient pattern so nearest-vs-linear differ; TEX1 set
    // explicitly. Agree cases stay exact under strict; disagree
    // cases flip (strict honors TEX1, ignores linearFilter).
    g.upload(kTexTbp, 1, GS_PSM_CT32, 0, 0, 32, 32, patternCT32(32, 32));
    GSDrawState s = baseState(GS_PRIM_SPRITE);
    s.prim.tme = true;
    s.prim.fst = true;
    s.context.tex0.tbp0 = kTexTbp;
    s.context.tex0.tbw = 1;
    s.context.tex0.psm = GS_PSM_CT32;
    s.context.tex0.tw = 5;
    s.context.tex0.th = 5;
    s.context.tex0.tcc = 1;
    gsregs::ClampReg clamp;
    s.context.clamp = clamp.encode();
    gsregs::Tex1Reg tex1;
    tex1.mmin = mmin;
    tex1.mmag = mmag;
    tex1.mxl = 0;
    s.context.tex1 = tex1.encode();
    s.context.miptbp1 = 0;
    s.context.miptbp2 = 0;
    s.textureWidth = 32;
    s.textureHeight = 32;
    s.linearFilter = linear;
    // 2x magnification (16 texels -> 32 pixels) so nearest-vs-
    // linear differ; 1:1 would make them identical.
    drawUvSprite(g, s, 16, 16, 48, 48, 0, 0, 256, 256);
    g.present();
}

void caseMip(Gen &g, uint8_t mxl)
{
    uploadMipChain(g);
    // Filtering agrees (nearest) so only mip selection flips.
    GSDrawState s = mipSpriteState(false, false, mxl, false, true);
    drawUvSprite(g, s, 16, 16, 48, 48, 0, 0, 512, 512);
    g.present();
}

void caseMipStq(Gen &g, uint8_t mxl)
{
    uploadMipChain(g);
    GSDrawState s = mipSpriteState(false, false, mxl, false, false);
    s.prim.type = GS_PRIM_TRIANGLE;
    GSPrimitiveBatch b{};
    b.vertexCount = 3;
    b.state = s;
    b.vertices[0] = vtx(8, 56, 0x100, 255, 255, 255, 0x80);
    b.vertices[1] = vtx(56, 56, 0x100, 255, 255, 255, 0x80);
    b.vertices[2] = vtx(32, 8, 0x100, 255, 255, 255, 0x80);
    b.vertices[0].s = 0.0f;
    b.vertices[0].t = 0.0f;
    b.vertices[1].s = 1.0f;
    b.vertices[1].t = 0.0f;
    b.vertices[2].s = 0.5f;
    b.vertices[2].t = 1.0f;
    g.submit(b);
    g.present();
}

// ---- G2: ignored-field isolation pairs ----

gsregs::DimxReg g2DimxMatrix()
{
    gsregs::DimxReg dimx;
    for (int y = 0; y < 4; ++y)
        for (int x = 0; x < 4; ++x)
            dimx.m[y][x] = static_cast<uint8_t>((x + y * 4) & 7u);
    return dimx;
}

void caseIsoDthe(Gen &g, uint64_t dthe)
{
    // DIMX held constant; only DTHE differs (single field).
    GSDrawState s = baseState(GS_PRIM_SPRITE);
    s.dimx = g2DimxMatrix().encode();
    s.dthe = dthe;
    g.submit(spriteBatch(8, 8, 56, 56, 0x100, 200, 150, 50, 0x80, s));
    g.present();
}

void caseIsoColclamp(Gen &g, uint64_t colclamp)
{
    // Overflowing blend: (Cs*255>>7)+Cd exceeds 255 on all
    // channels, so clamp-vs-wrap shows. Only COLCLAMP differs.
    GSDrawState bg = baseState(GS_PRIM_SPRITE);
    g.submit(spriteBatch(0, 0, kFW, kFH, 0x100, 40, 80, 160, 0x80, bg));
    GSDrawState fg = baseState(GS_PRIM_SPRITE);
    fg.prim.abe = true;
    gsregs::AlphaReg alpha;
    alpha.a = 0; // Cs
    alpha.b = 2; // zero
    alpha.c = 2; // FIX
    alpha.d = 1; // Cd
    alpha.fix = 0xFF;
    fg.context.alpha = alpha.encode();
    fg.colclamp = colclamp;
    g.submit(spriteBatch(16, 16, 48, 48, 0x100, 200, 120, 60, 0x80, fg));
    g.present();
}

void caseIsoScanmsk(Gen &g, uint64_t scanmsk)
{
    GSDrawState s = baseState(GS_PRIM_SPRITE);
    s.scanmsk = scanmsk;
    g.submit(spriteBatch(8, 8, 56, 56, 0x100, 30, 160, 200, 0x80, s));
    g.present();
}

void caseIsoAa1(Gen &g, bool aa1)
{
    GSDrawState s = baseState(GS_PRIM_SPRITE);
    s.prim.aa1 = aa1;
    g.submit(spriteBatch(8, 8, 56, 56, 0x100, 30, 160, 200, 0x80, s));
    g.present();
}

void caseIsoZte(Gen &g, bool zte, uint8_t ztst, bool preClear, uint32_t clearRgba)
{
    if (preClear)
    {
        GSContext ctx = baseContext();
        g.be.ClearFramebuffer(ctx, clearRgba);
    }
    GSDrawState s = baseState(GS_PRIM_SPRITE);
    gsregs::TestReg test;
    test.zte = zte;
    test.ztst = ztst;
    s.context.test = test.encode();
    ztestSetup(g, s);
    g.present();
}

// ---- blend cases ----

void blendPair(Gen &g, gsregs::AlphaReg alpha, bool pabe = false, bool fba = false,
               uint64_t colclamp = 1, uint64_t dthe = 0, uint64_t dimx = 0)
{
    GSDrawState bg = baseState(GS_PRIM_SPRITE);
    g.submit(spriteBatch(0, 0, kFW, kFH, 0x100, 40, 80, 160, 0x80, bg));
    GSDrawState fg = baseState(GS_PRIM_SPRITE);
    fg.prim.abe = true;
    fg.context.alpha = alpha.encode();
    fg.pabe = pabe;
    fg.context.fba = fba ? 1u : 0u;
    fg.colclamp = colclamp;
    fg.dthe = dthe;
    fg.dimx = dimx;
    g.submit(spriteBatch(16, 16, 48, 48, 0x100, 200, 120, 60, 0x80, fg));
    g.present();
}

// ---- alpha-test cases ----

void atestPair(Gen &g, gsregs::TestReg test, uint8_t fgAlpha)
{
    GSDrawState bg = baseState(GS_PRIM_SPRITE);
    g.submit(spriteBatch(0, 0, kFW, kFH, 0x100, 90, 90, 90, 0x80, bg));
    GSDrawState fg = baseState(GS_PRIM_SPRITE);
    fg.context.test = test.encode();
    g.submit(spriteBatch(16, 16, 48, 48, 0x100, 200, 60, 60, fgAlpha, fg));
    g.present();
}

void datePair(Gen &g, bool datm)
{
    GSDrawState bg = baseState(GS_PRIM_SPRITE);
    // left half carries destination alpha (bit set), right half does not.
    g.submit(spriteBatch(0, 0, 32, kFH, 0x100, 90, 90, 90, 0x80, bg));
    g.submit(spriteBatch(32, 0, kFW, kFH, 0x100, 90, 90, 90, 0x00, bg));
    GSDrawState fg = baseState(GS_PRIM_SPRITE);
    gsregs::TestReg test;
    test.zte = true;
    test.ztst = gsregs::kZtstAlways;
    test.date = true;
    test.datm = datm;
    fg.context.test = test.encode();
    g.submit(spriteBatch(8, 8, 56, 56, 0x100, 200, 60, 60, 0x80, fg));
    g.present();
}

// ---- fog cases ----

void caseFogFlat(Gen &g)
{
    GSDrawState s = baseState(GS_PRIM_TRIANGLE);
    s.prim.fge = true;
    s.fogR = 200;
    s.fogG = 40;
    s.fogB = 40;
    GSPrimitiveBatch b{};
    b.vertexCount = 3;
    b.state = s;
    b.vertices[0] = vtx(8, 56, 0x100, 100, 100, 100, 0x80);
    b.vertices[1] = vtx(56, 56, 0x100, 100, 100, 100, 0x80);
    b.vertices[2] = vtx(32, 8, 0x100, 100, 100, 100, 0x80);
    b.vertices[0].fog = 0x80;
    b.vertices[1].fog = 0x80;
    b.vertices[2].fog = 0x80;
    g.submit(b);
    g.present();
}

void caseFogGouraud(Gen &g)
{
    GSDrawState s = baseState(GS_PRIM_TRIANGLE);
    s.prim.iip = true;
    s.prim.fge = true;
    s.fogR = 40;
    s.fogG = 200;
    s.fogB = 40;
    GSPrimitiveBatch b{};
    b.vertexCount = 3;
    b.state = s;
    b.vertices[0] = vtx(8, 56, 0x100, 150, 150, 150, 0x80);
    b.vertices[1] = vtx(56, 56, 0x100, 150, 150, 150, 0x80);
    b.vertices[2] = vtx(32, 8, 0x100, 150, 150, 150, 0x80);
    b.vertices[0].fog = 0x00;
    b.vertices[1].fog = 0x80;
    b.vertices[2].fog = 0xFF;
    g.submit(b);
    g.present();
}

void caseFogTextured(Gen &g)
{
    TexSetup t;
    uploadTexels(g, t);
    gsregs::ClampReg clamp;
    GSDrawState s = texturedSpriteState(t, true, clamp);
    s.prim.fge = true;
    s.fogR = 40;
    s.fogG = 40;
    s.fogB = 200;
    drawUvSprite(g, s, 16, 16, 48, 48, 0, 0, 512, 512);
    // sprite path uses v1.fog for the whole rect; set both ends.
    g.present();
}

// ---- present cases ----

void casePresentBoth(Gen &g)
{
    GSContext ctx2 = baseContext();
    ctx2.frame.fbp = kFrame2Fbp;
    g.be.ClearFramebuffer(ctx2, 0xFFC00040u);
    GSContext ctx1 = baseContext();
    g.be.ClearFramebuffer(ctx1, 0xFF40C000u);
    gsregs::PmodeReg pmode;
    pmode.en1 = true;
    pmode.en2 = true;
    pmode.mmod = true;
    pmode.alp = 0x80;
    gsregs::Smode2Reg smode2;
    gsregs::DispfbReg dispfb1;
    dispfb1.fbp = kFrameFbp;
    dispfb1.fbw = kFrameFbw;
    gsregs::DispfbReg dispfb2;
    dispfb2.fbp = kFrame2Fbp;
    dispfb2.fbw = kFrameFbw;
    gsregs::DisplayReg display;
    display.dw = kFW - 1;
    display.dh = kFH - 1;
    GSPresentationRequest req{};
    req.pmode = pmode.encode();
    req.smode2 = smode2.encode();
    req.dispfb1 = dispfb1.encode();
    req.display1 = display.encode();
    req.dispfb2 = dispfb2.encode();
    req.display2 = display.encode();
    req.contextFrames[0].fbp = kFrameFbp;
    req.contextFrames[0].fbw = kFrameFbw;
    req.contextFrames[0].psm = GS_PSM_CT32;
    g.be.Present(req);
    ++g.presents;
}

void casePresent(Gen &g, gsregs::Smode2Reg smode2, gsregs::PmodeReg pmode, uint64_t vsync,
                 bool circuit2)
{
    GSContext ctx = baseContext();
    g.be.ClearFramebuffer(ctx, 0xFF2060A0u);
    gsregs::DispfbReg dispfb;
    dispfb.fbp = kFrameFbp;
    dispfb.fbw = kFrameFbw;
    gsregs::DisplayReg display;
    display.dw = kFW - 1;
    display.dh = kFH - 1;
    GSPresentationRequest req{};
    req.pmode = pmode.encode();
    req.smode2 = smode2.encode();
    if (!circuit2)
    {
        req.dispfb1 = dispfb.encode();
        req.display1 = display.encode();
    }
    else
    {
        req.dispfb2 = dispfb.encode();
        req.display2 = display.encode();
    }
    req.vsyncTick = vsync;
    req.contextFrames[0].fbp = kFrameFbp;
    req.contextFrames[0].fbw = kFrameFbw;
    req.contextFrames[0].psm = GS_PSM_CT32;
    g.be.Present(req);
    ++g.presents;
}

// ---- registry ----

using CaseFn = std::function<void(Gen &)>;
using CaseList = std::vector<std::pair<std::string, CaseFn>>;

CaseList allCases()
{
    CaseList cases;
    auto add = [&](const std::string &name, CaseFn fn) { cases.emplace_back(name, fn); };

    add("clear-ct32", [](Gen &g) { caseClear(g, GS_PSM_CT32, 0xFF804020u); });
    add("clear-ct24", [](Gen &g) { caseClear(g, GS_PSM_CT24, 0x00804020u); });
    add("clear-ct16", [](Gen &g) { caseClear(g, GS_PSM_CT16, 0x80804020u); });
    add("clear-ct16s", [](Gen &g) { caseClear(g, GS_PSM_CT16S, 0x80804020u); });

    add("transfer-h2l-ct32", [](Gen &g) { caseTransferH2L(g, GS_PSM_CT32); });
    add("transfer-h2l-ct24", [](Gen &g) { caseTransferH2L(g, GS_PSM_CT24); });
    add("transfer-h2l-ct16", [](Gen &g) { caseTransferH2L(g, GS_PSM_CT16); });
    add("transfer-h2l-t8", caseTransferH2LT8);
    add("transfer-h2l-t4", caseTransferH2LT4);
    add("transfer-l2l", caseTransferL2L);
    add("transfer-l2h", caseTransferL2H);

    add("prim-point", casePrimPoint);
    add("prim-line-flat", [](Gen &g) { casePrimLine(g, false); });
    add("prim-line-gouraud", [](Gen &g) { casePrimLine(g, true); });
    add("prim-trilist-flat", [](Gen &g) { triBatch(g, GS_PRIM_TRIANGLE, false, false); g.present(); });
    add("prim-trilist-gouraud", [](Gen &g) { triBatch(g, GS_PRIM_TRIANGLE, true, false); g.present(); });
    add("prim-tristrip", [](Gen &g) {
        GSDrawState s = baseState(GS_PRIM_TRISTRIP);
        s.prim.iip = true;
        s.prim.ctxt = true;
        GSPrimitiveBatch b{};
        b.vertexCount = 3;
        b.state = s;
        b.vertices[0] = vtx(8, 56, 0x100, 255, 0, 0, 0x80);
        b.vertices[1] = vtx(56, 56, 0x100, 0, 255, 0, 0x80);
        b.vertices[2] = vtx(32, 8, 0x100, 0, 0, 255, 0x80);
        g.submit(b);
        g.present();
    });
    add("prim-trifan", [](Gen &g) {
        GSDrawState s = baseState(GS_PRIM_TRIFAN);
        s.prim.aa1 = true;
        s.prim.fix = true;
        s.scanmsk = 2;
        GSPrimitiveBatch b{};
        b.vertexCount = 3;
        b.state = s;
        b.vertices[0] = vtx(32, 32, 0x100, 255, 255, 255, 0x80);
        b.vertices[1] = vtx(8, 8, 0x100, 255, 255, 255, 0x80);
        b.vertices[2] = vtx(56, 40, 0x100, 255, 255, 255, 0x80);
        g.submit(b);
        g.present();
    });
    add("prim-sprite", [](Gen &g) {
        GSDrawState s = baseState(GS_PRIM_SPRITE);
        g.submit(spriteBatch(8, 8, 56, 56, 0x100, 30, 160, 200, 0x80, s));
        g.present();
    });
    add("prim-scissor", casePrimScissor);
    add("prim-xyoffset", casePrimXyoffset);
    add("prim-fbmsk", casePrimFbmsk);

    add("ztest-never", [](Gen &g) { caseZtest(g, false, gsregs::kZtstNever, GS_PSM_Z32, false, true); });
    add("ztest-always", [](Gen &g) { caseZtest(g, true, gsregs::kZtstAlways, GS_PSM_Z32, false); });
    add("ztest-gequal", [](Gen &g) { caseZtest(g, true, gsregs::kZtstGequal, GS_PSM_Z32, false); });
    add("ztest-gequal-z24", [](Gen &g) { caseZtest(g, true, gsregs::kZtstGequal, GS_PSM_Z24, false); });
    add("ztest-gequal-z16", [](Gen &g) { caseZtest(g, true, gsregs::kZtstGequal, GS_PSM_Z16, false); });
    add("ztest-gequal-z16s", [](Gen &g) { caseZtest(g, true, gsregs::kZtstGequal, GS_PSM_Z16S, false); });
    add("ztest-greater", [](Gen &g) { caseZtest(g, true, gsregs::kZtstGreater, GS_PSM_Z32, false); });
    add("ztest-zmsk", [](Gen &g) { caseZtest(g, true, gsregs::kZtstGequal, GS_PSM_Z32, true); });

    add("tex-ct32", [](Gen &g) { caseTexBasic(g, TexSetup{}); });
    add("tex-ct24", [](Gen &g) {
        TexSetup t;
        t.psm = GS_PSM_CT24;
        caseTexBasic(g, t);
    });
    add("tex-ct16", [](Gen &g) {
        TexSetup t;
        t.psm = GS_PSM_CT16;
        caseTexBasic(g, t);
    });
    add("tex-t8-csm1", [](Gen &g) {
        TexSetup t;
        t.psm = GS_PSM_T8;
        t.csm = 0;
        t.csa = 1;
        caseTexBasic(g, t);
    });
    add("tex-t8-csm2", [](Gen &g) {
        TexSetup t;
        t.psm = GS_PSM_T8;
        t.csm = 1;
        t.cov = 16;
        caseTexBasic(g, t);
    });
    add("tex-t4-csm1", [](Gen &g) {
        TexSetup t;
        t.psm = GS_PSM_T4;
        t.csm = 0;
        t.csa = 1;
        caseTexBasic(g, t);
    });
    add("tex-t4-csm2", [](Gen &g) {
        TexSetup t;
        t.psm = GS_PSM_T4;
        t.csm = 1;
        t.cpsm = GS_PSM_CT16;
        t.cov = 16;
        caseTexBasic(g, t);
    });
    add("tex-t8h", [](Gen &g) {
        TexSetup t;
        t.psm = GS_PSM_T8H;
        t.csm = 1;
        t.cov = 16;
        caseTexBasic(g, t);
    });
    add("tex-stq", caseTexStq);
    add("tex-uv", caseTexUvTri);
    add("tex-clamp-repeat", [](Gen &g) { caseTexClamp(g, gsregs::kWrapRepeat, 0, 0, 0, 0); });
    add("tex-clamp-clamp", [](Gen &g) { caseTexClamp(g, gsregs::kWrapClamp, 0, 0, 0, 0); });
    add("tex-clamp-rclamp", [](Gen &g) { caseTexClamp(g, gsregs::kWrapRegionClamp, 8, 24, 8, 24); });
    add("tex-clamp-rrepeat", [](Gen &g) { caseTexClamp(g, gsregs::kWrapRegionRepeat, 8, 7, 8, 7); });
    add("tex-nearest", [](Gen &g) { caseTexFilter(g, false); });
    add("tex-linear", [](Gen &g) { caseTexFilter(g, true); });
    add("tex-texa", caseTexTexa);

    add("blend-a-cs", [](Gen &g) {
        gsregs::AlphaReg a;
        a.a = 0;
        a.b = 1;
        a.c = 0;
        a.d = 1;
        blendPair(g, a);
    });
    add("blend-a-cd", [](Gen &g) {
        gsregs::AlphaReg a;
        a.a = 1;
        a.b = 0;
        a.c = 0;
        a.d = 1;
        blendPair(g, a);
    });
    add("blend-b-zero", [](Gen &g) {
        gsregs::AlphaReg a;
        a.a = 0;
        a.b = 2;
        a.c = 0;
        a.d = 1;
        blendPair(g, a);
    });
    add("blend-c-ad", [](Gen &g) {
        gsregs::AlphaReg a;
        a.a = 0;
        a.b = 1;
        a.c = 1;
        a.d = 1;
        blendPair(g, a);
    });
    add("blend-c-fix", [](Gen &g) {
        gsregs::AlphaReg a;
        a.a = 0;
        a.b = 1;
        a.c = 2;
        a.d = 1;
        a.fix = 0x80;
        blendPair(g, a);
    });
    add("blend-d-cs", [](Gen &g) {
        gsregs::AlphaReg a;
        a.a = 0;
        a.b = 1;
        a.c = 0;
        a.d = 0;
        blendPair(g, a);
    });
    add("blend-pabe", [](Gen &g) {
        gsregs::AlphaReg a;
        GSDrawState bg = baseState(GS_PRIM_SPRITE);
        g.submit(spriteBatch(0, 0, kFW, kFH, 0x100, 40, 80, 160, 0x80, bg));
        GSDrawState fg = baseState(GS_PRIM_SPRITE);
        fg.prim.abe = true;
        fg.context.alpha = a.encode();
        fg.pabe = true;
        // src alpha MSB clear: blending disabled, raw write.
        g.submit(spriteBatch(8, 8, 32, 32, 0x100, 200, 120, 60, 0x40, fg));
        // src alpha MSB set: normal blend.
        g.submit(spriteBatch(32, 32, 56, 56, 0x100, 200, 120, 60, 0xC0, fg));
        g.present();
    });
    add("blend-fba", [](Gen &g) {
        gsregs::AlphaReg a;
        blendPair(g, a, false, true);
    });
    add("blend-colclamp", [](Gen &g) {
        gsregs::AlphaReg a;
        blendPair(g, a, false, false, 0);
    });
    add("blend-dthe", [](Gen &g) {
        gsregs::AlphaReg a;
        gsregs::DimxReg dimx;
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x)
                dimx.m[y][x] = static_cast<uint8_t>((x + y * 4) & 7u);
        blendPair(g, a, false, false, 1, 1, dimx.encode());
    });

    const uint8_t kAref = 0x80;
    auto atest = [&](uint8_t atst) {
        return [=](Gen &g) {
            gsregs::TestReg t;
            t.zte = true;
            t.ztst = gsregs::kZtstAlways;
            t.ate = true;
            t.atst = atst;
            t.aref = kAref;
            atestPair(g, t, 0x80);
        };
    };
    add("atest-never", atest(gsregs::kAtstNever));
    add("atest-always", atest(gsregs::kAtstAlways));
    add("atest-less", atest(gsregs::kAtstLess));
    add("atest-lequal", atest(gsregs::kAtstLequal));
    add("atest-equal", atest(gsregs::kAtstEqual));
    add("atest-gequal", atest(gsregs::kAtstGequal));
    add("atest-greater", atest(gsregs::kAtstGreater));
    add("atest-notequal", atest(gsregs::kAtstNotequal));
    auto afail = [&](uint8_t af) {
        return [=](Gen &g) {
            gsregs::TestReg t;
            t.zte = true;
            t.ztst = gsregs::kZtstAlways;
            t.ate = true;
            t.atst = gsregs::kAtstLess;
            t.aref = kAref;
            t.afail = af;
            atestPair(g, t, 0x80); // fails LESS, AFAIL decides the write mask
        };
    };
    add("atest-afail-fb", afail(gsregs::kAfailFbOnly));
    add("atest-afail-zb", afail(gsregs::kAfailZbOnly));
    add("atest-afail-rgb", afail(gsregs::kAfailRgbOnly));
    add("atest-date-datm0", [](Gen &g) { datePair(g, false); });
    add("atest-date-datm1", [](Gen &g) { datePair(g, true); });

    add("fog-flat", caseFogFlat);
    add("fog-gouraud", caseFogGouraud);
    add("fog-textured", caseFogTextured);

    add("present-progressive", [](Gen &g) {
        gsregs::Smode2Reg smode2;
        casePresent(g, smode2, gsregs::PmodeReg(), 0, false);
    });
    add("present-field", [](Gen &g) {
        gsregs::Smode2Reg smode2;
        smode2.interlaced = true;
        smode2.frameMode = false;
        casePresent(g, smode2, gsregs::PmodeReg(), 1, false);
    });
    add("present-frame", [](Gen &g) {
        gsregs::Smode2Reg smode2;
        smode2.interlaced = true;
        smode2.frameMode = true;
        casePresent(g, smode2, gsregs::PmodeReg(), 0, false);
    });
    add("present-circuit2", [](Gen &g) {
        gsregs::PmodeReg pmode;
        pmode.en1 = false;
        pmode.en2 = true;
        casePresent(g, gsregs::Smode2Reg(), pmode, 0, true);
    });
    add("present-both", casePresentBoth);

    // G2 step 1: TEX1 filtering + mip chains.
    add("tex1-filter-agree-nearest", [](Gen &g) { caseTex1Filter(g, false, false, false); });
    add("tex1-filter-agree-linear", [](Gen &g) { caseTex1Filter(g, true, true, true); });
    add("tex1-filter-disagree-lin", [](Gen &g) { caseTex1Filter(g, false, true, true); });
    add("tex1-filter-disagree-near", [](Gen &g) { caseTex1Filter(g, true, false, false); });
    add("tex1-filter-mixed-mmin", [](Gen &g) { caseTex1Filter(g, false, true, false); });
    add("mip-chain-mxl0", [](Gen &g) { caseMip(g, 0); });
    add("mip-chain-mxl1", [](Gen &g) { caseMip(g, 1); });
    add("mip-chain-mxl2", [](Gen &g) { caseMip(g, 2); });
    add("mip-chain-stq-mxl2", [](Gen &g) { caseMipStq(g, 2); });

    // G2 step 2: ignored-field isolation pairs.
    add("iso-dthe-off", [](Gen &g) { caseIsoDthe(g, 0); });
    add("iso-dthe-on", [](Gen &g) { caseIsoDthe(g, 1); });
    add("iso-colclamp-on", [](Gen &g) { caseIsoColclamp(g, 1); });
    add("iso-colclamp-off", [](Gen &g) { caseIsoColclamp(g, 0); });
    add("iso-scanmsk-zero", [](Gen &g) { caseIsoScanmsk(g, 0); });
    add("iso-scanmsk-set", [](Gen &g) { caseIsoScanmsk(g, 2); });
    add("iso-aa1-clear", [](Gen &g) { caseIsoAa1(g, false); });
    add("iso-aa1-set", [](Gen &g) { caseIsoAa1(g, true); });
    add("iso-zte-on-always",
        [](Gen &g) { caseIsoZte(g, true, gsregs::kZtstAlways, false, 0); });
    add("iso-zte-off-always",
        [](Gen &g) { caseIsoZte(g, false, gsregs::kZtstAlways, false, 0); });
    add("iso-zte-off-never",
        [](Gen &g) { caseIsoZte(g, false, gsregs::kZtstNever, true, 0xFF202020u); });

    return cases;
}

} // namespace

int main(int argc, char **argv)
{
    if (argc != 2)
    {
        std::cerr << "usage: gsgen <outdir>\n";
        return 2;
    }
    const std::string outdir = argv[1];
    std::error_code ec;
    std::filesystem::create_directories(outdir, ec);
    if (ec)
    {
        std::cerr << "gsgen: cannot create '" << outdir << "': " << ec.message() << "\n";
        return 2;
    }

    const CaseList cases = allCases();
    uint64_t totalBytes = 0;
    for (const auto &[name, fn] : cases)
    {
        const std::string path = outdir + "/" + name + ".gscap";
        std::vector<uint8_t> vram(kVramSize, 0);
        auto cpu = std::make_unique<GSCpuBackend>();
        int presents = 0;
        {
            GSRecordingBackend rec(std::move(cpu), path, kVramSize);
            rec.Initialize(vram.data(), kVramSize);
            Gen gen{rec, 0};
            fn(gen);
            presents = gen.presents;
            rec.finalize();
        }
        const uint64_t bytes = std::filesystem::file_size(path, ec);
        totalBytes += bytes;
        std::printf("%-22s %10llu bytes presents=%d\n", (name + ".gscap").c_str(),
                    (unsigned long long)bytes, presents);
    }
    std::printf("gsgen: %zu captures, %llu bytes total\n", cases.size(),
                (unsigned long long)totalBytes);
    return 0;
}

