#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <vector>
#include <cerrno>
#include <unistd.h>
#include "blur.hh"

#define likely(x)       __builtin_expect(!!(x), 1)
#define unlikely(x)     __builtin_expect(!!(x), 0)

#include "newhash/newhash.cc"

/* Magnitude of scaled scanline, where n = 0..1 = position between scanlines */
inline constexpr float ScanlineMagnitude(float n) { float c = 0.3f; return std::exp(-(n-0.5f)*(n-0.5f)/(2.f*c*c)); }

constexpr unsigned NumHorizPixels   = 640;
constexpr unsigned CellWidth0 = 2, CellBlank0 = 1; // R
constexpr unsigned CellWidth1 = 2, CellBlank1 = 1; // G
constexpr unsigned CellWidth2 = 2, CellBlank2 = 2; // B
constexpr unsigned TotalHorizRes = NumHorizPixels * (CellWidth0 + CellBlank0 + CellWidth1 + CellBlank1 + CellWidth2 + CellBlank2);
constexpr unsigned Cell0Start = 0,                   Cell0End = Cell0Start + CellWidth0;
constexpr unsigned Cell1Start = Cell0End+CellBlank0, Cell1End = Cell1Start + CellWidth1;
constexpr unsigned Cell2Start = Cell1End+CellBlank1, Cell2End = Cell2Start + CellWidth2;

constexpr unsigned NumVertPixels    = 400;
constexpr unsigned CellHeight0 = 5; // Height of RGB triplet
constexpr unsigned CellHeight1 = 1; // Blank after RGB triplet
constexpr unsigned CellStagger = 3; // Offset of successive columns
constexpr unsigned TotalVertRes = NumVertPixels * (CellHeight0 + CellHeight1);

template<unsigned Start,unsigned End>
static inline float GetMask(unsigned x, unsigned y)
{
    constexpr unsigned cellwidth = CellWidth0 + CellBlank0 + CellWidth1 + CellBlank1 + CellWidth2 + CellBlank2;
    unsigned hpix = x / cellwidth, hmod = x % cellwidth;

    constexpr unsigned cellheight = CellHeight0 + CellHeight1;
    unsigned vmod = (y + CellStagger * hpix) % cellheight;
    return (vmod < CellHeight0) & (hmod >= Start) & (hmod < End);
}

template<unsigned Shift>
static void ConvertPlane(unsigned num, const std::uint32_t* pixels, float* output)
{
    #pragma omp simd
    for(unsigned n=0; n<num; ++n)
    {
        unsigned p = pixels[n];
        p >>= Shift;
        p &= 0xFF;
        output[n] = p;// * (1.f / 255.f);
    }
}



template<int Radius, bool check>
static inline float Lanczos_pi(float x_pi)
{
    if(unlikely(x_pi == (float)0.0)) return (float)1.0;
    if(check)
    {
        if (x_pi <= (float)(-Radius*M_PI)
         || x_pi >= (float)( Radius*M_PI)) return (float)0.0;
    }

    float x_pi_div_Radius = x_pi / Radius;

    //float a = sin(x_pi)            / x_pi;
    //float b = sin(x_pi_div_Radius) / x_pi_div_Radius;
    //return a * b;

    return std::sin(x_pi) * std::sin(x_pi_div_Radius) / (x_pi * x_pi_div_Radius);
}
template<int Radius>
static inline float Lanczos(float x)
{
    return Lanczos_pi<Radius,true>(x * (float)M_PI);
}
template<typename SrcTab, typename DestTab>
class Lanczos2DBase
{
    const SrcTab& src; DestTab& tgt;

    // Note: In this vocabulary,
    //  y denotes outer loop and
    //  x denotes inner loop, but
    // it could be vice versa.
    int xinc_src, yinc_src;
    int xinc_tgt, yinc_tgt;
    int ylimit;
public:
    Lanczos2DBase(
        const SrcTab& src_, DestTab& tgt_,
        int sxi,int syi, int txi,int tyi, int ylim)
        : src(src_), tgt(tgt_),
          xinc_src(sxi), yinc_src(syi),
          xinc_tgt(txi), yinc_tgt(tyi),
          ylimit(ylim)
        { }

    void ScaleOne
        (int srcpos, int tgtpos, int nmax,
         const float contrib[], float density_rev) const
    {
        float res = 0.0;
        int srctemp = srcpos;

        //#pragma omp parallel for reduction(+:r,g,b)
        #pragma omp simd reduction(+:res)
        for(int n=0; n<nmax; ++n)
        {
            res     += contrib[n] * src[srctemp];
            srctemp += xinc_src;   // source x increment
        }

        // Multiplication is faster than division, so we use the reciprocal.
        res *= density_rev;

        tgt[tgtpos] = res;
    }

    void StripeLoop(int tx, int sx, int nmax, const float contrib[], float density) const
    {
        int srcpos = sx * xinc_src; // source x pos at y = 0
        int tgtpos = tx * xinc_tgt; // target x pos at y = 0

        /*
        fprintf(stderr, "StripeLoop sx=%d, tx=%d, srcpos=%d, tgtpos=%d, srcsize=%d, tgtsize=%d, ylimit=%d\n",
            sx,tx, srcpos, tgtpos,
            (int)src.size(), (int)tgt.size(), ylimit);*/

        const float density_rev = (density == 0.0f || density == 1.0f) ? 1.0f : (1.0f / density);

        for(int y=ylimit; y-->0; )
        {
            /*fprintf(stderr, "- within: srcpos=%d, tgtpos=%d, y=%d\n", srcpos,tgtpos, y);*/
            ScaleOne(srcpos, tgtpos, nmax, contrib, density_rev);

            srcpos += yinc_src;  // source y increment
            tgtpos += yinc_tgt;  // target y increment
        }
    }
};

template<typename SrcTab, typename DestTab>
class HorizScaler: public Lanczos2DBase<SrcTab,DestTab>
{
public:
    /*
            <-------------->
            <-------------->
            <-------------->
            <-------------->
            <-------------->
            <-------------->

            For each output column (out_size = {ow}),
            {h} rows (source and target) get processed

            On each row,
              {nmax} source columns get transformed
              into 1 target column

            Target:
               New column stride = {1}
               New row    stride = {ow}
            Source:
               Next column stride = {1}
               Next row    stride = {iw}
    */

    HorizScaler(
        int iw,int ow, int h,
        const SrcTab& src, DestTab& tgt)
        : Lanczos2DBase<SrcTab,DestTab>(
            src,tgt,
            1,  // xinc_src ok
            iw, // yinc_src ok
            1,  // xinc_tgt ok
            ow, // yinc_tgt ok
            h   // ylimit   ok
         ) { }
};

template<typename SrcTab, typename DestTab>
class VertScaler: public Lanczos2DBase<SrcTab,DestTab>
{
public:
    /*
            ^^^^^^^^^^^^^^^^
            ||||||||||||||||
            ||||||||||||||||
            ||||||||||||||||
            ||||||||||||||||
            vvvvvvvvvvvvvvvv

            For each output row (out_size = {oh}),
            {w} columns (source and target) get processed

            On each column,
              {nmax} source rows get transformed
              into 1 target row

            Target:
               New row    stride = {w}
               New column stride = {1}
            Source:
               Next row    stride = {w}
               Next column stride = {1}
    */

    VertScaler(
        int w,
        const SrcTab& src, DestTab& tgt)

        : Lanczos2DBase<SrcTab,DestTab>(
            src,tgt,
            w, // xinc_src ok
            1, // yinc_src ok
            w, // xinc_tgt ok
            1, // yinc_tgt ok
            w  // ylimit   ok
         ) { }
};

/*template<typename SrcTab, typename DestTab>
class ScalarScaler: private Lanczos2DBase<SrcTab, DestTab>
{
public:
    ScalarScaler(const SrcTab& src, DestTab& tgt)
        : Lanczos2DBase<SrcTab,DestTab>(src,tgt, 1,1,1,1,1) { }

    void StripeLoop(int tx, int sx, int nmax,
                    const float contrib[], float density) const
    {
        const float density_rev = (density == 0.0 || density == 1.0)
            ? 1.0
            : (1.0 / density);
        ScaleOne(sx, tx, nmax, contrib, density_rev);
    }
};*/

struct LanczosCoreCalcRes
{
    int start;
    int nmax;
    float density;
};

template<int FilterRadius>
inline LanczosCoreCalcRes LanczosCoreCalc
    (int in_size,
     float center, float support, float scale,
     float contrib[])
{
    const int start = std::max((int)(center-support+(float)0.5), 0);
    const int end   = std::min((int)(center+support+(float)0.5), in_size);
    const int nmax = end-start;

    const float scale_pi = scale * M_PI;

    const float s_min = -FilterRadius*M_PI;
    const float s_max =  FilterRadius*M_PI;

    float s_pi     = (start-center+(float)0.5) * scale_pi;

    float density  = 0.0;

    { int n=0;
      for(; n < nmax && unlikely(s_pi < s_min); ++n, s_pi += scale_pi)
        {}
      for(; n < nmax && likely(s_pi < s_max); ++n, s_pi += scale_pi)
      {
        float l = Lanczos_pi<FilterRadius,false> (s_pi);
        contrib[n] = l;
        density += l;
      }
    }

    LanczosCoreCalcRes res;
    res.start   = start;
    res.nmax    = nmax;
    res.density = density;
    return res;
}

/* A generic Lanczos scaler suitable for
 * converting something to something else
 * at once.
 * For image pixels, use Triplet<type>
 * For stereo samples, use Triplet<type, 2>
 * For mono samples, just use type
 */
template<typename Handler>
static void LanczosScale(int in_size, int out_size, Handler& target)
{
    const int FilterRadius = 2;
    const float blur         = 1.0f;

    const float factor       = out_size / (float)in_size;
    const float scale        = std::min(factor, (float)1.0) / blur;
    const float support      = FilterRadius / scale;

    const std::size_t contrib_size = std::min(in_size, 5+int(2*support));
    float contrib[contrib_size];

    /*fprintf(stderr, "Scaling (%d->%d), contrib=%d\n",
        in_size, out_size, (int)contrib_size);*/

    #pragma omp parallel for schedule(static)
    for(int outpos=0; outpos<out_size; ++outpos)
    {

        float center = (outpos+0.5f) / factor;
        LanczosCoreCalcRes res = LanczosCoreCalc<FilterRadius>(in_size, center, support, scale, contrib);
        target.StripeLoop(outpos, res.start, res.nmax, &contrib[0], res.density);
    }
}


static void VLanczos(unsigned in_width,unsigned in_height, unsigned out_height, const float* in, float* out)
{
    VertScaler<const float*, float*> handler_y(in_width, in, out);
    LanczosScale(in_height, out_height, handler_y);
}
static void HLanczos(unsigned in_width,unsigned in_height, unsigned out_width, const float* in, float* out)
{
    HorizScaler<const float*, float*> handler_x(in_width,out_width, in_height, in, out);
    LanczosScale(in_width, out_width, handler_x);
}

static std::uint32_t ClampWithDesaturation(int r,int g,int b)
{
    const int R = 2126, G = 7152, B = 722, sum=R+G+B;
    int luma = r*R + g*G + b*B;
    if(luma > 255*sum) { r=g=b=255; }
    else if(luma <= 0) { r=g=b=0; }
    else
    {
        auto spread = [&r,&g,&b,R,G,B](auto&& test, auto&& cap, int sign)
        {
            int cr,cg,cb, work,capacity;
            if((work = R*std::max(0, test(r))
                     + G*std::max(0, test(g))
                     + B*std::max(0, test(b)))
            && (capacity = R*std::max(0, (cr = cap(r)))
                         + G*std::max(0, (cg = cap(g)))
                         + B*std::max(0, (cb = cap(b)))))
            {
                int act = std::min(work, capacity);
                r += cr * sign * act / (cr > 0 ? capacity : work);
                g += cg * sign * act / (cg > 0 ? capacity : work);
                b += cb * sign * act / (cb > 0 ? capacity : work);
            }
        };
        // Find out the amount of excess color energy.
        // Dissipate it to capable channels,
        // and take it away from those that had excess.
        spread([](int c) { return c-255; }, // Amount of access
               [](int c) { return 255-c; }, // Capacity for reception
               1);
        // Find out the amount of color energy debt.
        // Borrow energy from capable channels,
        // and give it to channels that need it.
        spread([](int c) { return -c; }, // Amount of debt
               [](int c) { return c; },  // Capacity for borrowing
               -1);
    }
    return unsigned(r)*65536u + unsigned(g)*256u + b;
}


void ConvertPicture(unsigned in_width,
                    unsigned in_height,
                    unsigned out_width,
                    unsigned out_height,
                    unsigned NumScanlines,
                    const std::uint32_t* pixels,
                    std::uint32_t* outpixels)
{
    std::vector<float> plane(NumScanlines * in_width * 3);
    std::vector<float> tempplane(TotalVertRes * out_width * 3);
    std::vector<float> resuplane(out_width * out_height * 3);
    constexpr float Gamma = 2.0;

    if(in_height == NumScanlines)
    {
        ConvertPlane<16>(NumScanlines*in_width, pixels, &plane[NumScanlines*in_width*0 + 0]);
        ConvertPlane< 8>(NumScanlines*in_width, pixels, &plane[NumScanlines*in_width*1 + 0]);
        ConvertPlane< 0>(NumScanlines*in_width, pixels, &plane[NumScanlines*in_width*2 + 0]);

        #pragma omp parallel for simd schedule(static)
        for(unsigned n=0; n<NumScanlines*in_width*3; ++n)
            plane[n] = std::pow(plane[n] / 255.f, 1.0 / Gamma);
    }
    else
    {
        std::vector<float> indata(in_width * in_height * 3);
        ConvertPlane<16>(in_height*in_width, pixels, &indata[in_height*in_width*0 + 0]);
        ConvertPlane< 8>(in_height*in_width, pixels, &indata[in_height*in_width*1 + 0]);
        ConvertPlane< 0>(in_height*in_width, pixels, &indata[in_height*in_width*2 + 0]);

        #pragma omp parallel for simd schedule(static)
        for(unsigned n=0; n<in_height*in_width*3; ++n)
            indata[n] = std::pow(indata[n] / 255.f, 1.0 / Gamma);

        #pragma omp parallel for schedule(dynamic)
        for(unsigned n=0; n<3; ++n)
            VLanczos(in_width,in_height, NumScanlines, &indata[in_height*in_width*n], &plane[NumScanlines*in_width*n]);
    }

    #pragma omp parallel for schedule(static)
    for(unsigned y=0; y<TotalVertRes; ++y)
    {
        float srcy_flt = y * float(float(NumScanlines) / TotalVertRes);
        unsigned srcy = unsigned(srcy_flt);

        float ScaledScanline[TotalHorizRes/*in_width*/ * 3];
        float XScaledScanline[TotalHorizRes * 3];
        float XMask[TotalHorizRes * 3];

        float factor = ScanlineMagnitude(srcy_flt - srcy);

        #pragma omp simd collapse(2)
        for(unsigned n=0; n<3; ++n)
            for(unsigned x=0; x<in_width; ++x)
            {
                ScaledScanline[x + in_width*n] = plane[NumScanlines*in_width*n + srcy*in_width + x] * factor;
            }

        #pragma omp simd
        for(unsigned x=0; x<TotalHorizRes; ++x)
        {
            XMask[x + TotalHorizRes*0] = GetMask<Cell0Start,Cell0End>(x,y);
            XMask[x + TotalHorizRes*1] = GetMask<Cell1Start,Cell1End>(x,y);
            XMask[x + TotalHorizRes*2] = GetMask<Cell2Start,Cell2End>(x,y);
        }

        #pragma omp simd collapse(1)
        for(unsigned n=0; n<3; ++n)
            for(unsigned x=0; x<TotalHorizRes; ++x)
                XScaledScanline[x + TotalHorizRes*n] = ScaledScanline[x*in_width/TotalHorizRes + in_width*n];

        #pragma omp simd
        for(unsigned x=0; x<TotalHorizRes; ++x)
        {
            XScaledScanline[x + TotalHorizRes*0] *= XMask[x + TotalHorizRes*0];
            XScaledScanline[x + TotalHorizRes*1] *= XMask[x + TotalHorizRes*1];
            XScaledScanline[x + TotalHorizRes*2] *= XMask[x + TotalHorizRes*2];
        }

        for(unsigned n=0; n<3; ++n)
            HLanczos(TotalHorizRes,1, out_width, &XScaledScanline[TotalHorizRes*n], &tempplane[TotalVertRes*out_width*n + y*out_width]);
    }

    #pragma omp parallel for schedule(dynamic)
    for(unsigned n=0; n<3; ++n)
        VLanczos(out_width,TotalVertRes, out_height, &tempplane[TotalVertRes*out_width*n], &resuplane[out_height*out_width*n]);

    unsigned hpix = CellWidth0 + CellBlank0 + CellWidth1 + CellBlank1 + CellWidth2 + CellBlank2;
    unsigned vpix = CellHeight0 + CellHeight1;
    float sum = 0, sum2 = 0; unsigned facsum = 0, facsum2 = 0;
    for(unsigned y=0; y<vpix; ++y)
        for(unsigned x=0; x<hpix; ++x)
            { facsum += 1; sum += GetMask<Cell0Start,Cell0End>(x,y) + GetMask<Cell1Start,Cell1End>(x,y) + GetMask<Cell2Start,Cell2End>(x,y); }
    for(unsigned n=0; n<8; ++n)
        { facsum2 += 1; sum2 += ScanlineMagnitude(n/8.f); }
    float factor = facsum*facsum2 / (sum*sum2);

    #pragma omp parallel for simd schedule(static)
    for(unsigned n=0; n<out_width*out_height*3; ++n) resuplane[n] = (resuplane[n] /*+ 0.075f*/) * factor;

    std::vector<short> resuplanes(out_width * out_height * 3);
    std::vector<short> resuplanestmp(out_width * out_height * 3);
    std::vector<short> resuplaneout(out_width * out_height * 3);

    #pragma omp parallel for simd schedule(static)
    for(unsigned n=0; n<out_width*out_height*3; ++n)
        resuplanes[n] = 600.f * std::pow(resuplane[n], Gamma);

    for(unsigned n=0; n<3; ++n)
    {
        blur<3>(&resuplanes[n*out_width*out_height],
                &resuplaneout[n*out_width*out_height],
                &resuplanestmp[n*out_width*out_height],
                out_width, out_height, out_width / 640.f);
    }

    #pragma omp parallel for simd schedule(static)
    for(unsigned n=0; n<out_width*out_height*3; ++n)
        resuplanes[n] = 255.f * std::pow(resuplane[n], Gamma);

    #pragma omp parallel for schedule(static)
    for(unsigned n=0; n<out_width*out_height; ++n)
    {
        outpixels[n] = ClampWithDesaturation(resuplanes[out_height*out_width*0+n] + resuplaneout[out_height*out_width*0+n],
                                             resuplanes[out_height*out_width*1+n] + resuplaneout[out_height*out_width*1+n],
                                             resuplanes[out_height*out_width*2+n] + resuplaneout[out_height*out_width*2+n]);
    }
}

static long FullyWrite(int fd, const void* b, std::size_t length) // SafeWrite
{
    const unsigned char* buf = (const unsigned char*) b;
    auto origbuf = buf;
  Retry:;
    int result = write(fd, buf, length);
    if(result == -1 && errno==EAGAIN) goto Retry;
    if(result == -1 && errno==EINTR) goto Retry;
    if(result == 0) { std::fprintf(stderr, "\33[1mwrite: EOF\33[m\n"); return 0; }
    if(result < 0) { std::perror("write"); return -(long)errno; }
    length -= result;
    buf    += result;
    if(length) goto Retry;
    return buf-origbuf;
}
static long FullyRead(int fd, void* b, std::size_t length) // SafeRead
{
    unsigned char* buf = (unsigned char*) b;
    auto origbuf = buf;
  Retry:;
    int result = read(fd, buf, length);
    if(result == -1 && errno==EAGAIN) goto Retry;
    if(result == -1 && errno==EINTR) goto Retry;
    if(result == 0) { std::fprintf(stderr, "\33[1mread: EOF\33[m\n"); return 0; }
    if(result < 0) { std::perror("read"); return -(long)errno; }
    length -= result;
    buf    += result;
    if(length) goto Retry;
    return buf-origbuf;
}

int main(int argc, char** argv)
{
    if(argc != 6)
    {
        std::fprintf(stderr, "\33[1mInvalid parameters.\n"
                             "crt-filter <in-width> <in-height> <out-width> <out-height> <numscanlines>\33[m\n");
        return 1;
    }
    unsigned in_width  = std::atoi(argv[1]);
    unsigned in_height = std::atoi(argv[2]);
    unsigned out_width  = std::atoi(argv[3]);
    unsigned out_height = std::atoi(argv[4]);
    unsigned NumScanlines = std::atoi(argv[5]);
    std::vector<std::uint32_t> inbuf(in_width*in_height);
    std::vector<std::uint32_t> outbuf(out_width*out_height);

    constexpr unsigned NFrames = 4;
    newhash_t                  hashes[NFrames];
    std::vector<std::uint32_t> saved_outputs[NFrames];
    std::vector<std::uint32_t> saved_inputs[NFrames];
    for(;;)
    {
        if(FullyRead(0, &inbuf[0], inbuf.size()*4) < (long)inbuf.size()*4) break;

        newhash_t hash = newhash_calc((const unsigned char*)&inbuf[0],
                                      inbuf.size()*sizeof(inbuf[0]));
        bool found = false;
        for(unsigned n=0; n<NFrames; ++n)
            if(hash == hashes[n] && inbuf == saved_inputs[n])
            {
                outbuf = saved_outputs[n];
                found  = true;
                break;
            }
        if(!found)
        {
            ConvertPicture(in_width, in_height, out_width, out_height, NumScanlines, &inbuf[0], &outbuf[0]);

            static unsigned n = 0;
            saved_inputs[n]  = inbuf;
            saved_outputs[n] = outbuf;
            hashes[n]        = hash;
            n = (n+1)%NFrames;
        }

        if(FullyWrite(1, &outbuf[0], outbuf.size()*4) < (long)outbuf.size()*4) break;
    }
    return 0;
}
