#include <vector>
#include "audiofile/AudioFile.h"
#define MAX_GRAINS 100

class Grain{
    public:
    Grain(AudioFile<float>::AudioBuffer newBuffer, float newShape, float newCurve, float newSpeed, std::vector<float> newPan){
        buffer.resize(newBuffer.size());
        for(unsigned c = 0; c < buffer.size(); c++){
            buffer[c].resize(newBuffer[c].size());
            for(unsigned i = 0; i < buffer[c].size(); i++)
                buffer[c][i] = newBuffer[c][i];
        }
        shape = newShape;
        curve = newCurve;
        speed = newSpeed;
        pan = newPan;
        iter = 0;
    }

    std::vector<float> tick(){
        if(unsigned(iter) > buffer[0].size()){
            hasFinished = true;
            return std::vector<float>(2, 0.0f);
        }
        else{
            hasFinished = false;
            float phase = iter / buffer[0].size();
            float amp = 0.0f;
            if(phase < shape) amp = rack::math::rescale(phase, 0.0f, shape, 0.0f, 1.0f);
            else amp = rack::math::rescale(phase, shape, 1.0f, 1.0f, 0.0f);
            amp = std::pow(amp, curve);
            amp = rack::math::clamp(amp, 0.0f, 1.0f);
            std::vector<float> sampleOut(2,0.0f);
            for(unsigned c = 0; c < buffer.size(); c++){
                float interp = iter - unsigned(iter);
                sampleOut[c] = (buffer[c][unsigned(iter)] * (interp)) + (buffer[c][unsigned(iter - 1) % buffer[c].size()] * (1.0f - interp));
            }
            iter += speed;
            for(int i=0;i<sampleOut.size();++i)
	            sampleOut[i] = sampleOut[i] * amp * pan[i];
            return sampleOut;
        }
    }


    std::vector<float>pan{2, 1.0f};
    AudioFile<float>::AudioBuffer buffer;
    float speed, shape, curve;
    float iter = 0;
    bool hasFinished = false;
};

class Scheduler{
    public:

    Scheduler(){
        iter = 0;
    }

    bool tick(){
        if(iter > delta){
            iter = 0;
            return 1;
        }
        else {
            iter++;
            return 0;
        }
    }
    unsigned int iter = 0;
    unsigned int delta = 500;
};

class Granular {
    public:
    Granular(){
        buffer.clear();
        activeGrains.clear();
    }

    void setBuffer(AudioFile<float>::AudioBuffer newBuffer){
        buffer.clear();
        if(newBuffer.size() > 1){
            buffer.resize(newBuffer.size());
            for(unsigned c = 0; c < buffer.size(); c++){
                buffer[c].resize(newBuffer[c].size());
                for(unsigned i = 0; i < buffer[c].size(); i++){
                    buffer[c][i] = newBuffer[c][i];
                }
            }
        }
        else{
            buffer.resize(2);
            for(unsigned c = 0; c < buffer.size(); c++){
                buffer[c].resize(newBuffer[0].size());
                for(unsigned i = 0; i < buffer[c].size(); i++){
                    buffer[c][i] = newBuffer[0][i];
                }
            }
        }
        return;
    }

    void trigger(){
        scheduler.iter = 0;
        if(!boundMode) playhead = math::rescale(start, 0.0, 1.0f, 0, buffer[0].size());
        else playhead = math::rescale(start, 0.0f, 1.0f, 0, buffer.size() / end) * (buffer[0].size() / end);
        isPlaying = true;
        return;
    }

    std::vector<float> process(){
        actualSize = int(sampleRate * 0.25 * size);
        scheduler.delta = int(actualSize * overlap);
        if(scheduler.tick() && activeGrains.size() < MAX_GRAINS && isPlaying){
            AudioFile<float>::AudioBuffer bufferSlice;
            bufferSlice.resize(buffer.size());
            for(unsigned c = 0; c < bufferSlice.size(); c++){
                bufferSlice[c].resize(actualSize);
                for(unsigned i = 0; i < actualSize; i++){
                    unsigned pos = (playhead + i) % buffer[c].size();
                    bufferSlice[c][i] = buffer[c][pos];
                }
            }
            activeGrains.push_back(new Grain(bufferSlice, shape, curve, speed, seedSpread()));
            playhead += (actualSize * overlap) * stretch;

            if(playhead > math::rescale(end, 0.0, 1.0f, buffer[0].size() * start, buffer[0].size())){
                if(loop) trigger();
                else isPlaying = false;
            }
		    //std::cout << activeGrains.size() << std::endl;
        }

        std::vector<float> signal(2,0.0f);
        signal.resize(buffer.size());
        std::vector<int>toRemove;
        for(unsigned g = 0; g < activeGrains.size(); g++){
            if(activeGrains[g]->hasFinished == true) toRemove.push_back(g);
            else {
                activeGrains[g]->speed = speed;
                std::vector<float> thisGrainOut;
                thisGrainOut.resize(buffer.size());
                thisGrainOut = activeGrains[g]->tick();
                for(int c = 0; c < thisGrainOut.size(); c++)
                    signal[c] = signal[c] + thisGrainOut[c];
            }
        }
        for(unsigned g = 0; g < toRemove.size(); g++){
            activeGrains.erase(activeGrains.begin() + toRemove[g]);
        }
        return signal;
    }

    std::vector<float> seedSpread(){
        float randomPan = (random::uniform()  * 2.0f ) -1.0f;
        float thisPan = rescale(clamp(randomPan, -1.f, 1.f), -1.f, 1.f, 0.f, 1.f);
        std::vector<float> pan(2, 1.0f);
        pan[0] += thisPan * spread;
        pan[1] += (1.0f - thisPan) * spread;
        return pan;
    }

    Scheduler scheduler;
    std::vector<Grain*> activeGrains;

    AudioFile<float>::AudioBuffer buffer;
    bool isPlaying = false;
    bool loop = true;
    unsigned playhead = 0;
    double sampleRate;
    unsigned actualSize;
    float size = 0.5f;
    float overlap = 0.5f;
    float speed = 1.0f;
    float stretch = 1.0f;
    float spread = 1.0f;
    bool boundMode = 0;
    float start = 0;
    float end = 0;
    float curve = 0.0f;
    float shape = 0.0f;
};
