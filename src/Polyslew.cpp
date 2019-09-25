#include "plugin.hpp"


#define PASSTHROUGH_RIGHT_VARIABLE_COUNT 34

struct Polyslew : Module {
	enum ParamIds {
		SHAPE_PARAM,
		SHAPE_CV_PARAM,
		UP_PARAM,
		DOWN_PARAM,
		UP_CV_PARAM,
		DOWN_CV_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		IN_INPUT,
		SHAPE_CV,
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

	// Expander
	float consumerMessage[PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};// this module must read from here
	float producerMessage[PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};// mother will write into here


	Polyslew() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(SHAPE_PARAM, -1.f, 1.f, 0.f, "");
		configParam(SHAPE_CV_PARAM, -1.f, 1.f, 0.f, "");
		configParam(UP_PARAM, 0.f, 1.f, 0.5f, "");
		configParam(DOWN_PARAM, 0.f, 1.f, 0.5f, "");
		configParam(UP_CV_PARAM, -1.f, 1.f, 0.f, "");
		configParam(DOWN_CV_PARAM, -1.f, 1.f, 0.f, "");
		
		leftExpander.producerMessage = producerMessage;
		leftExpander.consumerMessage = consumerMessage;
	}

	float out[16] = {0.0};

	void process(const ProcessArgs &args) override {

		//Expander In
		bool motherPresent = leftExpander.module && (leftExpander.module->model == modelPolyturing || leftExpander.module->model == modelPolyslew);
		bool messagePresent = false;
		int messageChannels = 0;
		float messageClock[16] = {};
		float messageCV[16] = {};

		if(motherPresent && !inputs[IN_INPUT].isConnected())  {
			float *messagesFromMother = (float*)leftExpander.consumerMessage;
			messagePresent = messagesFromMother[0] > 0 ? true : false;
			if (messagePresent){
				messageChannels = messagesFromMother[0];
				for(int i = 0; i < messageChannels; i++) messageClock[i] = messagesFromMother[i + 1];
				for(int i = 0; i < messageChannels; i++) messageCV[i] = messagesFromMother[i + 1 + messageChannels];
			}
		}

		int channels = messagePresent ? messageChannels : inputs[IN_INPUT].getChannels();
		outputs[OUT_OUTPUT].setChannels(channels);

		for (int c = 0; c < channels; c++){
			float in = messagePresent ? messageCV[c] : inputs[IN_INPUT].getVoltage(c);
			float shape = clamp(params[SHAPE_PARAM].getValue() + (inputs[SHAPE_CV].getVoltage() * params[SHAPE_CV_PARAM].getValue()), 0.0f, 1.0f);
			// minimum and maximum slopes in volts per second
			const float slewMin = 0.1f;
			const float slewMax = 10000.f;
			// Amount of extra slew per voltage difference
			const float shapeScale = 1/10.f;

			// Rise
			if (in > out[c]) {
				float rise = clamp(params[UP_PARAM].getValue() + (inputs[UP_CV].getVoltage() * params[UP_CV_PARAM].getValue()), 0.f, 1.f);
				float slew = slewMax * std::pow(slewMin / slewMax, rise);
				out[c] += slew * crossfade(1.f, shapeScale * (in - out[c]), shape) * args.sampleTime;
				if (out[c] > in)
					out[c] = in;
			}
			// Fall
			else if (in < out[c]) {
				float fall = clamp(params[DOWN_PARAM].getValue() + (inputs[DOWN_CV].getVoltage() * params[DOWN_CV_PARAM].getValue()), 0.f, 1.f);
				float slew = slewMax * std::pow(slewMin / slewMax, fall);
				out[c] -= slew * crossfade(1.f, shapeScale * (out[c] - in), shape) * args.sampleTime;
				if (out[c] < in)
					out[c] = in;
			}

			outputs[OUT_OUTPUT].setVoltage(out[c], c);
		}

		//Expander Out
		bool rightExpanderPresent = (rightExpander.module && (rightExpander.module->model == modelPolyturing || rightExpander.module->model == modelPolyslew));
		if(rightExpanderPresent) {
			float *messageToSlave = (float*) rightExpander.module->leftExpander.producerMessage;
			messageToSlave[0] = channels;
			for(int c = 0; c < channels; c++) messageToSlave[c + 1] = messageClock[c];
			for(int c = 0; c < channels; c++) messageToSlave[c + 1 + channels] = out[c];
			rightExpander.module->leftExpander.messageFlipRequested = true;
		}
	}
};

struct PolyslewWidget : ModuleWidget {
	PolyslewWidget(Polyslew *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/slew.svg")));

		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(2.551, 1.955))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(28.282, 1.955))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(2.558, 126.278))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(28.282, 126.278))));

		addParam(createParamCentered<stocKnob>(mm2px(Vec(15.413, 40.655)), module, Polyslew::SHAPE_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(15.395, 50.865)), module, Polyslew::SHAPE_CV_PARAM));
		addParam(createParamCentered<stocKnob>(mm2px(Vec(8.135, 82.039)), module, Polyslew::UP_PARAM));
		addParam(createParamCentered<stocKnob>(mm2px(Vec(22.606, 82.039)), module, Polyslew::DOWN_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(8.139, 92.143)), module, Polyslew::UP_CV_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(22.606, 92.143)), module, Polyslew::DOWN_CV_PARAM));

		addInput(createInputCentered<aPJackArancione>(mm2px(Vec(15.24, 20.606)), module, Polyslew::IN_INPUT));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(15.395, 61.017)), module, Polyslew::SHAPE_CV));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(8.139, 102.246)), module, Polyslew::UP_CV));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(22.606, 102.246)), module, Polyslew::DOWN_CV));

		addOutput(createOutputCentered<aPJackTurchese>(mm2px(Vec(15.24, 119.804)), module, Polyslew::OUT_OUTPUT));


	}
};


Model *modelPolyslew = createModel<Polyslew, PolyslewWidget>("Polyslew");