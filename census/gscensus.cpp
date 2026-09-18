// gscensus: feature census over .gscap captures. Emits census.json
// plus a Markdown table set, ranked by frequency, using the gsregs.h
// decoders. Usage:
//   gscensus <capture...> --json census.json --md census.md

#include "gscap.h"
#include "gsregs.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{

std::string psmName(uint8_t psm)
{
    switch (psm)
    {
    case GS_PSM_CT32:
        return "CT32";
    case GS_PSM_CT24:
        return "CT24";
    case GS_PSM_CT16:
        return "CT16";
    case GS_PSM_CT16S:
        return "CT16S";
    case GS_PSM_T8:
        return "T8";
    case GS_PSM_T4:
        return "T4";
    case GS_PSM_T8H:
        return "T8H";
    case GS_PSM_T4HL:
        return "T4HL";
    case GS_PSM_T4HH:
        return "T4HH";
    case GS_PSM_Z32:
        return "Z32";
    case GS_PSM_Z24:
        return "Z24";
    case GS_PSM_Z16:
        return "Z16";
    case GS_PSM_Z16S:
        return "Z16S";
    default:
        return "PSM" + std::to_string(psm);
    }
}

std::string primName(GSPrimType t)
{
    switch (t)
    {
    case GS_PRIM_POINT:
        return "point";
    case GS_PRIM_LINE:
        return "line";
    case GS_PRIM_LINESTRIP:
        return "linestrip";
    case GS_PRIM_TRIANGLE:
        return "trilist";
    case GS_PRIM_TRISTRIP:
        return "tristrip";
    case GS_PRIM_TRIFAN:
        return "trifan";
    case GS_PRIM_SPRITE:
        return "sprite";
    default:
        return "prim" + std::to_string(static_cast<int>(t));
    }
}

const char *kAtstNames[8] = {"NEVER", "ALWAYS", "LESS",  "LEQUAL",
                             "EQUAL", "GEQUAL", "GREATER", "NOTEQUAL"};
const char *kAfailNames[4] = {"KEEP", "FB_ONLY", "ZB_ONLY", "RGB_ONLY"};
const char *kZtstNames[4] = {"NEVER", "ALWAYS", "GEQUAL", "GREATER"};
const char *kWrapNames[4] = {"REPEAT", "CLAMP", "REGION_CLAMP", "REGION_REPEAT"};

struct Census
{
    int files = 0;
    int submits = 0;
    int presents = 0;
    long long primPixels = 0;

    std::map<std::string, long long> primType;
    std::map<std::string, long long> primTypePixels;
    std::map<std::string, long long> primFlags; // flag=1 -> count
    std::map<std::string, long long> texPsm;
    std::map<std::string, long long> texSize; // "WxH"
    std::map<std::string, long long> clut;    // "cpsm/csm/csa"
    std::map<std::string, long long> tex1;    // "mmin/mmag/mxl"
    std::map<std::string, long long> clamp;   // "wms/wmt"
    std::map<std::string, long long> texa;    // "aem0"/"aem1"
    std::map<std::string, long long> alpha;   // "A/B/C/D/fix"
    std::map<std::string, long long> blendMisc; // pabe/fba/colclamp0/dthe
    std::map<std::string, long long> atst;
    std::map<std::string, long long> afail;
    std::map<std::string, long long> dateDatm; // "date/datm"
    std::map<std::string, long long> ztst;
    std::map<std::string, long long> zbufPsm;
    long long zmsk = 0;
    std::map<std::string, long long> framePsm;
    long long fbmskNonzero = 0;
    std::map<std::string, long long> scissor; // "WxH"
    std::map<std::string, long long> scanmsk;
    long long fogPrims = 0;
    std::map<std::string, long long> transferDir;
    std::map<std::string, long long> transferPsm;
    long long transferCount = 0;
    long long uploadBytes = 0;
    long long clears = 0;
    long long consumeCalls = 0;
    long long consumeBytes = 0;
    long long readVramCalls = 0;
    long long writeVramCalls = 0;
    std::map<std::string, long long> pmode;  // "en1/en2/mmod/amod"
    std::map<std::string, long long> smode2; // "prog"/"field"/"frame"
    std::map<std::string, long long> dispfbPsm;
    std::map<std::string, long long> presentSize; // "WxH"

    void bump(std::map<std::string, long long> &m, const std::string &k)
    {
        m[k]++;
    }

    void addSubmit(const GSPrimitiveBatch &b)
    {
        submits++;
        const GSDrawState &s = b.state;
        const std::string pt = primName(s.prim.type);
        bump(primType, pt);
        long long area = 0;
        const GSVertex &v0 = b.vertices[0];
        const GSVertex &v1 = b.vertices[1];
        const GSVertex &v2 = b.vertices[2];
        switch (s.prim.type)
        {
        case GS_PRIM_POINT:
            area = 1;
            break;
        case GS_PRIM_LINE:
        case GS_PRIM_LINESTRIP:
            area = static_cast<long long>(std::abs(v1.x - v0.x) + std::abs(v1.y - v0.y));
            break;
        case GS_PRIM_SPRITE:
            area = static_cast<long long>(std::abs(v1.x - v0.x) * std::abs(v1.y - v0.y));
            break;
        default:
        {
            const float w =
                std::max({v0.x, v1.x, v2.x}) - std::min({v0.x, v1.x, v2.x});
            const float h =
                std::max({v0.y, v1.y, v2.y}) - std::min({v0.y, v1.y, v2.y});
            area = static_cast<long long>(w * h / 2);
            break;
        }
        }
        primPixels += area;
        primTypePixels[pt] += area;

        if (s.prim.iip)
            bump(primFlags, "iip");
        if (s.prim.tme)
            bump(primFlags, "tme");
        if (s.prim.fge)
            bump(primFlags, "fge");
        if (s.prim.abe)
            bump(primFlags, "abe");
        if (s.prim.aa1)
            bump(primFlags, "aa1");
        if (s.prim.fst)
            bump(primFlags, "fst");
        if (s.prim.fix)
            bump(primFlags, "fix");
        if (s.prim.ctxt)
            bump(primFlags, "ctxt");
        if (s.pabe)
            bump(primFlags, "pabe");

        const GSContext &c = s.context;
        if (s.prim.tme)
        {
            bump(texPsm, psmName(c.tex0.psm));
            bump(texSize, std::to_string(s.textureWidth) + "x" +
                             std::to_string(s.textureHeight));
            if (c.tex0.psm == GS_PSM_T8 || c.tex0.psm == GS_PSM_T8H ||
                c.tex0.psm == GS_PSM_T4 || c.tex0.psm == GS_PSM_T4HL ||
                c.tex0.psm == GS_PSM_T4HH)
                bump(clut, psmName(c.tex0.cpsm) + "/csm" + std::to_string(c.tex0.csm) +
                               "/csa" + std::to_string(c.tex0.csa));
            const gsregs::Tex1Reg t1 = gsregs::Tex1Reg::decode(c.tex1);
            bump(tex1, "mmin" + std::to_string(t1.mmin) + "/mmag" + std::to_string(t1.mmag) +
                           "/mxl" + std::to_string(t1.mxl));
            const gsregs::ClampReg cl = gsregs::ClampReg::decode(c.clamp);
            bump(clamp, std::string(kWrapNames[cl.wms]) + "/" + kWrapNames[cl.wmt]);
            bump(texa, s.texa.aem ? "aem1" : "aem0");
        }
        if (s.prim.abe || s.pabe)
        {
            const gsregs::AlphaReg a = gsregs::AlphaReg::decode(c.alpha);
            bump(alpha, "A" + std::to_string(a.a) + "/B" + std::to_string(a.b) + "/C" +
                            std::to_string(a.c) + "/D" + std::to_string(a.d) + "/fix" +
                            std::to_string(a.fix));
        }
        if (s.pabe)
            bump(blendMisc, "pabe");
        if ((c.fba & 1ull) != 0)
            bump(blendMisc, "fba");
        if (s.colclamp == 0)
            bump(blendMisc, "colclamp0");
        if (s.dthe != 0)
            bump(blendMisc, "dthe");

        const gsregs::TestReg t = gsregs::TestReg::decode(c.test);
        if (t.ate)
            bump(atst, kAtstNames[t.atst]);
        else
            bump(atst, "OFF");
        if (t.ate && t.afail != 0)
            bump(afail, kAfailNames[t.afail]);
        bump(dateDatm, std::string(t.date ? "date1" : "date0") + "/" +
                           (t.datm ? "datm1" : "datm0"));
        if (t.zte)
            bump(ztst, kZtstNames[t.ztst]);
        else
            bump(ztst, "OFF");
        bump(zbufPsm, psmName(c.zbuf.psm));
        if (c.zbuf.zmask)
            zmsk++;
        bump(framePsm, psmName(c.frame.psm));
        if (c.frame.fbmsk != 0)
            fbmskNonzero++;
        bump(scissor, std::to_string(int(c.scissor.x1) - int(c.scissor.x0) + 1) + "x" +
                          std::to_string(int(c.scissor.y1) - int(c.scissor.y0) + 1));
        if (s.scanmsk != 0)
            bump(scanmsk, "scanmsk" + std::to_string(s.scanmsk));
        if (s.prim.fge)
            fogPrims++;
    }

    void addTransfer(const GSTransferCommand &cmd)
    {
        transferCount++;
        const char *dir = "?";
        if (cmd.direction == 0)
            dir = "host->local";
        else if (cmd.direction == 1)
            dir = "local->host";
        else if (cmd.direction == 2)
            dir = "local->local";
        bump(transferDir, dir);
        uint8_t psm = cmd.bitbltbuf.dpsm;
        if (cmd.direction == 1)
            psm = cmd.bitbltbuf.spsm;
        else if (cmd.direction == 2)
            psm = cmd.bitbltbuf.spsm;
        bump(transferPsm, psmName(psm) + (cmd.direction == 2 ? "+local-local" : ""));
    }

    void addPresent(const GSPresentationRequest &req, const PresentationFrame &frame)
    {
        presents++;
        const gsregs::PmodeReg pm = gsregs::PmodeReg::decode(req.pmode);
        bump(pmode, std::string(pm.en1 ? "en1" : "--") + "/" +
                        (pm.en2 ? "en2" : "--") + (pm.mmod ? "/mmod" : "") +
                        (pm.amod ? "/amod" : ""));
        const gsregs::Smode2Reg sm = gsregs::Smode2Reg::decode(req.smode2);
        bump(smode2, !sm.interlaced ? "progressive" : (sm.frameMode ? "frame" : "field"));
        const gsregs::DispfbReg df =
            gsregs::DispfbReg::decode(pm.en2 && req.dispfb2 != 0 ? req.dispfb2 : req.dispfb1);
        bump(dispfbPsm, psmName(df.psm));
        bump(presentSize,
             std::to_string(frame.width) + "x" + std::to_string(frame.height));
    }
};

void writeTable(std::ostream &os, const std::string &title,
                const std::map<std::string, long long> &m)
{
    std::vector<std::pair<std::string, long long>> rows(m.begin(), m.end());
    std::sort(rows.begin(), rows.end(),
              [](const auto &a, const auto &b) { return a.second > b.second; });
    os << "### " << title << "\n\n| value | count |\n| --- | --- |\n";
    for (const auto &[k, v] : rows)
        os << "| " << k << " | " << v << " |\n";
    os << "\n";
}

void writeJsonMap(std::ostream &os, const std::map<std::string, long long> &m, bool &first)
{
    for (const auto &[k, v] : m)
    {
        if (!first)
            os << ",";
        os << "\"" << k << "\":" << v;
        first = false;
    }
}

} // namespace

int main(int argc, char **argv)
{
    std::vector<std::string> captures;
    std::string jsonPath;
    std::string mdPath;
    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--json" && i + 1 < argc)
            jsonPath = argv[++i];
        else if (arg == "--md" && i + 1 < argc)
            mdPath = argv[++i];
        else if (arg.rfind("--", 0) == 0)
        {
            std::cerr << "gscensus: unknown flag '" << arg << "'\n";
            return 2;
        }
        else
            captures.push_back(arg);
    }
    if (captures.empty() || jsonPath.empty() || mdPath.empty())
    {
        std::cerr << "usage: gscensus <capture...> --json census.json --md census.md\n";
        return 2;
    }

    try
    {
        Census c;
        for (const std::string &path : captures)
        {
            gscap::Reader reader(path);
            c.files++;
            for (size_t i = 0; i < reader.recordCount(); ++i)
            {
                const uint32_t tag = reader.recordAt(i).tag;
                if (tag == gscap::kSubmit)
                {
                    GSPrimitiveBatch b{};
                    reader.getSubmit(i, b);
                    c.addSubmit(b);
                }
                else if (tag == gscap::kBeginTransfer)
                {
                    GSTransferCommand cmd{};
                    reader.getBeginTransfer(i, cmd);
                    c.addTransfer(cmd);
                }
                else if (tag == gscap::kUploadImage)
                {
                    std::vector<uint8_t> data;
                    reader.getUpload(i, data);
                    c.uploadBytes += data.size();
                }
                else if (tag == gscap::kPresent)
                {
                    GSPresentationRequest req{};
                    PresentationFrame f{};
                    std::vector<uint8_t> vram;
                    reader.getPresent(i, req, f, vram);
                    c.addPresent(req, f);
                }
                else if (tag == gscap::kClearFramebuffer)
                {
                    GSContext ctx{};
                    uint32_t rgba = 0;
                    bool result = false;
                    reader.getClear(i, ctx, rgba, result);
                    (void)rgba;
                    (void)result;
                    c.clears++;
                    c.bump(c.framePsm, psmName(ctx.frame.psm));
                }
                else if (tag == gscap::kConsumeLocalToHost)
                {
                    uint32_t maxBytes = 0;
                    std::vector<uint8_t> data;
                    reader.getConsume(i, maxBytes, data);
                    c.consumeCalls++;
                    c.consumeBytes += data.size();
                }
                else if (tag == gscap::kReadVram)
                    c.readVramCalls++;
                else if (tag == gscap::kWriteVram)
                    c.writeVramCalls++;
            }
        }

        {
            std::ofstream os(jsonPath, std::ios::binary | std::ios::trunc);
            if (!os)
                throw std::runtime_error("cannot open --json path");
            os << "{\"files\":" << c.files << ",\"submits\":" << c.submits
               << ",\"presents\":" << c.presents << ",\"clears\":" << c.clears
               << ",\"prim_pixels\":" << c.primPixels
               << ",\"zmsk\":" << c.zmsk << ",\"fbmsk_nonzero\":" << c.fbmskNonzero
               << ",\"fog_prims\":" << c.fogPrims << ",\"transfers\":" << c.transferCount
               << ",\"upload_bytes\":" << c.uploadBytes
               << ",\"consume_calls\":" << c.consumeCalls
               << ",\"consume_bytes\":" << c.consumeBytes
               << ",\"readvram_calls\":" << c.readVramCalls
               << ",\"writevram_calls\":" << c.writeVramCalls;
            bool first = false;
            os << ",\"prim_type\":{";
            first = true;
            writeJsonMap(os, c.primType, first);
            os << "},\"prim_type_pixels\":{";
            first = true;
            writeJsonMap(os, c.primTypePixels, first);
            os << "},\"prim_flags\":{";
            first = true;
            writeJsonMap(os, c.primFlags, first);
            os << "},\"tex_psm\":{";
            first = true;
            writeJsonMap(os, c.texPsm, first);
            os << "},\"tex_size\":{";
            first = true;
            writeJsonMap(os, c.texSize, first);
            os << "},\"clut\":{";
            first = true;
            writeJsonMap(os, c.clut, first);
            os << "},\"tex1\":{";
            first = true;
            writeJsonMap(os, c.tex1, first);
            os << "},\"clamp\":{";
            first = true;
            writeJsonMap(os, c.clamp, first);
            os << "},\"texa\":{";
            first = true;
            writeJsonMap(os, c.texa, first);
            os << "},\"alpha\":{";
            first = true;
            writeJsonMap(os, c.alpha, first);
            os << "},\"blend_misc\":{";
            first = true;
            writeJsonMap(os, c.blendMisc, first);
            os << "},\"atst\":{";
            first = true;
            writeJsonMap(os, c.atst, first);
            os << "},\"afail\":{";
            first = true;
            writeJsonMap(os, c.afail, first);
            os << "},\"date_datm\":{";
            first = true;
            writeJsonMap(os, c.dateDatm, first);
            os << "},\"ztst\":{";
            first = true;
            writeJsonMap(os, c.ztst, first);
            os << "},\"zbuf_psm\":{";
            first = true;
            writeJsonMap(os, c.zbufPsm, first);
            os << "},\"frame_psm\":{";
            first = true;
            writeJsonMap(os, c.framePsm, first);
            os << "},\"scissor\":{";
            first = true;
            writeJsonMap(os, c.scissor, first);
            os << "},\"scanmsk\":{";
            first = true;
            writeJsonMap(os, c.scanmsk, first);
            os << "},\"transfer_dir\":{";
            first = true;
            writeJsonMap(os, c.transferDir, first);
            os << "},\"transfer_psm\":{";
            first = true;
            writeJsonMap(os, c.transferPsm, first);
            os << "},\"pmode\":{";
            first = true;
            writeJsonMap(os, c.pmode, first);
            os << "},\"smode2\":{";
            first = true;
            writeJsonMap(os, c.smode2, first);
            os << "},\"dispfb_psm\":{";
            first = true;
            writeJsonMap(os, c.dispfbPsm, first);
            os << "},\"present_size\":{";
            first = true;
            writeJsonMap(os, c.presentSize, first);
            os << "}}\n";
        }

        {
            std::ofstream os(mdPath, std::ios::binary | std::ios::trunc);
            if (!os)
                throw std::runtime_error("cannot open --md path");
            os << "# GS feature census\n\n";
            os << "Over " << c.files << " captures: " << c.submits << " primitives (~"
               << c.primPixels << " px est.), " << c.clears << " clears, " << c.presents
               << " presents, "
               << c.transferCount << " transfers (" << c.uploadBytes << " uploaded bytes), "
               << c.consumeCalls << " local->host consumes (" << c.consumeBytes
               << " bytes), " << c.readVramCalls << " ReadVram / " << c.writeVramCalls
               << " WriteVram calls.\n\n";
            os << "## Primitives\n\n";
            writeTable(os, "type (count)", c.primType);
            writeTable(os, "type (est. pixels)", c.primTypePixels);
            writeTable(os, "flags set", c.primFlags);
            os << "## Textures\n\n";
            writeTable(os, "TEX0 psm", c.texPsm);
            writeTable(os, "texture size", c.texSize);
            writeTable(os, "CLUT cpsm/csm/csa", c.clut);
            writeTable(os, "TEX1 mmin/mmag/mxl", c.tex1);
            writeTable(os, "CLAMP wms/wmt", c.clamp);
            writeTable(os, "TEXA aem", c.texa);
            os << "## Blending\n\n";
            writeTable(os, "ALPHA A/B/C/D/FIX", c.alpha);
            writeTable(os, "PABE/FBA/COLCLAMP0/DTHE", c.blendMisc);
            os << "## Tests\n\n";
            writeTable(os, "ATST (ATE on; OFF = ATE disabled)", c.atst);
            writeTable(os, "AFAIL (only failing draws)", c.afail);
            writeTable(os, "DATE/DATM", c.dateDatm);
            writeTable(os, "ZTST (ZTE on; OFF = ZTE disabled)", c.ztst);
            writeTable(os, "ZBUF psm", c.zbufPsm);
            os << "ZMSK draws: " << c.zmsk << "\n\n";
            writeTable(os, "FRAME psm", c.framePsm);
            os << "FBMSK nonzero draws: " << c.fbmskNonzero << "\n\n";
            writeTable(os, "SCISSOR WxH", c.scissor);
            writeTable(os, "SCANMSK nonzero", c.scanmsk);
            os << "Fogged (FGE) primitives: " << c.fogPrims << "\n\n";
            os << "## Transfers\n\n";
            writeTable(os, "direction", c.transferDir);
            writeTable(os, "psm", c.transferPsm);
            os << "## Presentation\n\n";
            writeTable(os, "PMODE circuits", c.pmode);
            writeTable(os, "SMODE2 mode", c.smode2);
            writeTable(os, "DISPFB psm", c.dispfbPsm);
            writeTable(os, "present size", c.presentSize);
        }

        std::cout << "gscensus: " << c.files << " files, " << c.submits << " submits, "
                  << c.presents << " presents\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "gscensus: error: " << e.what() << "\n";
        return 2;
    }
}
