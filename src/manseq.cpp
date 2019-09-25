#include "plugin.hpp"

#define PASSTHROUGH_RIGHT_VARIABLE_COUNT 34

struct Manseq : Module {
	enum ParamIds {
		REC_PARAM,
		MAN_PARAM,
		CLEAR_PARAM,
		STEPS_PARAM,
		ROTATE_PARAM,
		STEPS_CV_PARAM,
		ROTATE_CV_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		RESET_INPUT,
		A_INPUT,
		B_INPUT,
		REC_CV_INPUT,
		MAN_CV_INPUT,
		STEPS_CV,
		ROTATE_CV,
		NUM_INPUTS
	};
	enum OutputIds {
		TRIG_OUTPUT,
		CV_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		CLOCK_LIGHT,
		REC_LIGHT,
		MAN_LIGHT,
		CLEAR_LIGHT,
		NUM_LIGHTS
	};

	
	// Expander
	float consumerMessage[PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};// this module must read from here
	float producerMessage[PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};// mother will write into here



	Manseq() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(REC_PARAM, 0.f, 1.f, 0.f, "");
		configParam(MAN_PARAM, 0.f, 1.f, 0.f, "");
		configParam(CLEAR_PARAM, 0.f, 1.f, 0.f, "");
		configParam(ROTATE_PARAM, 0.f, 32.f, 16.f, "");
		configParam(STEPS_PARAM, 1.f, 32.f, 16.f, "");
		configParam(ROTATE_CV_PARAM, -1.f, 1.f, 0.f, "");
		configParam(STEPS_CV_PARAM, -1.f, 1.f, 0.f, "");

		leftExpander.producerMessage = producerMessage;
		leftExpander.consumerMessage = consumerMessage;
	}

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
        for (int c = 0; c < 16; c++)
				for (int i = 0; i < 32; i++) buffer[c][i] = 0;
	}
	
	dsp::SchmittTrigger clock_input;
	dsp::SchmittTrigger reset_input;
	dsp::SchmittTrigger rec_btn;
	dsp::SchmittTrigger clear_btn;
  	dsp::PulseGenerator ledPulse;
  	dsp::PulseGenerator outPulse[16];
	bool buffer[16][32] = {};
	int currentStep[16] = {};
	bool out[16] = {};
	bool rec = 0;
	float a[16] = {};
	float b[16] = {};
	int rotate = 0;
	int steps = 0;

	void process(const ProcessArgs &args) override {
		//Expander In
		bool motherPresent = leftExpander.module && (leftExpander.module->model == modelPolyturing || leftExpander.module->model == modelClock || leftExpander.module->model == modelBtnseq || leftExpander.module->model == modelManseq);
		bool messagePresent = false;
		int messageChannels = 0;
		float messageClock[16] = {};
		float messageCV[16] = {};

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

		outputs[CV_OUTPUT].setChannels(channels);
		outputs[CV_OUTPUT].setChannels(channels);

		if (inputs[REC_CV_INPUT].isConnected()) rec = (inputs[REC_CV_INPUT].getVoltage() > 1.0f) ? true : false;
		else if (rec_btn.process(params[REC_PARAM].getValue())) rec = !rec;
			
		if (clear_btn.process(params[CLEAR_PARAM].getValue())) 
			for (int c = 0; c < channels; c++)
				for (int i = 0; i < 32; i++) buffer[c][i] = 0;
			

		steps = params[STEPS_PARAM].getValue() + (inputs[STEPS_CV].getVoltage() * params[STEPS_CV_PARAM].getValue());
		rotate = rescale(params[ROTATE_PARAM].getValue(), -1.f, 1.0f, 0, steps); // + (inputs[ROTATE_CV].getVoltage() * params[ROTATE_CV_PARAM].getValue()
		bool reset = reset_input.process(rescale(inputs[RESET_INPUT].getVoltage(), 0.2f, 1.7f, 0.0f, 1.0f));
		if (reset)
			for(int c = 0; c < channels; c++) currentStep[c] = rotate % steps;


		if (inputs[CLOCK_INPUT].isConnected() || messagePresent){
			float clock = messagePresent ? messageClock[0] : inputs[CLOCK_INPUT].getVoltage();
			if (clock_input.process(rescale(clock, 0.2f, 1.7f, 0.0f, 1.0f))) {
				ledPulse.trigger(0.1);
				for(int c = 0; c < channels; c++){
					currentStep[c]++;
					currentStep[c] %= steps;					
					if (rec) buffer[c][currentStep[c]] = (params[MAN_PARAM].getValue()) ? true : false;
					if(buffer[c][currentStep[c]]) outPulse[c].trigger(0.001f);
				}
			}
		}

		lights[CLOCK_LIGHT].setBrightness(ledPulse.process(1.0 / 44100));
		lights[REC_LIGHT].setBrightness(rec);
		lights[MAN_LIGHT].setBrightness(params[MAN_PARAM].getValue());
		lights[CLEAR_LIGHT].setBrightness(params[CLEAR_PARAM].getValue());	

		for (int c = 0; c < channels; c++){
			out[c] = outPulse[c].process(1.0 / 44100);
			outputs[TRIG_OUTPUT].setVoltage((out[c] ? 10.f : 0.f), c);
			a[c] = (inputs[A_INPUT].isConnected()) ? inputs[A_INPUT].getVoltage(c % inputs[A_INPUT].getChannels()) : 10.f;
			b[c] = (inputs[B_INPUT].isConnected()) ? inputs[B_INPUT].getVoltage(c % inputs[B_INPUT].getChannels()) : 0.f;
			outputs[CV_OUTPUT].setVoltage((buffer[c][currentStep[c]] ? a[c] : b[c]), c);
		}

		//Expander Out
		bool rightExpanderPresent = (rightExpander.module && (rightExpander.module->model == modelPolyturing || rightExpander.module->model == modelClock || rightExpander.module->model == modelBtnseq || rightExpander.module->model == modelManseq));
		if(rightExpanderPresent) {
			float *messageToSlave = (float*) rightExpander.module->leftExpander.producerMessage;
			messageToSlave[0] = channels;
			for(int c = 0; c < channels; c++) messageToSlave[c + 1] = out[c] ? 10.f : 0.f;
			//for(int c = 0; c < channels; c++) messageToSlave[c + channels] = messagePresent ? out[c];
			rightExpander.module->leftExpander.messageFlipRequested = true;
		}
	}
};

struct ManseqWidget : ModuleWidget {
	ManseqWidget(Manseq *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/manseq.svg")));

		addParam(createParamCentered<LEDBezel>(mm2px(Vec(7.793, 46.217)), module, Manseq::REC_PARAM));
		addChild(createLightCentered<LargeLight <stocRedLight>>(mm2px(Vec(7.793 -0.31, 46.217 -0.31)), module, Manseq::REC_LIGHT));
		addParam(createParamCentered<LEDBezel>(mm2px(Vec(7.796, 59.118)), module, Manseq::MAN_PARAM));
		addChild(createLightCentered<LargeLight<stocRedLight>>(mm2px(Vec(7.796-0.31, 59.118-0.31)), module, Manseq::MAN_LIGHT));
		addParam(createParamCentered<LEDBezel>(mm2px(Vec(15.416, 68.914)), module, Manseq::CLEAR_PARAM));
		addChild(createLightCentered<LargeLight<stocRedLight>>(mm2px(Vec(15.416-0.31, 68.914-0.31)), module, Manseq::CLEAR_LIGHT));
		addParam(createParamCentered<stocSnapKnob>(mm2px(Vec(7.793, 82.259)), module, Manseq::STEPS_PARAM));
		addParam(createParamCentered<stocSnapKnob>(mm2px(Vec(23.314, 82.259)), module, Manseq::ROTATE_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(7.793, 92.186)), module, Manseq::STEPS_CV_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(23.314, 92.186)), module, Manseq::ROTATE_CV_PARAM));

		addInput(createInputCentered<aPJackArancione>(mm2px(Vec(7.793, 20.702)), module, Manseq::CLOCK_INPUT));
		addInput(createInputCentered<aPJackArancione>(mm2px(Vec(23.318, 20.702)), module, Manseq::RESET_INPUT));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(7.793, 33.366)), module, Manseq::A_INPUT));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(23.318, 33.366)), module, Manseq::B_INPUT));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(23.318, 46.214)), module, Manseq::REC_CV_INPUT));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(23.318, 59.118)), module, Manseq::MAN_CV_INPUT));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(7.793, 102.29)), module, Manseq::STEPS_CV));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(23.318, 102.29)), module, Manseq::ROTATE_CV));

		addOutput(createOutputCentered<aPJackTurchese>(mm2px(Vec(7.793, 119.653)), module, Manseq::TRIG_OUTPUT));
		addOutput(createOutputCentered<aPJackTurchese>(mm2px(Vec(23.318, 119.653)), module, Manseq::CV_OUTPUT));

		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(15.416, 20.653)), module, Manseq::CLOCK_LIGHT));

		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(2.335, 1.917))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(28.398, 1.917))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(2.335, 126.235))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(28.398, 126.235))));
	}
};


Model *modelManseq = createModel<Manseq, ManseqWidget>("Manseq");