#include "rack.hpp"


using namespace rack;

// Declare the Plugin, defined in plugin.cpp
extern Plugin *pluginInstance;
extern Model *modelPolyturing;
extern Model *modelManseq;
extern Model *modelBtnseq;
extern Model *modelClock;
extern Model *modelPolydelay;
extern Model *modelSpread;
extern Model *modelSlew;

// Declare each Model, defined in each module source file
// extern Model *modelMyModule;

//SCREW
struct stocScrew : app::SvgScrew {
	stocScrew() {
		setSvg(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Comp/stocScrew.svg")));
		sw->wrap();
		box.size = sw->box.size;
	}
};

//LIGHTS
template <typename BASE>
struct LedLight : BASE {
	LedLight() {
	  //this->box.size = Vec(20.0, 20.0);
	  this->box.size = mm2px(Vec(4.0, 4.0));
	}
};

struct stocLight : ModuleLightWidget {
	GeoGrayModuleLight() {
		bgColor = nvgRGB(0x8e, 0x8e, 0x8e);
		borderColor = nvgRGB(0x1d, 0x1d, 0x1b);//nvgRGBA(0, 0, 0, 0x60);
	}
	
	void drawLight(const DrawArgs &args) override { // from app/LightWidget.cpp (only nvgStrokeWidth of border was changed)
		float radius = (box.size.x / 2.0) +1;

		nvgBeginPath(args.vg);
		nvgCircle(args.vg, radius, radius, radius);

		// Background
		if (bgColor.a > 0.0) {
			nvgFillColor(args.vg, bgColor);
			nvgFill(args.vg);
		}

		// Foreground
		if (color.a > 0.0) {
			nvgFillColor(args.vg, color);
			nvgFill(args.vg);
		}

		// Border
		if (borderColor.a > 0.0) {
			nvgStrokeWidth(args.vg, 1.0);//0.5);
			nvgStrokeColor(args.vg, borderColor);
			nvgStroke(args.vg);
		}
	}
};

struct stocWhiteLight : stocLight {
	stocWhiteLight() {
		addBaseColor(SCHEME_WHITE);
	}
};

struct stocRedLight : stocLight {
	stocRedLight() {
		addBaseColor(SCHEME_RED);
	}
};

struct stocRedGreenBlueLight : stocLight {
	stocRedGreenBlueLight() {
		addBaseColor(SCHEME_RED);
		addBaseColor(SCHEME_GREEN);
		addBaseColor(SCHEME_BLUE);
	}
};

//BUTTON
struct stocButton : app::SvgSwitch {
	stocButton() {
		momentary = true;
		addFrame(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Comp/stocButton.svg")));
	}
};

//KNOBS

struct stocBigKnob : SVGKnob {
	stocBigKnob() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Comp/stocBigKnob.svg")));
	}
};

struct stocKnob : SVGKnob {
	stocKnob() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Comp/stocKnob.svg")));
	}
};

struct stocSnapKnob : SVGKnob {
	stocSnapKnob() {
		snap = true;
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Comp/stocKnob.svg")));
	}
};


struct stocAttn : SVGKnob {
	stocAttn() {
		minAngle = -0.75*M_PI;
		maxAngle = 0.75*M_PI;
		setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Comp/stocAttn.svg")));
	}
};

/////JACKS

struct aPJackArancione : SVGPort {
	aPJackArancione() {
		setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Comp/jack/aPjackarancione.svg")));
	}
};

struct aPJackAzzurro : SVGPort {
	aPJackAzzurro() {
		setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Comp/jack/aPjackazzurro.svg")));
	}
};

struct aPJackBianco : SVGPort {
	aPJackBianco() {
		setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Comp/jack/aPjackbianco.svg")));
	}
};

struct aPJackBlu : SVGPort {
	aPJackBlu() {
		setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Comp/jack/aPjackblu.svg")));
	}
};

struct aPJackFux : SVGPort {
	aPJackFux() {
		setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Comp/jack/aPjackfux.svg")));
	}
};

struct aPJackGiallo : SVGPort {
	aPJackGiallo() {
		setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Comp/jack/aPjackgiallo.svg")));
	}
};

struct aPJackNero : SVGPort {
	aPJackNero() {
		setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Comp/jack/aPjacknero.svg")));
	}
};

struct aPJackRosa : SVGPort {
	aPJackRosa() {
		setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Comp/jack/aPjackrosa.svg")));
	}
};

struct aPJackRosso : SVGPort {
	aPJackRosso() {
		setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Comp/jack/aPjackrosso.svg")));
	}
};

struct aPJackTurchese : SVGPort {
	aPJackTurchese() {
		setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Comp/jack/aPjackturchese.svg")));
	}
};

struct aPJackVerde : SVGPort {
	aPJackVerde() {
		setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Comp/jack/aPjackverde.svg")));
	}
};

struct aPJackViola : SVGPort {
	aPJackViola() {
		setSVG(APP->window->loadSvg(asset::plugin(pluginInstance, "res/Comp/jack/aPjackviola.svg")));
	}
};