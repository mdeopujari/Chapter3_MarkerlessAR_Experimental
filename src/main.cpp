/*****************************************************************************
*   Markerless AR desktop application.
******************************************************************************
*   by Khvedchenia Ievgen, 5th Dec 2012
*   http://computer-vision-talks.com
******************************************************************************
*   Ch3 of the book "Mastering OpenCV with Practical Computer Vision Projects"
*   Copyright Packt Publishing 2012.
*   http://www.packtpub.com/cool-projects-with-opencv/book
*****************************************************************************/

////////////////////////////////////////////////////////////////////
// File includes:
#include "ARDrawingContext.hpp"
#include "ARPipeline.hpp"
#include "DebugHelpers.hpp"

////////////////////////////////////////////////////////////////////
// Standard includes:
//#include <windows.h>
#include <opencv2/opencv.hpp>
//MD: Socket includes
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#pragma comment(lib, "Ws2_32.lib")
///////////////////////////////////////////////////////////////////

using namespace std;
/**
 * Processes a recorded video or live view from web-camera and allows you to adjust homography refinement and 
 * reprojection threshold in runtime.
 */
void processVideo(const cv::Mat& patternImage, CameraCalibration& calibration, cv::VideoCapture& capture);

/**
 * Processes single image. The processing goes in a loop.
 * It allows you to control the detection process by adjusting homography refinement switch and 
 * reprojection threshold in runtime.
 */
void processSingleImage(const cv::Mat& patternImage, CameraCalibration& calibration, const cv::Mat& image);

/**
 * Performs full detection routine on camera frame and draws the scene using drawing context.
 * In addition, this function draw overlay with debug information on top of the AR window.
 * Returns true if processing loop should be stopped; otherwise - false.
 */
bool processFrame(const cv::Mat& cameraFrame, ARPipeline& pipeline, ARDrawingContext& drawingCtx);
bool processFrame2(const cv::Mat& cameraFrame, ARPipeline& pipeline, ARDrawingContext& drawingCtx,ARDrawingContext& drawingCtx2,SOCKET ListenSocket,char *recvbuf,int recvbuflen); //MD

int main(int argc, const char * argv[])
{

	
	
	//MD: CV part starts here
	// Change this calibration to yours:
    CameraCalibration calibration(526.58037684199849f, 524.65577209994706f, 318.41744018680112f, 202.96659047014398f);
    
    if (argc < 2)
    {
        std::cout << "Input image not specified" << std::endl;
        std::cout << "Usage: markerless_ar_demo <pattern image> [filepath to recorded video or image]" << std::endl;
        return 1;
    }

    // Try to read the pattern:
    cv::Mat patternImage = cv::imread(argv[1]);
    if (patternImage.empty())
    {
        std::cout << "Input image cannot be read" << std::endl;
        return 2;
    }

    if (argc == 2)
    {
       cv::VideoCapture cap = cv::VideoCapture(0);
		processVideo(patternImage, calibration,cap );
    }
    else if (argc == 3)
    {
        std::string input = argv[2];
        cv::Mat testImage = cv::imread(input);
        if (!testImage.empty())
        {
            processSingleImage(patternImage, calibration, testImage);
        }
        else 
        {
            cv::VideoCapture cap;
            if (cap.open(input))
            {
                processVideo(patternImage, calibration, cap);
            }
        }
    }
    else
    {
        std::cerr << "Invalid number of arguments passed" << std::endl;
        return 1;
    }

    return 0;
}

void processVideo(const cv::Mat& patternImage, CameraCalibration& calibration, cv::VideoCapture& capture)
{
// MD:Socket setup
WSADATA wsaData;
int iResult;

// Initialize Winsock
iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
if (iResult != 0) {
    printf("WSAStartup failed: %d\n", iResult);
   // return 1;
}

#define DEFAULT_PORT "12345"

struct addrinfo *result = NULL, *ptr = NULL, hints;

ZeroMemory(&hints, sizeof (hints));
hints.ai_family = AF_INET;
hints.ai_socktype =SOCK_DGRAM;// SOCK_STREAM;
hints.ai_protocol = IPPROTO_UDP;//IPPROTO_TCP;
hints.ai_flags = AI_PASSIVE;


// Resolve the local address and port to be used by the server
iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
if (iResult != 0) {
    printf("getaddrinfo failed: %d\n", iResult);
    WSACleanup();
   // return 1;
}


SOCKET ListenSocket = INVALID_SOCKET;
// Create a SOCKET for the server to listen for client connections

ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);

if (ListenSocket == INVALID_SOCKET) {
    printf("Error at socket(): %ld\n", WSAGetLastError());
    freeaddrinfo(result);
    WSACleanup();
  //  return 1;
}


// Setup the TCP listening socket
    iResult = bind( ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
//        return 1;
    }

	freeaddrinfo(result);
	
#define DEFAULT_BUFLEN 512

char recvbuf[DEFAULT_BUFLEN];
int iSendResult; //iResult
int recvbuflen = DEFAULT_BUFLEN;
		
	
	// Grab first frame to get the frame dimensions
    cv::Mat currentFrame;  
    capture >> currentFrame;

    // Check the capture succeeded:
    if (currentFrame.empty())
    {
        std::cout << "Cannot open video capture device" << std::endl;
        return;
    }

    cv::Size frameSize(currentFrame.cols, currentFrame.rows);

    ARPipeline pipeline(patternImage, calibration);
    ARDrawingContext drawingCtx("Markerless AR", frameSize, calibration);
	ARDrawingContext drawingCtx2("Markerless AR 2",frameSize,calibration);


    bool shouldQuit = false;
    do
    {
        capture >> currentFrame;
        if (currentFrame.empty())
        {
            shouldQuit = true;
            continue;
        }

        //shouldQuit = processFrame(currentFrame, pipeline, drawingCtx);
		shouldQuit = processFrame2(currentFrame, pipeline, drawingCtx,drawingCtx2,ListenSocket,recvbuf,recvbuflen);
    } while (!shouldQuit);
}

void processSingleImage(const cv::Mat& patternImage, CameraCalibration& calibration, const cv::Mat& image)
{
    cv::Size frameSize(image.cols, image.rows);
    ARPipeline pipeline(patternImage, calibration);
    ARDrawingContext drawingCtx("Markerless AR", frameSize, calibration);

    bool shouldQuit = false;
    do
    {
        shouldQuit = processFrame(image, pipeline, drawingCtx);
    } while (!shouldQuit);
}

bool processFrame(const cv::Mat& cameraFrame, ARPipeline& pipeline, ARDrawingContext& drawingCtx)
{
    // Clone image used for background (we will draw overlay on it)
    cv::Mat img = cameraFrame.clone();

    // Draw information:
    if (pipeline.m_patternDetector.enableHomographyRefinement)
        cv::putText(img, "Pose refinement: On   ('h' to switch off)", cv::Point(10,15), CV_FONT_HERSHEY_PLAIN, 1, CV_RGB(0,200,0));
    else
        cv::putText(img, "Pose refinement: Off  ('h' to switch on)",  cv::Point(10,15), CV_FONT_HERSHEY_PLAIN, 1, CV_RGB(0,200,0));

    cv::putText(img, "RANSAC threshold: " + ToString(pipeline.m_patternDetector.homographyReprojectionThreshold) + "( Use'-'/'+' to adjust)", cv::Point(10, 30), CV_FONT_HERSHEY_PLAIN, 1, CV_RGB(0,200,0));

    // Set a new camera frame:
    drawingCtx.updateBackground(img);

    // Find a pattern and update it's detection status:
    drawingCtx.isPatternPresent = pipeline.processFrame(cameraFrame);

    // Update a pattern pose:
    drawingCtx.patternPose = pipeline.getPatternLocation();

    // Request redraw of the window:
    drawingCtx.updateWindow();

    // Read the keyboard input:
    int keyCode = cv::waitKey(5); 

    bool shouldQuit = false;
    if (keyCode == '+' || keyCode == '=')
    {
        pipeline.m_patternDetector.homographyReprojectionThreshold += 0.2f;
#undef min
		pipeline.m_patternDetector.homographyReprojectionThreshold = std::min(10.0f, pipeline.m_patternDetector.homographyReprojectionThreshold);
		//pipeline.m_patternDetector.homographyReprojectionThreshold = min(10.0f, pipeline.m_patternDetector.homographyReprojectionThreshold); //[MD]
    }
    else if (keyCode == '-')
    {
        pipeline.m_patternDetector.homographyReprojectionThreshold -= 0.2f;
#undef max
		pipeline.m_patternDetector.homographyReprojectionThreshold = std::max(0.0f, pipeline.m_patternDetector.homographyReprojectionThreshold);
		//pipeline.m_patternDetector.homographyReprojectionThreshold = max(0.0f, pipeline.m_patternDetector.homographyReprojectionThreshold);//[MD]
    }
    else if (keyCode == 'h')
    {
        pipeline.m_patternDetector.enableHomographyRefinement = !pipeline.m_patternDetector.enableHomographyRefinement;
    }
    else if (keyCode == 27 || keyCode == 'q')
    {
        shouldQuit = true;
    }

    return shouldQuit;
}

bool processFrame2(const cv::Mat& cameraFrame, ARPipeline& pipeline, ARDrawingContext& drawingCtx,ARDrawingContext& drawingCtx2, SOCKET ListenSocket, char *recvbuf, int recvbuflen)
{
    // Clone image used for background (we will draw overlay on it)
    cv::Mat img = cameraFrame.clone();

    // Draw information:
    if (pipeline.m_patternDetector.enableHomographyRefinement)
        cv::putText(img, "Pose refinement: On   ('h' to switch off)", cv::Point(10,15), CV_FONT_HERSHEY_PLAIN, 1, CV_RGB(0,200,0));
    else
        cv::putText(img, "Pose refinement: Off  ('h' to switch on)",  cv::Point(10,15), CV_FONT_HERSHEY_PLAIN, 1, CV_RGB(0,200,0));

    cv::putText(img, "RANSAC threshold: " + ToString(pipeline.m_patternDetector.homographyReprojectionThreshold) + "( Use'-'/'+' to adjust)", cv::Point(10, 30), CV_FONT_HERSHEY_PLAIN, 1, CV_RGB(0,200,0));

    // Set a new camera frame:
    drawingCtx.updateBackground(img);
	drawingCtx2.updateBackground(img);

    // Find a pattern and update it's detection status:
    drawingCtx.isPatternPresent = pipeline.processFrame(cameraFrame); // MD: Compute Pose happens here
	drawingCtx2.isPatternPresent = drawingCtx.isPatternPresent;
    // Update a pattern pose:
    drawingCtx.patternPose = pipeline.getPatternLocation(); // Returns pose3D Tranformation
	//drawingCtx2.patternPose = MD: Put the pose estimate by Orientation sensors here!
    int iResult = recv(ListenSocket, recvbuf, recvbuflen, 0); // MD: This is a blocking call! Need to keep transmitting in continous mode from device.
    if (iResult > 0) {
        printf("Bytes received from UDP Socket: %d\n", iResult);
		recvbuf[iResult]='\0'; //MD: Making it a null-terminated string!
		//printf("%s\n",recvbuf);
       
    } else if (iResult == 0)
        printf("Connection closing...\n");
    else {
        printf("recv failed: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
       // return 1;
    }
	// Request redraw of the window:
    drawingCtx.updateWindow();
	drawingCtx2.updateWindow();
    // Read the keyboard input:
    int keyCode = cv::waitKey(5); 

    bool shouldQuit = false;
    if (keyCode == '+' || keyCode == '=')
    {
        pipeline.m_patternDetector.homographyReprojectionThreshold += 0.2f;
#undef min
		pipeline.m_patternDetector.homographyReprojectionThreshold = std::min(10.0f, pipeline.m_patternDetector.homographyReprojectionThreshold);
		//pipeline.m_patternDetector.homographyReprojectionThreshold = min(10.0f, pipeline.m_patternDetector.homographyReprojectionThreshold); //[MD]
    }
    else if (keyCode == '-')
    {
        pipeline.m_patternDetector.homographyReprojectionThreshold -= 0.2f;
#undef max
		pipeline.m_patternDetector.homographyReprojectionThreshold = std::max(0.0f, pipeline.m_patternDetector.homographyReprojectionThreshold);
		//pipeline.m_patternDetector.homographyReprojectionThreshold = max(0.0f, pipeline.m_patternDetector.homographyReprojectionThreshold);//[MD]
    }
    else if (keyCode == 'h')
    {
        pipeline.m_patternDetector.enableHomographyRefinement = !pipeline.m_patternDetector.enableHomographyRefinement;
    }
    else if (keyCode == 27 || keyCode == 'q')
    {
        shouldQuit = true;
    }

    return shouldQuit;
}
