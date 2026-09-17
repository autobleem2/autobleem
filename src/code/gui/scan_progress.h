//
// SplashScanProgress: the scan's progress on the splash screen.
//
#pragma once

#include "../core/main.h"

//******************
// SplashScanProgress
//******************
// The scanning itself is ableem::GameScanner, which reports through a ScanProgressListener and has no idea
// what a screen is. This is the app's listener: each stage becomes a translated line on the splash, and a
// game that failed to verify stays up long enough to read. AutoBleem hands one to the GameScanner it makes.
class SplashScanProgress : public ScanProgressListener {
public:
    void onScanProgress(ScanStage stage, const std::string &detail, int done, int total) override;
};
