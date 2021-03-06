#include "plugin.hpp"


Plugin *pluginInstance;


void init(Plugin *p) {
	pluginInstance = p;

	// Add modules here, e.g.
	p->addModel(modelPolyturing);
	p->addModel(modelManseq);
	p->addModel(modelBtnseq);
	p->addModel(modelClock);
	p->addModel(modelPolydelay);
	p->addModel(modelSpread);
	p->addModel(modelPolyslew);
	
	// Any other plugin initialization may go here.
	// As an alternative, consider lazy-loading assets and lookup tables when your module is created to reduce startup times of Rack.
}
