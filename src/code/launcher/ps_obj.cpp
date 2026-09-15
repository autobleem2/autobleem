//
// Created by screemer on 2/11/19.
//

#include "ps_obj.h"
using namespace std;

//*******************************
// PsObj::PsObj
//*******************************
PsObj::PsObj(string name1, string texPath) : gui(Gui::getInstance()), renderer(gui->renderer()), name(name1) {
    if (texPath != "") {
        load(texPath);
    }
}

//*******************************
// PsObj::load
//*******************************
void PsObj::load(const string & imagePath) {
    tex = ableem::Texture::loadFile(renderer, imagePath);
    ableem::Size size = tex.size();
    w = size.w;
    h = size.h;
    x = 0, y = 0;
    ow = w;
    oh = h;
}

//*******************************
// PsObj::destroy
//*******************************
void PsObj::destroy() {
}

//*******************************
// PsObj::render
//*******************************
void PsObj::render() {
    if (visible) {
        ableem::Rect rect(x, y, w, h);
        ableem::Rect fullRect(0, 0, w, h);
        renderer.copy(tex, &fullRect, &rect);
    }
}
