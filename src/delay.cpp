#include "plugin.hpp"
#include "samplerate.h"
#define HISTORY_SIZE (1<<21)


struct Polydelay : Module {
	enum ParamIds {
		TIME_PARAM,
		TIME_CV_PARAM,
		SPREAD_PARAM,
		SPREAD_CV_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		IN_INPUT,
		TIME_CV,
		SPREAD_CV,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	dsp::DoubleRingBuffer<float, HISTORY_SIZE> historyBuffer[16];
	dsp::DoubleRingBuffer<float, 16> outBuffer[16];
	SRC_STATE *src[16];
	float lastWet[16];

	Polydelay() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(TIME_PARAM, 0.f, 1.f, 0.f, "");
		configParam(TIME_CV_PARAM, -1.f, 1.f, 0.f, "");
		configParam(SPREAD_PARAM, 0.f, 1.f, 0.f, "");
		configParam(SPREAD_CV_PARAM, -1.f, 1.f, 0.f, "");

		for (int c = 0; c < 16; c++){
			src[c] = src_new(SRC_SINC_FASTEST, 1, NULL);
			assert(src[c]);
		}
	}

	~Polydelay() {
		for (int c = 0; c < 16; c++) src_delete(src[c]);
	}



	void process(const ProcessArgs &args) override {
		int channels = inputs[IN_INPUT].getChannels();
		outputs[OUT_OUTPUT].setChannels(channels);	
		float delay  = params[TIME_PARAM].getValue()   + (params[TIME_CV_PARAM].getValue()   * inputs[TIME_CV].getVoltage()   / 5.f);
		float spread = params[SPREAD_PARAM].getValue() + (params[SPREAD_CV_PARAM].getValue() * inputs[SPREAD_CV].getVoltage() / 5.f);
			
		for (int c = 0; c < channels; c++){
			float in = inputs[IN_INPUT].getVoltage(c) + lastWet[c] * params[TIME_CV_PARAM].getValue();

			delay = clamp(delay, 0.f, 1.f);
			delay =  1e-3 * std::pow(10.f / 1e-3, delay);
			delay += rescale(spread, 0.f, 1.f, 0.f, delay) * c;
			// Number of delay samples
			float index = std::round(delay * args.sampleRate);

			// Push dry sample into history buffer
			if (!historyBuffer[c].full()) {
				historyBuffer[c].push(in);
			}

			// How many samples do we need consume to catch up?
			float consume = index - historyBuffer[c].size();

			if (outBuffer[c].empty()) {
				double ratio = 1.f;
				if (std::fabs(consume) >= 16.f) {
					// Here's where the delay magic is. Smooth the ratio depending on how divergent we are from the correct delay time.
					ratio = std::pow(10.f, clamp(consume / 10000.f, -1.f, 1.f));
				}

				SRC_DATA srcData;
				srcData.data_in = (const float*) historyBuffer[c].startData();
				srcData.data_out = (float*) outBuffer[c].endData();
				srcData.input_frames = std::min((int) historyBuffer[c].size(), 16);
				srcData.output_frames = outBuffer[c].capacity();
				srcData.end_of_input = false;
				srcData.src_ratio = ratio;
				src_process(src[c], &srcData);
				historyBuffer[c].startIncr(srcData.input_frames_used);
				outBuffer[c].endIncr(srcData.output_frames_gen);
			}

			float wet = 0.f;
			if (!outBuffer[c].empty()) {
				wet = outBuffer[c].shift();
				outputs[OUT_OUTPUT].setVoltage(wet, c);
			}
			lastWet[c] = wet;
		}
	}
};

struct PolydelayWidget : ModuleWidget {
	PolydelayWidget(Polydelay *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/delay.svg")));

		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(2.483, 2.088))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(17.837, 2.112))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(2.483, 126.412))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(17.837, 126.436))));

		addParam(createParamCentered<stocKnob>(mm2px(Vec(10.68, 40.407)), module, Polydelay::TIME_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(10.68, 50.809)), module, Polydelay::TIME_CV_PARAM));
		addParam(createParamCentered<stocKnob>(mm2px(Vec(10.68, 81.819)), module, Polydelay::SPREAD_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(10.68, 92.186)), module, Polydelay::SPREAD_CV_PARAM));

		addInput(createInputCentered<aPJackArancione>(mm2px(Vec(10.336, 20.691)), module, Polydelay::IN_INPUT));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(10.68, 59.118)), module, Polydelay::TIME_CV));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(10.68, 102.29)), module, Polydelay::SPREAD_CV));

		addOutput(createOutputCentered<aPJackTurchese>(mm2px(Vec(10.336, 119.744)), module, Polydelay::OUT_OUTPUT));

	}
};


Model *modelPolydelay = createModel<Polydelay, PolydelayWidget>("Polydelay");