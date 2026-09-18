#pragma once

// .gscap capture reader/writer. See docs/gscap-format.md for the format.
// The writer streams (no whole-file buffering); the reader loads the file
// and builds a record directory plus the trailing present index.

#include "runtime/gs/gs_backend.h"

#include <cstdint>
#include <string>
#include <vector>

namespace gscap
{

inline constexpr uint32_t kVersion = 1;
inline constexpr uint32_t kVramSize = 4u * 1024u * 1024u;

enum Tag : uint32_t
{
    kInitialize = 1,
    kReset = 2,
    kSubmit = 3,
    kBeginTransfer = 4,
    kUploadImage = 5,
    kFlush = 6,
    kTextureFlush = 7,
    kSync = 8,
    kPresent = 9,
    kClearFramebuffer = 10,
    kConsumeLocalToHost = 11,
    kReadVram = 12,
    kWriteVram = 13,
    kSnapshotVram = 14,
    kTransferSnapshot = 15,
    kIndex = 0xFFFFFFFEu,
};

struct RecordRef
{
    uint32_t tag = 0;
    uint64_t offset = 0; // file offset of the record's tag field
    uint32_t size = 0;   // payload size in bytes
};

class Writer
{
public:
    explicit Writer(const std::string &path, uint32_t vramSize);
    ~Writer();

    Writer(const Writer &) = delete;
    Writer &operator=(const Writer &) = delete;

    void writeInitialize(uint32_t vramSize);
    void writeReset();
    void writeSubmit(const GSPrimitiveBatch &batch);
    void writeBeginTransfer(const GSTransferCommand &cmd);
    void writeUpload(const uint8_t *data, uint32_t sizeBytes);
    void writeFlush();
    void writeTextureFlush();
    void writeSync(GSSyncReason reason);
    void writePresent(const GSPresentationRequest &request,
                      const PresentationFrame &frame,
                      const std::vector<uint8_t> &vramSnapshot);
    void writeClear(const GSContext &context, uint32_t rgba, bool result);
    void writeConsume(uint32_t maxBytes, const uint8_t *data, uint32_t count);
    void writeReadVram(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x, uint32_t y,
                       uint32_t result);
    void writeWriteVram(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x, uint32_t y,
                        uint32_t value);
    void writeSnapshot(const std::vector<uint8_t> &vram);
    void writeTransferSnapshot(const GSTransferSnapshot &snapshot);

    // Writes the present index + footer. Also called by the destructor.
    void finalize();

private:
    void beginRecord(uint32_t tag);
    void endRecord(uint32_t tag);

    void putU8(uint8_t v);
    void putU16(uint16_t v);
    void putU32(uint32_t v);
    void putU64(uint64_t v);
    void putF32(float v);
    void putF64(double v);
    void putBytes(const uint8_t *data, uint32_t size);
    void putBytes(const std::vector<uint8_t> &data);

    void putVertex(const GSVertex &v);
    void putFrameReg(const GSFrameReg &r);
    void putZbufReg(const GSZbufReg &r);
    void putScissorReg(const GSScissorReg &r);
    void putTex0Reg(const GSTex0Reg &r);
    void putXYOffsetReg(const GSXYOffsetReg &r);
    void putTexaReg(const GSTexaReg &r);
    void putTexClutReg(const GSTexClutReg &r);
    void putContext(const GSContext &c);
    void putPrimReg(const GSPrimReg &r);
    void putDrawState(const GSDrawState &s);
    void putBatch(const GSPrimitiveBatch &b);
    void putTransferCommand(const GSTransferCommand &c);
    void putPresentRequest(const GSPresentationRequest &r);

    struct Stream;
    Stream *m_stream = nullptr; // pimpl: std::ofstream + offset bookkeeping
    uint32_t m_vramSize = 0;
    std::vector<uint64_t> m_presentOffsets;
    bool m_finalized = false;
};

class Reader
{
public:
    explicit Reader(const std::string &path);

    uint32_t vramSize() const { return m_vramSize; }
    size_t recordCount() const { return m_records.size(); }
    const RecordRef &recordAt(size_t i) const { return m_records.at(i); }
    const std::vector<uint64_t> &presentOffsets() const { return m_presentOffsets; }

    void getInitialize(size_t i, uint32_t &vramSize) const;
    void getSubmit(size_t i, GSPrimitiveBatch &batch) const;
    void getBeginTransfer(size_t i, GSTransferCommand &cmd) const;
    void getUpload(size_t i, std::vector<uint8_t> &data) const;
    void getSync(size_t i, GSSyncReason &reason) const;
    void getPresent(size_t i, GSPresentationRequest &request, PresentationFrame &frame,
                    std::vector<uint8_t> &vramSnapshot) const;
    void getClear(size_t i, GSContext &context, uint32_t &rgba, bool &result) const;
    void getConsume(size_t i, uint32_t &maxBytes, std::vector<uint8_t> &data) const;
    void getReadVram(size_t i, uint32_t args[5], uint32_t &result) const;
    void getWriteVram(size_t i, uint32_t args[6]) const;
    void getSnapshot(size_t i, std::vector<uint8_t> &vram) const;
    void getTransferSnapshot(size_t i, GSTransferSnapshot &snapshot) const;

private:
    class Cursor
    {
    public:
        Cursor(const std::vector<uint8_t> &data, uint64_t payloadOffset, uint32_t payloadSize);
        uint8_t getU8();
        uint16_t getU16();
        uint32_t getU32();
        uint64_t getU64();
        float getF32();
        double getF64();
        void getBytes(uint8_t *dst, uint32_t size);
        void getBytes(std::vector<uint8_t> &dst, uint32_t size);
        void requireConsumed(const char *what) const;

        void getVertex(GSVertex &v);
        void getFrameReg(GSFrameReg &r);
        void getZbufReg(GSZbufReg &r);
        void getScissorReg(GSScissorReg &r);
        void getTex0Reg(GSTex0Reg &r);
        void getXYOffsetReg(GSXYOffsetReg &r);
        void getTexaReg(GSTexaReg &r);
        void getTexClutReg(GSTexClutReg &r);
        void getContext(GSContext &c);
        void getPrimReg(GSPrimReg &r);
        void getDrawState(GSDrawState &s);
        void getBatch(GSPrimitiveBatch &b);
        void getTransferCommand(GSTransferCommand &c);
        void getPresentRequest(GSPresentationRequest &r);

    private:
        const std::vector<uint8_t> &m_data;
        uint64_t m_pos = 0;
        uint64_t m_end = 0;
    };

    Cursor cursorFor(size_t i, uint32_t wantTag, const char *what) const;

    std::vector<uint8_t> m_data;
    uint32_t m_vramSize = 0;
    std::vector<RecordRef> m_records;
    std::vector<uint64_t> m_presentOffsets;
};

} // namespace gscap
