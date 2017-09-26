/*
 * IBMisc: Misc. Routines for IceBin (and other code)
 * Copyright (c) 2013-2016 by Elizabeth Fischer
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

// https://github.com/google/googletest/blob/master/googletest/docs/Primer.md

#include <gtest/gtest.h>
#include <ibmisc/fortranio.hpp>
#include <spsparse/eigen.hpp>
#include <icebin/modele/hntr.hpp>
#include <icebin/modele/grids.hpp>
#include <icebin/modele/z1qx1n_bs1.hpp>
#include <icebin/eigen_types.hpp>
#include <iostream>
#include <cstdio>
#include <fstream>
#include <cstdlib>

using namespace std;
using namespace ibmisc;
using namespace icebin;
using namespace icebin::modele;
using namespace blitz;
using namespace spsparse;

extern "C" void write_hntr40(
    int const &ima, int const &jma, float const &offia, float const &dlata,
    int const &imb, int const &jmb, float const &offib, float const &dlatb,
    float const &datmis);

// The fixture for testing class Foo.
class HntrTest : public ::testing::Test {
protected:
    // You can do set-up work for each test here.
    HntrTest() {}

    // You can do clean-up work that doesn't throw exceptions here.
    virtual ~HntrTest() {}

    // If the constructor and destructor are not enough for setting up
    // and cleaning up each test, you can define the following methods:

    // Code here will be called immediately after the constructor (right
    // before each test).
    virtual void SetUp() {
    }

    // Code here will be called immediately after each test (right
    // before the destructor).
    virtual void TearDown() {
        remove("hntr4_common");
    }

//    // The mock bar library shaed by all tests
//    MockBar m_bar;
};

void cmp_array(blitz::Array<double,1> const &cval, blitz::Array<float,1> const &val, std::string msg = "")
{
    for (int i=cval.lbound(0); i <= cval.ubound(0); ++i) {
        float cv = cval(i);
        float v = val(i);
        EXPECT_FLOAT_EQ(cv, v) << "i=" << i << " " << msg;
    }
}

void cmp_array(blitz::Array<double,1> const &cval, blitz::Array<double,1> const &val, std::string msg = "")
{
    for (int i=cval.lbound(0); i <= cval.ubound(0); ++i) {
        double cv = cval(i);
        double v = val(i);
        EXPECT_DOUBLE_EQ(cv, v) << msg;
    }
}

void cmp_array(blitz::Array<int,1> const &cval, blitz::Array<int,1> const &val, std::string msg = "")
{
    for (int i=cval.lbound(0); i <= cval.ubound(0); ++i) {
        EXPECT_EQ(cval(i), val(i)) << msg;
    }
}

void cmp_array_rel(blitz::Array<double,1> const &cval, blitz::Array<double,1> const &val, double epsilon, std::string msg = "")
{
    EXPECT_EQ(cval.extent(0), val.extent(0));
    EXPECT_EQ(cval.lbound(0), val.lbound(0));

    for (int i=cval.lbound(0); i <= cval.ubound(0); ++i) {
        double cv = cval(i);
        double v = val(i);
        EXPECT_NEAR(1., cv / v, epsilon) << msg;
    }
}
// ----------------------------------------------------------


void cmp_hntr4(HntrGrid const &grida, HntrGrid const &gridb)
{
    write_hntr40(
        grida.im, grida.jm, grida.offi, grida.dlat,
        gridb.im, gridb.jm, gridb.offi, gridb.dlat,
        0.);

    fortran::UnformattedInput fin("hntr4_common", Endian::LITTLE);
    float datmcb;
    int ina, jna, inb, jnb;
    fortran::read(fin) >> datmcb >> ina >> jna >> inb >> jnb >> fortran::endr;

    blitz::Array<double,1> sina(Range(0,5401));
    blitz::Array<double,1> sinb(Range(0,5401));
    blitz::Array<double,1> fmin(Range(1,10800));
    blitz::Array<double,1> fmax(Range(1,10800));
    blitz::Array<double,1> gmin(Range(1,5401));
    blitz::Array<double,1> gmax(Range(1,5401));
    blitz::Array<int,1> imin(Range(1,10800));
    blitz::Array<int,1> imax(Range(1,10800));
    blitz::Array<int,1> jmin(Range(1,5401));
    blitz::Array<int,1> jmax(Range(1,5401));
    fortran::read(fin) >> 
        sina >> sinb >>
        fmin >> fmax >> gmin >> gmax >>
        imin >> imax >> jmin >> jmax >> fortran::endr;

    fin.close();

    EXPECT_EQ(grida.im, ina);
    EXPECT_EQ(grida.jm, jna);
    EXPECT_EQ(gridb.im, inb);
    EXPECT_EQ(gridb.jm, jnb);

    Hntr hntr(grida, gridb, 0);
    // printf("------------- SINA\n");
    cmp_array(hntr.SINA, sina);
    // printf("------------- SINB\n");
    cmp_array(hntr.SINB, sinb);
    // printf("------------- FMIN\n");
    cmp_array(hntr.FMIN, fmin);
    // printf("------------- FMAX\n");
    cmp_array(hntr.FMAX, fmax);
    // printf("------------- IMIN\n");
    cmp_array(hntr.IMIN, imin);
    // printf("------------- IMAX\n");
    cmp_array(hntr.IMAX, imax);
    // printf("------------- GMIN\n");
    cmp_array(hntr.GMIN, gmin);
    // printf("------------- GMAX\n");
    cmp_array(hntr.GMAX, gmax);
    // printf("------------- JMIN\n");
    cmp_array(hntr.JMIN, jmin);
    // printf("------------- JMAX\n");
    cmp_array(hntr.JMAX, jmax);

    for (int i=hntr.JMAX.lbound(0); i <= hntr.JMAX.ubound(0); ++i) {
        EXPECT_TRUE(hntr.JMAX(i) <= hntr.Agrid.jm);
    }

}

TEST_F(HntrTest, simple_grids)
{
    icebin::modele::HntrGrid g4(8, 4, 0.0, 45.0*60);
    HntrGrid g8(16, 8, 0.0, 22.5*60);

    cmp_hntr4(g4, g8);
    cmp_hntr4(g2mx2m, g1qx1);
    cmp_hntr4(g10mx10m, g1qx1);
    cmp_hntr4(ghxh, g1qx1);
    cmp_hntr4(g1x1, g1qx1);
}

// ----------------------------------------------------------------
void hntr_overlap(
    MakeDenseEigenT::AccumT &ret,        // {dimB, dimA}
    std::array<HntrGrid const *, 2> hntrs,    // {hntrB, hntrA}
    double const R)            // Radius of the earth
{
    auto &dimB(*ret.dim(0).sparse_set);
    Hntr hntr_BvA(hntrs, 0);    // BvA

    hntr_BvA.overlap(ret, R, DimClip(&dimB));
}
// ----------------------------------------------------------------
extern "C" void call_hntr4(
    float const *WTA, int const &lwta,
    float const *A, int const &la,
    float  *B, int const &lb);

void cmp_regrid(
    HntrGrid const &gridA, HntrGrid const &gridB,
    blitz::Array<double,2> const &WTAc,    // 1-based indexing
    blitz::Array<double,2> const &Ac)      // 1-based indexing
{

    // Regrid with C++
    auto Bc(gridB.Array<double>());
    Hntr hntr(gridA, gridB, 0);
    hntr.regrid(WTAc, Ac, Bc);

    // Regrid with Fortran
    auto WTAf(gridA.Array<float>());
    auto Af(gridA.Array<float>());
    for (int j=1; j<=gridA.jm; ++j) {
    for (int i=1; i<=gridA.im; ++i) {
        WTAf(i,j) = WTAc(i,j);
        Af(i,j) = Ac(i,j);
    }}

    auto Bf(gridB.Array<float>());
    write_hntr40(
        gridA.im, gridA.jm, gridA.offi, gridA.dlat,
        gridB.im, gridB.jm, gridB.offi, gridB.dlat,
        0.);
    call_hntr4(
        WTAf.data(), WTAf.extent(0),
        Af.data(), Af.extent(0),
        Bf.data(), Bf.extent(0));

    // Compare C++ vs. Fortran
    cmp_array(reshape1(Bc,1), reshape1(Bf,1), "FORTRAN");

    // ----------------------------------------------------
    // Check conservation
    double Asum = 0;
    for (int j=1; j<=gridA.jm; ++j) {
    for (int i=1; i<=gridA.im; ++i) {
        Asum += Ac(i,j) * gridA.dxyp(j);
    }}

    double Bsum = 0;
    for (int j=1; j<=gridB.jm; ++j) {
    for (int i=1; i<=gridB.im; ++i) {
        Bsum += Bc(i,j) * gridB.dxyp(j);
    }}

    EXPECT_NEAR(1., Asum/Bsum, 1.e-10);
    // ----------------------------------------------------

    // MakeDenseEigen assumes real dimensions (cannot be nullptr)
    // For this test, they are set so the dense and sparse indexing
    // are equivalent
    SparseSetT dimB;
    for (int i=0; i<gridB.ndata(); ++i) dimB.add_dense(i);
    EXPECT_EQ(gridB.jm*gridB.im, dimB.dense_extent());

    SparseSetT dimA;
    for (int i=0; i<gridA.ndata(); ++i) dimA.add_dense(i);
    EXPECT_EQ(gridA.jm*gridA.im, dimA.dense_extent());

    // Regrid with scaled overlap matrix using matrix assembly machinery
    // Do it entirely in "sparse" indexing
    MakeDenseEigenT BvA_m(
        std::bind(&hntr_overlap, std::placeholders::_1,
            std::array<HntrGrid const *,2>{&gridB, &gridA}, 1.0),
        {SparsifyTransform::TO_DENSE},
        {&dimB, &dimA}, '.');
    EigenSparseMatrixT BvA(BvA_m.to_eigen());

    blitz::Array<double,1> sBvA(sum(BvA, 0, '-'));

    auto Ac1(reshape1(Ac,0));
    auto WTAc1(reshape1(WTAc,0));

    EigenColVectorT Bd1_e(map_eigen_diagonal(sBvA) * BvA * map_eigen_colvector(Ac1));
    blitz::Array<double,1> Bd1(to_blitz(Bd1_e));

//for (auto ii(begin(BvA)); ii != end(BvA); ++ii) printf("BvA(%d, %d) = %f*%f = %f\n", ii->row(), ii->col(), ii->value(), sBvA(ii->row()), ii->value() * sBvA(ii->row()));

    cmp_array(reshape1(Bd1,1), reshape1(Bf,1), "MATRIX");


    // ---------------------------------------------------------

    // Make sure sum matches (in C++)
    double sumA = 0;
    for (int j=1; j<=gridA.jm; ++j) {
    for (int i=1; i<=gridA.im; ++i) {
        sumA += WTAc(i,j) * Ac(i,j) * gridA.dxyp(j);
    }}
    double sumB = 0;
    for (int j=1; j<=gridB.jm; ++j) {
    for (int i=1; i<=gridB.im; ++i) {
        sumB += Bc(i,j) * gridB.dxyp(j);
    }}
    EXPECT_NEAR(1.0, sumA/sumB, 1.e-12);

}

double frand(double fMin, double fMax)
{
    double f = (double)rand() / RAND_MAX;
    double ret = fMin + f * (fMax - fMin);
    return ret;
}

void cmp_random_regrid(HntrGrid const &gridA, HntrGrid const &gridB)
{
    auto WTA(gridA.Array<double>());
    auto A(gridA.Array<double>());
    for (int j=1; j<=gridA.jm; ++j) {
    for (int i=1; i<=gridA.im; ++i) {
//        WTA(i,j) = frand(0.,1.);
        WTA(i,j) = 1.;    // Don't know how to make tests work out with WTA != 1
        A(i,j) = frand(0.,1.);
    }}
    cmp_regrid(gridA, gridB, WTA, A);
}

void test_overlap(std::array<HntrGrid const *,2> grids,
    bool check_spurious,
    std::string const &msg)
{
    std::vector<double> Rvals {1.0,2.0};


    Hntr hntrBvA(grids, 0);
    auto &gB(*grids[0]);
    auto &gA(*grids[1]);

    for (double R : Rvals) {
        TupleList<int,double,2> mBvA;
        double const R2 = R*R;
        hntrBvA.overlap(mBvA, R);

#if 0
for (auto ii=mBvA.begin(); ii != mBvA.end(); ++ii) {
    int ijB = ii->index(0);
    int const jB = ijB / gB.im;
    int const iB = ijB - (jB * gB.im);

    int ijA = ii->index(0);
    int const jA = ijA / gA.im;
    int const iA = ijA - (jA * gA.im);

    printf("mBvA[(%d %d), (%d %d)] = %f\n", iB,jB,iA,jA,ii->value());
}
#endif


        // Compute sums of areas for grids A and B
        blitz::Array<double,1> areaB(gB.ndata());
        areaB = 0;
        blitz::Array<double,1> areaA(gA.ndata());
        areaA = 0;
        for (auto ii=mBvA.begin(); ii != mBvA.end(); ++ii) {
            areaB(ii->index(0)) += ii->value();
            areaA(ii->index(1)) += ii->value();
        }

        double const epsilon = 1.e-12;

        // Check that computed B areas match
        for (int ijB=0; ijB<gB.ndata(); ++ijB) {
            int const jB = ijB / gB.im;
            int const iB = ijB - (jB * gB.im);

            EXPECT_NEAR(1., gB.dxyp(jB+1)*R2 / areaB(ijB), epsilon) << "Grid B R=" << R << " " << msg;
        }

        // Check that computed A areas match
        for (int ijA=0; ijA<gA.ndata(); ++ijA) {
            int const jA = ijA / gA.im;
            int const iA = ijA - (jA * gA.im);

            EXPECT_NEAR(1., gA.dxyp(jA+1)*R2 / areaA(ijA), epsilon) << "Grid A(" << iA << ", " << jA << ") R=" << R << " " << msg;
        }

        // Check total area
        double sumB = 0;
        for (int i=0; i<areaB.extent(0); ++i) sumB += areaB(i);
        EXPECT_NEAR(1., 4.*M_PI*R2 / sumB, epsilon) << msg;

        double sumA = 0;
        for (int i=0; i<areaA.extent(0); ++i) sumA += areaA(i);
        EXPECT_NEAR(1., 4.*M_PI*R2 / sumA, epsilon) << msg;

        // Check for spurious overlaps
        if (check_spurious) {
            for (auto ii=mBvA.begin(); ii != mBvA.end(); ++ii) {
                int const ijB = ii->index(0);
                int const ijA = ii->index(1);

                EXPECT_GT(ii->value(), areaB(ijB)*1.e-5);
                EXPECT_GT(ii->value(), areaB(ijB)*1.e-5);
            }
        }
    }

}


/** Tests the overlap matrix (area of exchange grid) conforms to
some basic properties such a matrix should have. */
TEST_F(HntrTest, overlap)
{
    HntrGrid gB(4, 2, 0.0, 90.0*60);
    HntrGrid gA(8, 4, 0.0, 45.0*60);

    test_overlap({&gB, &gA}, true, "small-sample");
    test_overlap({&gA, &gB}, true, "small-sample");
    test_overlap({&g1qx1, &g2hx2}, true, "ocean-atm");
    test_overlap({&g2hx2, &g1qx1}, true, "ocean-atm");
}


TEST_F(HntrTest, regrid1)
{
    HntrGrid g2(4, 2, 0.0, 90.0*60);
    HntrGrid g4(8, 4, 0.0, 45.0*60);
    HntrGrid g8(16, 8, 0.0, 22.5*60);


    auto vals4(g4.Array<double>());
    auto wt4(g4.Array<double>());

    for (int i=1; i<=g4.im; ++i) {
    for (int j=1; j<=g4.jm; ++j) {
        wt4(i,j) = 1.0;
        vals4(i,j) = i+j;
    }}

    cmp_regrid(g4, g2, wt4, vals4);
}


// ----------------------------------------------------------------

TEST_F(HntrTest, random_regrids)
{
    icebin::modele::HntrGrid g4(8, 4, 0.0, 45.0*60);
    HntrGrid g8(16, 8, 0.0, 22.5*60);

    cmp_random_regrid(g8, g4);
    cmp_random_regrid(g2mx2m, g1qx1);
    cmp_random_regrid(g10mx10m, g1qx1);
    cmp_random_regrid(ghxh, g1qx1);
    cmp_random_regrid(g1x1, g1qx1);
}

TEST_F(HntrTest, regrid)
{
    icebin::modele::HntrGrid g4(8, 4, 0.0, 45.0*60);
    HntrGrid g8(16, 8, 0.0, 22.5*60);
    auto vals8(g8.Array<double>());
    auto wt8(g8.Array<double>());
    auto vals4(g4.Array<double>());

    double sum8=0;
    for (int i=1; i<=g8.im; ++i) {
    for (int j=1; j<=g8.jm; ++j) {
        wt8(i,j) = 1.0;
        vals8(i,j) = i+j;
        sum8 += vals8(i,j) * g8.dxyp(j);
    }}


    Hntr hntr(g8, g4, 0);
    hntr.regrid(wt8, vals8, vals4);

    double sum4=0;
    for (int i=1; i<=g4.im; ++i) {
    for (int j=1; j<=g4.jm; ++j) {
        sum4 += vals4(i,j) * g4.dxyp(j);
    }}

    EXPECT_DOUBLE_EQ(sum4, sum8);

}
// ----------------------------------------------------------------

extern "C" void write_sgeom();

/** Test that our _dxyp arrays are the same as those computed in
    original code.  Actually, they are within 1e-8 of each other.  I
    don't know why.  Note they use the same sin() function, and sin()
    has matched exactly in other settings. */
TEST_F(HntrTest, sgeom)
{
    write_sgeom();

    int const IM2 = 10800;
    int const JM2 = 5400;
    int const IMH = 720;
    int const JMH = 360;
    int const IM = 288;
    int const JM = 180;
    blitz::Array<double,1> dxyp(Range(1,JM));
    blitz::Array<double,1> dxyph(Range(1,JMH));
    blitz::Array<double,1> dxyp2(Range(1,JM2));

    fortran::UnformattedInput fin("sgeom_common", Endian::LITTLE);
    fortran::read(fin) >> dxyp >> dxyph >> dxyp2 >> fortran::endr;
    fin.close();

    printf("-------------- g1qx1._dxyp\n");
    cmp_array_rel(g1qx1._dxyp, dxyp, 1e-8);
    printf("-------------- ghxh._dxyp\n");
    cmp_array_rel(ghxh._dxyp, dxyph, 1e-8);
    printf("-------------- g2mx2m._dxyp\n");
    cmp_array_rel(g2mx2m._dxyp, dxyp2, 1e-8);
}




int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
