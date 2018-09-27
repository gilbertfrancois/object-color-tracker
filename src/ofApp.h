//
// Created by G.F. Duivesteijn on 09/06/2018.
//

#pragma once

#include "ofMain.h"
#include "ofxOsc.h"
#include "ofxGui.h"
#include "ofxCv.h"

class ofApp : public ofBaseApp {

private:
    const int camWidth = 640;
    const int camHeight = 480;
    const int A = camWidth * camHeight;

    ofVideoGrabber *videoGrabber;
    vector<ofVideoDevice> video_device_list;
    ofTrueTypeFont font;
    ofxOscSender sender;
    ofxPanel gui;
    bool showGui = true;
    bool oscMessageSent = false;

    ofParameterGroup color_settings_group;
    ofParameter<int> tol_h;
    ofParameter<int> tol_s;
    ofParameter<int> tol_v;
    ofParameter<int> sample_radius;
    ofParameter<size_t> min_area_size;
    ofParameter<size_t> max_area_size;
    ofParameter<bool> one_blob_only;
    ofParameter<bool> lpf;

    ofParameterGroup display_settings_group;
    ofParameter<int> fps;
    ofParameter<bool> show_trail;
    ofParameter<bool> show_webcam_view;
    ofParameter<bool> show_contours;
    ofParameter<bool> show_help;

    ofParameterGroup camera_group;
    ofxButton cycle_cameras_button;
    ofParameter<int> current_camera_device_id;
    ofParameter<std::string> current_camera_device_name;

    ofParameterGroup comm_settings_group;
    ofParameter<std::string> msg;
    ofParameter<std::string> server;
    ofParameter<std::string> port;

    int Hmin = 0;
    int Hmax = 0;
    int Smin = 0;
    int Smax = 0;
    int Vmin = 0;
    int Vmax = 0;
    int Hmin_tol = 0;
    int Hmax_tol = 0;
    int Smin_tol = 0;
    int Smax_tol = 0;
    int Vmin_tol = 0;
    int Vmax_tol = 0;

    cv::Mat rgb;
    cv::Mat rgbm;
    cv::Mat hsb;
    cv::Mat h;
    cv::Mat s;
    cv::Mat b;
    cv::Mat ftr;
    cv::Mat maskH;
    cv::Mat maskS;
    cv::Mat maskV;

    int simplifyMode = CV_CHAIN_APPROX_SIMPLE;
    int contourFindingMode = CV_RETR_EXTERNAL;
    std::vector<std::vector<cv::Point>> allContours;
    std::vector<cv::Moments> mu;
    std::vector<cv::Point2f> mc;
    float max_area = 0.0f;
    int argmax_area = -1;

    std::vector<ofVec3f> buffer;
    int buffer_size = 50;
    int buffer_position = 0;

    ofVec3f pos;
    ofVec3f vel;
    ofVec3f acc;


public:

    //--------------------------------------------------------------

    void setup();

    void update();

    void draw();

    //--------------------------------------------------------------

    void setupCamera();

    void setupRingBuffer();

    void setupImageBuffers();

    void setupGui();

    //--------------------------------------------------------------

    void updateObjectLocation();

    void updateFilterMasks();

    void updateHSVRange();

    void updateNormalizedObjectLocationInBuffer(ofVec3f v);

    //--------------------------------------------------------------

    void sendOscMessage();

    //--------------------------------------------------------------

    void drawObjectCursor();

    void drawObjectCursorAtPosition(ofVec3f v);

    void drawMouseCursor() const;

    void drawTrail();

    void drawStatusMessage(ofVec3f v);

    void drawHelpPanel();

    //--------------------------------------------------------------

    void keyPressed(int key);

    void keyReleased(int key);

    void mouseMoved(int x, int y);

    void mouseDragged(int x, int y, int button);

    void mousePressed(int x, int y, int button);

    void mouseReleased(int x, int y, int button);

    void mouseEntered(int x, int y);

    void mouseExited(int x, int y);

    void windowResized(int w, int h);

    void gotMessage(ofMessage msg);

    void dragEvent(ofDragInfo dragInfo);

    void serverChanged(std::string &v);

    void portChanged(std::string &v);

    void msgChanged(std::string &v);

    void fpsChanged(int &v);

    void cameraDeviceIdChanged(int &v);

    //--------------------------------------------------------------

    void calibrate(int x, int y);

    void find_blobs();

    //--------------------------------------------------------------

    ofVec3f windowToNorm(ofVec3f v);

    ofVec3f normToWindow(ofVec3f v);

    int modn(int a, int b);

    void restartOscSender();

};
