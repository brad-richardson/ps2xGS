#include "recording_backend.h"

#include <vector>

GSRecordingBackend::GSRecordingBackend(std::unique_ptr<GSRasterBackend> inner,
                                       const std::string &capturePath, uint32_t vramSize)
    : m_inner(std::move(inner)), m_writer(std::make_unique<gscap::Writer>(capturePath, vramSize))
{
}

GSRecordingBackend::~GSRecordingBackend()
{
    finalize();
}

void GSRecordingBackend::finalize()
{
    if (m_writer)
        m_writer->finalize();
}

void GSRecordingBackend::Initialize(uint8_t *vram, uint32_t vramSize)
{
    m_inner->Initialize(vram, vramSize);
    m_writer->writeInitialize(vramSize);
}

void GSRecordingBackend::Reset()
{
    m_inner->Reset();
    m_writer->writeReset();
}

void GSRecordingBackend::Submit(const GSPrimitiveBatch &batch)
{
    m_inner->Submit(batch);
    m_writer->writeSubmit(batch);
}

void GSRecordingBackend::BeginTransfer(const GSTransferCommand &command)
{
    m_inner->BeginTransfer(command);
    m_writer->writeBeginTransfer(command);
}

void GSRecordingBackend::UploadImage(const uint8_t *data, uint32_t sizeBytes)
{
    m_inner->UploadImage(data, sizeBytes);
    m_writer->writeUpload(data, sizeBytes);
}

void GSRecordingBackend::Flush()
{
    m_inner->Flush();
    m_writer->writeFlush();
}

void GSRecordingBackend::TextureFlush()
{
    m_inner->TextureFlush();
    m_writer->writeTextureFlush();
}

void GSRecordingBackend::Sync(GSSyncReason reason)
{
    m_inner->Sync(reason);
    m_writer->writeSync(reason);
}

PresentationFrame GSRecordingBackend::Present(const GSPresentationRequest &request)
{
    PresentationFrame frame = m_inner->Present(request);
    std::vector<uint8_t> vram;
    m_inner->SnapshotVram(vram);
    m_writer->writePresent(request, frame, vram);
    return frame;
}

bool GSRecordingBackend::ClearFramebuffer(const GSContext &context, uint32_t rgba)
{
    const bool result = m_inner->ClearFramebuffer(context, rgba);
    m_writer->writeClear(context, rgba, result);
    return result;
}

uint32_t GSRecordingBackend::ConsumeLocalToHostBytes(uint8_t *dst, uint32_t maxBytes)
{
    std::vector<uint8_t> tmp(maxBytes);
    const uint32_t count = m_inner->ConsumeLocalToHostBytes(tmp.data(), maxBytes);
    if (count > 0 && dst != nullptr)
        std::copy(tmp.begin(), tmp.begin() + count, dst);
    m_writer->writeConsume(maxBytes, tmp.data(), count);
    return count;
}

uint32_t GSRecordingBackend::ReadVram(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x,
                                     uint32_t y) const
{
    const uint32_t result = m_inner->ReadVram(psm, base, bw, x, y);
    m_writer->writeReadVram(psm, base, bw, x, y, result);
    return result;
}

void GSRecordingBackend::WriteVram(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x, uint32_t y,
                                  uint32_t value)
{
    m_inner->WriteVram(psm, base, bw, x, y, value);
    m_writer->writeWriteVram(psm, base, bw, x, y, value);
}

void GSRecordingBackend::SnapshotVram(std::vector<uint8_t> &out) const
{
    m_inner->SnapshotVram(out);
    m_writer->writeSnapshot(out);
}

GSTransferSnapshot GSRecordingBackend::GetTransferSnapshot() const
{
    const GSTransferSnapshot snapshot = m_inner->GetTransferSnapshot();
    m_writer->writeTransferSnapshot(snapshot);
    return snapshot;
}
