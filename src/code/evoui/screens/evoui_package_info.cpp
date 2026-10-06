#include "evoui_package_info.h"
#include "evoui_package_picker.h"
#include "package_picker_logic.h"
#include "app.h"
#include "core/services/app_manifest.h"
#include "core/services/environment.h"

#include <fstream>

using namespace std;

namespace {
const size_t MaxReadmeLines = 24; // the readme's first lines: the page is a summary, not a reader

// the readme's non-blank lines, the first MaxReadmeLines of them, each trimmed
vector<string> readmeLines(const string &path) {
    vector<string> lines;
    if (path.empty())
        return lines;
    ifstream in(path);
    string line;
    while (lines.size() < MaxReadmeLines && getline(in, line)) {
        const size_t begin = line.find_first_not_of(" \t\r");
        if (begin == string::npos)
            continue;
        lines.push_back(line.substr(begin, line.find_last_not_of(" \t\r") - begin + 1));
    }
    return lines;
}

// "<where the stick is>/Packages/Doom" as "Packages/Doom"; a folder outside Packages/ (an engine's own) as it is
string locationOf(const string &root) {
    const string packages = Env::getPathToPackagesDir();
    const size_t slash = packages.rfind('/');
    const string stick = slash == string::npos ? string() : packages.substr(0, slash + 1);
    return !stick.empty() && root.compare(0, stick.size(), stick) == 0 ? root.substr(stick.size()) : root;
}
} // namespace

//*******************************
// GuiPackageInfo::init
//*******************************
void GuiPackageInfo::init() {
    sections = build();
    GuiFactsPage::init();
}

//*******************************
// GuiPackageInfo::build
//*******************************
vector<abgui::FactsSection> GuiPackageInfo::build() const {
    vector<abgui::FactsSection> out;

    abgui::FactsSection facts;
    facts.title = package.title;
    if (!package.version.empty())
        facts.rows.push_back({_("Version"), package.version});
    if (!package.licence.empty())
        facts.rows.push_back({_("Licence"), packageLicenceLabel(package.licence)});
    facts.rows.push_back({_("Source"), packageSourceLabel(package.source, package.inApp)});
    facts.rows.push_back({_("Location"), locationOf(package.root)});
    out.push_back(facts);

    abgui::FactsSection contents;
    contents.title = _("Contents");
    if (package.unknown || package.games.empty()) {
        contents.rows.push_back({_("Unknown data"), ""});
        contents.rows.push_back({"", _("Not recognised. If this is a game, see the manual: Your own games.")});
    }
    for (const PackageGame &game : package.games)
        contents.rows.push_back(
            {game.variant.empty() ? game.title : game.title + " (" + game.variant + ")", packageKindLabel(game.kind)});
    out.push_back(contents);

    abgui::FactsSection about;
    about.title = _("Description");
    if (!package.description.empty())
        about.rows.push_back({"", package.description});
    for (const string &line : readmeLines(package.readme))
        about.rows.push_back({"", line});
    if (!about.rows.empty())
        out.push_back(about);

    // the installed Apps whose Uses= names a kind of this package (their manifests are read now, once)
    vector<pair<string, vector<string>>> apps;
    for (const PsGamePtr &game : App::get().gameQuery().apps(AppCategory::All)) {
        if (!game || !game->app || game->package)
            continue;
        const AppManifest manifest = AppManifest::load(game->base, "app.ini", Env::appPlatformKeys());
        if (!manifest.uses.empty())
            apps.emplace_back(game->title, manifest.uses);
    }
    abgui::FactsSection runs;
    runs.title = _("Runs with");
    for (const string &title : packagepicker::runsWith(package.kinds(), apps))
        runs.rows.push_back({"", title});
    if (runs.rows.empty() && !package.unknown)
        runs.rows.push_back({"", _("Nothing installed runs this")});
    if (!runs.rows.empty() || package.unknown)
        out.push_back(runs);
    return out;
}
