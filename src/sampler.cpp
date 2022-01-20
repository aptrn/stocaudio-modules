#include "plugin.hpp"
#include "audiofile/AudioFile.h"
#include "Granular.h"
#include "Envelope.h"

struct Sampler : Module {
	enum ParamId {
		OVERLAP_PARAM,
		SIZE_PARAM,
		FOLDER_PARAM,
		SAMPLE_PARAM,
		STRETCH_PARAM,
		PITCH_PARAM,
		SPREAD_PARAM,
		SHAPE_PARAM,
		CURVE_PARAM,
		START_PARAM,
		END_PARAM,
		ATTACK_PARAM,
		DECAY_PARAM,
		SUSTAIN_PARAM,
		RELEASE_PARAM,
		PARAMS_LEN
	};
	enum InputId {
		TRIG_INPUT,
		INPUTS_LEN
	};
	enum OutputId {
		L_OUTPUT,
		R_OUTPUT,
		OUTPUTS_LEN
	};
	enum LightId {
		BLINK_LIGHT,
		LIGHTS_LEN
	};

	std::string dirPath = "";
	std::string newPath = "/home/ale/Music/sssamples";
    std::vector<std::vector<std::string>> database;
    std::vector<std::vector<AudioFile<float>>> databaseFiles;
	AudioFile<float>* audioFile;
	
	unsigned int selectedFolder = 0;
	unsigned int selectedFile = 0;

	unsigned int iter = 0;
	unsigned int count = 0;
	unsigned int channel = 0;

	Granular granular;
	ADSREnvelope ampEnv;

  	dsp::SchmittTrigger trigInput;

	Sampler() {
		config(PARAMS_LEN, INPUTS_LEN, OUTPUTS_LEN, LIGHTS_LEN);
		configParam(FOLDER_PARAM, 0.01f, 1.f, 0.5f, "Folder");
		configParam(SAMPLE_PARAM, 0.01f, 1.f, 0.5f, "Sample");
		configParam(OVERLAP_PARAM, 0.01f, 1.f, 0.5f, "Overlap");
		configParam(SIZE_PARAM, 0.01f, 1.f, 0.5f, "Size");
		configParam(STRETCH_PARAM, 0.01f, 2.f, 1.0f, "Stretch");
		configParam(PITCH_PARAM, 0.01f, 2.f, 1.0f, "Pitch");
		configParam(SPREAD_PARAM, 0.01f, 1.f, 1.0f, "Spread");
		configParam(SHAPE_PARAM, 0.01f, 1.f, 0.5f, "Shape");
		configParam(CURVE_PARAM, 0.01f, 1.f, 0.5f, "Curve");
		configParam(START_PARAM, 0.01f, 1.f, 0.0f, "Start");
		configParam(END_PARAM, 0.01f, 1.f, 1.0f, "End");
		configParam(ATTACK_PARAM, 0.0f, 1.f, 0.25f, "Attack");
		configParam(DECAY_PARAM, 0.0f, 1.f, 0.25f, "Decay");
		configParam(SUSTAIN_PARAM, 0.0f, 1.f, 1.0f, "Sustain");
		configParam(RELEASE_PARAM, 0.0f, 1.f, 0.25f, "Release");
		




		configInput(TRIG_INPUT, "Trigger");
		configOutput(L_OUTPUT, "Left");
		configOutput(R_OUTPUT, "Right");
		updateMainDirectory(newPath);
		updateLoadedFile(selectedFolder, selectedFile);

		//int sampleRate = audioFile.getSampleRate();
		//int bitDepth = audioFile.getBitDepth();
	}

	bool updateMainDirectory(std::string newPath){
		if(rack::system::isDirectory(newPath) && newPath != dirPath){
			dirPath = newPath;
			database.clear();
			databaseFiles.clear();
			std::vector<std::string> subDirs = rack::system::getEntries(dirPath, 0);
			for(unsigned int i = 0; i < subDirs.size(); i++){
				if(rack::system::isDirectory(subDirs[i])){
					std::vector<std::string> files = rack::system::getEntries(subDirs[i], 0);
					std::vector<std::string> validFiles;
					std::vector<AudioFile<float>> validFilesData;
					for(unsigned int f = 0; f < files.size(); f++){
						if(rack::system::getExtension(files[f]) == ".wav" || rack::system::getExtension(files[f]) == ".aiff"){
							validFiles.push_back(files[f]);
							
							AudioFile<float>newFile;
							newFile.load(files[f]);
							validFilesData.push_back(newFile);
						}
					}
					if(validFiles.size() > 0){
						database.push_back(validFiles);
						databaseFiles.push_back(validFilesData);
					}
				}
			}
			return true;
		}
		else return false;
	}

	void updateLoadedFile(unsigned int folderIndex, unsigned int fileIndex){
		folderIndex %= database.size();
		fileIndex %= database[folderIndex].size();
		
		if(folderIndex == selectedFolder && fileIndex == selectedFile){
			return;
		}
		else{
			audioFile = &databaseFiles[folderIndex][fileIndex];
			//AudioFile<float> sss;
			//sss.load("/home/ale/Music/aaa.wav");
			//audioFile = &sss;
			granular.setBuffer(audioFile->samples);
			//std::cout << database[folderIndex][fileIndex] << std::endl;
		}
		return;
	}

	void process(const ProcessArgs& args) override {
		/*
		if(iter > audioFile->getNumSamplesPerChannel()){
			if(count > 3){
				count = 0;
				//selectedFolder++;
				selectedFolder = random::uniform() * database.size();
			}
			else count++;

			iter = 0;
			//selectedFile++;
			selectedFile = random::uniform() * database[selectedFolder].size();
			std::cout << database[selectedFolder][selectedFile] << std::endl;
			updateLoadedFile(selectedFolder, selectedFile);
		}
		else iter++;


		float currentSample = audioFile->samples[channel][iter];
		outputs[SINE_OUTPUT].setVoltage(currentSample);
		*/

		updateMainDirectory(newPath);
		granular.sampleRate = args.sampleRate;
		granular.overlap = params[OVERLAP_PARAM].getValue();
		granular.size = params[SIZE_PARAM].getValue();
		granular.stretch = params[STRETCH_PARAM].getValue();
		granular.speed = params[PITCH_PARAM].getValue();
		granular.spread = params[SPREAD_PARAM].getValue();
		granular.shape = params[SHAPE_PARAM].getValue();
		granular.curve = params[CURVE_PARAM].getValue();

		granular.boundMode = 0;
		granular.start = params[START_PARAM].getValue();
		granular.end = params[END_PARAM].getValue();


		if(trigInput.process(inputs[TRIG_INPUT].getVoltage(), 0.1f, 2.f)){
			unsigned newFolder = params[FOLDER_PARAM].getValue() * database.size();
			unsigned newFile = params[SAMPLE_PARAM].getValue() * database[newFolder].size();
			updateLoadedFile(newFolder, newFile);
			granular.trigger();
		}

		ampEnv.maxSamples = (granular.buffer.size() - (granular.buffer.size() * granular.start)) * (1.0F - granular.stretch);
		ampEnv.times[0] = params[ATTACK_PARAM].getValue();
		ampEnv.times[1] = params[DECAY_PARAM].getValue();
		ampEnv.sustain = params[SUSTAIN_PARAM].getValue();
		ampEnv.times[2] = params[RELEASE_PARAM].getValue();


	
		std::vector<float> currentSample(2, 0.0f);
		currentSample = granular.process();
		float amplitudeEnvelope = ampEnv.tick(inputs[TRIG_INPUT].getVoltage() > 0.1);
		outputs[L_OUTPUT].setVoltage(currentSample[0] * amplitudeEnvelope);
		outputs[R_OUTPUT].setVoltage(currentSample[1] * amplitudeEnvelope);
	
	}
};


struct SamplerWidget : ModuleWidget {
	SamplerWidget(Sampler* module) {
		setModule(module);
		setPanel(createPanel(asset::plugin(pluginInstance, "res/MyModule.svg")));

		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
		addChild(createWidget<ScrewSilver>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
		addChild(createWidget<ScrewSilver>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(6.24, 18.063)), module, Sampler::FOLDER_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(24.24, 18.063)), module, Sampler::SAMPLE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(6.24, 25.00)), module, Sampler::OVERLAP_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(24.24, 25.00)), module, Sampler::SIZE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(6.24, 35.00)), module, Sampler::STRETCH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(24.24, 35.00)), module, Sampler::PITCH_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(6.24, 45.00)), module, Sampler::SHAPE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(24.24, 45.00)), module, Sampler::CURVE_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(6.24, 55.00)), module, Sampler::START_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(24.24, 55.00)), module, Sampler::END_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(15.24, 95.063)), module, Sampler::SPREAD_PARAM));

		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(6.24, 65.00)), module, Sampler::ATTACK_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(24.24, 65.00)), module, Sampler::DECAY_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(6.24, 75.00)), module, Sampler::SUSTAIN_PARAM));
		addParam(createParamCentered<RoundBlackKnob>(mm2px(Vec(24.24, 75.063)), module, Sampler::RELEASE_PARAM));

		addInput(createInputCentered<PJ301MPort>(mm2px(Vec(15.24, 90.478)), module, Sampler::TRIG_INPUT));

		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10.24, 108.713)), module, Sampler::L_OUTPUT));
		addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(20.24, 108.713)), module, Sampler::R_OUTPUT));

		//addChild(createLightCentered<MediumLight<RedLight>>(mm2px(Vec(15.24, 25.81)), module, Sampler::BLINK_LIGHT));
	}


	void appendContextMenu(Menu* menu) override {
		Sampler* module = dynamic_cast<Sampler*>(this->module);



		menu->addChild(createMenuLabel("Sample folders"));


		struct LabelField : ui::TextField {
			Sampler* module;
			void onSelectKey(const event::SelectKey& e) override {
				if (e.action == GLFW_PRESS && e.key == GLFW_KEY_ENTER) {
					module->newPath = text;

					ui::MenuOverlay* overlay = getAncestorOfType<ui::MenuOverlay>();
					overlay->requestDelete();
					e.consume(this);
				}

				if (!e.getTarget()) {
					ui::TextField::onSelectKey(e);
				}
			}
		};

		LabelField* labelField = new LabelField;
		labelField->placeholder = "/path/to/your/sampleFolders";
		labelField->box.size.x = 400;
		labelField->module = module;
		labelField->text = module->newPath;
		menu->addChild(labelField);
		//menu->addChild(construct<FolderMenuItem>(&MenuItem::text, "Sample folders", &FolderMenuItem::module, module));
			

	}
};


Model* modelSampler = createModel<Sampler, SamplerWidget>("sampler");