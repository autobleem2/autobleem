//
// CoverLoader: the carousel's covers decoded on a thread of their own.
//
#include "cover_loader.h"
#include "core/services/system.h"

#include <algorithm>
#include <set>

using namespace std;

//*******************************
// CoverLoader::CoverLoader / ~CoverLoader
//*******************************
CoverLoader::CoverLoader() : worker([this] { run(); }) {}

CoverLoader::~CoverLoader() {
    {
        lock_guard<mutex> lock(guard);
        stopping = true;
        queue.clear();
    }
    wake.notify_all();
    worker.join();
}

//*******************************
// CoverLoader::want
//*******************************
void CoverLoader::want(const vector<string> &paths) {
    {
        lock_guard<mutex> lock(guard);
        const set<string> wanted(paths.begin(), paths.end());
        for (auto it = ready.begin(); it != ready.end();) {
            if (wanted.count(it->first) == 0)
                it = ready.erase(it);
            else
                ++it;
        }
        queue.clear();
        for (const string &path : paths) {
            if (path == inFlight || ready.count(path) != 0 || find(queue.begin(), queue.end(), path) != queue.end())
                continue;
            queue.push_back(path);
        }
    }
    wake.notify_one();
}

//*******************************
// CoverLoader::take
//*******************************
bool CoverLoader::take(const string &path, ableem::Image &out) {
    lock_guard<mutex> lock(guard);
    auto it = ready.find(path);
    if (it == ready.end())
        return false;
    out = it->second;
    ready.erase(it);
    return true;
}

//*******************************
// CoverLoader::clear
//*******************************
void CoverLoader::clear() {
    lock_guard<mutex> lock(guard);
    queue.clear();
    ready.clear();
    inFlight.clear(); // what is being decoded now is dropped when it finishes
}

//*******************************
// CoverLoader::run
//*******************************
void CoverLoader::run() {
    System::lowerCurrentThreadPriority();
    unique_lock<mutex> lock(guard);
    for (;;) {
        wake.wait(lock, [this] { return stopping || !queue.empty(); });
        if (stopping)
            return;
        string path = queue.front();
        queue.pop_front();
        inFlight = path;
        lock.unlock();
        ableem::Image image = ableem::Image::loadFile(path);
        lock.lock();
        if (inFlight == path) // not cleared while it was decoding
            ready[path] = image;
        inFlight.clear();
    }
}
