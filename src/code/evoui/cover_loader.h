//
// CoverLoader: the carousel's covers decoded on a thread of their own.
//
#pragma once

#include <ableem/ui/texture.h>

#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

//******************
// CoverLoader
//******************
// A PNG decode is 10-40 ms on the console or a Pi - a frame or two lost every time a scroll brought a new
// cover in, when it ran on the render thread. This decodes the image files the carousel is about to need
// on a background thread (at the lowest priority), into ableem::Images; the carousel uploads a finished one
// on the render thread, which is cheap. Only the decode is here: the files' paths are worked out, and the
// cover composed, by the carousel.
class CoverLoader {
public:
    CoverLoader();
    ~CoverLoader();
    CoverLoader(const CoverLoader &) = delete;
    CoverLoader &operator=(const CoverLoader &) = delete;

    // the files the carousel wants now, the most urgent first: replaces what was still waiting; a file
    // being decoded or already decoded stays, and decoded ones no longer wanted are dropped
    void want(const std::vector<std::string> &paths);
    // takes a decoded image of `path` if there is one (an invalid Image if the file could not be read)
    bool take(const std::string &path, ableem::Image &out);
    // forgets everything waiting and decoded (the display is going away, or a new set of games)
    void clear();

private:
    void run();

    std::mutex guard;
    std::condition_variable wake;
    std::deque<std::string> queue;
    std::map<std::string, ableem::Image> ready;
    std::string inFlight;
    bool stopping = false;
    std::thread worker; // last: it starts in the constructor and uses everything above
};
