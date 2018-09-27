# Object Color Tracker



## About

This app serves as an input feature extractor for [Wekinator](http://www.wekinator.org) and allows you to use it as input device to play music, paint in the air, track people/objects on stage performance or in general, be very expressive without touching a computer. 

When choosing an object (or clothes) with distinct colour (e.g. bright green), you can calibrate the colour range by holding the object in front of the camera, clicking with the mouse on the object. Then the object will be tracked. The center of the object will act as pointer with 3 axis, *x*, *y* and *z*. The *x* and *y* values denote the position in the window, the *z* value denotes the area of the object with respect to the window size and allows you to be more expressive with the output app. 

**Note**: Only the largest detected object (blob) is used as "cursor". Other, smaller objects within calibrated colour range are ignored.

*Click on the image below to see a small demo (youtube).*

[![Demo video](https://img.youtube.com/vi/rXNgobgluP0/0.jpg)](http://www.youtube.com/watch?v=rXNgobgluP0)



## Calibration Settings

- Tolerance hue / saturation / value: Allowed range outside calibration patch. Fiddle with these values until you have isolated the object from the rest of the camera feed. 
- Minimum area size: If the area inside the contour is less than this value, the detection is ignored. This helps to suppress noise.
- Maximum area size: If the area inside the contour is greater than this value, the detection is ignored.
- Sample radius: The patch size for measuring the colour range.
- Low pass filter: Weak filter that adds a little bit of smoothing. Disable this feature if you think there is too much lag.



## Prebuild Binary

A ready-to-run binary is available for macOS and can be downloaded from the [releases](https://github.com/gilbertfrancois/object-color-tracker/releases) page.



## Compile

The app is build in C++ using [openFrameworks 0.10.0](http://www.openframeworks.cc) and uses the following addons:

- ofxOSC
- ofxGui
- ofxOpenCV
- ofxCv

Note that ofxCv is not standard packed in openFrameworks and needs to be downloaded separately from https://github.com/kylemcdonald/ofxCv. Choose the *master* branch for oF 0.10.0 and _stable_ branch if you're using oF 0.9.8. On macOS, use the ProjectGenerator to set the paths correctly and open the project with XCode. 



## Known Issues

- The app does not work well in a scene with strong directional light. When e.g. the light comes only from one side, the dynamic range of the object will be too large and the app might loose track of the object when moving around in front of the camera.

- Some cameras have quite aggressive white balance corrections. When for instance moving a green object in front of the camera, on some cameras the colour jumps continuously from too yellow to too blue and everything in between. This is very annoying since it is hard to calibrate properly so that the object is correctly detected in every white balance setting. Some fancy external cameras have "advanced features" which allows you to _freeze_ the white balance. 

  If you compile the code yourself and use macOS, you can try the following change in openFrameworks library. Edit file `[of_folder]/libs/openframeworks/video/ofAVFoundationGrabber.mm` and replace the function `-(void) startCapture ` with:


```objective-c
-(void) startCapture{

	[self.captureSession startRunning];

	[captureInput.device lockForConfiguration:nil];

    if( [captureInput.device isFocusModeSupported:AVCaptureFocusModeAutoFocus] ) {
        [captureInput.device setFocusMode:AVCaptureFocusModeAutoFocus ];
    }
    if( [captureInput.device isExposureModeSupported:AVCaptureExposureModeLocked] ) {
        NSLog(@"-- Attempt to lock exposure");
        [captureInput.device setExposureMode:AVCaptureExposureModeLocked ];
    }
    if ([captureInput.device isWhiteBalanceModeSupported:AVCaptureWhiteBalanceModeLocked]) {
        NSLog(@"-- Attempt to lock white balance");
        [captureInput.device setWhiteBalanceMode:AVCaptureWhiteBalanceModeLocked];
    }
}

```



## Upgrading OpenCV (optional)

At the time of writing, the ofxOpenCv add-on is shipped with OpenCV version 3.1.0. If you want to run with the latest version of OpenCv you can upgrade the static library and header files in ofxOpenCv by doing these 3 steps (macOS):

1. Install the latest version of OpenCV with `brew install opencv`. 

2. Run the following shell commands:

```bash
$ cp -R $OPENCV_DIR/include/*  $OPENFRAMEWORKS_DIR/addons/ofxOpenCv/libs/opencv/include/
```
```bash
$ libtool -static $OPENCV_DIR"/lib/lib*.a" -o $OPENFRAMEWORKS_DIR/addons/ofxOpenCv/libs/opencv/lib/osx/opencv.a
```

where `$OPENCV_DIR` points to the installation directory of OpenCV.

3. Add `OpenCL.framework` to the section `Linked Frameworks and Libraries`. 

Sorry, I don't know how to do it on Linux or Windows.



## License

The object color tracker is created by Gilbert François Duivesteijn and distributed under GPL v2 license. The library openFrameworks is distributed under the MIT License.



