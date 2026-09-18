// .gscap reader/writer implementation. Field order mirrors
// docs/gscap-format.md exactly; keep the two in sync.

#include "gscap.h"

#include <cstring>
#include <fstream>
#include <sstream>
#include <stdexcept>

namespace gscap
{
namespace
{

constexpr char kHeaderMagic[8] = {'G', 'S', 'C', 'A', 'P', '0', '0', '1'};
constexpr char kFooterMagic[8] = {'G', 'S', 'C', 'A', 'P', 'E', 'N', 'D'};

[[noreturn]] void fail(const std::string &what)
{
    throw std::runtime_error("gscap: " + what);
}

void putLe(std::ostream &os, uint64_t v, unsigned bytes, uint64_t &offset)
{
    for (unsigned i = 0; i < bytes; ++i)
        os.put(static_cast<char>((v >> (8u * i)) & 0xFFu));
    offset += bytes;
}

uint64_t getLe(const std::vector<uint8_t> &data, uint64_t pos, unsigned bytes)
{
    if (pos + bytes > data.size())
        fail("unexpected end of file");
    uint64_t v = 0;
    for (unsigned i = 0; i < bytes; ++i)
        v |= static_cast<uint64_t>(data[static_cast<size_t>(pos + i)]) << (8u * i);
    return v;
}

} // namespace

struct Writer::Stream
{
    std::ofstream os;
    uint64_t offset = 0;
    uint64_t recordStart = 0;
    uint32_t recordTag = 0;
};

Writer::Writer(const std::string &path, uint32_t vramSize) : m_vramSize(vramSize)
{
    m_stream = new Stream();
    m_stream->os.open(path, std::ios::binary | std::ios::trunc);
    if (!m_stream->os)
        fail("cannot open '" + path + "' for writing");
    m_stream->os.write(kHeaderMagic, 8);
    m_stream->offset += 8;
    putU32(kVersion);
    putU32(vramSize);
}

Writer::~Writer()
{
    try
    {
        finalize();
    }
    catch (...)
    {
    }
    delete m_stream;
}

void Writer::putU8(uint8_t v)
{
    m_stream->os.put(static_cast<char>(v));
    m_stream->offset += 1;
}

void Writer::putU16(uint16_t v) { putLe(m_stream->os, v, 2, m_stream->offset); }
void Writer::putU32(uint32_t v) { putLe(m_stream->os, v, 4, m_stream->offset); }
void Writer::putU64(uint64_t v) { putLe(m_stream->os, v, 8, m_stream->offset); }

void Writer::putF32(float v)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    putU32(bits);
}

void Writer::putF64(double v)
{
    uint64_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    putU64(bits);
}

void Writer::putBytes(const uint8_t *data, uint32_t size)
{
    if (size == 0)
        return;
    m_stream->os.write(reinterpret_cast<const char *>(data), size);
    m_stream->offset += size;
}

void Writer::putBytes(const std::vector<uint8_t> &data)
{
    putU32(static_cast<uint32_t>(data.size()));
    putBytes(data.data(), static_cast<uint32_t>(data.size()));
}

void Writer::beginRecord(uint32_t tag)
{
    m_stream->recordStart = m_stream->offset;
    m_stream->recordTag = tag;
    putU32(tag);
    putU32(0); // payload size placeholder, patched in endRecord
}

void Writer::endRecord(uint32_t tag)
{
    if (tag != m_stream->recordTag)
        fail("record tag mismatch on finalize");
    const uint64_t end = m_stream->offset;
    const uint32_t payloadSize = static_cast<uint32_t>(end - m_stream->recordStart - 8u);
    m_stream->os.seekp(static_cast<std::streamoff>(m_stream->recordStart + 4u));
    char sizeBytes[4];
    for (int i = 0; i < 4; ++i)
        sizeBytes[i] = static_cast<char>((payloadSize >> (8u * i)) & 0xFFu);
    m_stream->os.write(sizeBytes, 4);
    m_stream->os.seekp(static_cast<std::streamoff>(end));
    if (!m_stream->os)
        fail("failed writing record");
    if (tag == kPresent)
        m_presentOffsets.push_back(m_stream->recordStart);
}

void Writer::putVertex(const GSVertex &v)
{
    putF32(v.x);
    putF32(v.y);
    putF64(v.z);
    putU8(v.r);
    putU8(v.g);
    putU8(v.b);
    putU8(v.a);
    putF32(v.q);
    putF32(v.s);
    putF32(v.t);
    putU16(v.u);
    putU16(v.v);
    putU8(v.fog);
}

void Writer::putFrameReg(const GSFrameReg &r)
{
    putU32(r.fbp);
    putU32(r.fbw);
    putU8(r.psm);
    putU32(r.fbmsk);
}

void Writer::putZbufReg(const GSZbufReg &r)
{
    putU32(r.zbp);
    putU8(r.psm);
    putU8(r.zmask ? 1u : 0u);
}

void Writer::putScissorReg(const GSScissorReg &r)
{
    putU16(r.x0);
    putU16(r.x1);
    putU16(r.y0);
    putU16(r.y1);
}

void Writer::putTex0Reg(const GSTex0Reg &r)
{
    putU32(r.tbp0);
    putU8(r.tbw);
    putU8(r.psm);
    putU8(r.tw);
    putU8(r.th);
    putU8(r.tcc);
    putU8(r.tfx);
    putU32(r.cbp);
    putU8(r.cpsm);
    putU8(r.csm);
    putU8(r.csa);
    putU8(r.cld);
}

void Writer::putXYOffsetReg(const GSXYOffsetReg &r)
{
    putU16(r.ofx);
    putU16(r.ofy);
}

void Writer::putTexaReg(const GSTexaReg &r)
{
    putU8(r.ta0);
    putU8(r.aem ? 1u : 0u);
    putU8(r.ta1);
}

void Writer::putTexClutReg(const GSTexClutReg &r)
{
    putU8(r.cbw);
    putU8(r.cou);
    putU16(r.cov);
}

void Writer::putContext(const GSContext &c)
{
    putFrameReg(c.frame);
    putScissorReg(c.scissor);
    putTex0Reg(c.tex0);
    putXYOffsetReg(c.xyoffset);
    putZbufReg(c.zbuf);
    putU64(c.tex1);
    putU64(c.miptbp1);
    putU64(c.miptbp2);
    putU64(c.clamp);
    putU64(c.alpha);
    putU64(c.test);
    putU64(c.fba);
}

void Writer::putPrimReg(const GSPrimReg &r)
{
    putU8(static_cast<uint8_t>(r.type));
    putU8(r.iip ? 1u : 0u);
    putU8(r.tme ? 1u : 0u);
    putU8(r.fge ? 1u : 0u);
    putU8(r.abe ? 1u : 0u);
    putU8(r.aa1 ? 1u : 0u);
    putU8(r.fst ? 1u : 0u);
    putU8(r.ctxt ? 1u : 0u);
    putU8(r.fix ? 1u : 0u);
}

void Writer::putDrawState(const GSDrawState &s)
{
    putContext(s.context);
    putPrimReg(s.prim);
    putTexaReg(s.texa);
    putTexClutReg(s.texclut);
    putU8(s.pabe ? 1u : 0u);
    putU64(s.scanmsk);
    putU64(s.dimx);
    putU64(s.dthe);
    putU64(s.colclamp);
    putU8(s.fogR);
    putU8(s.fogG);
    putU8(s.fogB);
    putU16(s.textureWidth);
    putU16(s.textureHeight);
    putU8(s.linearFilter ? 1u : 0u);
}

void Writer::putBatch(const GSPrimitiveBatch &b)
{
    for (const GSVertex &v : b.vertices)
        putVertex(v);
    putU8(b.vertexCount);
    putDrawState(b.state);
}

void Writer::putTransferCommand(const GSTransferCommand &c)
{
    putU32(c.bitbltbuf.sbp);
    putU8(c.bitbltbuf.sbw);
    putU8(c.bitbltbuf.spsm);
    putU32(c.bitbltbuf.dbp);
    putU8(c.bitbltbuf.dbw);
    putU8(c.bitbltbuf.dpsm);
    putU16(c.trxpos.ssax);
    putU16(c.trxpos.ssay);
    putU16(c.trxpos.dsax);
    putU16(c.trxpos.dsay);
    putU8(c.trxpos.dir);
    putU16(c.trxreg.rrw);
    putU16(c.trxreg.rrh);
    putU32(c.direction);
}

void Writer::putPresentRequest(const GSPresentationRequest &r)
{
    putU64(r.pmode);
    putU64(r.smode2);
    putU64(r.dispfb1);
    putU64(r.display1);
    putU64(r.dispfb2);
    putU64(r.display2);
    putU64(r.bgcolor);
    putU64(r.vsyncTick);
    putFrameReg(r.contextFrames[0]);
    putFrameReg(r.contextFrames[1]);
    putFrameReg(r.preferredSource);
    putU32(r.preferredDestFbp);
    putU8(r.hasPreferredSource ? 1u : 0u);
}

void Writer::writeInitialize(uint32_t vramSize)
{
    beginRecord(kInitialize);
    putU32(vramSize);
    endRecord(kInitialize);
}

void Writer::writeReset()
{
    beginRecord(kReset);
    endRecord(kReset);
}

void Writer::writeSubmit(const GSPrimitiveBatch &batch)
{
    beginRecord(kSubmit);
    putBatch(batch);
    endRecord(kSubmit);
}

void Writer::writeBeginTransfer(const GSTransferCommand &cmd)
{
    beginRecord(kBeginTransfer);
    putTransferCommand(cmd);
    endRecord(kBeginTransfer);
}

void Writer::writeUpload(const uint8_t *data, uint32_t sizeBytes)
{
    beginRecord(kUploadImage);
    putU32(sizeBytes);
    putBytes(data, sizeBytes);
    endRecord(kUploadImage);
}

void Writer::writeFlush()
{
    beginRecord(kFlush);
    endRecord(kFlush);
}

void Writer::writeTextureFlush()
{
    beginRecord(kTextureFlush);
    endRecord(kTextureFlush);
}

void Writer::writeSync(GSSyncReason reason)
{
    beginRecord(kSync);
    putU8(static_cast<uint8_t>(reason));
    endRecord(kSync);
}

void Writer::writePresent(const GSPresentationRequest &request, const PresentationFrame &frame,
                          const std::vector<uint8_t> &vramSnapshot)
{
    beginRecord(kPresent);
    putPresentRequest(request);
    putU32(frame.width);
    putU32(frame.height);
    putU32(frame.displayFbp);
    putU32(frame.sourceFbp);
    putU8(frame.usedPreferred ? 1u : 0u);
    putBytes(frame.pixels);
    putU32(static_cast<uint32_t>(vramSnapshot.size()));
    putBytes(vramSnapshot.data(), static_cast<uint32_t>(vramSnapshot.size()));
    endRecord(kPresent);
}

void Writer::writeClear(const GSContext &context, uint32_t rgba, bool result)
{
    beginRecord(kClearFramebuffer);
    putContext(context);
    putU32(rgba);
    putU8(result ? 1u : 0u);
    endRecord(kClearFramebuffer);
}

void Writer::writeConsume(uint32_t maxBytes, const uint8_t *data, uint32_t count)
{
    beginRecord(kConsumeLocalToHost);
    putU32(maxBytes);
    putU32(count);
    putBytes(data, count);
    endRecord(kConsumeLocalToHost);
}

void Writer::writeReadVram(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x, uint32_t y,
                           uint32_t result)
{
    beginRecord(kReadVram);
    putU32(psm);
    putU32(base);
    putU32(bw);
    putU32(x);
    putU32(y);
    putU32(result);
    endRecord(kReadVram);
}

void Writer::writeWriteVram(uint32_t psm, uint32_t base, uint32_t bw, uint32_t x, uint32_t y,
                            uint32_t value)
{
    beginRecord(kWriteVram);
    putU32(psm);
    putU32(base);
    putU32(bw);
    putU32(x);
    putU32(y);
    putU32(value);
    endRecord(kWriteVram);
}

void Writer::writeSnapshot(const std::vector<uint8_t> &vram)
{
    beginRecord(kSnapshotVram);
    putU32(static_cast<uint32_t>(vram.size()));
    putBytes(vram.data(), static_cast<uint32_t>(vram.size()));
    endRecord(kSnapshotVram);
}

void Writer::writeTransferSnapshot(const GSTransferSnapshot &snapshot)
{
    beginRecord(kTransferSnapshot);
    putU32(snapshot.x);
    putU32(snapshot.y);
    putU32(snapshot.totalPixels);
    putU32(snapshot.copiedPixels);
    putU32(snapshot.direction);
    putU64(static_cast<uint64_t>(snapshot.localToHostPendingBytes));
    endRecord(kTransferSnapshot);
}

void Writer::finalize()
{
    if (m_finalized)
        return;
    m_finalized = true;
    const uint64_t indexOffset = m_stream->offset;
    beginRecord(kIndex);
    putU32(static_cast<uint32_t>(m_presentOffsets.size()));
    for (uint64_t off : m_presentOffsets)
        putU64(off);
    endRecord(kIndex);
    putU64(indexOffset);
    m_stream->os.write(kFooterMagic, 8);
    m_stream->offset += 8;
    m_stream->os.flush();
    if (!m_stream->os)
        fail("failed finalizing capture");
}

// ---- Reader ----

Reader::Cursor::Cursor(const std::vector<uint8_t> &data, uint64_t payloadOffset,
                       uint32_t payloadSize)
    : m_data(data), m_pos(payloadOffset), m_end(payloadOffset + payloadSize)
{
    if (m_end > data.size())
        fail("record extends past end of file");
}

uint8_t Reader::Cursor::getU8()
{
    if (m_pos + 1 > m_end)
        fail("unexpected end of record");
    return m_data[static_cast<size_t>(m_pos++)];
}

uint16_t Reader::Cursor::getU16()
{
    if (m_pos + 2 > m_end)
        fail("unexpected end of record");
    uint16_t v = static_cast<uint16_t>(getLe(m_data, m_pos, 2));
    m_pos += 2;
    return v;
}

uint32_t Reader::Cursor::getU32()
{
    if (m_pos + 4 > m_end)
        fail("unexpected end of record");
    uint32_t v = static_cast<uint32_t>(getLe(m_data, m_pos, 4));
    m_pos += 4;
    return v;
}

uint64_t Reader::Cursor::getU64()
{
    if (m_pos + 8 > m_end)
        fail("unexpected end of record");
    uint64_t v = getLe(m_data, m_pos, 8);
    m_pos += 8;
    return v;
}

float Reader::Cursor::getF32()
{
    uint32_t bits = getU32();
    float v = 0;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}

double Reader::Cursor::getF64()
{
    uint64_t bits = getU64();
    double v = 0;
    std::memcpy(&v, &bits, sizeof(v));
    return v;
}

void Reader::Cursor::getBytes(uint8_t *dst, uint32_t size)
{
    if (m_pos + size > m_end)
        fail("unexpected end of record");
    if (size > 0)
        std::memcpy(dst, m_data.data() + m_pos, size);
    m_pos += size;
}

void Reader::Cursor::getBytes(std::vector<uint8_t> &dst, uint32_t size)
{
    dst.resize(size);
    getBytes(dst.data(), size);
}

void Reader::Cursor::requireConsumed(const char *what) const
{
    if (m_pos != m_end)
        fail(std::string("trailing bytes in ") + what);
}

void Reader::Cursor::getVertex(GSVertex &v)
{
    v.x = getF32();
    v.y = getF32();
    v.z = getF64();
    v.r = getU8();
    v.g = getU8();
    v.b = getU8();
    v.a = getU8();
    v.q = getF32();
    v.s = getF32();
    v.t = getF32();
    v.u = getU16();
    v.v = getU16();
    v.fog = getU8();
}

void Reader::Cursor::getFrameReg(GSFrameReg &r)
{
    r.fbp = getU32();
    r.fbw = getU32();
    r.psm = getU8();
    r.fbmsk = getU32();
}

void Reader::Cursor::getZbufReg(GSZbufReg &r)
{
    r.zbp = getU32();
    r.psm = getU8();
    r.zmask = getU8() != 0;
}

void Reader::Cursor::getScissorReg(GSScissorReg &r)
{
    r.x0 = getU16();
    r.x1 = getU16();
    r.y0 = getU16();
    r.y1 = getU16();
}

void Reader::Cursor::getTex0Reg(GSTex0Reg &r)
{
    r.tbp0 = getU32();
    r.tbw = getU8();
    r.psm = getU8();
    r.tw = getU8();
    r.th = getU8();
    r.tcc = getU8();
    r.tfx = getU8();
    r.cbp = getU32();
    r.cpsm = getU8();
    r.csm = getU8();
    r.csa = getU8();
    r.cld = getU8();
}

void Reader::Cursor::getXYOffsetReg(GSXYOffsetReg &r)
{
    r.ofx = getU16();
    r.ofy = getU16();
}

void Reader::Cursor::getTexaReg(GSTexaReg &r)
{
    r.ta0 = getU8();
    r.aem = getU8() != 0;
    r.ta1 = getU8();
}

void Reader::Cursor::getTexClutReg(GSTexClutReg &r)
{
    r.cbw = getU8();
    r.cou = getU8();
    r.cov = getU16();
}

void Reader::Cursor::getContext(GSContext &c)
{
    getFrameReg(c.frame);
    getScissorReg(c.scissor);
    getTex0Reg(c.tex0);
    getXYOffsetReg(c.xyoffset);
    getZbufReg(c.zbuf);
    c.tex1 = getU64();
    c.miptbp1 = getU64();
    c.miptbp2 = getU64();
    c.clamp = getU64();
    c.alpha = getU64();
    c.test = getU64();
    c.fba = getU64();
}

void Reader::Cursor::getPrimReg(GSPrimReg &r)
{
    r.type = static_cast<GSPrimType>(getU8());
    r.iip = getU8() != 0;
    r.tme = getU8() != 0;
    r.fge = getU8() != 0;
    r.abe = getU8() != 0;
    r.aa1 = getU8() != 0;
    r.fst = getU8() != 0;
    r.ctxt = getU8() != 0;
    r.fix = getU8() != 0;
}

void Reader::Cursor::getDrawState(GSDrawState &s)
{
    getContext(s.context);
    getPrimReg(s.prim);
    getTexaReg(s.texa);
    getTexClutReg(s.texclut);
    s.pabe = getU8() != 0;
    s.scanmsk = getU64();
    s.dimx = getU64();
    s.dthe = getU64();
    s.colclamp = getU64();
    s.fogR = getU8();
    s.fogG = getU8();
    s.fogB = getU8();
    s.textureWidth = getU16();
    s.textureHeight = getU16();
    s.linearFilter = getU8() != 0;
}

void Reader::Cursor::getBatch(GSPrimitiveBatch &b)
{
    for (GSVertex &v : b.vertices)
        getVertex(v);
    b.vertexCount = getU8();
    getDrawState(b.state);
}

void Reader::Cursor::getTransferCommand(GSTransferCommand &c)
{
    c.bitbltbuf.sbp = getU32();
    c.bitbltbuf.sbw = getU8();
    c.bitbltbuf.spsm = getU8();
    c.bitbltbuf.dbp = getU32();
    c.bitbltbuf.dbw = getU8();
    c.bitbltbuf.dpsm = getU8();
    c.trxpos.ssax = getU16();
    c.trxpos.ssay = getU16();
    c.trxpos.dsax = getU16();
    c.trxpos.dsay = getU16();
    c.trxpos.dir = getU8();
    c.trxreg.rrw = getU16();
    c.trxreg.rrh = getU16();
    c.direction = getU32();
}

void Reader::Cursor::getPresentRequest(GSPresentationRequest &r)
{
    r.pmode = getU64();
    r.smode2 = getU64();
    r.dispfb1 = getU64();
    r.display1 = getU64();
    r.dispfb2 = getU64();
    r.display2 = getU64();
    r.bgcolor = getU64();
    r.vsyncTick = getU64();
    getFrameReg(r.contextFrames[0]);
    getFrameReg(r.contextFrames[1]);
    getFrameReg(r.preferredSource);
    r.preferredDestFbp = getU32();
    r.hasPreferredSource = getU8() != 0;
}

Reader::Reader(const std::string &path)
{
    std::ifstream is(path, std::ios::binary);
    if (!is)
        fail("cannot open '" + path + "' for reading");
    is.seekg(0, std::ios::end);
    const int64_t fileSize = is.tellg();
    if (fileSize < 48)
        fail("file too small to be a capture");
    is.seekg(0, std::ios::beg);
    m_data.resize(static_cast<size_t>(fileSize));
    is.read(reinterpret_cast<char *>(m_data.data()), fileSize);
    if (!is)
        fail("failed reading '" + path + "'");

    if (std::memcmp(m_data.data(), kHeaderMagic, 8) != 0)
        fail("bad header magic");
    if (getLe(m_data, 8, 4) != kVersion)
        fail("unsupported version");
    m_vramSize = static_cast<uint32_t>(getLe(m_data, 12, 4));

    // Footer: indexRecordOffset u64 + "GSCAPEND".
    const uint64_t footerPos = static_cast<uint64_t>(fileSize) - 16u;
    if (std::memcmp(m_data.data() + footerPos + 8, kFooterMagic, 8) != 0)
        fail("bad footer magic (file truncated?)");
    const uint64_t indexOffset = getLe(m_data, footerPos, 8);

    // Scan the record directory up to the index record.
    uint64_t pos = 16u;
    while (pos < indexOffset)
    {
        if (pos + 8 > m_data.size())
            fail("record header past end of file");
        const uint32_t tag = static_cast<uint32_t>(getLe(m_data, pos, 4));
        const uint32_t size = static_cast<uint32_t>(getLe(m_data, pos + 4, 4));
        if (tag == kIndex)
            fail("index record inside record stream");
        m_records.push_back(RecordRef{tag, pos, size});
        pos += 8u + size;
    }
    if (pos != indexOffset)
        fail("record stream does not end at index record");

    // Parse the index record.
    const uint32_t indexTag = static_cast<uint32_t>(getLe(m_data, indexOffset, 4));
    const uint32_t indexSize = static_cast<uint32_t>(getLe(m_data, indexOffset + 4, 4));
    if (indexTag != kIndex)
        fail("missing index record");
    Cursor idx(m_data, indexOffset + 8u, indexSize);
    const uint32_t count = idx.getU32();
    for (uint32_t k = 0; k < count; ++k)
        m_presentOffsets.push_back(idx.getU64());
    idx.requireConsumed("index record");
}

Reader::Cursor Reader::cursorFor(size_t i, uint32_t wantTag, const char *what) const
{
    const RecordRef &ref = m_records.at(i);
    if (ref.tag != wantTag)
        fail(std::string("record #") + std::to_string(i) + " is not " + what);
    return Cursor(m_data, ref.offset + 8u, ref.size);
}

void Reader::getInitialize(size_t i, uint32_t &vramSize) const
{
    Cursor c = cursorFor(i, kInitialize, "Initialize");
    vramSize = c.getU32();
    c.requireConsumed("Initialize");
}

void Reader::getSubmit(size_t i, GSPrimitiveBatch &batch) const
{
    Cursor c = cursorFor(i, kSubmit, "Submit");
    c.getBatch(batch);
    c.requireConsumed("Submit");
}

void Reader::getBeginTransfer(size_t i, GSTransferCommand &cmd) const
{
    Cursor c = cursorFor(i, kBeginTransfer, "BeginTransfer");
    c.getTransferCommand(cmd);
    c.requireConsumed("BeginTransfer");
}

void Reader::getUpload(size_t i, std::vector<uint8_t> &data) const
{
    Cursor c = cursorFor(i, kUploadImage, "UploadImage");
    const uint32_t size = c.getU32();
    c.getBytes(data, size);
    c.requireConsumed("UploadImage");
}

void Reader::getSync(size_t i, GSSyncReason &reason) const
{
    Cursor c = cursorFor(i, kSync, "Sync");
    reason = static_cast<GSSyncReason>(c.getU8());
    c.requireConsumed("Sync");
}

void Reader::getPresent(size_t i, GSPresentationRequest &request, PresentationFrame &frame,
                        std::vector<uint8_t> &vramSnapshot) const
{
    Cursor c = cursorFor(i, kPresent, "Present");
    c.getPresentRequest(request);
    frame.width = c.getU32();
    frame.height = c.getU32();
    frame.displayFbp = c.getU32();
    frame.sourceFbp = c.getU32();
    frame.usedPreferred = c.getU8() != 0;
    uint32_t pixelBytes = c.getU32();
    c.getBytes(frame.pixels, pixelBytes);
    uint32_t vramBytes = c.getU32();
    c.getBytes(vramSnapshot, vramBytes);
    c.requireConsumed("Present");
}

void Reader::getClear(size_t i, GSContext &context, uint32_t &rgba, bool &result) const
{
    Cursor c = cursorFor(i, kClearFramebuffer, "ClearFramebuffer");
    c.getContext(context);
    rgba = c.getU32();
    result = c.getU8() != 0;
    c.requireConsumed("ClearFramebuffer");
}

void Reader::getConsume(size_t i, uint32_t &maxBytes, std::vector<uint8_t> &data) const
{
    Cursor c = cursorFor(i, kConsumeLocalToHost, "ConsumeLocalToHostBytes");
    maxBytes = c.getU32();
    const uint32_t count = c.getU32();
    c.getBytes(data, count);
    c.requireConsumed("ConsumeLocalToHostBytes");
}

void Reader::getReadVram(size_t i, uint32_t args[5], uint32_t &result) const
{
    Cursor c = cursorFor(i, kReadVram, "ReadVram");
    for (int k = 0; k < 5; ++k)
        args[k] = c.getU32();
    result = c.getU32();
    c.requireConsumed("ReadVram");
}

void Reader::getWriteVram(size_t i, uint32_t args[6]) const
{
    Cursor c = cursorFor(i, kWriteVram, "WriteVram");
    for (int k = 0; k < 6; ++k)
        args[k] = c.getU32();
    c.requireConsumed("WriteVram");
}

void Reader::getSnapshot(size_t i, std::vector<uint8_t> &vram) const
{
    Cursor c = cursorFor(i, kSnapshotVram, "SnapshotVram");
    const uint32_t size = c.getU32();
    c.getBytes(vram, size);
    c.requireConsumed("SnapshotVram");
}

void Reader::getTransferSnapshot(size_t i, GSTransferSnapshot &snapshot) const
{
    Cursor c = cursorFor(i, kTransferSnapshot, "GetTransferSnapshot");
    snapshot.x = c.getU32();
    snapshot.y = c.getU32();
    snapshot.totalPixels = c.getU32();
    snapshot.copiedPixels = c.getU32();
    snapshot.direction = c.getU32();
    snapshot.localToHostPendingBytes = static_cast<size_t>(c.getU64());
    c.requireConsumed("GetTransferSnapshot");
}

} // namespace gscap
