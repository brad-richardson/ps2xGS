// G2 strict backend: GSCpuBackend wrapper honoring TEX1 filtering +
// mip selection, DIMX/DTHE, COLCLAMP, SCANMSK, aa1, and ZTE gating.
// Semantics (documented in docs/reports/G2.md):
// - ZTE off forces ZTST=ALWAYS (test disabled, always passes).
// - TEX1 filtering: linear = mmin||mmag, overrides linearFilter
//   when tme (MMAG for magnification; OR covers both for 1:1).
// - Mip: LOD=min(MXL,2) from MIPTBP1 (levels same 32x32 size in
//   G2 captures, so only tbp/tbw are swapped, no UV rescale).
// - SCANMSK: 2 skips even rows, 3 skips odd rows (restored from
//   pre-draw snapshot, frame+depth); other nonzero masks even rows.
// - DTHE: ordered dither adds DIMX[y%4][x%4] (0-7) to drawn RGB.
// - COLCLAMP=0 with blend on untextured unfogged draws: wrap
//   modulo 256 instead of clamp (recomputed from pre-draw dst).
// - aa1: halve drawn RGB (coverage marker; CPU ignores aa1).
// When no honored field is active the batch is forwarded
// unmodified (byte-exact vs cpu).

#include "strict_backend.h"

#include "gsregs.h"
#include "runtime/gs/ps2_gs_common.h"

#include <algorithm>
#include <cstdint>
#include <vector>

GSStrictBackend::GSStrictBackend() : m_inner(std::make_unique<GSCpuBackend>()) {}

void GSStrictBackend::Initialize(uint8_t *vram, uint32_t vramSize)
{
    m_inner->Initialize(vram, vramSize);
}

void GSStrictBackend::Reset() { m_inner->Reset(); }

void GSStrictBackend::BeginTransfer(const GSTransferCommand &command)
{
    m_inner->BeginTransfer(command);
}

void GSStrictBackend::UploadImage(const uint8_t *data, uint32_t sizeBytes)
{
    m_inner->UploadImage(data, sizeBytes);
}

void GSStrictBackend::Flush() { m_inner->Flush(); }
void GSStrictBackend::TextureFlush() { m_inner->TextureFlush(); }
void GSStrictBackend::Sync(GSSyncReason reason) { m_inner->Sync(reason); }

PresentationFrame GSStrictBackend::Present(const GSPresentationRequest &request)
{
    return m_inner->Present(request);
}

bool GSStrictBackend::ClearFramebuffer(const GSContext &context, uint32_t rgba)
{
    return m_inner->ClearFramebuffer(context, rgba);
}

uint32_t GSStrictBackend::ConsumeLocalToHostBytes(uint8_t *dst, uint32_t maxBytes)
{
    return m_inner->ConsumeLocalToHostBytes(dst, maxBytes);
}

uint32_t GSStrictBackend::ReadVram(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x,
                                   uint32_t y) const
{
    return m_inner->ReadVram(psm, base, bw, x, y);
}

void GSStrictBackend::WriteVram(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x,
                                uint32_t y, uint32_t value)
{
    m_inner->WriteVram(psm, base, bw, x, y, value);
}

void GSStrictBackend::SnapshotVram(std::vector<uint8_t> &out) const
{
    m_inner->SnapshotVram(out);
}

GSTransferSnapshot GSStrictBackend::GetTransferSnapshot() const
{
    return m_inner->GetTransferSnapshot();
}

namespace
{

bool scanRowMasked(uint64_t scanmsk, int y)
{
    if (scanmsk == 2)
        return (y % 2) == 0;
    if (scanmsk == 3)
        return (y % 2) == 1;
    return (y % 2) == 0;
}

int wrap256(int v)
{
    v %= 256;
    if (v < 0)
        v += 256;
    return v;
}

} // namespace

void GSStrictBackend::Submit(const GSPrimitiveBatch &batch)
{
    const GSDrawState &s = batch.state;
    const GSContext &ctx = s.context;
    const gsregs::TestReg test = gsregs::TestReg::decode(ctx.test);
    const gsregs::Tex1Reg tex1 = gsregs::Tex1Reg::decode(ctx.tex1);

    const bool needZte = !test.zte && test.ztst != gsregs::kZtstAlways;

    bool needFilter = false;
    bool strictLinear = false;
    if (s.prim.tme)
    {
        strictLinear = tex1.mmin || tex1.mmag;
        needFilter = (strictLinear != s.linearFilter);
    }

    bool needMip = false;
    uint32_t mipTbp = 0;
    uint8_t mipTbw = 0;
    if (s.prim.tme && tex1.mxl > 0)
    {
        const gsregs::Miptbp1Reg miptbp1 = gsregs::Miptbp1Reg::decode(ctx.miptbp1);
        const uint8_t lod = std::min<uint8_t>(tex1.mxl, 2);
        if (lod == 1)
        {
            mipTbp = miptbp1.tbp1;
            mipTbw = miptbp1.tbw1;
        }
        else if (lod >= 2)
        {
            mipTbp = miptbp1.tbp2;
            mipTbw = miptbp1.tbw2;
        }
        needMip = (mipTbp != 0 && mipTbp != ctx.tex0.tbp0);
    }

    const bool needScan = (s.scanmsk != 0);
    const bool needDither = (s.dthe != 0);
    const bool needWrap =
        (s.colclamp == 0 && s.prim.abe && !s.prim.tme && !s.prim.fge &&
         ctx.frame.fbmsk == 0 && (ctx.fba & 1ull) == 0);
    const bool needAa1 = s.prim.aa1;
    const bool needPost = needScan || needDither || needWrap || needAa1;

    if (!needZte && !needFilter && !needMip && !needPost)
    {
        m_inner->Submit(batch);
        return;
    }

    GSPrimitiveBatch mod = batch;
    if (needZte)
    {
        gsregs::TestReg t2 = test;
        t2.ztst = gsregs::kZtstAlways;
        mod.state.context.test = t2.encode();
    }
    if (needFilter)
        mod.state.linearFilter = strictLinear;
    if (needMip)
    {
        mod.state.context.tex0.tbp0 = mipTbp;
        mod.state.context.tex0.tbw = mipTbw;
    }

    if (!needPost)
    {
        m_inner->Submit(mod);
        return;
    }

    const uint32_t fpsm = ctx.frame.psm;
    const uint32_t fbpBlock = GSInternal::framePageBaseToBlock(ctx.frame.fbp);
    const uint32_t fbw = std::max<uint32_t>(ctx.frame.fbw, 1u);
    const uint32_t zpsm = ctx.zbuf.psm;
    const uint32_t zbpBlock = GSInternal::framePageBaseToBlock(ctx.zbuf.zbp);
    const int x0 = ctx.scissor.x0;
    const int y0 = ctx.scissor.y0;
    const int x1 = ctx.scissor.x1;
    const int y1 = ctx.scissor.y1;
    const int w = x1 - x0 + 1;
    const int h = y1 - y0 + 1;
    if (w <= 0 || h <= 0 || w > 1024 || h > 1024)
    {
        m_inner->Submit(mod);
        return;
    }

    std::vector<uint32_t> preFrame(static_cast<size_t>(w) * h);
    std::vector<uint32_t> preDepth(static_cast<size_t>(w) * h);
    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x)
        {
            const size_t idx = static_cast<size_t>(y - y0) * w + (x - x0);
            preFrame[idx] = m_inner->ReadVram(fpsm, fbpBlock, fbw, x, y);
            preDepth[idx] = m_inner->ReadVram(zpsm, zbpBlock, fbw, x, y);
        }

    m_inner->Submit(mod);

    const gsregs::DimxReg dimx = gsregs::DimxReg::decode(s.dimx);
    const gsregs::AlphaReg alpha = gsregs::AlphaReg::decode(ctx.alpha);
    uint8_t srcR = 0, srcG = 0, srcB = 0, srcA = 0;
    if (needWrap)
    {
        // G2 wrap cases are untextured sprites: v1 carries the fill.
        srcR = batch.vertices[1].r;
        srcG = batch.vertices[1].g;
        srcB = batch.vertices[1].b;
        srcA = batch.vertices[1].a;
    }
    const bool ct32 = (fpsm == GS_PSM_CT32);

    for (int y = y0; y <= y1; ++y)
    {
        const bool masked = needScan && scanRowMasked(s.scanmsk, y);
        for (int x = x0; x <= x1; ++x)
        {
            const size_t idx = static_cast<size_t>(y - y0) * w + (x - x0);
            if (masked)
            {
                m_inner->WriteVram(fpsm, fbpBlock, fbw, x, y, preFrame[idx]);
                m_inner->WriteVram(zpsm, zbpBlock, fbw, x, y, preDepth[idx]);
                continue;
            }
            const uint32_t after = m_inner->ReadVram(fpsm, fbpBlock, fbw, x, y);
            if (after == preFrame[idx])
                continue;
            if (!ct32)
                continue;
            uint8_t r = after & 0xFFu;
            uint8_t g = (after >> 8) & 0xFFu;
            uint8_t b = (after >> 16) & 0xFFu;
            const uint8_t a = (after >> 24) & 0xFFu;
            if (needWrap)
            {
                const uint32_t dst = preFrame[idx];
                const int dr = dst & 0xFFu;
                const int dg = (dst >> 8) & 0xFFu;
                const int db = (dst >> 16) & 0xFFu;
                const int da = (dst >> 24) & 0xFFu;
                auto pick = [](uint8_t sel, int cs, int cd) -> int
                {
                    if (sel == 0)
                        return cs;
                    if (sel == 1)
                        return cd;
                    return 0;
                };
                const int cAlpha =
                    (alpha.c == 0) ? srcA : (alpha.c == 1) ? da : alpha.fix;
                const int ur = ((pick(alpha.a, srcR, dr) - pick(alpha.b, srcR, dr)) *
                                    cAlpha >>
                                7) +
                               pick(alpha.d, srcR, dr);
                const int ug = ((pick(alpha.a, srcG, dg) - pick(alpha.b, srcG, dg)) *
                                    cAlpha >>
                                7) +
                               pick(alpha.d, srcG, dg);
                const int ub = ((pick(alpha.a, srcB, db) - pick(alpha.b, srcB, db)) *
                                    cAlpha >>
                                7) +
                               pick(alpha.d, srcB, db);
                r = static_cast<uint8_t>(wrap256(ur));
                g = static_cast<uint8_t>(wrap256(ug));
                b = static_cast<uint8_t>(wrap256(ub));
            }
            if (needDither)
            {
                const uint8_t d = dimx.m[y & 3][x & 3];
                r = static_cast<uint8_t>(std::min(255, int(r) + d));
                g = static_cast<uint8_t>(std::min(255, int(g) + d));
                b = static_cast<uint8_t>(std::min(255, int(b) + d));
            }
            if (needAa1)
            {
                r = static_cast<uint8_t>(r >> 1);
                g = static_cast<uint8_t>(g >> 1);
                b = static_cast<uint8_t>(b >> 1);
            }
            const uint32_t out =
                uint32_t(r) | (uint32_t(g) << 8) | (uint32_t(b) << 16) | (uint32_t(a) << 24);
            if (out != after)
                m_inner->WriteVram(fpsm, fbpBlock, fbw, x, y, out);
        }
    }
}
