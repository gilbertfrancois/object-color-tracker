//
// Created by G.F. Duivesteijn on 09/06/2018.
//

#include "ofMain.h"
#include "ofApp.h"

//========================================================================
int main() {
    ofGLFWWindowSettings settings;
    settings.resizable = false;
    settings.setSize(640, 480);
    ofCreateWindow(settings);
    return ofRunApp(new ofApp);
}
