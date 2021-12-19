#include "plugin.hpp"

#define _USE_MATH_DEFINES
#include <math.h>
#include <map>

enum CV_Mode {
	ENVELOPE_VALUE,
	RANDOM_BP,
	RANDOM_POSITIVE,
	RANDOM_NEG,
	COUNTER,
	BURST_RANDOM,
	SHAPE_RANDOM,
	CURVE_RANDOM		
};
enum EOC_Mode {
	ENVELOPE_EOC,
	TRIGGER_AUTO
};
enum Ramp_Mode {
	ENVELOPE_PHASE,
	BURST_PHASE,
	CLOCK_DIV_PHASE
};

struct PolyBurst : Module {
	enum ParamId {
		CLOCK_PARAM,
		LOOP_PARAM,
		TRIG_PARAM,
		AUTO_PARAM,
		TIME_PARAM,
		TIME_CV_PARAM,
		MIN_PARAM,
		DENSITY_PARAM,
		MAX_PARAM,
		SYNC_PARAM,
		JITTER_PARAM,
		DENSITY_CV_PARAM,
		DEPTH_PARAM,
		RAND_DEPTH_PARAM,
		CURVE_PARAM,
		SHAPE_PARAM,
		RAND_PARAM,
		CURVE_CV_PARAM,
		RAND_CURVE_PARAM,
		SHAPE_CV_PARAM,
		RAND_SHAPE_PARAM,
		RAND_CV_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		CLOCK_INPUT,
		TRIG_INPUT,
		TIME_CV_INPUT,
		DENSITY_CV_INPUT,
		DEPTH_CV_INPUT,
		RAND_DEPTH_CV_INPUT,
		CURVE_CV_INPUT,
		SHAPE_CV_INPUT,
		RAND_CV_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		RAMP_OUTPUT,
		EOC_OUTPUT,
		MAIN_OUTPUT,
		CV_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		LOOP_LIGHT,
		TRIG_LIGHT,
		AUTO_LIGHT,
		ENV_LIGHT,
		SYNC_LIGHT,
		LIGHTS_LEN
	};

	PolyBurst() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		
		configParam(CLOCK_PARAM, 0.f, 1.f, 0.5f, "Clock Mult/Div");
		configParam(TRIG_PARAM, 0.f, 1.f, 0.f, "Trig Button");
		configParam(LOOP_PARAM, 0.f, 1.f, 0.f, "Loop");
		configParam(AUTO_PARAM, 0.f, 1.f, 0.f, "Auto Trigger");

		configParam(TIME_PARAM, 0.f, 1.f, 0.5f, "Time");
		configParam(TIME_CV_PARAM, -1.0f, 1.f, 0.0f, "Time CV");

		configParam(MIN_PARAM, 0.f, 1.f, 0.f, "Minimum Density");
		configParam(DENSITY_PARAM, 0.f, 1.f, 0.5f, "Density");
		configParam(MAX_PARAM, 0.f, 1.f, 1.f, "Maximum Density");

		configParam(DENSITY_CV_PARAM, -1.0f, 1.f, 0.0f, "Density CV");
		configParam(DEPTH_PARAM, 0.f, 1.f, 0.0f, "Envelope Depth");
		configParam(RAND_DEPTH_PARAM, -1.0f, 1.f, 0.f, "Random Depth");

		configParam(JITTER_PARAM, -1.f, 1.f, 0.f, "Jitter Amount");
		configParam(SYNC_PARAM, 0.f, 1.f, 0.f, "Sync Button");


		configParam(RAND_PARAM, 0.f, 1.f, 0.5f, "Random Flow");
		configParam(CURVE_PARAM, 0.f, 1.f, 0.5f, "Curve");
		configParam(SHAPE_PARAM, 0.f, 1.f, 0.5f, "Shape");

		configParam(CURVE_CV_PARAM, -1.0f, 1.f, 0.0f, "Curve CV");
		configParam(SHAPE_CV_PARAM, -1.0f, 1.f, 0.0f, "Shape CV");
		configParam(RAND_CV_PARAM, -1.0f, 1.f, 0.0f, "Random Flow CV");

		configParam(RAND_SHAPE_PARAM, -1.0f, 1.f, 0.0f, "Random Shape");
		configParam(RAND_CURVE_PARAM, -1.0f, 1.f, 0.0f, "Random Curve");

		configInput(CLOCK_INPUT, "Clock");
		configInput(TRIG_INPUT, "Trig");
		configInput(TIME_CV_INPUT, "Time CV");
		configInput(DENSITY_CV_INPUT, "Density CV");
		configInput(DEPTH_CV_INPUT, "Depth CV");
		configInput(RAND_DEPTH_CV_INPUT, "Random Depth CV");
		configInput(CURVE_CV_INPUT, "Curve CV");
		configInput(SHAPE_CV_INPUT, "Shape CV");
		configInput(RAND_CV_INPUT, "Random Flow CV");

		configOutput(RAMP_OUTPUT, "Ramp Output");
		configOutput(EOC_OUTPUT, "EOC Output");
		configOutput(MAIN_OUTPUT, "Trigger Output");
		configOutput(CV_OUTPUT, "CV Output");
		random::init();
	}

	bool autoParam = false;
	bool autoTrig = false;
	bool loop = false;
	bool sync = false;
	int count = 0;

	CV_Mode cvMode = ENVELOPE_VALUE;
	EOC_Mode eocMode = ENVELOPE_EOC;
	Ramp_Mode rampMode = ENVELOPE_PHASE;
	int cvcounter = 0;

  	dsp::SchmittTrigger clockTrigger;
	dsp::PulseGenerator trigGenerator;
	dsp::PulseGenerator eocGenerator;
	
	float lastPhase = 0.0f;

	float timer = 0.0f;
	float clockDelta = 0.0f;

	bool isRunning[16] = { false };
	float envelopeTimer[16] = { 0.0f };

	float burstTimer[16] = { 0.0f };
	float randomBurst[16] = { 0.0f };
	float randomCurve[16] = { 0.0f };
	float randomShape[16] = { 0.0f };
	float lastRandomBurst[16] = { 0.0f };
	float lastRandomCurve[16] = { 0.0f };
	float lastRandomShape[16] = { 0.0f };
	float interpRandomBurst[16] = { 0.0f };
	float interpRandomCurve[16] = { 0.0f };
	float interpRandomShape[16] = { 0.0f };
	float jitter[16] = { 0.0f };

	float burstPhase[16] = { 0.0f };

  	dsp::SchmittTrigger trigTrigger[16];
	dsp::PulseGenerator pulseGenerator[16];

	float getMultiplier(float mult){
		if(mult < 1.0f) return pow(2, mult);
		else return mult;
	}

	float processPhase(float phase, float multiplier){
		float p = rescale(phase, 0.0f, getMultiplier(multiplier), 0.0f, multiplier < 1.0f ? 1.0f : multiplier);
		return fmod(p, 1.0f);
	}

	float processPhase(float timer, float multiplier, float timerDelta){
		float p = rescale(timer, 0.0f, getMultiplier(multiplier) * timerDelta, 0.0f, multiplier < 1.0f ? 1.0f : multiplier);
		return fmod(p, 1.0f);
	}

	float processPhase(float timer, float multiplier, float timerDelta, float smooth){
		float p = rescale(timer, 0.0f, multiplier * timerDelta, 0.0f, multiplier < 1.0f ? 1.0f : multiplier);
		return fmod(p, 1.0f);
	}

	void process(const ProcessArgs& args) override {
  		float delta = args.sampleTime;
		
		autoParam = params[AUTO_PARAM].getValue() > 0.0f ? true : false;
		lights[AUTO_LIGHT].setBrightness(autoParam ? 1.0f : 0.0f);

		sync = params[SYNC_PARAM].getValue() > 0.0f ? true : false;
		lights[SYNC_LIGHT].setBrightness(sync ? 1.0f : 0.0f);

		loop = params[LOOP_PARAM].getValue() > 0.0f ? true : false;
		lights[LOOP_LIGHT].setBrightness(loop ? 1.0f : 0.0f);


		if(inputs[CLOCK_INPUT].isConnected()){
			int channels =  inputs[TRIG_INPUT].isConnected() ? inputs[TRIG_INPUT].getChannels() : 1;
			outputs[RAMP_OUTPUT].setChannels(channels);
			outputs[CV_OUTPUT].setChannels(channels);
			outputs[MAIN_OUTPUT].setChannels(channels);
			
			timer += delta;
      		int mult = (int)(params[CLOCK_PARAM].getValue() * 8 - 4);

			if(clockTrigger.process(inputs[CLOCK_INPUT].getVoltage(), 0.1f, 2.f)){ //clock handling
				count++;
				if(mult < 1 || count >= mult){
					count = 0;
					clockDelta = timer;
					timer = 0;	
				}	
			}

			//float masterPhase = rescale(float(timer), 0.0f, float(clockDelta) , 0.0f, 1.0f); //delta to ramp

			float processedDelta = clockDelta * getMultiplier(mult);
			float processedPhase = processPhase(timer, mult, clockDelta);
			if(processedPhase <= lastPhase && autoParam) autoTrig = true;
			lastPhase = processedPhase;

			if(rampMode == CLOCK_DIV_PHASE) outputs[RAMP_OUTPUT].setVoltage(processedPhase * 10.0f);  //output ramp


			for(int c = 0; c < channels; c++){
				float time =  clamp(params[TIME_PARAM].getValue() + (params[TIME_CV_PARAM].getValue() * inputs[TIME_CV_INPUT].getVoltage(c)), 0.0f, 1.0f);
				time = (int)(time  * 7.0f) + 1;

				if(trigTrigger[c].process(inputs[TRIG_INPUT].getVoltage(c) + params[TRIG_PARAM].getValue(), 0.1f, 1.f) || autoTrig){ //trigger handling
					autoTrig = false;
					isRunning[c] = true;
					envelopeTimer[c] = 0.0f;
					burstTimer[c] = 0.0f;
					trigGenerator.trigger(0.1f);

					lastRandomCurve[c] = randomCurve[c];
					lastRandomShape[c] = randomShape[c];
					randomShape[c] = random::uniform();
					randomCurve[c] = random::uniform();

					if(eocMode == TRIGGER_AUTO)  eocGenerator.trigger(0.1f);
					cvcounter = 0;
				}

				//Ramp for envelope
				float randFlow = clamp(params[RAND_PARAM].getValue() + (params[RAND_CV_PARAM].getValue() * inputs[RAND_CV_INPUT].getVoltage(c)), 0.0f, 1.0f);
				float envelopeMult = getMultiplier(time) * processedDelta;
				float envelopePhase = processPhase(envelopeTimer[c], time, processedDelta * time);
				if(envelopeTimer[c] > envelopeMult){
					if (!loop) isRunning[c] = false;
					else envelopeTimer[c] = 0;


					lastRandomShape[c] = randomShape[c];
					randomShape[c] = random::uniform();


					lastRandomCurve[c] = randomCurve[c];
					randomCurve[c] = random::uniform();


					lastRandomBurst[c] = randomBurst[c];
					randomBurst[c] = random::uniform();

					if(eocMode == ENVELOPE_EOC) eocGenerator.trigger(0.1f);
				}
				envelopeTimer[c] += delta;


				float remapE = (6 * pow(envelopePhase, 5)) - (15 * pow(envelopePhase, 4)) + (10 * pow(envelopePhase, 3));
				interpRandomShape[c] = remapE < 0.00000000000001 ? randomShape[c] : crossfade (lastRandomShape[c], randomShape[c], remapE);
				interpRandomCurve[c] = remapE < 0.00000000000001 ? randomCurve[c] : crossfade (lastRandomCurve[c], randomCurve[c], remapE);
				interpRandomBurst[c] = remapE < 0.00000000000001 ? randomBurst[c] : crossfade (lastRandomBurst[c], randomBurst[c], remapE);
				
				if(isRunning[c]){
					float actualB = (randomBurst[c] * (1.0f - randFlow)) + (interpRandomBurst[c] * randFlow);
					float actualRS = (randomShape[c] * (1.0f - randFlow)) + (interpRandomShape[c] * randFlow);
					float actualRC = (randomCurve[c] * (1.0f - randFlow)) + (interpRandomCurve[c] * randFlow);
					float shape = clamp(params[SHAPE_PARAM].getValue() + (params[SHAPE_CV_PARAM].getValue() * inputs[SHAPE_CV_INPUT].getVoltage(c)) + (actualRS * params[RAND_SHAPE_PARAM].getValue()), 0.0f, 1.0f);
					float curve = clamp(params[CURVE_PARAM].getValue() + (params[CURVE_CV_PARAM].getValue() * inputs[CURVE_CV_INPUT].getVoltage(c)) + (actualRC * params[RAND_CURVE_PARAM].getValue()), 0.0f, 1.0f);
					curve  = curve >= 0.5f ? rescale(curve, 0.5f, 1.0f, 1.01f, 2.0f) : rescale(curve, 0.0f, 0.5f, 0.25f, 0.99f);
					float depth = clamp(params[DEPTH_PARAM].getValue() + (inputs[DEPTH_CV_INPUT].getVoltage(c) / 10.0f), 0.0f, 1.0f);
			
					//Envelope Shaping
					float envelopeValue;
					if(envelopePhase <= shape) envelopeValue = pow(rescale(envelopePhase, 0.0f, shape, 0.0f, 1.0f), curve);
					else envelopeValue = pow(rescale(envelopePhase, shape, 1.0f, 1.0f, 0.0f), curve);
					
					if(rampMode == ENVELOPE_PHASE) outputs[RAMP_OUTPUT].setVoltage(envelopePhase * 10.0f, c);  //output ramp

					if(cvMode == ENVELOPE_VALUE) outputs[CV_OUTPUT].setVoltage(envelopeValue*10.0f, c);
					if(cvMode == BURST_RANDOM) outputs[CV_OUTPUT].setVoltage(actualB*10.0f, c);
					if(cvMode == SHAPE_RANDOM) outputs[CV_OUTPUT].setVoltage(actualRS*10.0f, c);
					if(cvMode == CURVE_RANDOM) outputs[CV_OUTPUT].setVoltage(actualRC*10.0f, c);
					
					float min = params[MIN_PARAM].getValue();
					float max = params[MAX_PARAM].getValue();
					min = rescale(pow(min, 3), 0.0f, 1.0f, (getMultiplier(mult) * time) / 512.0f, (time * getMultiplier(mult)) / 2.0f);
					max = rescale(pow(max, 3), 0.0f, 1.0f, (getMultiplier(mult) * time) / 512.0f, (time * getMultiplier(mult)) / 2.0f);
				
					//Ramp for burst generation
					float density = clamp(params[DENSITY_PARAM].getValue() + (params[DENSITY_CV_PARAM].getValue() * inputs[DENSITY_CV_INPUT].getVoltage(c)), 0.0f, 1.0f);
					density += rescale((1.0f - envelopeValue) * depth, 0.0f, 1.0f, 0.0f, 1.0f - density);
					density += params[RAND_DEPTH_PARAM].getValue() > 0.0f ? rescale(actualB, 0.0f, 1.0f, 0.0f, 1.0f - density) * params[RAND_DEPTH_PARAM].getValue(): rescale(actualB, 0.0, 1.0f, 0.0f, density) * params[RAND_DEPTH_PARAM].getValue();
					density += params[JITTER_PARAM].getValue() > 0.0f ? rescale(jitter[c], 0.0f, 1.0f, 0.0f, 1.0f - density) * params[JITTER_PARAM].getValue(): rescale(jitter[c], 0.0, 1.0f, 0.0f, density) * params[JITTER_PARAM].getValue();
					density = clamp(density, 0.0f, 1.0f);
					//density += params[RAND_DEPTH_PARAM].getValue() * actualB > 0.0f ? rescale(params[RAND_DEPTH_PARAM].getValue() * actualB, 0.0f, 1.0f, 0.0f, 1.0f - density) : rescale(params[RAND_DENSITY_PARAM].getValue() * actualB, -1.0f, 0.0f, density, 0.0f);
					float burstMult = rescale(pow(density, 2), 0.0f, 1.0f, min, max);
					if(sync) burstMult = getMultiplier(int(burstMult * 8 - 4));

					burstPhase[c] = processPhase(burstTimer[c], burstMult, 1.0f, 1.0f);

					if(rampMode == BURST_PHASE) outputs[RAMP_OUTPUT].setVoltage(burstPhase[c] * 10.0f, c);  //output ramp

					if(burstTimer[c] >= burstMult){
						pulseGenerator[c].trigger(1e-3f);
						burstTimer[c] = 0.0f;
						jitter[c] = random::uniform();

						if(cvMode == RANDOM_BP) outputs[CV_OUTPUT].setVoltage((random::uniform()*10.0f) - 5.0f, c);
						if(cvMode == RANDOM_POSITIVE) outputs[CV_OUTPUT].setVoltage(random::uniform()* 5.0f, c);
						if(cvMode == RANDOM_NEG) outputs[CV_OUTPUT].setVoltage(random::uniform()* -5.0f, c);

						cvcounter += 1;
						cvcounter %= 10;
						if(cvMode == COUNTER) outputs[CV_OUTPUT].setVoltage(cvcounter, c);
					} 
					else burstTimer[c] += delta;
					
				} 
				for(int c = 0; c < channels; c++){
					outputs[MAIN_OUTPUT].setVoltage(pulseGenerator[c].process(args.sampleTime) ? 10.0f : 0.0f, c);
					outputs[EOC_OUTPUT].setVoltage(eocGenerator.process(args.sampleTime) ? 10.0f : 0.0f);
				}


			}
			
			lights[TRIG_LIGHT].setBrightness(trigGenerator.process(args.sampleTime) + (params[TRIG_PARAM].getValue() > 0.0f ? 1.0f : 0.0f));
		}
		
	}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "cvMode", json_integer(cvMode));
		json_object_set_new(rootJ, "eocMode", json_integer(eocMode));
		json_object_set_new(rootJ, "rampMode", json_integer(rampMode));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* cvModeJ = json_object_get(rootJ, "cvMode");
		if (cvModeJ)
			cvMode = CV_Mode(json_integer_value(cvModeJ));

		json_t* eocModeJ = json_object_get(rootJ, "eocMode");
		if (eocModeJ)
			eocMode = EOC_Mode(json_integer_value(eocModeJ));

		json_t* rampModeJ = json_object_get(rootJ, "rampMode");
		if (rampModeJ)
			rampMode = Ramp_Mode(json_integer_value(rampModeJ));
	}
};


struct PolyBurstWidget : ModuleWidget {
	PolyBurstWidget(PolyBurst* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/polyBurst.svg")));

		addChild(createWidgetCentered<stocScrew>(Vec(7, 7)));
		addChild(createWidgetCentered<stocScrew>(Vec(box.size.x -7, 7)));
		addChild(createWidgetCentered<stocScrew>(Vec(7, box.size.y - 7)));
		addChild(createWidgetCentered<stocScrew>(Vec(box.size.x - 7, box.size.y - 7)));

		addParam(createParamCentered<newKnob>(mm2px(Vec(20.32, 19.645)), module, PolyBurst::CLOCK_PARAM));
		addParam(createParamCentered<stocButton>(mm2px(Vec(34.138, 19.645)), module, PolyBurst::LOOP_PARAM));
		addParam(createParamCentered<stocButtonMom>(mm2px(Vec(20.32, 31.729)), module, PolyBurst::TRIG_PARAM));
		addParam(createParamCentered<stocButton>(mm2px(Vec(34.138, 31.729)), module, PolyBurst::AUTO_PARAM));
		addParam(createParamCentered<newKnob>(mm2px(Vec(20.32, 45.144)), module, PolyBurst::TIME_PARAM));
		addParam(createParamCentered<newAttn>(mm2px(Vec(28.163, 48.443)), module, PolyBurst::TIME_CV_PARAM));
		addParam(createParamCentered<newAttn>(mm2px(Vec(6.514, 55.617)), module, PolyBurst::MIN_PARAM));
		addParam(createParamCentered<newAttn>(mm2px(Vec(34.15, 55.617)), module, PolyBurst::MAX_PARAM));
		addParam(createParamCentered<newKnob>(mm2px(Vec(20.332, 60.443)), module, PolyBurst::DENSITY_PARAM));
		addParam(createParamCentered<stocButton>(mm2px(Vec(6.13, 66.612)), module, PolyBurst::SYNC_PARAM));
		addParam(createParamCentered<newAttn>(mm2px(Vec(34.15, 66.612)), module, PolyBurst::JITTER_PARAM));
		addParam(createParamCentered<newAttn>(mm2px(Vec(20.332, 69.057)), module, PolyBurst::DENSITY_CV_PARAM));
		addParam(createParamCentered<newKnob>(mm2px(Vec(6.502, 80.221)), module, PolyBurst::DEPTH_PARAM));
		addParam(createParamCentered<newKnob>(mm2px(Vec(34.138, 80.221)), module, PolyBurst::RAND_DEPTH_PARAM));
		addParam(createParamCentered<newKnob>(mm2px(Vec(6.502, 95.931)), module, PolyBurst::CURVE_PARAM));
		addParam(createParamCentered<newKnob>(mm2px(Vec(20.32, 95.931)), module, PolyBurst::SHAPE_PARAM));
		addParam(createParamCentered<newKnob>(mm2px(Vec(34.138, 95.931)), module, PolyBurst::RAND_PARAM));
		addParam(createParamCentered<newAttn>(mm2px(Vec(6.502, 104.578)), module, PolyBurst::CURVE_CV_PARAM));
		addParam(createParamCentered<newAttn>(mm2px(Vec(13.411, 104.578)), module, PolyBurst::RAND_CURVE_PARAM));
		addParam(createParamCentered<newAttn>(mm2px(Vec(20.32, 104.578)), module, PolyBurst::SHAPE_CV_PARAM));
		addParam(createParamCentered<newAttn>(mm2px(Vec(27.229, 104.578)), module, PolyBurst::RAND_SHAPE_PARAM));
		addParam(createParamCentered<newAttn>(mm2px(Vec(34.138, 104.578)), module, PolyBurst::RAND_CV_PARAM));

		addInput(createInputCentered<newJack>(mm2px(Vec(6.503, 19.647)), module, PolyBurst::CLOCK_INPUT));
		addInput(createInputCentered<newJack>(mm2px(Vec(6.503, 31.73)), module, PolyBurst::TRIG_INPUT));
		
		addInput(createInputCentered<newJack>(mm2px(Vec(34.14, 45.083)), module, PolyBurst::TIME_CV_INPUT));
		
		addInput(createInputCentered<newJack>(mm2px(Vec(20.32, 75.212)), module, PolyBurst::DENSITY_CV_INPUT));
		addInput(createInputCentered<newJack>(mm2px(Vec(15.666, 80.289)), module, PolyBurst::DEPTH_CV_INPUT));
		
		addInput(createInputCentered<newJack>(mm2px(Vec(24.974, 80.289)), module, PolyBurst::RAND_DEPTH_CV_INPUT));
		addInput(createInputCentered<newJack>(mm2px(Vec(6.503, 110.613)), module, PolyBurst::CURVE_CV_INPUT));
		addInput(createInputCentered<newJack>(mm2px(Vec(20.322, 110.613)), module, PolyBurst::SHAPE_CV_INPUT));
		addInput(createInputCentered<newJack>(mm2px(Vec(34.14, 110.613)), module, PolyBurst::RAND_CV_INPUT));

		addOutput(createOutputCentered<newJack>(mm2px(Vec  (6.502, 45.083)), module, PolyBurst::RAMP_OUTPUT));
		addOutput(createOutputCentered<newJack>(mm2px(Vec(6.503, 121.442)), module, PolyBurst::EOC_OUTPUT));
		addOutput(createOutputCentered<newJack>(mm2px(Vec(20.322, 121.442)), module, PolyBurst::MAIN_OUTPUT));
		addOutput(createOutputCentered<newJack>(mm2px(Vec(34.14, 121.442)), module, PolyBurst::CV_OUTPUT));

		addChild(createLightCentered<LargeLight<stocWhiteLight>>(mm2px(Vec(34.138 - 0.35, 19.645 - 0.35)), module, PolyBurst::LOOP_LIGHT));
		addChild(createLightCentered<LargeLight<stocWhiteLight>>(mm2px(Vec(20.32 - 0.35, 31.729 - 0.35)), module, PolyBurst::TRIG_LIGHT));
		addChild(createLightCentered<LargeLight<stocWhiteLight>>(mm2px(Vec(34.138 - 0.35, 31.729 - 0.35)), module, PolyBurst::AUTO_LIGHT));
		addChild(createLightCentered<MediumLight<stocWhiteLight>>(mm2px(Vec(12.477 - 0.35, 43.709 - 0.35)), module, PolyBurst::ENV_LIGHT));
		addChild(createLightCentered<LargeLight<stocWhiteLight>>(mm2px(Vec(6.13 - 0.35, 66.612 - 0.35)), module, PolyBurst::SYNC_LIGHT));
	}

	void appendContextMenu(Menu* menu) override {
		PolyBurst* module = dynamic_cast<PolyBurst*>(this->module);

		menu->addChild(new MenuEntry);

		menu->addChild(createMenuLabel("CV Modes"));
		
		struct CvModeItem : MenuItem {
			PolyBurst* module;
			CV_Mode cvMode;
			void onAction(const event::Action& e) override {
				module->cvMode = cvMode;
			}
		};
		
	static std::map<CV_Mode, const std::string> CVModeString = {
																{CV_Mode(0), "Envelope value"},
																{CV_Mode(1), "Random bp"},
																{CV_Mode(2), "Random positive"},
																{CV_Mode(3), "Random neg"},
																{CV_Mode(4), "Counter"},
																{CV_Mode(5), "Burst random"},
																{CV_Mode(6), "Shape random"},
																{CV_Mode(7), "Curve random"},
															};

		for (int i = 0; i < 8; i++) {
			CvModeItem* modeItem = createMenuItem<CvModeItem>(CVModeString[CV_Mode(i)]);
			modeItem->rightText = CHECKMARK(module->cvMode == CV_Mode(i));
			modeItem->module = module;
			modeItem->cvMode = CV_Mode(i);
			menu->addChild(modeItem);
		}


	menu->addChild(createMenuLabel("EOC Mode"));

	struct eocModeItem : MenuItem {
		PolyBurst* module;
		EOC_Mode eocMode;
		void onAction(const event::Action& e) override {
			module->eocMode = eocMode;
		}
	};

	static std::map<EOC_Mode, const std::string> EOCModeString = {
																{EOC_Mode(0), "Envelope EOC"},
																{EOC_Mode(1), "Trigger in"},
															};

	for (int i = 0; i < 2; i++) {
		eocModeItem* modeItem = createMenuItem<eocModeItem>(EOCModeString[EOC_Mode(i)]);
		modeItem->rightText = CHECKMARK(module->eocMode == EOC_Mode(i));
		modeItem->module = module;
		modeItem->eocMode = EOC_Mode(i);
		menu->addChild(modeItem);
	}


	menu->addChild(createMenuLabel("Ramp Mode"));

	struct RampModeItem : MenuItem {
		PolyBurst* module;
		Ramp_Mode rampMode;
		void onAction(const event::Action& e) override {
			module->rampMode = rampMode;
		}
	};

	static std::map<Ramp_Mode, const std::string> RampModeString = {
																{Ramp_Mode(0), "Envelope"},
																{Ramp_Mode(1), "Burst"},
																{Ramp_Mode(2), "Clock Mult/Div"}
															};

	for (int i = 0; i < 4; i++) {
		RampModeItem* modeItem = createMenuItem<RampModeItem>(RampModeString[Ramp_Mode(i)]);
		modeItem->rightText = CHECKMARK(module->rampMode == Ramp_Mode(i));
		modeItem->module = module;
		modeItem->rampMode = Ramp_Mode(i);
		menu->addChild(modeItem);
	}

	}
};


Model* modelPolyBurst = createModel<PolyBurst, PolyBurstWidget>("polyBurst");