#include "plugin.hpp"
#include "samplerate.h"
#define HISTORY_SIZE (1<<21)


struct Polydelay : Module {
	enum ParamIds {
		TIME_PARAM,
		TIMEL_CV_PARAM,
		TIMER_CV_PARAM,
		SPREAD_PARAM,
		MIX_PARAM,
		FEED_PARAM,
		MIX_CV_PARAM,
		FEED_CV_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		L_INPUT,
		R_INPUT,
		TIMEL_CV,
		TIMER_CV,
		MIX_CV,
		FEED_CV,
		NUM_INPUTS
	};
	enum OutputIds {
		OUTL_OUTPUT,
		OUTR_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	dsp::DoubleRingBuffer<float, HISTORY_SIZE> historyBufferL[16];
	dsp::DoubleRingBuffer<float, HISTORY_SIZE> historyBufferR[16];
	dsp::DoubleRingBuffer<float, 16> outBufferL[16];
	dsp::DoubleRingBuffer<float, 16> outBufferR[16];
	SRC_STATE *srcL[16];
	SRC_STATE *srcR[16];
	float lastL[16];
	float lastR[16];

	Polydelay() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(TIME_PARAM, 0.f, 1.f, 0.5f, "Delay Time");
		configParam(TIMEL_CV_PARAM, -1.f, 1.f, 0.f, "Time CV amount Left");
		configParam(TIMER_CV_PARAM, -1.f, 1.f, 0.f, "Time CV amount Right");
		configParam(SPREAD_PARAM, 0.f, 1.f, 0.f, "Poly Time Spread");
		configParam(MIX_PARAM, 0.f, 1.f, 0.5f, "Dry/Wet Mix");
		configParam(FEED_PARAM, -1.f, 1.f, 0.f, "Feedback amount");
		configParam(MIX_CV_PARAM, -1.f, 1.f, 0.f, "Dry/Wet CV amount");
		configParam(FEED_CV_PARAM, -1.f, 1.f, 0.f, "Feedback CV amount");
		configInput(L_INPUT, "Left Input");
		configInput(R_INPUT, "Right Input");
		configInput(TIMEL_CV, "Time CV Left");
		configInput(TIMER_CV, "Time CV Right");
		configInput(MIX_CV, "Dry/Wet CV");
		configInput(FEED_CV, "Feedback CV");
		configOutput(OUTL_OUTPUT, "Left Output");
		configOutput(OUTR_OUTPUT, "Right Output");

		for (int c = 0; c < 16; c++){
			srcL[c] = src_new(SRC_SINC_FASTEST, 1, NULL);
			assert(srcL[c]);
			srcR[c] = src_new(SRC_SINC_FASTEST, 1, NULL);
			assert(srcR[c]);
		}
	}

	~Polydelay() {
		for (int c = 0; c < 16; c++) src_delete(srcL[c]);
		for (int c = 0; c < 16; c++) src_delete(srcR[c]);
	}



	void process(const ProcessArgs &args) override {

		int channels = inputs[L_INPUT].getChannels();
		outputs[OUTL_OUTPUT].setChannels(channels);	
		bool stereoMode = inputs[R_INPUT].isConnected() ? true : false;
		if (stereoMode) outputs[OUTR_OUTPUT].setChannels(inputs[R_INPUT].getChannels());	
		else outputs[OUTR_OUTPUT].setChannels(channels);	
		
		float spread = params[SPREAD_PARAM].getValue();
		float mix = clamp(params[MIX_PARAM].getValue() + (params[MIX_CV_PARAM].getValue() * inputs[MIX_CV].getVoltage()), 0.0f, 1.0f);
		
		for (int c = 0; c < channels; c++){
			float feedback = params[FEED_PARAM].getValue() + (params[FEED_CV_PARAM].getValue() * inputs[FEED_CV].getPolyVoltage(c));
			float in = inputs[L_INPUT].getVoltage(c) + lastL[c] * feedback;
			float modulationL;
			if (!inputs[TIMEL_CV].isConnected()) modulationL = params[TIMEL_CV_PARAM].getValue() * 0.5;

			else  modulationL = params[TIMEL_CV_PARAM].getValue() * (inputs[TIMEL_CV].getPolyVoltage(c) / 10.f);
			float delay  = params[TIME_PARAM].getValue() + modulationL;
			delay = clamp(delay, 0.f, 1.f);
			delay =  1e-3 * std::pow(10.f / 1e-3, delay);
			delay += rescale(spread, 0.f, 1.f, 0.f, delay) * c;
			// Number of delay samples
			float index = std::round(delay * args.sampleRate);

			// Push dry sample into history buffer
			if (!historyBufferL[c].full()) {
				historyBufferL[c].push(in);
			}

			// How many samples do we need consume to catch up?
			float consume = index - historyBufferL[c].size();

			if (outBufferL[c].empty()) {
				double ratio = 1.f;
				if (std::fabs(consume) >= 16.f) {
					// Here's where the delay magic is. Smooth the ratio depending on how divergent we are from the correct delay time.
					ratio = std::pow(10.f, clamp(consume / 10000.f, -1.f, 1.f));
				}

				SRC_DATA srcData;
				srcData.data_in = (const float*) historyBufferL[c].startData();
				srcData.data_out = (float*) outBufferL[c].endData();
				srcData.input_frames = std::min((int) historyBufferL[c].size(), 16);
				srcData.output_frames = outBufferL[c].capacity();
				srcData.end_of_input = false;
				srcData.src_ratio = ratio;
				src_process(srcL[c], &srcData);
				historyBufferL[c].startIncr(srcData.input_frames_used);
				outBufferL[c].endIncr(srcData.output_frames_gen);
			}

			float wet = 0.f;
			if (!outBufferL[c].empty()) {
				wet = outBufferL[c].shift();
				float proc = (inputs[L_INPUT].getVoltage(c) * (1 - mix)) + (wet * mix);
				outputs[OUTL_OUTPUT].setVoltage(proc, c);
			}
			lastL[c] = wet;
		}

		for (int c = 0; c < channels; c++){
			float feedback = params[FEED_PARAM].getValue() + (params[FEED_CV_PARAM].getValue() * inputs[FEED_CV].getVoltage(c));
			float in;
			if (stereoMode) in = inputs[R_INPUT].getVoltage(c) + lastR[c] * feedback;
			else in = inputs[L_INPUT].getVoltage(c) + lastR[c] * feedback;
			float modulationL;
			if (!inputs[TIMER_CV].isConnected()) modulationL = params[TIMER_CV_PARAM].getValue() * 0.5;
			else  modulationL = params[TIMER_CV_PARAM].getValue() * inputs[TIMER_CV].getVoltage(c);
			float delay  = params[TIME_PARAM].getValue() + modulationL;	
			delay = clamp(delay, 0.f, 1.f);
			delay =  1e-3 * std::pow(10.f / 1e-3, delay);
			delay += rescale(spread, 0.f, 1.f, 0.f, delay) * c;
			// Number of delay samples
			float index = std::round(delay * args.sampleRate);

			// Push dry sample into history buffer
			if (!historyBufferR[c].full()) {
				historyBufferR[c].push(in);
			}

			// How many samples do we need consume to catch up?
			float consume = index - historyBufferR[c].size();

			if (outBufferR[c].empty()) {
				double ratio = 1.f;
				if (std::fabs(consume) >= 16.f) {
					// Here's where the delay magic is. Smooth the ratio depending on how divergent we are from the correct delay time.
					ratio = std::pow(10.f, clamp(consume / 10000.f, -1.f, 1.f));
				}

				SRC_DATA srcData;
				srcData.data_in = (const float*) historyBufferR[c].startData();
				srcData.data_out = (float*) outBufferR[c].endData();
				srcData.input_frames = std::min((int) historyBufferR[c].size(), 16);
				srcData.output_frames = outBufferR[c].capacity();
				srcData.end_of_input = false;
				srcData.src_ratio = ratio;
				src_process(srcR[c], &srcData);
				historyBufferR[c].startIncr(srcData.input_frames_used);
				outBufferR[c].endIncr(srcData.output_frames_gen);
			}

			float wet = 0.f;
			if (!outBufferR[c].empty()) {
				wet = outBufferR[c].shift();
				float proc = (inputs[L_INPUT].getVoltage(c) * (1 - mix)) + (wet * mix);
				outputs[OUTR_OUTPUT].setVoltage(proc, c);
			}
			lastR[c] = wet;
		}
	
	}
};

struct PolydelayWidget : ModuleWidget {
	PolydelayWidget(Polydelay *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/polydelay.svg")));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(2.551, 1.955))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(28.282, 1.955))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(2.558, 126.278))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(28.282, 126.278))));

		addParam(createParamCentered<stocKnob>(mm2px(Vec(15.413, 40.655)), module, Polydelay::TIME_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(8.139, 50.762)), module, Polydelay::TIMEL_CV_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(22.606, 50.762)), module, Polydelay::TIMER_CV_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(15.416, 65.988)), module, Polydelay::SPREAD_PARAM));
		addParam(createParamCentered<stocKnob>(mm2px(Vec(8.135, 82.039)), module, Polydelay::MIX_PARAM));
		addParam(createParamCentered<stocKnob>(mm2px(Vec(22.606, 82.039)), module, Polydelay::FEED_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(8.139, 92.143)), module, Polydelay::MIX_CV_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(22.606, 92.143)), module, Polydelay::FEED_CV_PARAM));

		addInput(createInputCentered<aPJackArancione>(mm2px(Vec(8.139, 20.606)), module, Polydelay::L_INPUT));
		addInput(createInputCentered<aPJackArancione>(mm2px(Vec(22.606, 20.606)), module, Polydelay::R_INPUT));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(8.139, 60.862)), module, Polydelay::TIMEL_CV));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(22.606, 60.862)), module, Polydelay::TIMER_CV));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(8.139, 102.246)), module, Polydelay::MIX_CV));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(22.606, 102.246)), module, Polydelay::FEED_CV));

		addOutput(createOutputCentered<aPJackTurchese>(mm2px(Vec(8.139, 119.804)), module, Polydelay::OUTL_OUTPUT));
		addOutput(createOutputCentered<aPJackTurchese>(mm2px(Vec(22.606, 119.804)), module, Polydelay::OUTR_OUTPUT));


	}
};


Model *modelPolydelay = createModel<Polydelay, PolydelayWidget>("Polydelay");