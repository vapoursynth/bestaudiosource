//  Copyright (c) 2020 Fredrik Mellbin
//
//  Permission is hereby granted, free of charge, to any person obtaining a copy
//  of this software and associated documentation files (the "Software"), to deal
//  in the Software without restriction, including without limitation the rights
//  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
//  copies of the Software, and to permit persons to whom the Software is
//  furnished to do so, subject to the following conditions:
//
//  The above copyright notice and this permission notice shall be included in
//  all copies or substantial portions of the Software.
//
//  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
//  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
//  THE SOFTWARE.

#include "audiosource.h"
#include <VapourSynth.h>
#include <VSHelper.h>
#include <vector>

struct BestAudioSourceData {
    VSAudioInfo AI = {};
    BestAudioSource *A = nullptr;
    ~BestAudioSourceData() {
        delete A;
    }
};

static const VSFrameRef *VS_CC BestAudioSourceGetFrame(int n, int activationReason, void **instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    BestAudioSourceData *d = reinterpret_cast<BestAudioSourceData *>(*instanceData);

    if (activationReason == arInitial) {
        int samplesOut = int64ToIntS(VSMIN(d->AI.format->samplesPerFrame, d->AI.numSamples - n * d->AI.format->samplesPerFrame));
        VSFrameRef * f = vsapi->newAudioFrame(d->AI.format, d->AI.sampleRate, samplesOut, nullptr, core);

        std::vector<uint8_t *> tmp;
        tmp.reserve(d->AI.format->numChannels);
        for (int p = 0; p < d->AI.format->numChannels; p++)
            tmp.push_back(vsapi->getWritePtr(f, p));
        try {
            d->A->GetAudio(tmp.data(), n * d->AI.format->samplesPerFrame, samplesOut);
        } catch (AudioException &e) {
            vsapi->setFilterError(e.what(), frameCtx);
            vsapi->freeFrame(f);
            return nullptr;
        }
        return f;
    }

    return nullptr;
}

static void VS_CC BestAudioSourceFree(void *instanceData, VSCore *core, const VSAPI *vsapi) {
    delete reinterpret_cast<BestAudioSourceData *>(instanceData);
}

static void VS_CC CreateBestAudioSource(const VSMap *in, VSMap *out, void *, VSCore *core, const VSAPI *vsapi) {
    int err;
    const char *Source = vsapi->propGetData(in, "source", 0, nullptr);
    int Track = int64ToIntS(vsapi->propGetInt(in, "track", 0, &err));
    if (err)
        Track = -1;
    int AjustDelay = int64ToIntS(vsapi->propGetInt(in, "adjustdelay", 0, &err));
    bool ExactSamples = !!vsapi->propGetInt(in, "exactsamples", 0, &err);

    BestAudioSourceData *D = new BestAudioSourceData();

    try {
        D->A = new BestAudioSource(Source, Track);
        if (ExactSamples)
            D->A->GetExactDuration();
        const AudioProperties &AP = D->A->GetAudioProperties();
        D->AI.format = vsapi->queryAudioFormat(AP.IsFloat, AP.BytesPerSample * 8, AP.ChannelLayout, core);
        if (!D->AI.format)
            throw AudioException("Unsupported audio format from decoder (probably 8-bit)");
        D->AI.sampleRate = AP.SampleRate;
        D->AI.numSamples = AP.NumSamples;
        D->AI.numFrames = int64ToIntS((AP.NumSamples + D->AI.format->samplesPerFrame - 1) / D->AI.format->samplesPerFrame);
    } catch (AudioException &E) {
        delete D;
        vsapi->setError(out, E.what());
        return;
    }

    vsapi->createAudioFilter(in, out, "Source", &D->AI, 1, BestAudioSourceGetFrame, BestAudioSourceFree, fmUnordered, nfMakeLinear, D, core);
}

VS_EXTERNAL_API(void) VapourSynthPluginInit(VSConfigPlugin configFunc, VSRegisterFunction registerFunc, VSPlugin *plugin) {
    configFunc("com.vapoursynth.bestaudiosource", "bas", "Best Audio Source", VAPOURSYNTH_API_VERSION, 1, plugin);
    registerFunc("Source", "source:data;track:int:opt;adjustdelay:int:opt;exactsamples:int:opt;", CreateBestAudioSource, nullptr, plugin);
}
