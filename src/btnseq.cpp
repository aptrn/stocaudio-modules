#include "plugin.hpp"
#define PASSTHROUGH_RIGHT_VARIABLE_COUNT 33


struct Btnseq : Module {
	enum ParamIds {
		ENUMS(B_PARAM, 8),
		STEPS_PARAM,
		ROTATE_PARAM,
		STEPS_CV_PARAM,
		ROTATE_CV_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		RESET_INPUT,
		STEPS_CV_INPUT,
		ROTATE_CV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		TRIG_OUTPUT,
		GATE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		MAN_LIGHT,
		ENUMS(B_LIGHT, 3 * 8),
		NUM_LIGHTS
	};

	// Expander
	float consumerMessage[PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};// this module must read from here
	float producerMessage[PASSTHROUGH_RIGHT_VARIABLE_COUNT] = {};// mother will write into here

	Btnseq() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		for (int i = 0; i < 8; i++){
			configParam(B_PARAM +i, 0.f, 1.f, 0.f, "");
		}
		configParam(STEPS_PARAM, 0.f, 8.f, 4.f, "");
		configParam(ROTATE_PARAM, 0.f, 8.f, 0.f, "");
		configParam(STEPS_CV_PARAM, -1.f, 1.f, 0.f, "");
		configParam(ROTATE_CV_PARAM, -1.f, 1.f, 0.f, "");

		leftExpander.producerMessage = producerMessage;
		leftExpander.consumerMessage = consumerMessage;
	}
	json_t *dataToJson() override {
		json_t *sequenza = json_object();
		json_t *celle = json_array();
		for (int i = 0; i < 16; i++){
			for (int j = 0; j < 8; j++){
                json_t *cella = json_real(buf[i][j]);
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
                for (int j = 0; j < 8; j++){
                    json_t *cella = json_array_get(celle, 8 * i + j);
                    if (cella) buf[i][j] = json_real_value(cella);
                }   
            }
        }
    }
	

	dsp::SchmittTrigger btn[8];
	dsp::SchmittTrigger clock_input[16];
	dsp::SchmittTrigger reset_input;
	dsp::PulseGenerator ledPulse;
	dsp::PulseGenerator outPulse[16];

	bool buf[8][16]  = {0};
	int current[16] = {0};
	int count[16] = {0};
	int lastRotate = 0;

	
	void reset() {
		for(int i = 0; i < 8; i++){
			for(int c = 0; c < 16; c++) buf[i][c] = 0;
		}
	}
	void process(const ProcessArgs &args) override {
		//Expander In
		bool motherPresent = leftExpander.module && (leftExpander.module->model == modelPolyturing || leftExpander.module->model == modelClock || leftExpander.module->model == modelBtnseq || leftExpander.module->model == modelManseq);
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
				for(int i = 0; i < messageChannels; i++) messageCV[i] = messagesFromMother[i + messageChannels];
			}
		}

		int channels = messagePresent ? messageChannels : inputs[CLOCK_INPUT].getChannels();

		outputs[TRIG_OUTPUT].setChannels(channels);
		outputs[GATE_OUTPUT].setChannels(channels);

		for(int i = 0; i < 8; i++){
			if(btn[i].process(params[B_PARAM + i].getValue()) > 0.2)
				for(int c = 0; c < 16; c++) buf[i][c] = !buf[i][c];
		}

		int steps = clamp(int(params[STEPS_PARAM].getValue()) + int(params[STEPS_CV_PARAM].getValue() * (inputs[STEPS_CV_INPUT].getVoltage() / 10.f)), 1, 8);
		int rotate = rescale(clamp(int(params[ROTATE_PARAM].getValue()) + int(params[ROTATE_CV_PARAM].getValue() * (inputs[ROTATE_CV_INPUT].getVoltage() / 10.f)), 0, 7), 0, 7, 0, steps -1);
		if (lastRotate != rotate) {
			for (int c = 0; c < channels; c++){
				count[c] = 0;
				for (int i = 0; i > 8; i++)
					buf[i][c] = buf[(i - (rotate - lastRotate)) % 8][c];	
			}
			lastRotate = rotate;
		}

		if (reset_input.process(rescale(inputs[RESET_INPUT].getVoltage(), 0.2f, 1.7f, 0.0f, 1.0f)))
			for(int c = 0; c < channels; c++) current[c] = rotate;

		int firstStep = rotate;
		int lastStep = (rotate + steps) - 1 % steps;
		
		if (inputs[CLOCK_INPUT].isConnected() || messagePresent){
			for(int c = 0; c < channels; c++){
				float clock = messagePresent ? messageClock[c] : inputs[CLOCK_INPUT].getVoltage(c);
				if (clock_input[c].process(rescale(clock, 0.2f, 1.7f, 0.0f, 1.0f))) {
					ledPulse.trigger(0.1f);
					count[c]++;
					count[c] %= steps;

					current[c] = (rotate + count[c]) % 8;
					
					if (count[c] == 0) current[c] = rotate;
					if (buf[current[c]][c]) outPulse[c].trigger(0.001f);
				}
			}
		}

		for (int i = 0; i < 8; i++){
			lights[B_LIGHT + i * 3 + 0].setBrightness(0.f);
			lights[B_LIGHT + i * 3 + 1].setBrightness(0.f);
			lights[B_LIGHT + i * 3 + 2].setBrightness(0.f);
		} 
		
		for (int i = 0; i < steps; i++){
			int a = (rotate + i) % 8;
			lights[B_LIGHT + a * 3 + 0].setBrightness(0.1f);
		}
		//led[firstStep] = 2;
		for (int i = 0; i < 8; i++){
			if (buf[i][0] == true)lights[B_LIGHT + i * 3 + 0].setBrightness(0.6f);
			if (i == current[0])  lights[B_LIGHT + i * 3 + 1].setBrightness(1.0f);
		}

		float out[16];
		for(int c = 0; c < channels; c++){
			out[c] = outPulse[c].process(1.0 / 44100);
			outputs[TRIG_OUTPUT].setVoltage((out[c] ? 10.f : 0.f), c);
		//	outputs[TRIG_OUTPUT].setVoltage(outPulse[c].process(1.0 / 44100), c);
			outputs[GATE_OUTPUT].setVoltage(buf[current[c]][c] > 0 ? 10.0f : 0.0f, c);
		}
		lights[MAN_LIGHT].setBrightness(ledPulse.process(1.0 / 44100));

		//Expander Out
		bool rightExpanderPresent = (rightExpander.module && (rightExpander.module->model == modelPolyturing || rightExpander.module->model == modelClock || rightExpander.module->model == modelBtnseq || rightExpander.module->model == modelManseq));
		if(rightExpanderPresent) {
			float *messageToSlave = (float*) rightExpander.module->leftExpander.producerMessage;
			messageToSlave[0] = channels;
			for(int c = 0; c < channels; c++) messageToSlave[c + 1] = out[c] ? 10.f : 0.0f;
			//for(int c = 0; c < channels; c++) messageToSlave[c + channels] = messagePresent ? out[c];
			rightExpander.module->leftExpander.messageFlipRequested = true;
		}
	}
};

struct BtnseqWidget : ModuleWidget {
	BtnseqWidget(Btnseq *module) {
		setModule(module);
		setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/btnseq.svg")));

		addParam(createParamCentered<LEDBezel>(mm2px(Vec(23.301, 33.303)), module, Btnseq::B_PARAM + 4));
		addChild(createLightCentered<LargeLight<stocRedGreenBlueLight>>(mm2px(Vec(23.301-0.31, 33.303-0.31)), module, Btnseq::B_LIGHT + 4 * 3));
		addParam(createParamCentered<LEDBezel>(mm2px(Vec(7.93, 33.381)), module, Btnseq::B_PARAM + 0));
		addChild(createLightCentered<LargeLight<stocRedGreenBlueLight>>(mm2px(Vec(7.93-0.31, 33.381-0.31)), module, Btnseq::B_LIGHT + 0 * 3));
		addParam(createParamCentered<LEDBezel>(mm2px(Vec(23.297, 44.017)), module, Btnseq::B_PARAM + 5));
		addChild(createLightCentered<LargeLight<stocRedGreenBlueLight>>(mm2px(Vec(23.297-0.31, 44.017-0.31)), module, Btnseq::B_LIGHT + 5 * 3));
		addParam(createParamCentered<LEDBezel>(mm2px(Vec(7.93, 44.077)), module, Btnseq::B_PARAM + 1));
		addChild(createLightCentered<LargeLight<stocRedGreenBlueLight>>(mm2px(Vec(7.93-0.31, 44.077-0.31)), module, Btnseq::B_LIGHT + 1 * 3));
		addParam(createParamCentered<LEDBezel>(mm2px(Vec(23.297, 54.727)), module, Btnseq::B_PARAM + 6));
		addChild(createLightCentered<LargeLight<stocRedGreenBlueLight>>(mm2px(Vec(23.297-0.31, 54.727-0.31)), module, Btnseq::B_LIGHT + 6 * 3));
		addParam(createParamCentered<LEDBezel>(mm2px(Vec(7.934, 54.787)), module, Btnseq::B_PARAM + 2));
		addChild(createLightCentered<LargeLight<stocRedGreenBlueLight>>(mm2px(Vec(7.934-0.31, 54.787-0.31)), module, Btnseq::B_LIGHT + 2 * 3));
		addParam(createParamCentered<LEDBezel>(mm2px(Vec(23.301, 65.614)), module, Btnseq::B_PARAM + 7));
		addChild(createLightCentered<LargeLight<stocRedGreenBlueLight>>(mm2px(Vec(23.301-0.31, 65.614-0.31)), module, Btnseq::B_LIGHT + 7 * 3));
		addParam(createParamCentered<LEDBezel>(mm2px(Vec(7.93, 65.677)), module, Btnseq::B_PARAM + 3));
		addChild(createLightCentered<LargeLight<stocRedGreenBlueLight>>(mm2px(Vec(7.93-0.31, 65.677-0.31)), module, Btnseq::B_LIGHT + 3 * 3));

		addParam(createParamCentered<stocSnapKnob>(mm2px(Vec(7.793, 82.259)), module, Btnseq::STEPS_PARAM));
		addParam(createParamCentered<stocSnapKnob>(mm2px(Vec(23.314, 82.259)), module, Btnseq::ROTATE_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(7.793, 92.186)), module, Btnseq::STEPS_CV_PARAM));
		addParam(createParamCentered<stocAttn>(mm2px(Vec(23.314, 92.186)), module, Btnseq::ROTATE_CV_PARAM));

		addInput(createInputCentered<aPJackArancione>(mm2px(Vec(7.793, 20.702)), module, Btnseq::CLOCK_INPUT));
		addInput(createInputCentered<aPJackArancione>(mm2px(Vec(23.318, 20.702)), module, Btnseq::RESET_INPUT));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(7.793, 102.29)), module, Btnseq::STEPS_CV_INPUT));
		addInput(createInputCentered<aPJackAzzurro>(mm2px(Vec(23.318, 102.29)), module, Btnseq::ROTATE_CV_INPUT));

		addOutput(createOutputCentered<aPJackTurchese>(mm2px(Vec(7.793, 119.653)), module, Btnseq::TRIG_OUTPUT));
		addOutput(createOutputCentered<aPJackTurchese>(mm2px(Vec(23.318, 119.653)), module, Btnseq::GATE_OUTPUT));

		addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(15.416, 20.653)), module, Btnseq::MAN_LIGHT));

		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(2.335, 1.917))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(28.398, 1.917))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(2.335, 126.235))));
		addChild(createWidgetCentered<stocScrew>(mm2px(Vec(28.398, 126.235))));
		
	}
};


Model *modelBtnseq = createModel<Btnseq, BtnseqWidget>("Btnseq");