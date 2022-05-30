#pragma once

void initFrameCapture();

bool isFrameCaptureSupported();

void beginFrameCapture();
void endFrameCapture();

class FrameCaptureScope
{
public:
	FrameCaptureScope() { beginFrameCapture(); }
	FrameCaptureScope(const FrameCaptureScope&) = delete;
	FrameCaptureScope(FrameCaptureScope&&) = delete;
	~FrameCaptureScope() { endFrameCapture(); }

	FrameCaptureScope& operator=(const FrameCaptureScope&) = delete;
	FrameCaptureScope& operator=(FrameCaptureScope&&) = delete;
};