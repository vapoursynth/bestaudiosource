//  Copyright (c) 2020-2021 Fredrik Mellbin
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
#include <VapourSynth4.h>
#include <vector>
#include <algorithm>
#include <memory>

struct BestAudioSourceData {
    VSAudioInfo AI = {};
    std::unique_ptr<BestAudioSource> A;
};

static const VSFrame *VS_CC BestAudioSourceGetFrame(int n, int activationReason, void *instanceData, void **frameData, VSFrameContext *frameCtx, VSCore *core, const VSAPI *vsapi) {
    BestAudioSourceData *d = reinterpret_cast<BestAudioSourceData *>(instanceData);

    if (activationReason == arInitial) {
        int64_t samplesOut = std::min<int64_t>(VS_AUDIO_FRAME_SAMPLES, d->AI.numSamples - n * static_cast<int64_t>(VS_AUDIO_FRAME_SAMPLES));
        VSFrame *f = vsapi->newAudioFrame(&d->AI.format, static_cast<int>(samplesOut), nullptr, core);

        std::vector<uint8_t *> tmp;
        tmp.reserve(d->AI.format.numChannels);
        for (int p = 0; p < d->AI.format.numChannels; p++)
            tmp.push_back(vsapi->getWritePtr(f, p));
        try {
            d->A->GetAudio(tmp.data(), n * static_cast<int64_t>(VS_AUDIO_FRAME_SAMPLES), samplesOut);
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
    const char *Source = vsapi->mapGetData(in, "source", 0, nullptr);
    int Track = vsapi->mapGetIntSaturated(in, "track", 0, &err);
    if (err)
        Track = -1;
    int AdjustDelay = vsapi->mapGetIntSaturated(in, "adjustdelay", 0, &err);
    bool ExactSamples = !!vsapi->mapGetInt(in, "exactsamples", 0, &err);

    FFmpegOptions opts;
    opts.enable_drefs = !!vsapi->mapGetInt(in, "enable_drefs", 0, &err);
    opts.use_absolute_path = !!vsapi->mapGetInt(in, "use_absolute_path", 0, &err);
    opts.drc_scale = vsapi->mapGetFloat(in, "drc_scale", 0, &err);

    BestAudioSourceData *D = new BestAudioSourceData();

    try {
        D->A.reset(new BestAudioSource(Source, Track, AdjustDelay, &opts));
        if (ExactSamples)
            D->A->GetExactDuration();
        const AudioProperties &AP = D->A->GetAudioProperties();
        if (!vsapi->queryAudioFormat(&D->AI.format, AP.IsFloat, AP.BitsPerSample, AP.ChannelLayout, core))
            throw AudioException("Unsupported audio format from decoder (probably 8-bit)");
        D->AI.sampleRate = AP.SampleRate;
        D->AI.numSamples = AP.NumSamples;
        D->AI.numFrames = static_cast<int>((AP.NumSamples + VS_AUDIO_FRAME_SAMPLES - 1) / VS_AUDIO_FRAME_SAMPLES);
        if ((AP.NumSamples + VS_AUDIO_FRAME_SAMPLES - 1) / VS_AUDIO_FRAME_SAMPLES > std::numeric_limits<int>::max())
            throw AudioException("Unsupported audio format from decoder (probably 8-bit)");
    } catch (AudioException &e) {
        delete D;
        vsapi->mapSetError(out, (std::string("BestAudioSource: ") + e.what()).c_str());
        return;
    }

    vsapi->createAudioFilter(out, "Source", &D->AI, BestAudioSourceGetFrame, BestAudioSourceFree, fmUnordered, nullptr, 0, D, core);
}

VS_EXTERNAL_API(void) VapourSynthPluginInit2(VSPlugin *plugin, const VSPLUGINAPI *vspapi) {
    vspapi->configPlugin("com.vapoursynth.bestaudiosource", "bas", "Best Audio Source", VS_MAKE_VERSION(0, 8), VAPOURSYNTH_API_VERSION, 0, plugin);
    vspapi->registerFunction("Source", "source:data;track:int:opt;adjustdelay:int:opt;exactsamples:int:opt;enable_drefs:int:opt;use_absolute_path:int:opt;drc_scale:float:opt;", "clip:anode;", CreateBestAudioSource, nullptr, plugin);
}
