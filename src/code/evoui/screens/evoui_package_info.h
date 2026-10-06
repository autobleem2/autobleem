//
// GuiPackageInfo: the info view of a package of the Packages row (autobleem-main docs/packages.md 7) - its version,
// licence, source and place, the games in it, its description and readme, and which installed Apps run it. Read-only:
// Cross on a package opens this and nothing is launched; Circle closes it.
//
#pragma once

#include "core/services/package_service.h"
#include "gui/screens/gui_facts_page.h"

#include <string>
#include <vector>

//******************
// GuiPackageInfo
//******************
class GuiPackageInfo : public GuiFactsPage {
public:
    GuiPackageInfo(ableem::GuiBase &gui, PackageInfo package) : GuiFactsPage(gui), package(std::move(package)) {}

    void init() override;

protected:
    std::string title() override { return package.title; }
    std::vector<abgui::FactsSection> collect() override { return sections; }

private:
    PackageInfo package;
    std::vector<abgui::FactsSection> sections; // built once when the view opens: the Apps' manifests are read there

    std::vector<abgui::FactsSection> build() const;
};
