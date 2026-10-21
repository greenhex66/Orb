// HEX-Q ORBIT - offline DSP self-test (no JUCE needed).
#include "DSP/OrbitEngine.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <random>
#include <vector>

static std::size_t g_allocs = 0;
static bool g_count = false;
void* operator new (std::size_t n)
{
    if (g_count) ++g_allocs;
    if (void* p = std::malloc (n ? n : 1)) return p;
    throw std::bad_alloc();
}
void operator delete (void* p) noexcept { std::free (p); }
void operator delete (void* p, std::size_t) noexcept { std::free (p); }

using namespace hexq::orbit;
using Vec = std::vector<float>;
static const double PI = 3.14159265358979323846;
static int g_pass = 0, g_fail = 0;

static void check (bool ok, const char* name, double v = 0.0)
{
    ok ? ++g_pass : ++g_fail;
    std::printf ("[%s] %-72s %.5f\n", ok ? "PASS" : "FAIL", name, v);
}

static Vec noise (size_t n, unsigned seed, float amp = 0.5f)
{
    std::mt19937 rng (seed); std::uniform_real_distribution<float> d (-amp, amp);
    Vec v (n); for (auto& x : v) x = d (rng); return v;
}
static Vec sine (size_t n, double f, double sr, float amp = 0.5f, double ph = 0.0)
{
    Vec v (n); for (size_t i = 0; i < n; ++i) v[i] = (float) (amp * std::sin (2.0 * PI * f * (double) i / sr + ph)); return v;
}

static void run (OrbitEngine& e, Vec& L, Vec& R, const Settings& s, int block = 512, HostInfo h = {})
{
    for (size_t pos = 0; pos < L.size(); pos += (size_t) block)
        e.process (L.data() + pos, R.data() + pos, (int) std::min ((size_t) block, L.size() - pos), s, h);
}

static double rms (const Vec& v, size_t a = 0, size_t b = 0)
{
    if (b == 0) b = v.size();
    double s = 0; for (size_t i = a; i < b; ++i) s += (double) v[i] * v[i];
    return std::sqrt (s / (double) (b - a));
}
static Vec midOf (const Vec& L, const Vec& R) { Vec m (L.size()); for (size_t i = 0; i < L.size(); ++i) m[i] = 0.5f * (L[i] + R[i]); return m; }
static Vec sideOf (const Vec& L, const Vec& R) { Vec m (L.size()); for (size_t i = 0; i < L.size(); ++i) m[i] = 0.5f * (L[i] - R[i]); return m; }
static double maxDiff (const Vec& a, const Vec& b, size_t from = 0)
{ double m = 0; for (size_t i = from; i < a.size(); ++i) m = std::max (m, (double) std::abs (a[i] - b[i])); return m; }
static double maxStep (const Vec& v, size_t from = 1) { double m = 0; for (size_t i = std::max<size_t> (from, 1); i < v.size(); ++i) m = std::max (m, (double) std::abs (v[i] - v[i - 1])); return m; }

int main()
{
    const double SR = 48000.0;
    const size_t N = 48000;
    std::printf ("=== HEX-Q ORBIT DSP self-test ===\n\n-- 1. SPACE mode\n");
    {
        Settings s; s.mode = Mode::Space; s.width = 1.0f; s.space = 0.0f;
        Vec iL = noise (N, 1), iR = noise (N, 2), L = iL, R = iR;
        OrbitEngine e; e.prepare (SR); run (e, L, R, s);
        check (std::max (maxDiff (L, iL), maxDiff (R, iR)) < 1e-6, "neutral (width 100 %, space 0 %): stereo image unchanged (max diff)", std::max (maxDiff (L, iL), maxDiff (R, iR)));

        s.width = 0.0f; s.space = 0.6f; L = iL; R = iR; OrbitEngine e2; e2.prepare (SR); run (e2, L, R, s);
        check (maxDiff (L, R) < 1e-6, "width 0 %: output is pure mono, no side component (max |L-R|)", maxDiff (L, R));

        s.width = 2.0f; s.space = 0.0f; L = iL; R = iR; OrbitEngine e3; e3.prepare (SR); run (e3, L, R, s);
        const double sideRatio = rms (sideOf (L, R)) / rms (sideOf (iL, iR));
        const double midDiff = maxDiff (midOf (L, R), midOf (iL, iR));
        check (std::abs (sideRatio - 2.0) < 0.01, "width 200 %: side component doubles (ratio)", sideRatio);
        check (midDiff < 1e-6, "width 200 %: mid component unchanged (max diff)", midDiff);

        // dual-mono input (L == R): width alone cannot create anything; Space must create real stereo
        Vec dm = noise (N, 3), a = dm, b = dm;
        s.width = 1.0f; s.space = 1.0f; OrbitEngine e4; e4.prepare (SR); run (e4, a, b, s);
        const double sm = rms (sideOf (a, b)) / rms (midOf (a, b));
        double c = 0, aa = 0, bb = 0; for (size_t i = 4800; i < N; ++i) { c += a[i] * b[i]; aa += a[i] * a[i]; bb += b[i] * b[i]; }
        check (sm > 0.8, "dual-mono + space 100 %: synthetic side / mid RMS ratio", sm);
        check (c / std::sqrt (aa * bb) < 0.5, "dual-mono + space 100 %: L/R correlation drops (not a plain copy)", c / std::sqrt (aa * bb));
    }

    std::printf ("\n-- 2. mono-sum behaviour (mono compatibility, measured)\n");
    {
        std::mt19937 rng (11); std::uniform_real_distribution<float> u (0.0f, 1.0f);
        double worstSpace = 0, worstCs = 0;
        for (int t = 0; t < 40; ++t)
        {
            Vec iL = noise (9600, 100 + t), iR = noise (9600, 200 + t);
            Settings s; s.width = 2.0f * u (rng); s.space = u (rng); s.center = u (rng); s.mix = u (rng);

            s.mode = Mode::Space; Vec L = iL, R = iR; OrbitEngine e; e.prepare (SR); run (e, L, R, s);
            const double cosT = std::cos (s.space * 0.78539816339);
            for (size_t i = 0; i < L.size(); ++i)
                worstSpace = std::max (worstSpace, (double) std::abs (0.5f * (L[i] + R[i]) - cosT * 0.5f * (iL[i] + iR[i])));

            s.mode = Mode::CenterSpace; L = iL; R = iR; OrbitEngine e2; e2.prepare (SR); run (e2, L, R, s);
            for (size_t i = 0; i < L.size(); ++i)
            {
                const double m = 0.5 * (iL[i] + iR[i]);
                worstCs = std::max (worstCs, std::abs (0.5 * (L[i] + R[i]) - (m + (m * s.center - m) * s.mix)));
            }
        }
        check (worstSpace < 1e-5, "SPACE: mono sum = mid * cos(theta), side cancels (max err, 40 random settings)", worstSpace);
        check (worstCs < 1e-5, "CENTER+SPACE: mono sum = mid * centre, side cancels (max err, 40 random settings)", worstCs);
    }

    std::printf ("\n-- 3. CENTER + SPACE mode\n");
    {
        double worst = 0;
        for (float sp : { 0.0f, 0.5f, 1.0f }) for (float w : { 0.0f, 1.0f, 2.0f })
        {
            Vec iL = noise (N, 5), iR = noise (N, 6), L = iL, R = iR;
            Settings s; s.mode = Mode::CenterSpace; s.center = 0.8f; s.space = sp; s.width = w; s.mix = 1.0f;
            OrbitEngine e; e.prepare (SR); run (e, L, R, s);
            Vec m = midOf (L, R), mi = midOf (iL, iR); for (auto& x : mi) x *= 0.8f;
            worst = std::max (worst, maxDiff (m, mi));
        }
        check (worst < 1e-5, "centre never disappears: mid = input mid * Center Level for every Space/Width", worst);

        Vec iL = noise (N, 7), iR = noise (N, 8), L = iL, R = iR;
        Settings s; s.mode = Mode::CenterSpace; s.center = 1.0f; s.space = 0.0f; s.width = 1.0f; s.mix = 1.0f;
        OrbitEngine e; e.prepare (SR); run (e, L, R, s);
        check (std::max (maxDiff (L, iL), maxDiff (R, iR)) < 1e-6, "minimum settings: original sound preserved (max diff)", std::max (maxDiff (L, iL), maxDiff (R, iR)));

        s.space = 1.0f; s.width = 2.0f; s.mix = 0.0f; L = iL; R = iR; OrbitEngine e2; e2.prepare (SR); run (e2, L, R, s);
        check (std::max (maxDiff (L, iL), maxDiff (R, iR)) < 1e-6, "wet/dry 0 %: fully dry (max diff)", std::max (maxDiff (L, iL), maxDiff (R, iR)));

        // bass safety of the synthetic spread
        auto sideMid = [&] (double f)
        {
            Vec a = sine (N, f, SR), b = a; Settings q; q.mode = Mode::CenterSpace; q.space = 1.0f; q.width = 2.0f; q.mix = 1.0f;
            OrbitEngine en; en.prepare (SR); run (en, a, b, q);
            return rms (sideOf (a, b), 24000) / rms (midOf (a, b), 24000);
        };
        const double lo = sideMid (50.0), hi = sideMid (2000.0);
        check (lo < 0.15, "dual-mono 50 Hz: synthetic side stays small (side/mid)", lo);
        check (hi > 0.5, "dual-mono 2 kHz: synthetic side present (side/mid)", hi);

        // SPACE vs CENTER+SPACE must sound different
        Vec dm = noise (N, 9), a = dm, b = dm, c = dm, d = dm;
        Settings sa; sa.mode = Mode::Space; sa.space = 0.7f; sa.width = 1.0f;
        Settings sb = sa; sb.mode = Mode::CenterSpace; sb.mix = 1.0f; sb.center = 1.0f;
        OrbitEngine ea, eb; ea.prepare (SR); eb.prepare (SR); run (ea, a, b, sa); run (eb, c, d, sb);
        Vec diff (N); for (size_t i = 0; i < N; ++i) diff[i] = a[i] - c[i];
        const double rel = rms (diff) / rms (dm);
        check (rel > 0.15, "SPACE and CENTER+SPACE give clearly different results (rel. RMS diff)", rel);
        const double midA = rms (midOf (a, b)) / rms (dm), midC = rms (midOf (c, d)) / rms (dm);
        std::printf ("      (space 70 %%: SPACE centre %.2f x input, CENTER+SPACE centre %.2f x input)\n", midA, midC);
    }

    std::printf ("\n-- 4. PAN mode\n");
    {
        // movement rate and range
        Vec L = sine (4 * N, 440.0, SR, 0.5f), R = L;
        Settings s; s.mode = Mode::Pan; s.rateHz = 1.0f; s.depth = 1.0f; s.phaseDeg = 180.0f; s.mix = 1.0f; s.wave = Wave::Sine;
        OrbitEngine e; e.prepare (SR); run (e, L, R, s);
        const size_t win = 960; int flips = 0; double prev = 0, minRatio = 1e9, maxDelta = 0;
        for (size_t a = 24000; a + win < L.size(); a += win)
        {
            const double l = rms (L, a, a + win), r = rms (R, a, a + win), dlt = l - r;
            if (prev != 0 && ((dlt > 0) != (prev > 0))) ++flips;
            prev = dlt; maxDelta = std::max (maxDelta, std::abs (dlt)); minRatio = std::min ({ minRatio, l / std::max (r, 1e-9), r / std::max (l, 1e-9) });
        }
        check (flips >= 5 && flips <= 8, "1 Hz sine pan, 3.5 s: L/R dominance flips (expected ~7 half-cycles)", flips);
        check (minRatio < 0.1, "depth 100 %: signal reaches (almost) full left and full right (min weak/strong)", minRatio);

        // depth 0 and 50 %
        L = sine (N, 440.0, SR, 0.5f); R = L; Vec iL = L;
        Settings z = s; z.depth = 0.0f; OrbitEngine e0; e0.prepare (SR); run (e0, L, R, z);
        check (maxDiff (L, iL) < 1e-6 && maxDiff (R, iL) < 1e-6, "depth 0 %: no movement, output = input (max diff)", maxDiff (L, iL));

        auto spread = [&] (float depth)
        {
            Vec a = sine (2 * N, 440.0, SR, 0.5f), b = a; Settings q = s; q.depth = depth; OrbitEngine en; en.prepare (SR); run (en, a, b, q);
            double mn = 1e9, mx = 0;
            for (size_t k = 24000; k + win < a.size(); k += win) { const double r = rms (b, k, k + win) / rms (a, k, k + win); mn = std::min (mn, r); mx = std::max (mx, r); }
            return mx - mn;
        };
        const double s50 = spread (0.5f), s100 = spread (1.0f);
        check (s50 > 0.2 && s50 < s100, "depth 50 % moves less than depth 100 % (R gain range)", s50);

        // no clipping: gains never exceed 1
        double peak = 0;
        for (Wave w : { Wave::Sine, Wave::Triangle, Wave::Square })
        {
            Vec a = sine (N, 997.0, SR, 1.0f), b = a; Settings q = s; q.wave = w; q.rateHz = 4.0f; OrbitEngine en; en.prepare (SR); run (en, a, b, q);
            for (size_t i = 0; i < a.size(); ++i) peak = std::max ({ peak, (double) std::abs (a[i]), (double) std::abs (b[i]) });
        }
        check (peak <= 1.00001, "full-scale input: output never exceeds 0 dBFS in PAN (peak)", peak);

        // square wave: no clicks
        Vec a = sine (N, 200.0, SR, 0.5f), b = a; Settings q = s; q.wave = Wave::Square; q.rateHz = 3.0f; OrbitEngine en; en.prepare (SR); run (en, a, b, q);
        check (std::max (maxStep (a), maxStep (b)) < 0.05, "square LFO: abrupt moves are smoothed, no click (max sample step)", std::max (maxStep (a), maxStep (b)));

        // stereo phase: 0 deg = both channels together, 180 deg = opposite
        auto corrGain = [&] (float ph)
        {
            Vec a = sine (2 * N, 440.0, SR, 0.5f), b = a; Settings p = s; p.phaseDeg = ph; OrbitEngine en2; en2.prepare (SR); run (en2, a, b, p);
            double sumd = 0; int n = 0;
            for (size_t k = 24000; k + win < a.size(); k += win) { sumd += std::abs (rms (a, k, k + win) - rms (b, k, k + win)); ++n; }
            return sumd / n;
        };
        const double d0 = corrGain (0.0f), d180 = corrGain (180.0f);
        check (d0 < 0.02 && d180 > 0.1, "stereo phase 0 deg: no L/R movement; 180 deg: full ping-pong (mean |L-R| rms)", d180);
    }

    std::printf ("\n-- 5. tempo sync\n");
    {
        const double bpm = 120.0; const int blk = 256;
        Vec L = sine (4 * N, 440.0, SR, 0.5f), R = L;
        Settings s; s.mode = Mode::Pan; s.sync = true; s.beatsPerCycle = 1.0f; s.depth = 1.0f; s.phaseDeg = 180.0f; s.mix = 1.0f;
        OrbitEngine e; e.prepare (SR);
        HostInfo h; h.bpm = bpm; h.playing = true; h.havePpq = true; h.ppq = 0.0;
        for (size_t pos = 0; pos < L.size(); pos += blk)
        {
            e.process (L.data() + pos, R.data() + pos, blk, s, h);
            h.ppq += (double) blk / SR * bpm / 60.0;
        }
        auto at = [&] (double ppqPos) { const size_t c = (size_t) (ppqPos * 60.0 / bpm * SR); return std::make_pair (rms (L, c - 240, c + 240), rms (R, c - 240, c + 240)); };
        auto a = at (2.25), b = at (2.75);
        check (a.second < 0.05 * a.first, "1/4 note, ppq .25: right channel silent, left full (R/L)", a.second / a.first);
        check (b.first < 0.05 * b.second, "1/4 note, ppq .75: left channel silent, right full (L/R)", b.first / b.second);

        // rate follows division: count L/R flips for 1/8 (2 beats/s -> 4 half-cycles/s at 120 bpm)
        Vec L2 = sine (2 * N, 440.0, SR, 0.5f), R2 = L2; Settings s8 = s; s8.beatsPerCycle = 0.5f;
        OrbitEngine e8; e8.prepare (SR); HostInfo h2 = h; h2.playing = false;   // transport stopped: free-runs at the host tempo
        run (e8, L2, R2, s8, 512, h2);
        int flips = 0; double prev = 0;
        for (size_t k = 9600; k + 960 < L2.size(); k += 960) { const double d = rms (L2, k, k + 960) - rms (R2, k, k + 960); if (prev != 0 && ((d > 0) != (prev > 0))) ++flips; prev = d; }
        check (flips >= 12 && flips <= 16, "1/8 note at 120 bpm (4 Hz), 1.8 s, transport stopped: half-cycle flips (~14)", flips);
    }

    std::printf ("\n-- 6. mode switching, bypass, mono check\n");
    {
        Vec L = sine (4 * N, 300.0, SR, 0.5f), R = sine (4 * N, 300.0, SR, 0.5f, 1.0);
        Settings s; s.space = 0.7f; s.width = 1.5f; s.mix = 0.8f; s.rateHz = 2.0f; s.phaseDeg = 180.0f;
        OrbitEngine e; e.prepare (SR);
        for (size_t pos = 0, k = 0; pos < L.size(); pos += 4800, ++k)
        { s.mode = (Mode) (k % 3); e.process (L.data() + pos, R.data() + pos, 4800, s, {}); }
        {   // reference: the largest step the signal has in steady state in any single mode (start-up transient excluded)
            double steady = 0;
            for (int md = 0; md < 3; ++md)
            {
                Vec a = sine (4 * N, 300.0, SR, 0.5f), b = sine (4 * N, 300.0, SR, 0.5f, 1.0); Settings q = s; q.mode = (Mode) md;
                OrbitEngine en; en.prepare (SR); en.process (a.data(), b.data(), (int) a.size(), q, {});
                steady = std::max ({ steady, maxStep (a, 4800), maxStep (b, 4800) });
            }
            const double sw = std::max (maxStep (L, 4800), maxStep (R, 4800));
            check (sw < 1.25 * steady, "switching mode every 100 ms: steps stay within 1.25x of steady-state (max step)", sw);
            std::printf ("      (largest step in any single mode: %.4f)\n", steady);
        }

        Vec a = sine (3 * N, 300.0, SR, 0.5f), b = a, ia = a; Settings q; q.mode = Mode::Pan; q.rateHz = 2.0f;
        OrbitEngine e2; e2.prepare (SR);
        for (size_t pos = 0, k = 0; pos < a.size(); pos += 4800, ++k) { q.bypass = (k % 5) >= 3; e2.process (a.data() + pos, b.data() + pos, 4800, q, {}); }
        check (std::max (maxStep (a), maxStep (b)) < 0.08, "bypass on/off while panning: no click (max sample step)", std::max (maxStep (a), maxStep (b)));

        Vec c = sine (N, 300.0, SR, 0.5f), d = c, ic = c; q.bypass = true; OrbitEngine e3; e3.prepare (SR); run (e3, c, d, q);
        check (maxDiff (c, ic, 24000) == 0.0, "bypass: output identical to input after the fade", maxDiff (c, ic, 24000));

        Vec m1 = noise (N, 21), m2 = noise (N, 22); Settings mc; mc.mode = Mode::Space; mc.space = 0.6f; mc.width = 1.5f; mc.monoCheck = true;
        OrbitEngine e4; e4.prepare (SR); run (e4, m1, m2, mc);
        check (maxDiff (m1, m2, 24000) < 1e-6, "mono check: both channels carry the mono sum (max |L-R|)", maxDiff (m1, m2, 24000));
    }

    std::printf ("\n-- 7. stability, block sizes, denormals, allocations, CPU\n");
    {
        bool ok = true; double peak = 0;
        for (double sr : { 22050.0, 44100.0, 48000.0, 96000.0, 192000.0 })
        {
            OrbitEngine e; e.prepare (sr); std::mt19937 rng (4); std::uniform_real_distribution<float> u (0.0f, 1.0f), nz (-1.0f, 1.0f);
            for (int blk = 0; blk < 300; ++blk)
            {
                Settings s; s.mode = (Mode) (int) (u (rng) * 2.999f); s.width = 2 * u (rng); s.space = u (rng); s.center = u (rng); s.mix = u (rng);
                s.rateHz = 0.05f + 10 * u (rng); s.depth = u (rng); s.wave = (Wave) (int) (u (rng) * 2.999f); s.phaseDeg = 180 * u (rng);
                s.sync = u (rng) > 0.5f; s.beatsPerCycle = 0.25f * std::pow (2.0f, std::floor (u (rng) * 4.99f)); s.bypass = u (rng) > 0.9f; s.monoCheck = u (rng) > 0.8f;
                HostInfo h; h.bpm = 20 + 380 * u (rng); h.ppq = 100 * u (rng); h.playing = u (rng) > 0.5f; h.havePpq = true;
                Vec L (256), R (256); for (auto& x : L) x = nz (rng); for (auto& x : R) x = nz (rng);
                e.process (L.data(), R.data(), 256, s, h);
                for (size_t i = 0; i < 256; ++i) { if (! std::isfinite (L[i]) || ! std::isfinite (R[i])) ok = false; peak = std::max ({ peak, (double) std::abs (L[i]), (double) std::abs (R[i]) }); }
            }
        }
        check (ok && peak < 20.0, "random automation, 5 sample rates, all modes: finite and bounded (peak)", peak);

        double worst = 0;
        for (Mode md : { Mode::Space, Mode::CenterSpace, Mode::Pan })
        {
            Settings s; s.mode = md; s.width = 1.4f; s.space = 0.6f; s.mix = 0.7f; s.rateHz = 1.5f;
            Vec iL = noise (N, 31), iR = noise (N, 32), rL = iL, rR = iR; OrbitEngine er; er.prepare (SR); run (er, rL, rR, s, 512);
            for (int b : { 1, 7, 33, 100, 480, 1024, 4096 })
            { Vec L = iL, R = iR; OrbitEngine en; en.prepare (SR); run (en, L, R, s, b); worst = std::max ({ worst, maxDiff (L, rL), maxDiff (R, rR) }); }
        }
        check (worst < 1e-5, "same result for host block sizes 1..4096, all modes (max diff)", worst);

        OrbitEngine e; e.prepare (SR); Settings s; s.mode = Mode::Space; s.space = 1.0f; Vec L = noise (N, 41), R = noise (N, 42); run (e, L, R, s);
        for (int sec = 0; sec < 3; ++sec) { std::fill (L.begin(), L.end(), 0.0f); std::fill (R.begin(), R.end(), 0.0f); run (e, L, R, s); }
        check (std::max (rms (L), rms (R)) < 1e-12, "3 s of silence after noise: output decays to zero (no denormal tails)", std::max (rms (L), rms (R)));

        for (Mode md : { Mode::Space, Mode::CenterSpace, Mode::Pan })
        {
            Settings q; q.mode = md; q.space = 0.8f; q.width = 1.5f; q.rateHz = 2.0f;
            OrbitEngine en; en.prepare (SR); Vec a = noise (512, 51), b = noise (512, 52); en.process (a.data(), b.data(), 512, q, {});
            const int blocks = (int) (SR * 20 / 512);
            g_allocs = 0; g_count = true;
            const auto t0 = std::chrono::steady_clock::now();
            for (int i = 0; i < blocks; ++i) en.process (a.data(), b.data(), 512, q, {});
            const auto t1 = std::chrono::steady_clock::now(); g_count = false;
            const double pct = 100.0 * std::chrono::duration<double> (t1 - t0).count() / 20.0;
            char label[96]; std::snprintf (label, sizeof label, "mode %d: heap allocations while processing / CPU %% of one core (this PC)", (int) md);
            check (g_allocs == 0 && pct < 2.0, label, pct);
        }
    }

    std::printf ("\nRESULT: %d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
