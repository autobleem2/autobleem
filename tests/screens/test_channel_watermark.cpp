//
// The channel watermark's text (UIREV-40, evoui/channel_watermark.h): the build names its channel (AB_BUILD_CHANNEL),
// the tag follows it - DEV + the short commit hash, NIGHTLY + the short version, ALPHA / BETA / RC (the tag's own
// word) + the short version, a release nothing. The version text is parsed only for the short version: no "v", no
// git hash, the commit count after a dot. The drawing (GuiLauncher::renderChannelWatermark) needs a live Gui and is
// the walk's job.
//
#include "doctest/doctest.h"

#include "channel_watermark.h"

using ChannelWatermark::Channel;
using ChannelWatermark::channelFromName;
using ChannelWatermark::tagFor;

TEST_CASE("the channel comes from the build's AB_BUILD_CHANNEL value; unset or unknown is dev") {
    CHECK(channelFromName("dev") == Channel::Dev);
    CHECK(channelFromName("nightly") == Channel::Nightly);
    CHECK(channelFromName("prerelease") == Channel::Prerelease);
    CHECK(channelFromName("release") == Channel::Release);
    CHECK(channelFromName(nullptr) == Channel::Dev);
    CHECK(channelFromName("") == Channel::Dev);
    CHECK(channelFromName("whatever") == Channel::Dev);
}

TEST_CASE("a dev (hand) build says DEV and its short commit hash, whatever the version text") {
    const ChannelWatermark::Tag tag = tagFor(Channel::Dev, "v2.0.0-alpha2-470-g1760cc8", "c7357bb");
    CHECK(tag.shown());
    CHECK(tag.channel == Channel::Dev);
    CHECK(tag.word == "DEV");
    CHECK(tag.version == "c7357bb");
    CHECK(tagFor(Channel::Dev, "nightly", "c7357bb1234567").version == "c7357bb"); // a long hash is cut to 7
    CHECK(tagFor(Channel::Dev, "", "abc").version == "abc");
}

TEST_CASE("a nightly says NIGHTLY and the short version: the count after a dot, the hash gone") {
    const ChannelWatermark::Tag nightly = tagFor(Channel::Nightly, "v2.0.0-alpha2-314-g1760cc8", "1760cc8");
    CHECK(nightly.channel == Channel::Nightly);
    CHECK(nightly.word == "NIGHTLY");
    CHECK(nightly.version == "2.0.0-a2.314");

    CHECK(tagFor(Channel::Nightly, "v2.0.0-17-gabcdef0", "").version == "2.0.0.17");
    CHECK(tagFor(Channel::Nightly, "v2.0.0-alpha2-17", "").version == "2.0.0-a2.17"); // no hash in the VERSION file
    CHECK(tagFor(Channel::Nightly, "v2.0.0-rc1-dirty", "").version == "2.0.0-rc1");
    CHECK(tagFor(Channel::Nightly, "v2.0.0-alpha2-17-g1760cc8-1760cc8", "").version == "2.0.0-a2.17");
    CHECK(tagFor(Channel::Nightly, "v2.0.0-rc1", "").word ==
          "NIGHTLY"); // the channel is the build's, not the version's
}

TEST_CASE("a pre-release says the tag's own word and the short version") {
    const ChannelWatermark::Tag alpha = tagFor(Channel::Prerelease, "v2.0.0-alpha1", "c7357bb");
    CHECK(alpha.channel == Channel::Prerelease);
    CHECK(alpha.word == "ALPHA");
    CHECK(alpha.version == "2.0.0-a1");

    const ChannelWatermark::Tag beta = tagFor(Channel::Prerelease, "v2.0.0-beta3", "");
    CHECK(beta.word == "BETA");
    CHECK(beta.version == "2.0.0-b3");

    const ChannelWatermark::Tag rc = tagFor(Channel::Prerelease, "v2.0.0-rc1", "");
    CHECK(rc.word == "RC");
    CHECK(rc.version == "2.0.0-rc1");

    // a word the tag does not carry: the old TESTING stays the name
    CHECK(tagFor(Channel::Prerelease, "v2.0.0-pre0", "").word == "TESTING");
    CHECK(tagFor(Channel::Prerelease, "v2.0.0-pre0", "").version == "2.0.0-pre0");
}

TEST_CASE("a release shows no watermark, whatever the version text") {
    CHECK_FALSE(tagFor(Channel::Release, "v2.0.0", "c7357bb").shown());
    CHECK(tagFor(Channel::Release, "v2.0.0", "c7357bb").word.empty());
    CHECK(tagFor(Channel::Release, "v2.0.0", "c7357bb").version.empty());
    CHECK_FALSE(tagFor(Channel::Release, "v2.0.0-alpha1-12-gabcdef0", "abcdef0").shown());
}

TEST_CASE("something that is not a version gives no short version") {
    CHECK(tagFor(Channel::Nightly, "", "").version.empty());
    CHECK(tagFor(Channel::Nightly, "dev", "").version.empty());
    CHECK(tagFor(Channel::Nightly, "v", "").version.empty());
    CHECK(tagFor(Channel::Prerelease, "dev", "").word == "TESTING");
}

TEST_CASE("the tag sits under a two-pad plate and gets out of a taller one's way") {
    CHECK(ChannelWatermark::yBelow(0) == ChannelWatermark::Y);  // no plate
    CHECK(ChannelWatermark::yBelow(43) == ChannelWatermark::Y); // one pad: the same place
    CHECK(ChannelWatermark::yBelow(66) == ChannelWatermark::Y); // two pads end at 66
    CHECK(ChannelWatermark::yBelow(87) >= 87 + 1);              // three pads: below the plate
    CHECK(ChannelWatermark::X == 12);
    CHECK(ChannelWatermark::Alpha == 204);
}

TEST_CASE("the tag's design sizes: the word bold 13, the version medium 14, the chip the word + 14") {
    CHECK(ChannelWatermark::WordPx == 13);
    CHECK(ChannelWatermark::VersionPx == 14);
    CHECK(ChannelWatermark::ChipHeight == 24);
    CHECK(ChannelWatermark::Y == 72);
    CHECK(ChannelWatermark::chipWidth(21) == 35); // "DEV" at 13 px bold is about 21 px wide
    CHECK(ChannelWatermark::chipWidth(0) == 14);
}

TEST_CASE("the real package strings: describe, describe + -n<fingerprint>, plain tags") {
    struct Row {
        const char *input;
        Channel channel;
        const char *version;
    };
    // a nightly build: the channel is the build's, the version text only gives the short version
    const Row nightlies[] = {
        {"2.0.0-a2-314-gc7357bb-n72fd67", Channel::Nightly, "2.0.0-a2.314"}, // a nightly package's VERSION
        {"v2.0.0-a2-314-gc7357bb-n72fd67", Channel::Nightly, "2.0.0-a2.314"},
        {"2.0.0-a2-314-gc7357bb", Channel::Nightly, "2.0.0-a2.314"}, // describe only
        {"2.0.0-a2-314-gc7357bb-dirty", Channel::Nightly, "2.0.0-a2.314"},
        {"2.0.0-a2-314-gc7357bb-n72fd67-dirty", Channel::Nightly, "2.0.0-a2.314"},
        {"2.0.0-a2-314-n72fd67", Channel::Nightly, "2.0.0-a2.314"}, // fingerprint without the hash
    };
    for (const Row &row : nightlies) {
        INFO(row.input);
        const ChannelWatermark::Tag tag = tagFor(row.channel, row.input, "c7357bb");
        CHECK(tag.channel == row.channel);
        CHECK(tag.word == "NIGHTLY");
        CHECK(tag.version == row.version);
    }

    // a plain pre-release tag on a pre-release build: no count, a short word the tag does not carry says TESTING
    const ChannelWatermark::Tag pre = tagFor(Channel::Prerelease, "2.0.0-a2", "c7357bb");
    CHECK(pre.shown());
    CHECK(pre.version == "2.0.0-a2");
    CHECK(pre.word == "TESTING");

    // a plain release tag on a release build: nothing shown
    CHECK_FALSE(tagFor(Channel::Release, "2.0.0", "c7357bb").shown());
}
