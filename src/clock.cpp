#include "plugin.hpp"



#define PASSTHROUGH_RIGHT_VARIABLE_COUNT 34

struct Clock : Module {
	enum ParamIds {
		DIV_PARAM,
		DIV_CV_PARAM,
		SWING_PARAM,
		SWING_CV_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		DIV_CV,
		SWING_CV,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		IN_LIGHT,
		OUT_LIGHT,
		NUM_LIGHTS
	};

	// Expander
	float consumerMessage[PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};// this module must read from here
	float producerMessage[PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};// mother will write into here



	Clock() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(DIV_PARAM, 0.f, 16.f, 8.f, "");
		configParam(DIV_CV_PARAM, -1.f, 1.f, 0.f, "");
		configParam(SWING_PARAM, 0.f, 1.f, 0.f, "");
		configParam(SWING_CV_PARAM, -1.f, 1.f, 0.f, "");
		
		leftExpander.producerMessage = producerMessage;
		leftExpander.consumerMessage = consumerMessage;
	}
	
	dsp::SchmittTrigger clock_input[16];
	dsp::PulseGenerator sendPulse[16];
	dsp::PulseGenerator inPulse;
	dsp::PulseGenerator outPulse;
	bool sendingOutput[16] = {false};
	bool canPulse[16] = {false};
	long long int currentStep = 0;
	long long int previousStep[16] = {0};
	long long int expectedStep[16] = {0};
	long stepGap[16] = {0};
	long stepGapPrevious[16] = {0};
	long long int nextPulseStep[16] = {0};
	bool isSync[16] = {0};
	bool swing[16] = {0};
	enum ClkModModeIds {
		X1,	// work at x1.
		DIV,	// divider mode.
		MULT	// muliplier mode.
	};
	int clkModulatorMode[16] = {X1};
	//float ratio_list[31] = {64.0f, 32.0f, 24.0f, 16.0f, 15.0f, 12.0f, 10.0f, 9.0f, 8.0f, 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.5f, 1.0f/3.0f, 0.25f, 0.2f, 1.0f/6.0f, 1.0f/7.0f, 0.125f, 1.0f/9.0f, 0.1f, 1.0f/12.0f, 1.0f/15.0f, 0.0625f, 1.0f/24.0f, 0.03125f, 0.015625f};
	float ratio_list[17] = {9.0f, 8.0f, 7.0f, 6.0f, 5.0f, 4.0f, 3.0f, 2.0f, 1.0f, 0.5f, 1.0f/3.0f, 0.25f, 0.2f, 1.0f/6.0f, 1.0f/7.0f, 0.125f, 1.0f/9.0f};
	int pulseDivCounter[16] = {63};
	int pulseMultCounter[16] = {0};
	float time = 1/44100;

	void onSampleRateChange() override {
		time = (float)(APP->engine->getSampleTime());
	}		
	

	void process(const ProcessArgs &args) override{

		//Expander In
		bool motherPresent = leftExpander.module && (leftExpander.module->model == modelPolyturing || leftExpander.module->model == modelClock || leftExpander.module->model == modelBtnseq || leftExpander.module->model == modelManseq || leftExpander.module->model == modelPolyslew);
		bool messagePresent = false;
		int messageChannels = 0;
		float messageClock[16] = {0};
		float messageCV[16] = {0};

		if(motherPresent && !inputs[CLOCK_INPUT].isConnected())  {
			float *messagesFromMother = (float*)leftExpander.consumerMessage;
			messagePresent = messagesFromMother[0] > 0 ? true : false;
			if (messagePresent){
				messageChannels = messagesFromMother[0];
				for(int i = 0; i < messageChannels; i++) messageClock[i] = messagesFromMother[i + 1];
				for(int i = 0; i < messageChannels; i++) messageCV[i] = messagesFromMother[i + 1 + messageChannels];
			}
		}

		int channels = messagePresent ? messageChannels : inputs[CLOCK_INPUT].getChannels();

		float ratio_kn[channels];
		float ratio[channels];

		for(int c = 0;c < channels; c++){
			if (motherPresent && !inputs[DIV_CV].isConnected() && leftExpander.module->model == modelPolyturing) ratio_kn[c] = int(params[DIV_PARAM].getValue()) + int(rescale(messageCV[c], -10.f, 10.f, -9.f, 9.f) * params[DIV_CV_PARAM].getValue());
			else ratio_kn[c] = int(params[DIV_PARAM].getValue()) + int(rescale(inputs[DIV_CV].getVoltage(c), -5.f, 5.f, -9.f, 9.f) * params[DIV_CV_PARAM].getValue());
			//int jitter = rescale(params[SWING_PARAM].getValue(), 0.f, 1.f, 0, stepGap[c] -1);
			if (ratio_kn[c]  == 8) clkModulatorMode[c] = X1;
			else if (ratio_kn[c]  < 8) clkModulatorMode[c] =  DIV;
			else if (ratio_kn[c]  > 8) clkModulatorMode[c] =  MULT;
			
			ratio[c] = ratio_list[int(ratio_kn[c])];
		}



		if (inputs[CLOCK_INPUT].isConnected() || messagePresent) {
			// Increment step number.
			currentStep++;
			
			// Using Schmitt trigger (SchmittTrigger is provided by dsp/digital.hpp) to detect thresholds from CLK input connector. Calibration: +1.7V (rising edge), low +0.2V (falling edge).
			
			
			for (int c = 0; c < channels; c++){
				float clock = messagePresent ? messageClock[c] : inputs[CLOCK_INPUT].getVoltage(c);
				if (clock_input[c].process(rescale(clock, 0.2f, 1.7f, 0.0f, 1.0f))) {
					inPulse.trigger(0.1f);
					if (previousStep[c] == 0) {
						// No "history", it's the first pulse recebived on CLK input after a frequency change. Not synchronized.
						//currentStep = 0;
						expectedStep[c] = 0;
						stepGap[c] = 0;
						stepGapPrevious[c] = 0;
						// Not synchronized.
						isSync[c] = false;
						
						//canPulse[c] = (clkModulatorMode[c] != MULT); // MULT needs second pulse to establish source frequency.
						pulseDivCounter[c] = 0; // Used for DIV mode exclusively!
						pulseMultCounter[c] = 0; // Used for MULT mode exclusively!
						
						previousStep[c] = currentStep;
					}
					else {
						// It's the second pulse received on CLK input after a frequency change.
						stepGapPrevious[c] = stepGap[c];
						stepGap[c] = currentStep - previousStep[c];
						expectedStep[c] = currentStep + stepGap[c];
						// The frequency is known, we can determine the pulse duration (defined by SETUP).
						// The pulse duration also depends of clocking ratio, such "X1", multiplied or divided, and its ratio.
						isSync[c] = true;
						if (stepGap[c] > stepGapPrevious[c])
							isSync[c] = ((stepGap[c] - stepGapPrevious[c]) < 2);
							else if (stepGap[c] < stepGapPrevious[c])
								isSync[c] = ((stepGapPrevious[c] - stepGap[c]) < 2);
						previousStep[c] = currentStep;
					}
					
					int jitter = rescale(params[SWING_PARAM].getValue() + ((params[SWING_CV_PARAM].getValue() * inputs[SWING_CV].getVoltage(c)) / 5.f), 0.f, 1.f, 0, (round(stepGap[c] * ratio[c]) / 4));
					int jit = rescale(params[SWING_PARAM].getValue() + ((params[SWING_CV_PARAM].getValue() * inputs[SWING_CV].getVoltage(c)) / 5.f), 0.f, 1.f, 0, stepGap[c]);
						
					switch (clkModulatorMode[c]) {
						case X1:
							nextPulseStep[c] = currentStep + jit;
							if (currentStep == nextPulseStep[c]) canPulse[c] = true;
						
						break;
							
						case DIV:
	
							if (pulseDivCounter[c] == 0) {
								pulseDivCounter[c] = int(ratio[c] - 1); // Ratio is controlled by knob.
								nextPulseStep[c] = currentStep + jit;
							}
							else {
								pulseDivCounter[c]--;
								canPulse[c] = false;
							}
							if (currentStep == nextPulseStep[c]) canPulse[c] = true;
						
							break;
						case MULT:
							// Multiplier mode scenario: pulsing only when source frequency is established.
							
							if (isSync[c]) {
								// Next step for pulsing in multiplier mode.
								
								//if (isRatioCVmod) {
								//	// Ratio is CV-controlled.
								//	nextPulseStep[c] = currentStep + round(stepGap[c] / rateRatioCV);
								//	pulseMultCounter[c] = int(rateRatioCV) - 1;
								//}
								//else {
								// Ratio is controlled by knob.							
								//}
								//
								int veroGap;
								if (swing[c]){ 
									veroGap = round(stepGap[c] * ratio[c]) - jitter;
									swing[c] = 0;
								}
								else {
									veroGap = round(stepGap[c] * ratio[c]) + jitter;
									swing[c] = 1;
								}
								//veroGap = round(stepGap[c] * ratio[c]);  			//"DISABILITA SWING"
								//nextPulseStep[c] = currentStep + round(stepGap[c] * ratio[c]);
								nextPulseStep[c] = currentStep + veroGap;
								
								pulseMultCounter[c] = round(1.0f / ratio[c]) - 1;
								canPulse[c] = true;
							}
					}
				}
				else {
					if (isSync[c] && (nextPulseStep[c] == currentStep) && (clkModulatorMode[c] == X1)) {
						canPulse[c] = true;
					}
					if (isSync[c] && (nextPulseStep[c] == currentStep) && (clkModulatorMode[c] == DIV)) {
						canPulse[c] = true;
					}

					if (isSync[c] && (nextPulseStep[c] == currentStep) && (clkModulatorMode[c] == MULT)) {

						nextPulseStep[c] = currentStep + round(stepGap[c] * ratio[c]); // Ratio is controlled by knob.

						if (pulseMultCounter[c] > 0) {
							pulseMultCounter[c]--;
							canPulse[c] = true;
						}
						else {
							canPulse[c] = false;
							isSync[c] = false;
						}
					}
				}
			}
		}
		else{
			for (int c = 0; c < channels; c++){
			currentStep = 0;
			expectedStep[c] = 0;
			stepGap[c] = 0;
			stepGapPrevious[c] = 0;
			}
		}

		for (int c = 0; c < channels; c++) {
			if (canPulse[c]) {
				outPulse.trigger(0.1);
				sendPulse[c].trigger(0.1f);
				canPulse[c] = false;
			}
			sendingOutput[c] = sendPulse[c].process(44100/4.f);
			outputs[OUT_OUTPUT].setChannels(channels);
			outputs[OUT_OUTPUT].setVoltage((sendingOutput[c] ? 10.f : 0.0f), c);
			lights[IN_LIGHT].setBrightness(inPulse.process(1.0 / 44100));
			lights[OUT_LIGHT].setBrightness(outPulse.process(1.0 / 44100));
		}
		
		//Expander Out
		bool rightExpanderPresent = (rightExpander.module && (rightExpander.module->model == modelPolyturing || rightExpander.module->model == modelClock || rightExpander.module->model == modelBtnseq || rightExpander.module->model == modelManseq));
		if(rightExpanderPresent) {
			float *messageToSlave = (float*) rightExpander.module->leftExpander.producerMessage;
			messageToSlave[0] = channels;
			for(int c = 0; c < channels; c++) messageToSlave[c + 1] = sendingOutput[c] ? 10.f : 0.0f;
			//for(int c = 0; c < channels; c++) messageToSlave[c + channels] = messagePresent ? out[c];
			rightExpander.module->leftExpander.messageFlipRequested = true;
		}
	}
};

struct ClockWidget : ModuleWidget {
	ClockWidget(Clock *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/clock.svg")));

		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(2.483, 2.088))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(17.837, 2.112))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(2.483, 126.412))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(17.837, 126.436))));

		addParam(createParamCentered<stocSnapKnob>(mm2px(Vec(10.68, 40.407)), module, Clock::DIV_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(10.68, 50.809)), module, Clock::DIV_CV_PARAM));
		addParam(createParamCentered<stocKnob>(mm2px(Vec(10.68, 81.819)), module, Clock::SWING_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(10.68, 92.186)), module, Clock::SWING_CV_PARAM));

		addInput(createInputCentered<aPJackArancione>(mm2px(Vec(10.336, 20.691)), module, Clock::CLOCK_INPUT));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(10.68, 59.118)), module, Clock::DIV_CV));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(10.68, 102.29)), module, Clock::SWING_CV));

		addOutput(createOutputCentered<aPJackTurchese>(mm2px(Vec(10.336, 119.744)), module, Clock::OUT_OUTPUT));

		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(16.806, 20.642)), module, Clock::IN_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(3.866, 119.79)), module, Clock::OUT_LIGHT));


	}
};


Model *modelClock = createModel<Clock, ClockWidget>("Clock");