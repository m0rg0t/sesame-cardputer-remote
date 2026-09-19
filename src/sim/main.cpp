#include <Arduino.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

std::uint32_t gSimMillis = 0;

// Compile the shipping firmware and its drawing functions into this native
// harness. Hardware and network APIs are replaced by sim/stubs; no UI code is
// copied or reimplemented for the screenshots.
#include "../main.cpp"

namespace {

namespace fs = std::filesystem;
std::string shotsDirectory;

void saveFrame(const char* name)
{
    std::vector<lgfx::rgb888_t> pixels(kScreenWidth * kScreenHeight);
    canvas.readRect(0, 0, kScreenWidth, kScreenHeight, pixels.data());
    const fs::path path = fs::path(shotsDirectory) / (std::string(name) + ".ppm");
    auto* file = std::fopen(path.string().c_str(), "wb");
    if (!file) {
        std::perror(path.string().c_str());
        std::exit(1);
    }
    std::fprintf(file, "P6\n%d %d\n255\n", kScreenWidth, kScreenHeight);
    for (const auto& pixel : pixels) {
        const unsigned char rgb[] = {pixel.r, pixel.g, pixel.b};
        std::fwrite(rgb, 1, sizeof(rgb), file);
    }
    std::fclose(file);
}

void resetScenario()
{
    gSimMillis = 4200;
    WiFi.connected = true;
    WiFi.ssid = "Studio WiFi";
    statusLine = "READY";
    wifiNotice = "";
    joiningSsid = "";
    wifiNetworks.clear();
    selectedWifi = 0;
    wifiScanning = false;
    wifiTargetSsid = "";
    passwordDraft = "";
    discoveryAttempted = false;
    robotOnline = true;
    robotIp = IPAddress(192, 168, 1, 42);
    discoverySource = "mDNS";
    robotCommand = "stand";
    robotFace = "HAPPY";
    selectedAction = 2;
    launchStartedMs = 0;
    launchSent = false;
    launchSucceeded = false;
    movementActive = false;
    activeMovementKey = '\0';
    movementName = "";
}

void captureScenarios()
{
    resetScenario();
    screen = Screen::Home;
    draw();
    saveFrame("home-online");

    movementActive = true;
    activeMovementKey = 'w';
    movementName = "HOLD forward";
    draw();
    saveFrame("move-forward");

    resetScenario();
    screen = Screen::Launch;
    launchStartedMs = 3500;
    gSimMillis = 4150;
    draw();
    saveFrame("launch-wave");

    resetScenario();
    screen = Screen::Wifi;
    wifiNotice = "NETWORKS / 5";
    wifiNetworks = {
        {"Sesame-Controller", -42, true},
        {"Studio WiFi", -51, true},
        {"Workshop", -64, true},
        {"Robot Lab", -73, false},
        {"Guest 2.4G", -82, true},
    };
    selectedWifi = 1;
    draw();
    saveFrame("wifi-list");

    resetScenario();
    screen = Screen::Discovering;
    robotOnline = false;
    statusLine = "QUERYING mDNS";
    discoveryAttempted = false;
    draw();
    saveFrame("mdns-discovery");
}

int run(bool*)
{
    M5Cardputer.Display.init();
    M5Cardputer.Display.setRotation(1);
    canvas.setColorDepth(16);
    if (!canvas.createSprite(kScreenWidth, kScreenHeight)) return 1;
    fs::create_directories(shotsDirectory);
    captureScenarios();
    // Panel_sdl owns the host event loop. Screenshot generation is a batch
    // command, so end it as soon as every frame has been flushed to disk.
    std::exit(0);
}

}  // namespace

int main(int argc, char** argv)
{
    for (int index = 1; index + 1 < argc; ++index) {
        if (std::strcmp(argv[index], "--shots") == 0) {
            shotsDirectory = argv[++index];
        }
    }
    if (shotsDirectory.empty()) {
        std::fprintf(stderr, "usage: %s --shots DIRECTORY\n", argv[0]);
        return 2;
    }
    return lgfx::Panel_sdl::main(run, 128);
}
