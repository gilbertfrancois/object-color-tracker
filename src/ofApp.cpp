//
// Created by G.F. Duivesteijn on 09/06/2018.
//

#include "ofApp.h"

#define clamp255(x) (x > 255 ? 255 : x)
#define clamp0(x)   (x < 0   ? 0   : x)

//--------------------------------------------------------------
void ofApp::setup() {

    setupCamera();

    setupImageBuffers();

    setupGui();

    setupRingBuffer();

    sender.setup(server.get(), std::stoi(port.get()));

    ofSetFrameRate(static_cast<int>(fps.get()));
    ofSetVerticalSync(true);
}

void ofApp::update() {

    if (videoGrabber == nullptr) {
        return;
    }

    // read next frame from camera
    videoGrabber->update();

    if (videoGrabber->isFrameNew()) {

        updateFilterMasks();

        // Find contours of colour patches within calibrated range
        cv::findContours(ftr, allContours, contourFindingMode, simplifyMode);

        // Compute center and area of all closed contours,
        // updates moments (mu), mass centers (mc), max_area and argmax_area.
        find_blobs();

        // Update ring buffer position
        buffer_position = (buffer_position + 1) % buffer_size;

        // Update object location, if applicable
        updateObjectLocation();

        sendOscMessage();
    }
}

void ofApp::draw() {

    // Draw contours
    if (show_contours.get()) {
        cv::drawContours(rgb, allContours, -1, cv::Scalar::all(255));
    }

    // Draw webcam feed
    if (show_webcam_view.get()) {
        ofxCv::drawMat(rgb, 0, 0);
    }

    // Draw Settings Panel
    if (showGui) {
        gui.draw();
    }

    // Show only the largest blob or all blobs within range of area from settings.
    drawObjectCursor();

    // Draw trail from object
    if (one_blob_only.get() && show_trail.get()) {
        drawTrail();
    }

    // Draw mouse cursor with calibration patch size
    drawMouseCursor();

    if (oscMessageSent) {
        drawStatusMessage(buffer.at(static_cast<unsigned long>(buffer_position)));
    }

    if (show_help.get()) {
        drawHelpPanel();
    }
}

//--------------------------------------------------------------

void ofApp::setupCamera() {
    videoGrabber = new ofVideoGrabber;
    video_device_list = videoGrabber->listDevices();
    for (int i = 0; i < video_device_list.size(); i++) {
        if (video_device_list[i].bAvailable) {
            ofLogNotice() << "Device: " << video_device_list[i].id << ": " << video_device_list[i].deviceName;
        } else {
            ofLogNotice() << "Device: " << video_device_list[i].id << ": " << video_device_list[i].deviceName
                          << " - unavailable ";
        }
    }
    videoGrabber->setDeviceID(0);
    videoGrabber->setup(camWidth, camHeight);
}

void ofApp::setupRingBuffer() {
    for (size_t i = 0; i < buffer_size; i++) {
        buffer.push_back(ofVec3f(-1, -1));
    }
}

void ofApp::setupImageBuffers() {
    rgb = cv::Mat(camWidth, camHeight, CV_8UC3);
    rgbm = cv::Mat(camWidth, camHeight, CV_8UC3);
    hsb = cv::Mat(camWidth, camHeight, CV_8UC3);
    h = cv::Mat(camWidth, camHeight, CV_8UC1);
    s = cv::Mat(camWidth, camHeight, CV_8UC1);
    b = cv::Mat(camWidth, camHeight, CV_8UC1);
    maskH = cv::Mat(camWidth, camHeight, CV_8UC1);
    maskS = cv::Mat(camWidth, camHeight, CV_8UC1);
    maskV = cv::Mat(camWidth, camHeight, CV_8UC1);
    ftr = cv::Mat(camWidth, camHeight, CV_8UC1);
}

void ofApp::setupGui() {

    port.set("Port", "6448");
    fps.set("FPS", 30, 0, 60);
    current_camera_device_id.set("Camera ID", 0, 0, static_cast<int>(video_device_list.size()));

    server.addListener(this, &ofApp::serverChanged);
    port.addListener(this, &ofApp::portChanged);
    msg.addListener(this, &ofApp::msgChanged);
    fps.addListener(this, &ofApp::fpsChanged);
    current_camera_device_id.addListener(this, &ofApp::cameraDeviceIdChanged);

    color_settings_group.setName("Calibration");
    color_settings_group.add(tol_h.set("Tolerance Hue", 2, 0, 20));
    color_settings_group.add(tol_s.set("Tolerance Saturation", 15, 0, 100));
    color_settings_group.add(tol_v.set("Tolerance Value", 40, 0, 100));
    color_settings_group.add(min_area_size.set("Minimum area size", 200, 10, 10000));
    color_settings_group.add(max_area_size.set("Maximum area size",
            static_cast<unsigned long> (0.25 * ofGetWindowWidth() * ofGetWindowHeight()),
            1000, static_cast<unsigned long>(0.5 * ofGetWindowWidth() * ofGetWindowHeight())));
    color_settings_group.add(sample_radius.set("Sample radius", 12, 1, 20));
    one_blob_only.set("Only largest blob", true);
    color_settings_group.add(lpf.set("Low pass filter", true));

    comm_settings_group.setName("Communication");
    comm_settings_group.add(server.set("Server", "localhost"));
    comm_settings_group.add(port.set("Port", "6448"));
    comm_settings_group.add(msg.set("Message", "/wek/inputs"));

    display_settings_group.setName("Display");
    display_settings_group.add(show_webcam_view.set("Show camera preview", true));
    display_settings_group.add(fps.set("FPS", 30, 0, 60));
    display_settings_group.add(show_contours.set("Show contours", true));
    display_settings_group.add(show_trail.set("Show trail", true));
    display_settings_group.add(show_help.set("Show help", false));

    camera_group.setName("Camera info");
    camera_group.add(current_camera_device_name.set("", video_device_list[current_camera_device_id.get()].deviceName));

    gui.setup("Control Center");
    gui.setDefaultBackgroundColor(ofColor(0, 0, 0, 16));
    gui.setDefaultFillColor(ofColor(0, 0, 0, 64));
    gui.setDefaultTextColor(ofColor(255, 255, 255, 255));
    gui.add(color_settings_group);
    gui.add(display_settings_group);
    gui.add(camera_group);
    gui.add(comm_settings_group);
    gui.minimizeAll();
}

//--------------------------------------------------------------

void ofApp::updateObjectLocation() {
    if (allContours.size() > 0 &&                       // only look for object position if contours have been found
            one_blob_only.get() &&                      // only save position in buffer if user wants one blob
            argmax_area >= 0 &&                         // prevent out-of-bounds exception
            argmax_area < allContours.size() &&         // prevent out-of-bounds exception
            max_area > min_area_size.get() &&           // only store object position if the blob is in range of
            max_area < max_area_size.get())             //      area from settings panel
    {
        updateNormalizedObjectLocationInBuffer(windowToNorm(ofVec3f(mc[argmax_area].x, mc[argmax_area].y, max_area)));

    } else {
        pos = ofVec3f(-1, -1, -1);
        vel = ofVec3f(0.0f);
        acc = ofVec3f(0.0f);
        buffer.at(static_cast<unsigned long>(buffer_position)) = pos;
    }
}

void ofApp::updateFilterMasks() {
    rgbm = ofxCv::toCv(*videoGrabber);

    // Mirror camera image
    cv::flip(rgbm, rgb, 1);
    cv::cvtColor(rgb, hsb, cv::COLOR_RGB2HSV_FULL);

    // Add a bit of blur to eliminate camera noise.
    cv::blur(hsb, hsb, cv::Size(10, 10));

    // Split the hsv image per channel into grayscale images
    cv::Mat tmp[3];
    cv::split(hsb, tmp);
    h = tmp[0];
    s = tmp[1];
    b = tmp[2];

    // Get the latest tolerance values set by the user
    updateHSVRange();

    cv::inRange(h, Hmin_tol, Hmax_tol, maskH);
    cv::inRange(s, Smin_tol, Smax_tol, maskS);
    cv::inRange(b, Vmin_tol, Vmax_tol, maskV);

    // Add all 3 masks with binary AND operation
    ftr = maskH & maskS & maskV;

    // Fill holes
    cv::Mat st_elem = getStructuringElement(cv::MORPH_ELLIPSE, cv::Size(5, 5));
    cv::erode(ftr, ftr, st_elem);
    cv::dilate(ftr, ftr, st_elem);
    cv::dilate(ftr, ftr, st_elem);
    cv::erode(ftr, ftr, st_elem);
}

void ofApp::updateHSVRange() {

    // Augments the tolerance from the settings panel to the measured min/max values.
    Hmin_tol = clamp0(Hmin - tol_h.get());
    Hmax_tol = clamp255(Hmax + tol_h.get());
    Smin_tol = clamp0  (Smin - tol_s.get());
    Smax_tol = clamp255(Smax + tol_s.get());
    Vmin_tol = clamp0(Vmin - tol_v.get());
    Vmax_tol = clamp255(Vmax + tol_v.get());
}

void ofApp::updateNormalizedObjectLocationInBuffer(ofVec3f v) {

    int im1 = modn(buffer_position - 1, buffer_size);
    int im2 = modn(buffer_position - 2, buffer_size);
    ofVec3f vm1 = buffer.at(static_cast<unsigned long>(im1));
    ofVec3f vm2 = buffer.at(static_cast<unsigned long>(im2));
    if (vm1.x == -1) {
        vm1 = v;
    }
    if (vm2.x == -1) {
        vm2 = vm1;
    }
    if (lpf.get()) {
        v = (3 * v + 2 * vm1 + vm2) / 6.0f;

    }
    buffer.at(static_cast<unsigned long>(buffer_position)) = v;

    // Set the time delta relative to 30fps instead of actual time delta to prevent underflow of v and a.
    float dt = ofGetFrameRate() / 30.0f;
    pos = v;
    vel = (v - vm1) / dt;
    // compute acceleration at t-1, because we do not have a value at t+1 yet.
    acc = (vm2 - 2 * vm1 + v) / (dt * dt);
}

//--------------------------------------------------------------

void ofApp::sendOscMessage() {
    if (pos.x != -1) {
        ofxOscMessage m;
        m.setAddress(string(msg.get()));

        // position
        m.addFloatArg(pos.x);
        m.addFloatArg(pos.y);
        m.addFloatArg(pos.z);

        // velocity
        m.addFloatArg(vel.x);
        m.addFloatArg(vel.y);
        m.addFloatArg(vel.z);

        // accelleration
        m.addFloatArg(acc.x);
        m.addFloatArg(acc.y);
        m.addFloatArg(acc.z);

        sender.sendMessage(m, false);
        oscMessageSent = true;
    } else {
        oscMessageSent = false;
    }
}

//--------------------------------------------------------------

void ofApp::drawObjectCursor() {
    if (allContours.size() > 0) {
        if (one_blob_only.get() && argmax_area != -1) {
            if (max_area > min_area_size.get() && max_area < max_area_size.get()) {
                drawObjectCursorAtPosition(buffer.at(static_cast<unsigned long>(buffer_position)));
            }
        } else {
            buffer.at(static_cast<unsigned long>(buffer_position)) = ofVec3f(-1, -1);
            for (int i = 0; i < allContours.size(); i++) {
                if (mu[i].m00 > min_area_size.get() && mu[i].m00 < max_area_size.get()) {
                    drawObjectCursorAtPosition(ofVec3f(mc[i].x, mc[i].y));
                }
            }
        }
    }
}

void ofApp::drawObjectCursorAtPosition(ofVec3f v) {
    ofVec3f vs = normToWindow(v);
    ofPushStyle();
    ofFill();
    ofSetColor(0, 0, 0);
    ofDrawCircle(vs.x, vs.y, ofGetWindowWidth() / 100.0f);
    ofSetColor(255, 255, 255);
    ofDrawCircle(vs.x, vs.y, ofGetWindowWidth() / 150.0f);
    ofPopStyle();
}

void ofApp::drawMouseCursor() const {
    ofPushStyle();
    ofFill();
    ofSetColor(255, 255, 255, 64);
    ofDrawCircle(ofGetMouseX(), ofGetMouseY(), sample_radius);
    ofNoFill();
    ofSetColor(255, 255, 255, 255);
    ofDrawCircle(ofGetMouseX(), ofGetMouseY(), sample_radius);
    ofPopStyle();
}

void ofApp::drawTrail() {

    int j = 0;
    ofPushStyle();
    ofFill();
    for (int i = buffer_position + buffer_size; i > buffer_position; i--) {

        const unsigned long ii = static_cast<unsigned long>(modn(i, buffer_size));
        if (buffer.at(ii).x != -1.0) {
            const ofVec3f v = normToWindow(buffer.at(ii));
            const float radius = sqrt(v.z) * (buffer_size - j) / (buffer_size);
            ofSetColor(255, 255, 255, 32 * (buffer_size - j) / buffer_size);
            ofDrawCircle(v.x, v.y, radius);
        }
        j++;
    }
    ofPopStyle();
}

void ofApp::drawStatusMessage(ofVec3f v) {
    ofPushStyle();
    ofFill();
    ofSetColor(255, 255, 255, 200);
    std::string buf =
            "Sending message " + string(msg.get()) +
                    " to " + string(server.get()) +
                    " on port " + ofToString(port.get()) + " with 9 inputs.";
    ofDrawBitmapString(buf, 10, ofGetWindowHeight() - 55);
    buf = " x = " + ofToString(pos.x, 5, 8, ' ') + ",  y = " + ofToString(pos.y, 5, 8, ' ') + ",  z = " + ofToString(pos.z, 5, 8, ' ');
    ofDrawBitmapString(buf, 10, ofGetWindowHeight() - 40);
    buf = "vx = " + ofToString(vel.x, 5, 8, ' ') + ", vy = " + ofToString(vel.y, 5, 8, ' ') + ", vz = " + ofToString(vel.z, 5, 8, ' ');
    ofDrawBitmapString(buf, 10, ofGetWindowHeight() - 25);
    buf = "ax = " + ofToString(acc.x, 5, 8, ' ') + ", ay = " + ofToString(acc.y, 5, 8, ' ') + ", az = " + ofToString(acc.z, 5, 8, ' ');
    ofDrawBitmapString(buf, 10, ofGetWindowHeight() - 10);


    ofPopStyle();
}

void ofApp::drawHelpPanel() {

    int offset = ofGetWindowWidth() / 7;
    int offset2 = offset * 2;
    int line_height = 15;
    int offset_y = (ofGetWindowHeight() - 10 * line_height) / 2;

    ofPushStyle();
    ofSetColor(0, 0, 0, 128);
    ofDrawRectangle(offset, offset, ofGetWindowWidth() - 2 * offset, ofGetWindowHeight() - 2 * offset);

    ofSetColor(255);
    ofDrawBitmapString("Keyboard shortcuts", ofVec2f(offset2, offset_y));
    ofDrawBitmapString("------------------", ofVec2f(offset2, offset_y + 1 * line_height));
    ofDrawBitmapString("", ofVec2f(offset2, offset_y + 2 * line_height));
    ofDrawBitmapString("[ 1 ] Webcam view", ofVec2f(offset2, offset_y + 3 * line_height));
    ofDrawBitmapString("[ 2 ] Contours", ofVec2f(offset2, offset_y + 4 * line_height));
    ofDrawBitmapString("[ 3 ] Object trail", ofVec2f(offset2, offset_y + 5 * line_height));
    ofDrawBitmapString("[ s ] Setup panel", ofVec2f(offset2, offset_y + 6 * line_height));
    ofDrawBitmapString("[ c ] Cycle though available cameras", ofVec2f(offset2, offset_y + 7 * line_height));
    ofDrawBitmapString("[ h ] Help", ofVec2f(offset2, offset_y + 8 * line_height));
    ofDrawBitmapString("Author:  Gilbert Francois Duivesteijn", ofVec2f(offset2, ofGetWindowHeight() - offset - 3 * line_height));
    ofDrawBitmapString("License: GPLv2", ofVec2f(offset2, ofGetWindowHeight() - offset - 2 * line_height));


    ofPopStyle();
}


//--------------------------------------------------------------

void ofApp::keyPressed(int key) {
    int new_id = 0;
    const int device_list_size = static_cast<int>(video_device_list.size());
    switch (key) {
        case '1':
            show_webcam_view.set(!show_webcam_view.get());
            break;
        case '2':
            show_contours.set(!show_contours.get());
            break;
        case '3':
            show_trail.set(!show_trail.get());
            break;
        case 's':
            showGui = !showGui;
            break;
        case 'c':
            new_id = current_camera_device_id.get() + 1 >= device_list_size ? 0 : current_camera_device_id.get() + 1;
            current_camera_device_id.set(new_id);
            break;
        case 'h':
            show_help.set(!show_help.get());
            showGui = !show_help.get();
            break;
        default:
            break;
    }
}

void ofApp::keyReleased(int key) {

}

void ofApp::mouseMoved(int x, int y) {
    mouseX = x;
    mouseY = y;
}

void ofApp::mouseDragged(int x, int y, int button) {

}

void ofApp::mousePressed(int x, int y, int button) {
    calibrate(x, y);
}

void ofApp::mouseReleased(int x, int y, int button) {

}

void ofApp::mouseEntered(int x, int y) {

}

void ofApp::mouseExited(int x, int y) {

}

void ofApp::windowResized(int w, int h) {

}

void ofApp::gotMessage(ofMessage msg) {

}

void ofApp::dragEvent(ofDragInfo dragInfo) {

}

void ofApp::restartOscSender() {
    sender.clear();
    sender.setup(server.get(), std::stoi(port.get()));
};

void ofApp::serverChanged(std::string &v) {
    restartOscSender();
    ofLog(OF_LOG_NOTICE, "Server changed to " + server.get());
}

void ofApp::portChanged(std::string &v) {
    restartOscSender();
    ofLog(OF_LOG_NOTICE, "Port changed to " + port.get());
};

void ofApp::msgChanged(std::string &v) {
    restartOscSender();
    ofLog(OF_LOG_NOTICE, "Message changed to " + msg.get());
};

void ofApp::fpsChanged(int &v) {
    ofLog(OF_LOG_NOTICE, "FPS value set to " + std::to_string(v) + ".");
    ofSetFrameRate(v);
}

void ofApp::cameraDeviceIdChanged(int &v) {
    if (videoGrabber != nullptr) {
        videoGrabber->close();
        delete videoGrabber;
    }
    videoGrabber = new ofVideoGrabber;
    if (current_camera_device_id.get() >= video_device_list.size()) {
        current_camera_device_id.set(static_cast<int>(video_device_list.size() - 1));
    }
    ofLog(OF_LOG_NOTICE, "Camera ID value set to " + std::to_string(v) + ".");
    videoGrabber->setDeviceID(current_camera_device_id.get());
    videoGrabber->setup(camWidth, camHeight);
    current_camera_device_name.set("", video_device_list[current_camera_device_id.get()].deviceName);
}

//--------------------------------------------------------------

void ofApp::calibrate(int x, int y) {

    Hmin = 255;
    Hmax = 0;
    Smin = 255;
    Smax = 0;
    Vmin = 255;
    Vmax = 0;

    int orgX = static_cast<int>(round(((double) ofGetWindowWidth() / 2.0) - ((double) camWidth / 2.0)));
    int orgY = static_cast<int>(round(((double) ofGetWindowHeight() / 2.0) - ((double) camHeight / 2.0)));

    int imin = -sample_radius.get();
    int imax = sample_radius.get();


    for (int i = imin; i <= imax; i += 1) {
        for (int j = imin; j <= imax; j += 1) {
            int mx = (x + i - orgX) % camWidth;
            int my = (y + j - orgY) % camHeight;
            int h_i = static_cast<int>(h.data[my * camWidth + mx]);
            int s_i = static_cast<int>(s.data[my * camWidth + mx]);
            int v_i = static_cast<int>(b.data[my * camWidth + mx]);

            Hmin = Hmin > h_i ? h_i : Hmin;
            Hmax = Hmax < h_i ? h_i : Hmax;
            Smin = Smin > s_i ? s_i : Smin;
            Smax = Smax < s_i ? s_i : Smax;
            Vmin = Vmin > v_i ? v_i : Vmin;
            Vmax = Vmax < v_i ? v_i : Vmax;
        }
    }

    updateHSVRange();

    ofLog(OF_LOG_NOTICE, "Calibrated values:");
    ofLog(OF_LOG_NOTICE, "H = [" + std::to_string(Hmin) + ", " + std::to_string(Hmax) + "]");
    ofLog(OF_LOG_NOTICE, "S = [" + std::to_string(Smin) + ", " + std::to_string(Smax) + "]");
    ofLog(OF_LOG_NOTICE, "V = [" + std::to_string(Vmin) + ", " + std::to_string(Vmax) + "]");
    ofLog(OF_LOG_NOTICE, "r = " + std::to_string(sample_radius.get()));
}

/**
 * Finds closed contours (blobs) and updates moments (mu), mass centers (mc), max_area and argmax_area.
 */
void ofApp::find_blobs() {

    max_area = 0.0;
    argmax_area = -1;

    // Compute the moments of the closed contours
    mu.clear();
    mu.resize(allContours.size());
    for (int i = 0; i < allContours.size(); i++) {
        mu[i] = moments(allContours[i], false);

        // Find the largest blob, mu[i].m00 is the area of the blob
        max_area = max_area > mu[i].m00 ? max_area : static_cast<float>(mu[i].m00);

        // Find the index of the largest blob
        argmax_area = max_area > mu[i].m00 ? argmax_area : i;
    }

    //  Compute the mass centers
    mc.clear();
    mc.resize(allContours.size());
    for (int i = 0; i < allContours.size(); i++) {
        mc[i] = cv::Point2f(static_cast<float>(mu[i].m10 / mu[i].m00), static_cast<float>(mu[i].m01 / mu[i].m00));
    }
}

//--------------------------------------------------------------

ofVec3f ofApp::normToWindow(ofVec3f v) {
    float x = ofMap(v.x, 0.0f, 1.0f, 0, ofGetWindowWidth());
    float y = ofMap(v.y, 0.0f, 1.0f, 0, ofGetWindowHeight());
    float z = ofMap(v.z, 0.0f, 1.0f, 0, ofGetWindowWidth() * ofGetWindowHeight());
    return ofVec3f(x, y, z);
}

ofVec3f ofApp::windowToNorm(ofVec3f v) {
    float x = ofMap(v.x, 0, ofGetWindowWidth(), 0.0f, 1.0f);
    float y = ofMap(v.y, 0, ofGetWindowHeight(), 0.0f, 1.0f);
    float z = ofMap(v.z, 0, ofGetWindowWidth() * ofGetWindowHeight(), 0.0f, 1.0f);

    return ofVec3f(x, y, z);
}

int ofApp::modn(int a, int b) {
    // in Python a%b works for negative numbers
    // in C++ use: (b + (a%b)) % b
    // https://stackoverflow.com/questions/7594508/modulo-operator-with-negative-values
    return (b + (a % b)) % b;
}

