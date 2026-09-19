//
// UpdateRomsApp: the program - AppBase (the stick's config.ini, theme and language, the Gui) and run(),
// which shows the one screen until the job is done and the window closed.
//
#pragma once

#include "app_base.h"
#include "core/update_roms_job.h"

//******************
// UpdateRomsApp
//******************
class UpdateRomsApp : public AppBase {
public:
    explicit UpdateRomsApp(const UpdateRomsJob::Setup &setup);
    int run();

private:
    UpdateRomsJob::Setup setup_;
};
