//
// The channel watermark's text (UIREV-40, evoui/channel_watermark.h): a release shows no tag, a tagged pre-release is
// TESTING, commits past a tag (or a dirty tree) is NIGHTLY; the short version has no "v" and no git hash, the commit
// count after a dot. The drawing (GuiLauncher::renderChannelWatermark) needs a live Gui and is the walk's job.
//
#include "doctest/doctest.h"

#include "channel_watermark.h"

using ChannelWatermark::Channel;
using ChannelWatermark::tagFor;

TEST_CASE("a release shows no watermark") {
    CHECK_FALSE(tagFor("v2.0.0").shown());
    CHECK_FALSE(tagFor("2.0.0").shown());
    CHECK(tagFor("v2.0.0").word.empty());
    CHECK(tagFor("v2.0.0").version.empty());
}

TEST_CASE("something that is not a version shows no watermark") {
    CHECK_FALSE(tagFor("").shown());
    CHECK_FALSE(tagFor("dev").shown());
    CHECK_FALSE(tagFor("v").shown());
}

TEST_CASE("a tagged pre-release is TESTING") {
    const ChannelWatermark::Tag rc = tagFor("v2.0.0-rc1");
    CHECK(rc.channel == Channel::Testing);
    CHECK(rc.word == "TESTING");
    CHECK(rc.version == "2.0.0-rc1");

    const ChannelWatermark::Tag alpha = tagFor("v2.0.0-alpha2");
    CHECK(alpha.channel == Channel::Testing);
    CHECK(alpha.version == "2.0.0-a2");
    CHECK(tagFor("v2.0.0-beta1").version == "2.0.0-b1");
}

TEST_CASE("commits past a tag are NIGHTLY, the count after a dot and the hash gone") {
    const ChannelWatermark::Tag nightly = tagFor("v2.0.0-alpha2-470-g1760cc8");
    CHECK(nightly.channel == Channel::Nightly);
    CHECK(nightly.word == "NIGHTLY");
    CHECK(nightly.version == "2.0.0-a2.470");

    CHECK(tagFor("v2.0.0-17-gabcdef0").version == "2.0.0.17");
    CHECK(tagFor("v2.0.0-17-gabcdef0").channel == Channel::Nightly);
    CHECK(tagFor("v2.0.0-alpha2-17").version == "2.0.0-a2.17"); // no hash in the VERSION file
}

TEST_CASE("a dirty tree is NIGHTLY even on a tag") {
    const ChannelWatermark::Tag dirty = tagFor("v2.0.0-rc1-dirty");
    CHECK(dirty.channel == Channel::Nightly);
    CHECK(dirty.version == "2.0.0-rc1");
}

TEST_CASE("a hash the package appends after the count is dropped too") {
    CHECK(tagFor("v2.0.0-alpha2-17-g1760cc8-1760cc8").version == "2.0.0-a2.17");
}

TEST_CASE("the tag sits under a two-pad plate and gets out of a taller one's way") {
    CHECK(ChannelWatermark::yBelow(0) == ChannelWatermark::Y);  // no plate
    CHECK(ChannelWatermark::yBelow(43) == ChannelWatermark::Y); // one pad: the same place
    CHECK(ChannelWatermark::yBelow(66) == ChannelWatermark::Y); // two pads end at 66
    CHECK(ChannelWatermark::yBelow(87) >= 87 + 1);              // three pads: below the plate
    CHECK(ChannelWatermark::X == 12);
    CHECK(ChannelWatermark::Alpha == 204);
}
