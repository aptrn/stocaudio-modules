#include "plugin.hpp"


struct Slew : Module {
	enum ParamIds {
		UP_PARAM,
		UP_CV_PARAM,
		DOWN_PARAM,
		DOWN_CV_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		IN_INPUT,
		UP_CV,
		DOWN_CV,
		NUM_INPUTS
	};
	enum OutputIds {
		OUT_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	Slew() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(UP_PARAM, -1.f, 1.f, 0.f, "");
		configParam(UP_CV_PARAM, -1.f, 1.f, 0.f, "");
		configParam(DOWN_PARAM, -1.f, 1.f, 0.f, "");
		configParam(DOWN_CV_PARAM, -1.f, 1.f, 0.f, "");
	}

	float out[16] = {0.0};

	void process(const ProcessArgs &args) override {
		int channels = inputs[IN_INPUT].getChannels();
		outputs[OUT_OUTPUT].setChannels(channels);

		for (int c = 0; c < channels; c++){
			float in = inputs[IN_INPUT].getVoltage(c);
			float shape = rescale(params[DOWN_CV_PARAM].getValue(), -1.f, 1.f, 0.f, 1.f);
			//float amount = rescale(params[DOWN_CV_PARAM].getValue(), -1.f, 1.f, -10/10.f, 10/10.f);
			// minimum and maximum slopes in volts per second
			const float slewMin = 0.01;
			const float slewMax = 10000.f;
			// Amount of extra slew per voltage difference
			const float shapeScale = 1/10.f;

			// Rise
			if (in > out[c]) {
				float rise = rescale(params[DOWN_PARAM].getValue(), -1.f, 1.f, 0.f, 1.f);
				float slew = slewMax * std::pow(slewMin / slewMax, rise);
				out[c] += slew * crossfade(1.f, shapeScale * (in - out[c]), shape) * args.sampleTime;
				if (out[c] > in)
					out[c] = in;
			}
			// Fall
			else if (in < out[c]) {
				float fall = rescale(params[UP_PARAM].getValue(), 1.f, -1.f, 0.f, 1.f);
				float slew = slewMax * std::pow(slewMin / slewMax, fall);
				out[c] -= slew * crossfade(1.f, shapeScale * (out[c] - in), shape) * args.sampleTime;
				if (out[c] < in)
					out[c] = in;
			}

			outputs[OUT_OUTPUT].setVoltage(out[c], c);
		}
	}
};

struct SlewWidget : ModuleWidget {
	SlewWidget(Slew *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/slew.svg")));


		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(2.483, 2.088))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(17.837, 2.112))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(2.483, 126.412))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(17.837, 126.436))));

		addParam(createParamCentered<stocKnob>(mm2px(Vec(10.68, 40.407)), module, Slew::UP_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(10.68, 50.809)), module, Slew::UP_CV_PARAM));
		addParam(createParamCentered<stocKnob>(mm2px(Vec(10.68, 81.819)), module, Slew::DOWN_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(10.68, 92.186)), module, Slew::DOWN_CV_PARAM));

		addInput(createInputCentered<aPJackArancione>(mm2px(Vec(10.336, 20.691)), module, Slew::IN_INPUT));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(10.68, 59.118)), module, Slew::UP_CV));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(10.68, 102.29)), module, Slew::DOWN_CV));

		addOutput(createOutputCentered<aPJackTurchese>(mm2px(Vec(10.336, 119.744)), module, Slew::OUT_OUTPUT));


	
	}
};


Model *modelSlew = createModel<Slew, SlewWidget>("Slew");