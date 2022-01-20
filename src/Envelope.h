class ADSREnvelope{
    public:

    ADSREnvelope(){
        iter = 0;
        activeSegment = 0;
    }

    float tick(bool gate){
        if(gate){
            if(activeSegment == 3) {
                    activeSegment = 0;
		            std::cout << activeSegment  << std::endl;
                }
            if(activeSegment < 2){
                if(iter > times[activeSegment] * maxSamples){
                    iter = 0;
                    activeSegment++;

		            std::cout << activeSegment  << std::endl;
                }
                else {
                    iter++;
                }
            }
        }
        else{
            if(activeSegment != 3){
                activeSegment = 3;
                iter = 0;
		            std::cout << activeSegment  << std::endl;
            }
            if(iter > times[activeSegment] * maxSamples){
                activeSegment = 0;
                iter = 0;
		            std::cout << activeSegment  << std::endl;
            }
            else{
                iter++;
            }
        }

        if(activeSegment == 0){
            return math::rescale(float(iter / (times[activeSegment] * maxSamples)), 0.0f, 1.0f, 0.0f, 1.0f);
        }
        else if(activeSegment == 1){
            return math::rescale(float(iter / (times[activeSegment] * maxSamples)), 0.0f, 1.0f, 1.0f, sustain);
        }
        else if(activeSegment == 2){
            return sustain;
        }
        else if(activeSegment == 3){
            return math::rescale(float(iter / (times[activeSegment] * maxSamples)), 0.0f, 1.0f, sustain, 0.0f);
        }
    }


    double maxSamples;
    unsigned int iter = 0;
    unsigned int activeSegment = 0;
    unsigned int times[3]{500};
    float sustain = 1.0f;
    bool gate = false;
};