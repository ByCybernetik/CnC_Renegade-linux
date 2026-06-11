#include "framgrab.h"
#include <cstring>

FrameGrabClass::FrameGrabClass(const char *filename, MODE mode, int, int, int, float framerate)
	: Filename(filename), FrameRate(framerate), Mode(mode), Counter(0), AVIFile(0), Bitmap(0), Stream(0)
{
	memset(&AVIStreamInfo, 0, sizeof(AVIStreamInfo));
	memset(&BitmapInfoHeader, 0, sizeof(BitmapInfoHeader));
}

FrameGrabClass::~FrameGrabClass() {}

void FrameGrabClass::CleanupAVI() {}

void FrameGrabClass::GrabAVI(void *) {}

void FrameGrabClass::GrabRawFrame(void *) {}

void FrameGrabClass::ConvertGrab(void *) {}

void FrameGrabClass::Grab(void *) {}

void FrameGrabClass::ConvertFrame(void *) {}
