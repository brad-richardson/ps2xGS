#pragma once

// GSRecordingBackend: a GSRasterBackend that owns an inner backend,
// forwards every call, and appends the record to a .gscap file.
// The writer streams; finalization happens in the destructor (or
// explicitly via finalize()).

#include "gscap.h"
#include "runtime/gs/gs_backend.h"

#include <memory>
#include <string>

class GSRecordingBackend final : public GSRasterBackend
{
public:
    // Takes ownership of |inner|. Opens |capturePath| for writing with
    // the given VRAM size in the file header.
    GSRecordingBackend(std::unique_ptr<GSRasterBackend> inner, const std::string &capturePath,
                       uint32_t vramSize = gscap::kVramSize);
    ~GSRecordingBackend() override;

    GSRecordingBackend(const GSRecordingBackend &) = delete;
    GSRecordingBackend &operator=(const GSRecordingBackend &) = delete;

    void finalize();

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
    void WriteVram(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x, uint32_t y,
                   uint32_t value) override;
    void SnapshotVram(std::vector<uint8_t> &out) const override;
    GSTransferSnapshot GetTransferSnapshot() const override;

private:
    std::unique_ptr<GSRasterBackend> m_inner;
    std::unique_ptr<gscap::Writer> m_writer;
};
