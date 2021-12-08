#include "plugin.hpp"

#define PASSTHROUGH_RIGHT_VARIABLE_COUNT 34


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
		CLOCK_LED,
		NUM_LIGHTS
	};
	// Expander
	float consumerMessage[PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};// this module must read from here
	float producerMessage[PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};// mother will write into here

	Polyturing() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(LOCK_PARAM, 0.f, 1.f, 0.5f, "Lock sequence");
		configParam(LOCK_CV_PARAM, -1.f, 1.f, 0.f, "Lock CV amount");
		configParam(SCALE_PARAM, 0.f, 1.f, 0.5f, "Amplitude scale");
		configParam(OFFSET_PARAM, -5.f, 5.f, 0.f, "Voltage offset");
		configParam(STEP_PARAM, 1, 32, 16, "Sequence steps");
		configParam(SCALE_CV_PARAM, -1.f, 1.f, 0.f, "Scale CV amount");
		configParam(OFFSET_CV_PARAM, -1.f, 1.f, 0.f, "Offset CV amount");
		configParam(STEP_CV_PARAM, -1.f, 1.f, 0.f, "Steps CV amount");
		configParam(SCALE_RAND_PARAM, -1.f, 1.f, 0.f, "Scale sequence amount");
		configParam(OFFSET_RAND_PARAM, -1.f, 1.f, 0.f, "Offset sequence amount");
		configParam(STEP_RAND_PARAM, -1.f, 1.f, 0.f, "Steps sequence amount");
		configInput(MAIN_INPUT, "Sampled input");
		configInput(CLOCK_INPUT, "Clock input");
		configInput(LOCK_CV, "Lock CV");
		configInput(SCALE_CV, "Scale CV");
		configInput(OFFSET_CV, "Offset CV");
		configInput(STEP_CV, "Steps CV");
		configOutput(MAIN_OUTPUT, "CV output");
		configLight(CLOCK_LED, "Clock LED");

		leftExpander.producerMessage = producerMessage;
		leftExpander.consumerMessage = consumerMessage;
	}

	dsp::SchmittTrigger clock_input[16];
	dsp::PulseGenerator led_pulse;
	int currentStep[32];
	int oldChannels = 1;
    float in, now;
    float out[16];
    float buffer[16][32];
	float lock;
	float after[16];


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

		//Expander In
		bool motherPresent = leftExpander.module && (leftExpander.module->model == modelPolyturing);
		bool messagePresent = false;
		int messageChannels = 0;
		float messageClock[16] = {};
		float messageCV[16] = {};
		float messageLock = 0;

		if(motherPresent && !inputs[CLOCK_INPUT].isConnected())  {
			float *messagesFromMother = (float*)leftExpander.consumerMessage;
			messagePresent = messagesFromMother[0] > 0 ? true : false;
			if (messagePresent){
				messageChannels = messagesFromMother[0];
				for(int i = 0; i < messageChannels; i++) messageClock[i] = messagesFromMother[i + 1];
				for(int i = 0; i < messageChannels; i++) messageCV[i] = messagesFromMother[i + 1 + messageChannels];
				messageLock = messagesFromMother[33];
			}
		}
	
		int channels =  messagePresent ? messageChannels : std::max(inputs[CLOCK_INPUT].getChannels(), inputs[MAIN_INPUT].getChannels());

		if(channels > oldChannels){ //seed sequence for newly added channels
			for(int c = oldChannels; c < channels; c++){
				for(int s = 0; s < 32; s++){
					if (inputs[MAIN_INPUT].isConnected()) buffer[c][s] = buffer[0][s];
					else {
						buffer[c][s] = random::uniform() * 2;
					}
				}
			}
		}
		
		outputs[MAIN_OUTPUT].setChannels(channels);
		if (messagePresent || inputs[CLOCK_INPUT].isConnected()){
			for (int c = 0; c < channels; c++){
				float clock = !inputs[CLOCK_INPUT].isConnected() ? messageClock[c] : inputs[CLOCK_INPUT].getVoltage(c); 
				if (clock_input[c].process(rescale(clock, 0.2f, 1.7f, 0.0f, 1.0f))){
					led_pulse.trigger(0.1f);
					currentStep[c]++;
					int steps = (params[STEP_PARAM].getValue() -1) + (inputs[STEP_CV].getVoltage() * params[STEP_CV_PARAM].getValue()) + (now * params[STEP_RAND_PARAM].getValue());
					if (currentStep[c] > steps) currentStep[c] = 0; 
						if (inputs[MAIN_INPUT].isConnected()) in = inputs[MAIN_INPUT].getVoltage(c % inputs[MAIN_INPUT].getChannels()); 
						else in = 2.0 * random::normal();
						lock = messagePresent ? messageLock : (params[LOCK_PARAM].getValue() + (inputs[LOCK_CV].getVoltage() * params[LOCK_CV_PARAM].getValue()));
						if (random::uniform() > lock){
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
			after[c] = (out[c] * scale) + offset;
			outputs[MAIN_OUTPUT].setVoltage(after[c], c); 
		}
		lights[CLOCK_LED].setBrightness(led_pulse.process(1.0 / 44100));

		//Expander Out
		bool rightExpanderPresent = (rightExpander.module && (rightExpander.module->model == modelPolyturing));
		if(rightExpanderPresent) {
			float *messageToSlave = (float*) rightExpander.module->leftExpander.producerMessage;
			messageToSlave[0] = channels;
			for(int c = 0; c < channels; c++) messageToSlave[c + 1] = messagePresent ? messageClock[c] : inputs[CLOCK_INPUT].getVoltage(c);
			for(int c = 0; c < channels; c++) messageToSlave[c + 1 + channels] = after[c];
			messageToSlave[33] = lock;
			rightExpander.module->leftExpander.messageFlipRequested = true;
		}
		oldChannels = channels;
	}
};

struct PolyturingWidget : ModuleWidget {
	PolyturingWidget(Polyturing *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/polyturing.svg")));
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

		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(20.429, 22.871)), module, Polyturing::CLOCK_LED));

	}
};


Model *modelPolyturing = createModel<Polyturing, PolyturingWidget>("Polyturing");