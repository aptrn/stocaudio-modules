#include "plugin.hpp"

#define PASSTHROUGH_RIGHT_VARIABLE_COUNT 18


struct Polyturing : Module {
	enum ParamIds {
		LOCK_PARAM,
		LOCK_CV_PARAM,
		SCALE_PARAM,
		OFFSET_PARAM,
		STEP_PARAM,
		SCALE_CV_PARAM,
		OFFSET_CV_PARAM,
		STEP_CV_PARAM,
		SCALE_RAND_PARAM,
		OFFSET_RAND_PARAM,
		STEP_RAND_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		MAIN_INPUT,
		CLOCK_INPUT,
		LOCK_CV,
		SCALE_CV,
		OFFSET_CV,
		STEP_CV,
		NUM_INPUTS
	};
	enum OutputIds {
		MAIN_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		MANUAL_LED,
		NUM_LIGHTS
	};
	// Expander
	float consumerMessage[PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};// this module must read from here
	float producerMessage[PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};// mother will write into here

	Polyturing() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(LOCK_PARAM, 0.f, 1.f, 0.5f, "");
		configParam(LOCK_CV_PARAM, -1.f, 1.f, 0.f, "");
		configParam(SCALE_PARAM, 0.f, 1.f, 0.5f, "");
		configParam(OFFSET_PARAM, -5.f, 5.f, 0.f, "");
		configParam(STEP_PARAM, 1.f, 32.f, 16.f, "");
		configParam(SCALE_CV_PARAM, -1.f, 1.f, 0.f, "");
		configParam(OFFSET_CV_PARAM, -1.f, 1.f, 0.f, "");
		configParam(STEP_CV_PARAM, -1.f, 1.f, 0.f, "");
		configParam(SCALE_RAND_PARAM, -1.f, 1.f, 0.f, "");
		configParam(OFFSET_RAND_PARAM, -1.f, 1.f, 0.f, "");
		configParam(STEP_RAND_PARAM, -1.f, 1.f, 0.f, "");

		leftExpander.producerMessage = producerMessage;
		leftExpander.consumerMessage = consumerMessage;
	}

	dsp::SchmittTrigger clock_input[16];
	dsp::PulseGenerator led_pulse;
	int currentStep[32];
    float in, now;
    float out[16];
    float buffer[16][32];


	json_t *dataToJson() override {
		json_t *sequenza = json_object();
		json_t *celle = json_array();
		for (int i = 0; i < 16; i++){
			for (int j = 0; j < 32; j++){
                json_t *cella = json_real(buffer[i][j]);
				json_array_append_new(celle, cella);
			}
		}
		json_object_set_new(sequenza, "celles", celle);
		return sequenza;
	}

    void dataFromJson(json_t *sequenza) override {
        json_t *celle = json_object_get(sequenza, "celles");
        if (celle){
            for (int i = 0; i < 16; i++){
                for (int j = 0; j < 32; j++){
                    json_t *cella = json_array_get(celle, 32 * i + j);
                    if (cella) buffer[i][j] = json_real_value(cella);
                }   
            }
        }
    }

    void reset() {
        for (int c = 0; c < 16; c++){
            for (int i = 0; i < 32; i++){
                buffer[c][i] = 0;
            }
        }
	}

	void process(const ProcessArgs &args) override{
		bool motherPresent = leftExpander.module && (leftExpander.module->model == modelPolyturing || leftExpander.module->model == modelClock || leftExpander.module->model == modelBtnseq || leftExpander.module->model == modelManseq);
		bool clockConnected = 0;
		int clockChannels = 0;
		float clockInput[16] = {0};

		if(motherPresent && !inputs[CLOCK_INPUT].isConnected())  {
			float *messagesFromMother = (float*)leftExpander.consumerMessage;
			clockConnected = messagesFromMother[0];
			if (clockConnected){
				clockChannels = messagesFromMother[0];
				for(int i = 0; i < clockChannels; i++) clockInput[i] = messagesFromMother[i + 2];
			}
		}
	
		int channels =  clockConnected ? clockChannels : inputs[CLOCK_INPUT].getChannels();
		outputs[MAIN_OUTPUT].setChannels(channels);
		if (clockConnected || inputs[CLOCK_INPUT].isConnected()){
			for (int c = 0; c < channels; c++){
				float clock = clockConnected ? clockInput[c] : inputs[CLOCK_INPUT].getVoltage(c % inputs[CLOCK_INPUT].getChannels()); 
				if (clock_input[c].process(rescale(clock, 0.2f, 1.7f, 0.0f, 1.0f))){
				
						led_pulse.trigger(0.1f);
				
					currentStep[c]++;
					int steps = params[STEP_PARAM].getValue() + (inputs[STEP_CV].getVoltage() * params[STEP_CV_PARAM].getValue()) + (now * params[STEP_RAND_PARAM].getValue());
					if (currentStep[c] > steps) currentStep[c] = 0; 
						if (inputs[MAIN_INPUT].isConnected()) in = (c < inputs[MAIN_INPUT].getChannels()) ? inputs[MAIN_INPUT].getVoltage(c) : inputs[MAIN_INPUT].getVoltage(c % inputs[MAIN_INPUT].getChannels()); 
						else in = 2.0 * random::normal();
						if (random::uniform() > (params[LOCK_PARAM].getValue() + (inputs[LOCK_CV].getVoltage() * params[LOCK_CV_PARAM].getValue()))){
							out[c] = in;
							buffer[c][currentStep[c]] = in;
							for (int i = currentStep[c] + 1; i < 32; i++) buffer[c][i] = 2 * random::uniform();
						}
						else{
							out[c] = buffer[c][currentStep[c]];
					}    
				}
			}
		}
		
		for (int c = 0; c < channels; c++) {
			now = out[c];
			float scale = params[SCALE_PARAM].getValue() + (inputs[SCALE_CV].getVoltage() * params[SCALE_CV_PARAM].getValue()) + (now *  params[SCALE_RAND_PARAM].getValue());
			float offset = params[OFFSET_PARAM].getValue() + (inputs[OFFSET_CV].getVoltage() * params[OFFSET_CV_PARAM].getValue()) + (now *  params[OFFSET_RAND_PARAM].getValue());
			outputs[MAIN_OUTPUT].setVoltage((out[c] * scale) + offset, c); 
		}
		lights[MANUAL_LED].setBrightness(led_pulse.process(1.0 / 44100));

		bool rightExpanderPresent = (rightExpander.module && (rightExpander.module->model == modelPolyturing || rightExpander.module->model == modelClock || rightExpander.module->model == modelBtnseq || rightExpander.module->model == modelManseq));
		if(rightExpanderPresent) {
			float *messageToSlave = (float*) rightExpander.module->leftExpander.producerMessage;
			messageToSlave[0] = clockConnected ? clockChannels : inputs[CLOCK_INPUT].getChannels();
			messageToSlave[1] = out[0];
			for(int i = 0; i < channels; i++) messageToSlave[i + 2] = clockConnected ? clockInput[i] : inputs[CLOCK_INPUT].getVoltage(i);
			rightExpander.module->leftExpander.messageFlipRequested = true;
		}
		
	}
};

struct PolyturingWidget : ModuleWidget {
	PolyturingWidget(Polyturing *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/polyturing.svg")));
/*
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
*/	
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(38.728, 1.941))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(2.265, 1.945))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(2.265, 126.267))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(38.728, 126.267))));

		addParam(createParamCentered<stocBigKnob>(mm2px(Vec(20.514, 42.853)), module, Polyturing::LOCK_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(15.346, 63.384)), module, Polyturing::LOCK_CV_PARAM));
		addParam(createParamCentered<stocKnob>(mm2px(Vec(6.382, 82.046)), module, Polyturing::SCALE_PARAM));
		addParam(createParamCentered<stocKnob>(mm2px(Vec(20.493, 82.046)), module, Polyturing::OFFSET_PARAM));
		addParam(createParamCentered<stocSnapKnob>(mm2px(Vec(34.167, 82.046)), module, Polyturing::STEP_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(3.683, 90.707)), module, Polyturing::SCALE_CV_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(17.794, 90.707)), module, Polyturing::OFFSET_CV_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(31.468, 90.707)), module, Polyturing::STEP_CV_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(9.088, 94.866)), module, Polyturing::SCALE_RAND_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(23.199, 94.866)), module, Polyturing::OFFSET_RAND_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(36.865, 94.866)), module, Polyturing::STEP_RAND_PARAM));

		addInput(createInputCentered<aPJackArancione>(mm2px(Vec(7.528, 18.282)), module, Polyturing::MAIN_INPUT));
		addInput(createInputCentered<aPJackArancione>(mm2px(Vec(33.464, 18.282)), module, Polyturing::CLOCK_INPUT));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(6.382, 63.384)), module, Polyturing::LOCK_CV));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(6.308, 102.077)), module, Polyturing::SCALE_CV));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(20.419, 102.077)), module, Polyturing::OFFSET_CV));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(34.092, 102.077)), module, Polyturing::STEP_CV));

		addOutput(createOutputCentered<aPJackTurchese>(mm2px(Vec(20.496, 119.635)), module, Polyturing::MAIN_OUTPUT));

		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(20.429, 22.871)), module, Polyturing::MANUAL_LED));

	}
};


Model *modelPolyturing = createModel<Polyturing, PolyturingWidget>("Polyturing");