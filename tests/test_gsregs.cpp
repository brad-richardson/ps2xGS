// Register encode/decode round trips for every helper in gsregs.h.

#include "gsregs.h"

#include <cstdio>
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

int main()
{
    for (int ate = 0; ate < 2; ++ate)
        for (int atst = 0; atst < 8; ++atst)
            for (uint8_t aref : {uint8_t(0), uint8_t(0x80), uint8_t(0xFF)})
                for (int afail = 0; afail < 4; ++afail)
                    for (int date = 0; date < 2; ++date)
                        for (int datm = 0; datm < 2; ++datm)
                            for (int zte = 0; zte < 2; ++zte)
                                for (int ztst = 0; ztst < 4; ++ztst)
                                {
                                    gsregs::TestReg r;
                                    r.ate = ate;
                                    r.atst = atst;
                                    r.aref = aref;
                                    r.afail = afail;
                                    r.date = date;
                                    r.datm = datm;
                                    r.zte = zte;
                                    r.ztst = ztst;
                                    check(gsregs::TestReg::decode(r.encode()) == r, "TestReg");
                                }

    for (int a = 0; a < 3; ++a)
        for (int b = 0; b < 3; ++b)
            for (int c = 0; c < 3; ++c)
                for (int d = 0; d < 3; ++d)
                    for (uint8_t fix : {uint8_t(0), uint8_t(0x80), uint8_t(0xFF)})
                    {
                        gsregs::AlphaReg r;
                        r.a = a;
                        r.b = b;
                        r.c = c;
                        r.d = d;
                        r.fix = fix;
                        check(gsregs::AlphaReg::decode(r.encode()) == r, "AlphaReg");
                    }

    for (int wms = 0; wms < 4; ++wms)
        for (int wmt = 0; wmt < 4; ++wmt)
            for (uint16_t m : {uint16_t(0), uint16_t(1), uint16_t(0x1FF), uint16_t(0x3FF)})
            {
                gsregs::ClampReg r;
                r.wms = wms;
                r.wmt = wmt;
                r.minu = m;
                r.maxu = m;
                r.minv = m;
                r.maxv = m;
                check(gsregs::ClampReg::decode(r.encode()) == r, "ClampReg");
            }

    for (int lcm = 0; lcm < 2; ++lcm)
        for (int mxl = 0; mxl < 8; ++mxl)
            for (int mmag = 0; mmag < 2; ++mmag)
                for (int mmin = 0; mmin < 2; ++mmin)
                    for (int mtba = 0; mtba < 2; ++mtba)
                        for (int l = 0; l < 4; ++l)
                            for (uint16_t k : {uint16_t(0), uint16_t(1), uint16_t(0xFFF)})
                            {
                                gsregs::Tex1Reg r;
                                r.lcm = lcm;
                                r.mxl = mxl;
                                r.mmag = mmag;
                                r.mmin = mmin;
                                r.mtba = mtba;
                                r.l = l;
                                r.k = k;
                                check(gsregs::Tex1Reg::decode(r.encode()) == r, "Tex1Reg");
                            }

    for (uint8_t ta0 : {uint8_t(0), uint8_t(0x20), uint8_t(0xFF)})
        for (int aem = 0; aem < 2; ++aem)
            for (uint8_t ta1 : {uint8_t(0), uint8_t(0xB0), uint8_t(0xFF)})
            {
                gsregs::TexaReg r;
                r.ta0 = ta0;
                r.aem = aem;
                r.ta1 = ta1;
                check(gsregs::TexaReg::decode(r.encode()) == r, "TexaReg");
            }

    for (uint8_t v : {uint8_t(0), uint8_t(0x40), uint8_t(0xFF)})
    {
        gsregs::FogColReg r;
        r.fcr = v;
        r.fcg = v;
        r.fcb = v;
        check(gsregs::FogColReg::decode(r.encode()) == r, "FogColReg");
    }

    {
        gsregs::DimxReg zero;
        check(gsregs::DimxReg::decode(zero.encode()) == zero, "DimxReg-zero");
        gsregs::DimxReg ramp;
        for (int y = 0; y < 4; ++y)
            for (int x = 0; x < 4; ++x)
                ramp.m[y][x] = (x + y * 4) & 7;
        check(gsregs::DimxReg::decode(ramp.encode()) == ramp, "DimxReg-ramp");
        gsregs::DimxReg full;
        for (auto &row : full.m)
            for (auto &e : row)
                e = 7;
        check(gsregs::DimxReg::decode(full.encode()) == full, "DimxReg-full");
    }

    for (int bits = 0; bits < 32; ++bits)
        for (uint8_t alp : {uint8_t(0), uint8_t(0x80), uint8_t(0xFF)})
        {
            gsregs::PmodeReg r;
            r.en1 = bits & 1;
            r.en2 = bits & 2;
            r.mmod = bits & 4;
            r.amod = bits & 8;
            r.slbg = bits & 16;
            r.alp = alp;
            check(gsregs::PmodeReg::decode(r.encode()) == r, "PmodeReg");
        }

    for (int i = 0; i < 2; ++i)
        for (int f = 0; f < 2; ++f)
        {
            gsregs::Smode2Reg r;
            r.interlaced = i;
            r.frameMode = f;
            check(gsregs::Smode2Reg::decode(r.encode()) == r, "Smode2Reg");
        }

    for (uint16_t fbp : {uint16_t(0), uint16_t(0x1FF)})
        for (uint8_t fbw : {uint8_t(0), uint8_t(10), uint8_t(0x3F)})
            for (uint8_t psm : {uint8_t(0), uint8_t(1), uint8_t(20)})
            {
                gsregs::DispfbReg r;
                r.fbp = fbp;
                r.fbw = fbw;
                r.psm = psm;
                r.dbx = 7;
                r.dby = 63;
                check(gsregs::DispfbReg::decode(r.encode()) == r, "DispfbReg");
            }

    for (uint16_t d : {uint16_t(0), uint16_t(63), uint16_t(0xFFF)})
    {
        gsregs::DisplayReg r;
        r.dw = d;
        r.dh = d;
        r.magh = 3;
        check(gsregs::DisplayReg::decode(r.encode()) == r, "DisplayReg");
    }

    if (g_failures == 0)
        std::printf("test_gsregs: all round trips pass\n");
    return g_failures == 0 ? 0 : 1;
}
