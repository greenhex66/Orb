#pragma once
// HEX-Q ORBIT - stereo imaging + auto-pan engine. Pure C++ (no JUCE), no heap allocation.
#include "Biquad.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace hexq::orbit
{
enum class Mode : int { Space = 0, CenterSpace = 1, Pan = 2 };
enum class Wave : int { Sine = 0, Triangle = 1, Square = 2 };

struct Settings
{
    Mode mode = Mode::Space;
    bool bypass = false;
    bool monoCheck = false;

    float width = 1.0f;          // 0..2   (100 % = unchanged)
    float space = 0.35f;         // 0..1
    float center = 1.0f;         // 0..1   (Center + Space)
    float mix = 1.0f;            // 0..1   (Center + Space wet/dry, Pan mix)

    float rateHz = 1.0f;         // 0.05..10 (free mode)
    bool sync = false;           // tempo sync
    float beatsPerCycle = 1.0f;  // 4 (1/1), 2 (1/2), 1 (1/4), 0.5 (1/8), 0.25 (1/16)
    float depth = 1.0f;          // 0..1
    Wave wave = Wave::Sine;
    float phaseDeg = 180.0f;     // 0..180 : phase offset of the right-channel LFO
};

struct HostInfo
{
    double bpm = 120.0;
    double ppq = 0.0;
    bool playing = false;
    bool havePpq = false;
};

struct Meter
{
    float correlation = 1.0f;    // -1..+1, L/R correlation of the output
    float monoSumDb = 0.0f;      // level of the output's mono sum relative to the input's mono sum
};

// LFO shape, aligned with a sine (starts at 0, rising). phase: any real number, output -1..+1
inline float lfoShape (Wave w, float phase) noexcept
{
    phase -= std::floor (phase);
    switch (w)
    {
        case Wave::Triangle:
            return phase < 0.25f ? 4.0f * phase : (phase < 0.75f ? 2.0f - 4.0f * phase : 4.0f * phase - 4.0f);
        case Wave::Square:
            return phase < 0.5f ? 1.0f : -1.0f;
        case Wave::Sine:
        default:
            return std::sin (6.28318530718f * phase);
    }
}

// Per-channel gain of the auto-pan. depth 0 -> exactly 1 (no movement).
// depth 1 -> sin(pi/2 * u) with u = 0..1 (u follows the LFO). Never exceeds 1 -> no clipping from panning.
// With the right LFO 180 degrees apart, gL^2 + gR^2 = 1 at full depth (equal-power pan).
inline float panGain (float lfo, float depth) noexcept
{
    const float u = 0.5f * (lfo + 1.0f);
    return (1.0f - depth) + depth * std::sin (1.57079632679f * u);
}

namespace detail
{
    constexpr int kMaxDelay = 4096;

    // Schroeder all-pass (flat magnitude, scrambles phase)
    struct Allpass
    {
        std::array<float, kMaxDelay> buf {};
        int len = 64, pos = 0;
        float g = 0.5f;

        void setup (int length, float gain) noexcept
        {
            len = std::clamp (length, 8, kMaxDelay);
            g = gain;
            clear();
        }

        void clear() noexcept { buf.fill (0.0f); pos = 0; }

        float process (float x) noexcept
        {
            const float wd = buf[(size_t) pos];
            const float w = x + g * wd;
            const float y = wd - g * w;
            buf[(size_t) pos] = (std::abs (w) < 1.0e-20f) ? 0.0f : w;
            if (++pos >= len) pos = 0;
            return y;
        }
    };

    // Mid -> synthetic side signal: high-pass (keeps bass out of the stereo field) + two all-passes.
    struct Decorrelator
    {
        dsp::Coeffs hp;
        dsp::BiquadState hpState;
        Allpass ap1, ap2;

        void prepare (double sr)
        {
            hp = dsp::makeCoeffs (dsp::FilterType::HighPass, sr, 200.0, 0.0, 0.7071);
            ap1.setup ((int) std::lround (0.0043 * sr), 0.5f);
            ap2.setup ((int) std::lround (0.0021 * sr), 0.45f);
            hpState = {};
        }

        void clear() noexcept { hpState = {}; ap1.clear(); ap2.clear(); }

        float process (float m) noexcept
        {
            const double h = dsp::processSample (hp, hpState, (double) m);
            dsp::flushDenormals (hpState);
            return ap2.process (ap1.process ((float) h));
        }
    };

    inline void slew (float& x, float target, float coef) noexcept
    {
        x += (target - x) * coef;
        if (std::abs (target - x) < 1.0e-5f)
            x = target;
    }
}

class OrbitEngine
{
public:
    void prepare (double sr) noexcept
    {
        sampleRate = sr;
        coefP    = 1.0f - std::exp (-1.0f / (0.015f * (float) sr));    // parameter smoothing 15 ms
        coefPan  = 1.0f - std::exp (-1.0f / (0.0015f * (float) sr));   // pan gain smoothing 1.5 ms (de-clicks square)
        rampStep = (float) (1.0 / (0.020 * sr));                       // mode crossfade 20 ms
        lpCoef   = 1.0f - std::exp (-6.28318530718f * 150.0f / (float) sr);
        deco.prepare (sr);
        reset();
        initialised = false;
    }

    void reset() noexcept
    {
        deco.clear();
        lpState = 0.0f;
    }

    Meter getMeter() const noexcept { return meter; }

    void process (float* L, float* R, int n, const Settings& s, const HostInfo& host) noexcept
    {
        using detail::slew;
        if (n <= 0)
            return;

        const float tW = std::clamp (s.width, 0.0f, 2.0f);
        const float tSp = std::clamp (s.space, 0.0f, 1.0f);
        const float tCe = std::clamp (s.center, 0.0f, 1.0f);
        const float tMx = std::clamp (s.mix, 0.0f, 1.0f);
        const float tDp = std::clamp (s.depth, 0.0f, 1.0f);
        const float tBy = s.bypass ? 1.0f : 0.0f;
        const float tMc = s.monoCheck ? 1.0f : 0.0f;

        if (! initialised)
        {
            snapTo (tW, tSp, tCe, tMx, tDp, tBy, tMc);
            curMode = prevMode = s.mode;
            xf = 1.0f;
            initialised = true;
        }

        if (byS >= 1.0f && tBy >= 1.0f)        // fully bypassed: audio is left untouched, zero CPU
        {
            asleep = true;
            return;
        }
        if (asleep)
        {
            reset();
            snapTo (tW, tSp, tCe, tMx, tDp, tBy, tMc);
            asleep = false;
        }

        if (s.mode != curMode)                 // mode change: crossfade from the old mode to the new one
        {
            prevMode = curMode;
            curMode = s.mode;
            xf = 0.0f;
        }

        // ---- LFO timing ---------------------------------------------------------------------
        double phaseInc;
        if (s.sync)
        {
            const double bpm = std::clamp (host.bpm, 20.0, 400.0);
            const double beats = std::max (0.0625, (double) s.beatsPerCycle);
            phaseInc = (bpm / 60.0) / beats / sampleRate;
            if (host.playing && host.havePpq)
            {
                const double p = host.ppq / beats;
                lfoPhase = p - std::floor (p);        // locked to the host timeline
            }
        }
        else
        {
            phaseInc = (double) std::clamp (s.rateHz, 0.05f, 10.0f) / sampleRate;
        }
        const float phaseOff = std::clamp (s.phaseDeg, 0.0f, 180.0f) / 360.0f;

        const bool fading = (xf < 1.0f);
        const bool needDeco = (curMode != Mode::Pan) || (fading && prevMode != Mode::Pan);
        const bool needPan  = (curMode == Mode::Pan)  || (fading && prevMode == Mode::Pan);

        double sLR = 0, sLL = 0, sRR = 0, sMi = 0, sMo = 0;

        for (int i = 0; i < n; ++i)
        {
            slew (wS, tW, coefP);   slew (spS, tSp, coefP); slew (ceS, tCe, coefP);
            slew (mxS, tMx, coefP); slew (dpS, tDp, coefP);
            slew (byS, tBy, coefP); slew (mcS, tMc, coefP);

            if (std::abs (spS - lastSpace) > 1.0e-6f)
            {
                lastSpace = spS;
                const float th = spS * 0.78539816339f;       // 0..45 degrees
                cosT = std::cos (th);
                sinT = std::sin (th);
            }

            const float inL = L[i], inR = R[i];
            const float m = 0.5f * (inL + inR);
            const float sd = 0.5f * (inL - inR);

            const float d = needDeco ? deco.process (m) : 0.0f;
            lpState += lpCoef * (sd - lpState);
            if (std::abs (lpState) < 1.0e-20f) lpState = 0.0f;
            const float sLo = lpState;

            if (needPan)
            {
                const float ph = (float) lfoPhase;
                detail::slew (gLs, panGain (lfoShape (s.wave, ph), dpS), coefPan);
                detail::slew (gRs, panGain (lfoShape (s.wave, ph + phaseOff), dpS), coefPan);
            }
            lfoPhase += phaseInc;
            if (lfoPhase >= 1.0) lfoPhase -= 1.0;

            float oL, oR;
            render (curMode, inL, inR, m, sd, d, sLo, oL, oR);
            if (xf < 1.0f)
            {
                float pL, pR;
                render (prevMode, inL, inR, m, sd, d, sLo, pL, pR);
                xf = std::min (1.0f, xf + rampStep);
                oL = pL + (oL - pL) * xf;
                oR = pR + (oR - pR) * xf;
            }

            // meter (measured on the processed signal, before Mono Check / Bypass)
            const float mo = 0.5f * (oL + oR);
            sLR += (double) oL * oR;  sLL += (double) oL * oL;  sRR += (double) oR * oR;
            sMi += (double) m * m;    sMo += (double) mo * mo;

            if (mcS > 0.0f)                                    // Mono Check: audition the mono sum
            {
                oL += (mo - oL) * mcS;
                oR += (mo - oR) * mcS;
            }
            if (byS > 0.0f)                                    // global bypass (crossfade)
            {
                oL += (inL - oL) * byS;
                oR += (inR - oR) * byS;
            }

            L[i] = oL;
            R[i] = oR;
        }

        // ---- meter update -------------------------------------------------------------------
        const float a = 1.0f - std::exp (-(float) n / (0.3f * (float) sampleRate));
        if (sLL > 1.0e-12 && sRR > 1.0e-12)
            meter.correlation += a * ((float) (sLR / std::sqrt (sLL * sRR)) - meter.correlation);
        if (sMi > 1.0e-10)
        {
            const float db = (float) std::clamp (10.0 * std::log10 ((sMo + 1.0e-15) / sMi), -24.0, 24.0);
            meter.monoSumDb += a * (db - meter.monoSumDb);
        }
    }

private:
    void render (Mode md, float inL, float inR, float m, float sd, float d, float sLo,
                 float& oL, float& oR) const noexcept
    {
        switch (md)
        {
            case Mode::Space:
            {
                // Energy is traded from the centre into a decorrelated side signal (centre: 0 .. -3 dB).
                // Width scales the whole side component; width 0 = pure mono.
                const float mp = m * cosT;
                const float sp = wS * (sd + d * sinT);
                oL = mp + sp;
                oR = mp - sp;
                break;
            }
            case Mode::CenterSpace:
            {
                // Centre stays at Center Level whatever Space is. Spread layer = real side above ~150 Hz
                // (scaled by Width) + decorrelated side. Bass part of the side is left alone.
                const float mw = m * ceS;
                const float sw = sLo + wS * ((sd - sLo) + spS * 0.8f * d);
                oL = inL + ((mw + sw) - inL) * mxS;
                oR = inR + ((mw - sw) - inR) * mxS;
                break;
            }
            case Mode::Pan:
            default:
            {
                oL = inL + (inL * gLs - inL) * mxS;
                oR = inR + (inR * gRs - inR) * mxS;
                break;
            }
        }
    }

    void snapTo (float w, float sp, float ce, float mx, float dp, float by, float mc) noexcept
    {
        wS = w; spS = sp; ceS = ce; mxS = mx; dpS = dp; byS = by; mcS = mc;
        gLs = gRs = 1.0f;
        lastSpace = -1.0f;
    }

    double sampleRate = 44100.0;
    detail::Decorrelator deco;
    float lpState = 0.0f, lpCoef = 0.02f;
    double lfoPhase = 0.0;

    float wS = 1.0f, spS = 0.0f, ceS = 1.0f, mxS = 1.0f, dpS = 1.0f, byS = 0.0f, mcS = 0.0f;
    float gLs = 1.0f, gRs = 1.0f;
    float coefP = 0.002f, coefPan = 0.01f, rampStep = 0.001f;
    float cosT = 1.0f, sinT = 0.0f, lastSpace = -1.0f;

    Mode curMode = Mode::Space, prevMode = Mode::Space;
    float xf = 1.0f;
    bool initialised = false, asleep = false;

    Meter meter;
};
} // namespace hexq::orbit
