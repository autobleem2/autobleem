#include "DebugTimer.h"
#include <chrono>
#include <iostream>

using namespace std;

#ifndef NDEBUG  // if debug build
//*******************************
// DebugTimer::DebugTimer
//*******************************
// to use create a DebugTimer variable passing it the name of the function or other text.
// When the object goes out of scope it will output the time delay that has passed to cout.
//
DebugTimer::DebugTimer(const string & _description) : description(_description) {
    ticks_start = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now().time_since_epoch()).count();
    cout << description << ": start timer" << endl;
}

//*******************************
// DebugTimer::~DebugTimer
//*******************************
DebugTimer::~DebugTimer() {
    ticks_end = chrono::duration_cast<chrono::milliseconds>(chrono::steady_clock::now().time_since_epoch()).count();
	float time = float(ticks_end - ticks_start) / 1000.0;
    cout << description << ": " << time << " seconds" << endl;
    };
#endif
