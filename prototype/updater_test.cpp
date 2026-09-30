// CPU-only checks for include/Updater.h (no network).
#include "Updater.h"

#include <iostream>
#include <string>

using namespace gol3d;

namespace {

int failures = 0;

void expect(bool condition, const std::string& what) {
    if (!condition) {
        std::cerr << "FAIL: " << what << "\n";
        ++failures;
    }
}

// Trimmed shape of GET /repos/{owner}/{repo}/releases/latest.
const char* RELEASE_JSON = R"({
  "url": "https://api.github.com/repos/CameronWeller/3DGameOfLife-Vulkan-Edition/releases/1",
  "html_url": "https://github.com/CameronWeller/3DGameOfLife-Vulkan-Edition/releases/tag/v0.1.12",
  "tag_name": "v0.1.12",
  "name": "3D Life 0.1.12",
  "author": { "login": "github-actions[bot]", "html_url": "https://github.com/apps/github-actions" },
  "assets": [
    {
      "name": "gol3d-0.1.12-windows-x64.exe",
      "uploader": { "name": "not this one" },
      "digest": "sha256:ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
      "browser_download_url": "https://github.com/CameronWeller/3DGameOfLife-Vulkan-Edition/releases/download/v0.1.12/gol3d-0.1.12-windows-x64.exe"
    },
    {
      "name": "gol3d-0.1.12-linux-x86_64.AppImage",
      "digest": null,
      "browser_download_url": "https://github.com/CameronWeller/3DGameOfLife-Vulkan-Edition/releases/download/v0.1.12/gol3d-0.1.12-linux-x86_64.AppImage"
    }
  ],
  "body": "Notes with \"quotes\" and { braces }"
})";

} // namespace

int main() {
    expect(sha256Hex("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", "sha256 of empty");
    expect(sha256Hex("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", "sha256 of abc");
    expect(sha256Hex(std::string(1000, 'a')).size() == 64, "sha256 of a multi-block message");

    expect(isNewerVersion("0.1.12", "0.1.9"), "0.1.12 is newer than 0.1.9");
    expect(!isNewerVersion("0.1.9", "0.1.12"), "0.1.9 is older than 0.1.12");
    expect(!isNewerVersion("0.1.12", "0.1.12"), "same version is not newer");
    expect(isNewerVersion("1.0", "0.9.9"), "different lengths compare numerically");
    expect(!isNewerVersion("0.1.x", "0.1.0"), "non-numeric versions are ignored");

    auto release = parseLatestRelease(RELEASE_JSON);
    expect(release.has_value(), "release JSON parses");
    if (release) {
        expect(release->version == "0.1.12", "tag v0.1.12 becomes version 0.1.12");
        expect(release->pageUrl.find("/releases/tag/v0.1.12") != std::string::npos, "release page URL");
        expect(release->assets.size() == 2, "two assets");
        if (release->assets.size() == 2) {
            expect(release->assets[0].name == "gol3d-0.1.12-windows-x64.exe", "asset name, not the nested uploader name");
            expect(release->assets[0].sha256 == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",
                   "asset digest");
            expect(release->assets[1].sha256.empty(), "missing digest stays empty");
            expect(isTrustedDownloadUrl(release->assets[1].url), "release asset URL is trusted");
        }
    }
    expect(!parseLatestRelease("{\"message\": \"Not Found\"}").has_value(), "error replies are rejected");

    expect(!isTrustedDownloadUrl("https://example.com/gol3d.exe"), "other hosts are not trusted");
    expect(!isTrustedDownloadUrl(std::string(RELEASE_DOWNLOAD_PREFIX) + "v1/x;rm -rf ~"), "shell characters are rejected");
    expect(assetNameFor("0.2.0", InstallMethod::OpenPage).empty(), "no asset for open-page installs");
    expect(assetNameFor("0.2.0", InstallMethod::WindowsInstaller).rfind("gol3d-0.2.0-windows-", 0) == 0, "installer name");
    expect(assetNameFor("0.2.0", InstallMethod::AppImage).rfind("gol3d-0.2.0-linux-", 0) == 0, "AppImage name");

    if (failures) return 1;
    std::cout << "updater: all checks passed\n";
    return 0;
}
