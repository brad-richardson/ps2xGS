#pragma once

// G3 steps 2-3: spike backend. Fork of upstream
// runtime/gs/gs_cpu_backend.h (same pinned upstream revision; the
// upstream file is untouched) with the two upstream TODO spikes:
//   - CLUT cache (LookupCLUT): per-Submit memo of resolved palette
//     entries. Within one Submit every LookupCLUT parameter except
//     the texel index is fixed (single batch state), so a 256-entry
//     generation-tagged table is exact unless a draw writes its own
//     CLUT footprint mid-batch (no capture does; identity tables
//     are the proof).
//   - RMW single address lookup (WritePixel, step 3): the frmw
//     read+write pair shares one computed VRAM address (CT32 fast
//     path; other PSMs keep the two-call path).

#include "runtime/gs/gs_backend.h"

#include <array>
#include <cstdint>
#include <functional>
#include <mutex>
#include <vector>

class GSSpikeBackend final : public GSRasterBackend
{
public:
    GSSpikeBackend();

    void Initialize(uint8_t *vram, uint32_t vramSize) override;
    void Reset() override;

    void Submit(const GSPrimitiveBatch &batch) override;
    void BeginTransfer(const GSTransferCommand &command) override;
    void UploadImage(const uint8_t *data, uint32_t sizeBytes) override;

    void Flush() override;
    void TextureFlush() override;
    void Sync(GSSyncReason reason) override;
    PresentationFrame Present(const GSPresentationRequest &request) override;

    bool ClearFramebuffer(const GSContext &context, uint32_t rgba) override;
    uint32_t ConsumeLocalToHostBytes(uint8_t *dst, uint32_t maxBytes) override;

    uint32_t ReadVram(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x, uint32_t y) const override;
    void WriteVram(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x, uint32_t y, uint32_t value) override;
    void SnapshotVram(std::vector<uint8_t> &out) const override;
    GSTransferSnapshot GetTransferSnapshot() const override;

private:
    void ResetUnlocked();
    uint32_t ReadVramUnlocked(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x, uint32_t y) const;
    void WriteVramUnlocked(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x, uint32_t y, uint32_t value);

    void DrawPrimitive(const GSPrimitiveBatch &batch);
    void DrawSprite(const GSPrimitiveBatch &batch);
    void DrawTriangle(const GSPrimitiveBatch &batch);
    void DrawLine(const GSPrimitiveBatch &batch);
    void WritePixel(const GSDrawState &state, int x, int y, int z, uint8_t r, uint8_t g, uint8_t b, uint8_t a, uint8_t fog);
    uint32_t SampleTexture(const GSDrawState &state, float s, float t, float q, uint16_t u, uint16_t v);
    uint32_t LookupCLUT(const GSDrawState &state, uint8_t index, uint32_t cbp, uint8_t cpsm, uint8_t csm, uint8_t csa, uint8_t sourcePsm);

    void PerformLocalToLocalTransfer();
    void PerformLocalToHostTransfer();
    PresentationFrame PresentFromLocalMemory(const GSPresentationRequest &request);
    bool CopyFrameToHostRgba(const GSFrameReg &frame,
                             uint32_t width,
                             uint32_t height,
                             std::vector<uint8_t> &outPixels,
                             bool preserveAlpha,
                             bool useLocalMemoryLayout,
                             bool frameBaseIsPages,
                             uint32_t sourceOriginX,
                             uint32_t sourceOriginY) const;

    using WriteVramFunc = std::function<void(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t)>;
    using ReadVramFunc = std::function<uint32_t(uint8_t *, uint32_t, uint32_t, uint32_t, uint32_t)>;

    static constexpr size_t kPsmHandlerCount = 1u << 6u;
    mutable std::mutex m_mutex;
    uint8_t *m_vram = nullptr;
    uint32_t m_vramSize = 0;
    std::array<ReadVramFunc, kPsmHandlerCount> m_readVramFuncs{};
    std::array<WriteVramFunc, kPsmHandlerCount> m_writeVramFuncs{};

    GSTransferCommand m_transfer{};
    GSTransferSnapshot m_transferState{};
    std::vector<uint8_t> m_localToHostBuffer;
    size_t m_localToHostReadPos = 0;

    // G3 CLUT cache: per-Submit memo keyed by raw texel index.
    void invalidateClutCache();
    std::array<uint32_t, 256> m_clutCache{};
    std::array<uint32_t, 256> m_clutTag{};
    uint32_t m_clutGen = 0; // 0 = invalid; bumped per Submit/TextureFlush/Reset
};
