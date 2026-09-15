//
// Created by screemer on 2018-12-15.
//
#pragma once

#include "../main.h"
#include <memory>

//******************
// Scanner
//******************
// The scanning itself is ableem::GameScanner. This app-side singleton supplies what the library deliberately
// does not have: the splash-screen progress (translated) and the "do we need to scan at all" flag main.cpp
// maintains.
class Scanner : public GameScanner, private ScanProgressListener {
public:
    bool forceScan = false;

    Scanner(Scanner const &) = delete;
    Scanner &operator=(Scanner const &) = delete;

    static std::shared_ptr<Scanner> getInstance() {
        static std::shared_ptr<Scanner> s{new Scanner};
        return s;
    }

private:
    Scanner() : GameScanner(this) {}
    void onScanProgress(ScanStage stage, const std::string &detail) override;
};
