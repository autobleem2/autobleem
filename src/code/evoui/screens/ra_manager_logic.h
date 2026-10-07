//
// The RetroArch manager's decisions and texts (System menu -> RetroArch..., GuiRaManager / GuiRaJobProgress), pure:
// which menu rows show, which panel rows there are and whether they are greyed, the state lines, the progress panel's
// title, detail and bar, and the result. tests/screens/test_ra_manager_logic.cpp holds them; the screens only draw.
//
#pragma once

#include "core/main.h"
#include "core/services/ra_job_service.h"
#include "human_bytes.h"

#include <algorithm>
#include <string>
#include <vector>

namespace RaManager {

//*** the menus

// System menu -> "RetroArch...": wherever the platform has a runner for it (retroarch_job_command)
inline bool systemMenuRowShown(bool jobSupported) {
    return jobSupported;
}

// the Quick menu's "Install RetroArch...": only while the program is absent (where "RetroArch cores" is hidden)
inline bool quickMenuInstallRowShown(bool jobSupported, bool retroArchInstalled) {
    return jobSupported && !retroArchInstalled;
}

//*** the panel

enum class RowKind { Install, Update, Remove, RemoveBios };

struct Row {
    RowKind kind = RowKind::Install;
    bool enabled = true; // false: drawn greyed, Cross does nothing
    std::string title;
    std::string description;
};

inline RaJobService::Action actionFor(RowKind kind) {
    switch (kind) {
    case RowKind::Install:
        return RaJobService::Action::Install;
    case RowKind::Update:
        return RaJobService::Action::Update;
    case RowKind::Remove:
        return RaJobService::Action::Remove;
    default:
        return RaJobService::Action::RemoveWithBios;
    }
}

// what the panel offers for what is installed. Install and Update download, so they need a network; Remove does not.
// Update only when the catalog has a version other than the installed one. Not installed: just Install. The row that
// also deletes the BIOS files is a row of its own with its own confirmation (the caller asks).
inline std::vector<Row> panelRows(const RaJobService::Inspection &state) {
    std::vector<Row> rows;
    if (!state.ready)
        return rows;
    if (!state.installed) {
        Row install;
        install.kind = RowKind::Install;
        install.title = _("Install");
        install.enabled = state.networkUp;
        install.description =
            state.networkUp ? _("Download RetroArch with its cores and apps") : _("Needs a network connection");
        rows.push_back(install);
        return rows;
    }
    if (state.updateAvailable()) {
        Row update;
        update.kind = RowKind::Update;
        update.title = _("Update");
        update.enabled = state.networkUp;
        update.description =
            state.networkUp ? _("Update to") + " " + state.latestVersion : _("Needs a network connection");
        rows.push_back(update);
    }
    Row remove;
    remove.kind = RowKind::Remove;
    remove.title = _("Remove");
    remove.enabled = !state.systemPackage;
    remove.description = state.systemPackage ? _("Installed by the system - it cannot be removed here")
                                             : _("Delete RetroArch and its cores - games, saves and BIOS files stay");
    rows.push_back(remove);
    Row bios;
    bios.kind = RowKind::RemoveBios;
    bios.title = _("Also delete the BIOS files");
    bios.enabled = !state.systemPackage;
    bios.description = state.systemPackage ? remove.description : _("Remove RetroArch and the BIOS files too");
    rows.push_back(bios);
    return rows;
}

// the panel's state lines under its title: what is installed (version, size on the stick), then the news
inline std::vector<std::string> stateLines(const RaJobService::Inspection &state) {
    std::vector<std::string> lines;
    if (!state.ready) {
        lines.push_back(_("Checking..."));
        return lines;
    }
    if (!state.installed) {
        lines.push_back(_("Not installed"));
    } else {
        std::string line = _("Installed");
        if (!state.version.empty())
            line += " " + state.version;
        if (state.sizeBytes > 0)
            line += "  -  " + humanBytes(state.sizeBytes);
        lines.push_back(line);
    }
    if (!state.networkUp)
        lines.push_back(_("Needs a network connection"));
    else if (state.updateAvailable())
        lines.push_back(_("Update available") + ": " + state.latestVersion);
    else if (state.installed && !state.version.empty() && state.version == state.latestVersion)
        lines.push_back(_("Up to date"));
    return lines;
}

//*** the question before a job

inline std::string confirmQuestion(RowKind kind) {
    switch (kind) {
    case RowKind::Install:
        return _("Install RetroArch? It is downloaded from the AutoBleem site - this can take a few minutes.");
    case RowKind::Update:
        return _("Update RetroArch? The new version is downloaded from the AutoBleem site.");
    case RowKind::Remove:
        return _("Remove RetroArch? Your games, saves and BIOS files stay.");
    default:
        return _("Remove RetroArch and delete the BIOS files too? Cores that need them will not start until the "
                 "files are put back.");
    }
}

//*** the progress panel

// a phase title the runner writes (ra_job_service.h), in the language; any other text is shown as it is
inline std::string phaseTitle(const std::string &title) {
    if (title == "Preparing")
        return _("Preparing");
    if (title == "Downloading RetroArch")
        return _("Downloading RetroArch");
    if (title == "Installing RetroArch")
        return _("Installing RetroArch");
    if (title == "Downloading cores")
        return _("Downloading cores");
    if (title == "Installing cores")
        return _("Installing cores");
    if (title == "Downloading libraries and apps")
        return _("Downloading libraries and apps");
    if (title == "Downloading BIOS files")
        return _("Downloading BIOS files");
    if (title == "Finishing")
        return _("Finishing");
    if (title == "Removing RetroArch")
        return _("Removing RetroArch");
    if (title == "Removing BIOS files")
        return _("Removing BIOS files");
    return title;
}

inline bool isRemoval(RaJobService::Action action) {
    return action == RaJobService::Action::Remove || action == RaJobService::Action::RemoveWithBios;
}

struct ProgressView {
    std::string title;            // the panel's heading: the job, or its result
    std::string detail;           // the phase "(2/4) Downloading cores  12.0 MB / 700.0 MB", or why it ended
    double fraction = -1;         // < 0: no bar
    std::vector<std::string> log; // the runner's last lines (while it runs and after a failure)
    bool over = false;
    bool canStop = false; // Circle asks "Stop?"
};

inline ProgressView progressView(const RaJobService::Status &status) {
    ProgressView view;
    const RaJobService::Action action = status.action;
    switch (status.phase) {
    case RaJobService::Phase::Idle:
    case RaJobService::Phase::Running: {
        view.title = action == RaJobService::Action::Update ? _("Updating RetroArch")
                     : isRemoval(action)                    ? _("Removing RetroArch")
                                                            : _("Installing RetroArch");
        if (status.stopping) {
            view.detail = _("Stopping...");
        } else if (status.steps > 0) {
            view.detail = "(" + std::to_string(status.step) + "/" + std::to_string(status.steps) + ") " +
                          phaseTitle(status.title);
            if (status.total > 0)
                view.detail += "  " + humanBytes(status.done) + " / " + humanBytes(status.total);
        } else {
            view.detail = _("Preparing");
        }
        view.fraction = std::max(0.0, status.fraction());
        view.log = status.log;
        view.canStop = !status.stopping;
        break;
    }
    case RaJobService::Phase::Succeeded:
        view.over = true;
        view.title = action == RaJobService::Action::Update ? _("RetroArch is updated")
                     : isRemoval(action)                    ? _("RetroArch is removed")
                                                            : _("RetroArch is installed");
        view.fraction = 1;
        break;
    case RaJobService::Phase::Stopped:
        view.over = true;
        view.title = _("Stopped");
        view.detail = _("Start it again to carry on where it stopped");
        break;
    case RaJobService::Phase::Failed:
        view.over = true;
        view.title = action == RaJobService::Action::Update ? _("The update failed")
                     : isRemoval(action)                    ? _("The removal failed")
                                                            : _("The installation failed");
        switch (status.failure) {
        case RaJobService::Failure::NoNetwork:
            view.detail = _("No network connection");
            break;
        case RaJobService::Failure::NoSpace:
            view.detail = _("Not enough free space on the stick");
            break;
        default:
            view.detail = status.log.empty() ? _("No reason given") : status.log.back();
            break;
        }
        break;
    }
    return view;
}

} // namespace RaManager
