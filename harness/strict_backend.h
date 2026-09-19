#pragma once

// G2 strict backend: wraps GSCpuBackend and honors the G0-§9-ignored
// fields (TEX1 filtering + mip selection, DIMX/DTHE, COLCLAMP,
// SCANMSK, aa1, ZTE gating of ZTST). When all honored fields are at
// default/agreeing values the batch is forwarded unmodified, so
// non-touching captures stay byte-exact vs cpu (specificity). The
// cpu backend is untouched.

#include "runtime/gs/gs_backend.h"
#include "runtime/gs/gs_cpu_backend.h"

#include <memory>

class GSStrictBackend : public GSRasterBackend
{
public:
    GSStrictBackend();

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
    std::unique_ptr<GSCpuBackend> m_inner;
};
