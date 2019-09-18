#include "plugin.hpp"


struct Spread : Module {
	enum ParamIds {
		STEREO_PARAM,
		STEREO_CV_PARAM,
		VOLUME_PARAM,
		VOLUME_CV_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		L_INPUT,
		R_INPUT,
		STEREO_CV,
		VOLUME_CV,
		NUM_INPUTS
	};
	enum OutputIds {
		L_OUTPUT,
		R_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	Spread() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(STEREO_PARAM, -1.f, 1.f, 0.f, "");
		configParam(STEREO_CV_PARAM, -1.f, 1.f, 0.f, "");
		configParam(VOLUME_PARAM, 0.f, 1.f, 1.f, "");
		configParam(VOLUME_CV_PARAM, -1.f, 1.f, 0.f, "");
	}

	float outl[16];
	float outr[16];

	void process(const ProcessArgs &args) override {
		int channels = inputs[L_INPUT].getChannels();
		float spread = params[STEREO_PARAM].getValue() + (clamp(inputs[STEREO_CV].getVoltage() * params[STEREO_CV_PARAM].getValue(), -10.f, 10.f) / 5.f);
		if (channels > 1){
			for (int c = 0; c < channels; c++){
				float thisPan = rescale(c + 1, 1, channels, -spread, spread);
				outl[c] = inputs[L_INPUT].getVoltage(c) * rescale(thisPan, -1.f, 1.f, 0.f, 1.f);
				if (inputs[R_INPUT].isConnected()){
					//ruotare i canali in stereo?
					outr[c] = inputs[R_INPUT].getVoltage(c) * rescale(thisPan, -1.f, 1.f, 1.f, 0.f);
				} 
				else outr[c] = inputs[L_INPUT].getVoltage(c) * rescale(thisPan, -1.f, 1.f, 1.f, 0.f);

				outputs[L_OUTPUT].setVoltage(outl[c], c); 
				outputs[R_OUTPUT].setVoltage(outr[c], c); 
			}
			float outputL = 0;
			float outputR = 0;
			for (int c = 0; c < channels; c++) {
				outputL += outl[c] * ((params[VOLUME_PARAM].getValue() + clamp(inputs[VOLUME_CV].getVoltage() * params[VOLUME_CV_PARAM].getValue(), -10.f, 10.f) / 10.f));
				outputR += outr[c] * ((params[VOLUME_PARAM].getValue() + clamp(inputs[VOLUME_CV].getVoltage() * params[VOLUME_CV_PARAM].getValue(), -10.f, 10.f) / 10.f));
			}
			outputs[L_OUTPUT].setVoltage(outputL);
			outputs[R_OUTPUT].setVoltage(outputR); 
		}
		else{
			float thisPan = rescale(clamp(spread, -1.f, 1.f), -1.f, 1.f, 0.f, 1.f);
			float outputL = inputs[L_INPUT].getVoltage() * ((params[VOLUME_PARAM].getValue() + clamp(inputs[VOLUME_CV].getVoltage() * params[VOLUME_CV_PARAM].getValue(), -10.f, 10.f) / 10.f));
			float outputR = inputs[R_INPUT].getVoltage() * ((params[VOLUME_PARAM].getValue() + clamp(inputs[VOLUME_CV].getVoltage() * params[VOLUME_CV_PARAM].getValue(), -10.f, 10.f) / 10.f));
			
			if (inputs[R_INPUT].isConnected()){
				outputs[R_OUTPUT].setVoltage((outputL * thisPan) + (outputR * (1 - thisPan)));
				outputs[L_OUTPUT].setVoltage((outputR * thisPan) + (outputL* (1 - thisPan)));
			}
			else {
				outputs[R_OUTPUT].setVoltage(outputL * thisPan);
				outputs[L_OUTPUT].setVoltage(outputL * (1 - thisPan));
			}
		}
	}
};

struct SpreadWidget : ModuleWidget {
	SpreadWidget(Spread *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/spread.svg")));

		addParam(createParamCentered<stocKnob>(mm2px(Vec(10.196, 40.554)), module, Spread::STEREO_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(10.196, 50.809)), module, Spread::STEREO_CV_PARAM));
		addParam(createParamCentered<stocKnob>(mm2px(Vec(10.196, 81.967)), module, Spread::VOLUME_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(10.196, 92.186)), module, Spread::VOLUME_CV_PARAM));

		addInput(createInputCentered<aPJackArancione>(mm2px(Vec(5.249, 20.691)), module, Spread::L_INPUT));
		addInput(createInputCentered<aPJackArancione>(mm2px(Vec(15.071, 20.715)), module, Spread::R_INPUT));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(10.196, 59.118)), module, Spread::STEREO_CV));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(10.196, 102.29)), module, Spread::VOLUME_CV));

		addOutput(createOutputCentered<aPJackTurchese>(mm2px(Vec(5.249, 119.744)), module, Spread::L_OUTPUT));
		addOutput(createOutputCentered<aPJackTurchese>(mm2px(Vec(15.071, 119.768)), module, Spread::R_OUTPUT));

		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(2.483, 2.088))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(17.837, 2.112))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(2.483, 126.412))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(17.837, 126.436))));
	}
};


Model *modelSpread = createModel<Spread, SpreadWidget>("Spread");