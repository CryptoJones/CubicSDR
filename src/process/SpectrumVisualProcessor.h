#pragma once

#include "VisualProcessor.h"
#include "DemodDefs.h"
#include "fftw3.h"
#include <cmath>

class SpectrumVisualData : public ReferenceCounter {
public:
    std::vector<float> spectrum_points;
    double fft_ceiling, fft_floor;
};

typedef ThreadQueue<SpectrumVisualData *> SpectrumVisualDataQueue;

class SpectrumVisualProcessor : public VisualProcessor<DemodulatorThreadIQData, SpectrumVisualData> {
public:
    SpectrumVisualProcessor();
    ~SpectrumVisualProcessor();
    
    bool isView();
    void setView(bool bView);
    
    void setFFTAverageRate(float fftAverageRate);
    float getFFTAverageRate();
    
    void setCenterFrequency(long long centerFreq_in);
    long long getCenterFrequency();
    
    void setBandwidth(long bandwidth_in);
    long getBandwidth();
    
    int getDesiredInputSize();
    
    void setup(int fftSize);
    
protected:
    void process();
    
    ReBuffer<SpectrumVisualData> outputBuffers;
    std::atomic_bool is_view;
    std::atomic_int fftSize;
    std::atomic_llong centerFreq;
    std::atomic_long bandwidth;
    
private:
    long lastInputBandwidth;
    long lastBandwidth;
    
    fftwf_complex *fftwInput, *fftwOutput, *fftInData, *fftLastData;
    unsigned int lastDataSize;
    fftwf_plan fftw_plan;
    
    double fft_ceil_ma, fft_ceil_maa;
    double fft_floor_ma, fft_floor_maa;
    float fft_average_rate;
    
    std::vector<double> fft_result;
    std::vector<double> fft_result_ma;
    std::vector<double> fft_result_maa;
    
    msresamp_crcf resampler;
    double resamplerRatio;
    nco_crcf freqShifter;
    long shiftFrequency;
    
    std::vector<liquid_float_complex> shiftBuffer;
    std::vector<liquid_float_complex> resampleBuffer;
    int desiredInputSize;
};


class FFTDataDistributor : public VisualProcessor<DemodulatorThreadIQData, DemodulatorThreadIQData> {
public:
    FFTDataDistributor() : linesPerSecond(DEFAULT_WATERFALL_LPS), lineRateAccum(0.0) {
    }
    
    void setFFTSize(int fftSize) {
        this->fftSize = fftSize;
    }
    
    void setLinesPerSecond(int lines) {
        this->linesPerSecond = lines;
    }
    
    int getLinesPerSecond() {
        return this->linesPerSecond;
    }
protected:
    void process() {
        while (!input->empty()) {
            if (!isAnyOutputEmpty()) {
                return;
            }
            DemodulatorThreadIQData *inp;
            input->pop(inp);

            int fftSize = this->fftSize;
            
            if (fftSize > inp->data.size()) {
                fftSize = inp->data.size();
            }

            // number of milliseconds contained in input
            double inputTime = (double)inp->data.size() / (double)inp->sampleRate;
            // number of lines in input
            int inputLines = floor((double)inp->data.size()/(double)fftSize);
            
            // ratio required to achieve the desired rate
            double lineRateStep = ((double)linesPerSecond * inputTime)/(double)inputLines;
            
            if (inp) {
                if (inp->data.size() >= fftSize) {
                    if (lineRateAccum + (lineRateStep * floor((double)inp->data.size()/(double)fftSize)) < 1.0) {
                         // move along, nothing to see here..
                        lineRateAccum += (lineRateStep * inp->data.size()/fftSize);
                    } else for (int i = 0, iMax = inp->data.size()-fftSize; i <= iMax; i += fftSize) {
                        lineRateAccum += lineRateStep;
                        
                        if (lineRateAccum >= 1.0) {
                            DemodulatorThreadIQData *outp = outputBuffers.getBuffer();
                            outp->frequency = inp->frequency;
                            outp->sampleRate = inp->sampleRate;
                            outp->data.assign(inp->data.begin()+i,inp->data.begin()+i+fftSize);
                            distribute(outp);
                            
                            while (lineRateAccum >= 1.0) {
                                lineRateAccum -= 1.0;
                            }
                        }
                    }
                }
                inp->decRefCount();
            }
        }
    }
    
    ReBuffer<DemodulatorThreadIQData> outputBuffers;
    int fftSize;
    int linesPerSecond;
    double lineRateAccum;
};