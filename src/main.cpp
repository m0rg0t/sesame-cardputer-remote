#include <Arduino.h>
#include <ESPmDNS.h>
#include <HTTPClient.h>
#include <M5Cardputer.h>
#include <WiFi.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace {

constexpr int kScreenWidth = 240;
constexpr int kScreenHeight = 135;
constexpr char kDefaultSsid[] = "Sesame-Controller";
constexpr char kDefaultPassword[] = "12345678";
constexpr char kRobotMdnsHost[] = "sesame-robot";
constexpr char kControllerHostname[] = "sesame-cardputer";
constexpr char kBuildIdentity[] =
    "cardputer-sesame-robot/application/" SESAME_REMOTE_VERSION;
constexpr std::uint32_t kWifiTimeoutMs = 10000;
constexpr std::uint32_t kStatusPollMs = 5000;
constexpr std::size_t kMaxWifiNetworks = 14;

constexpr std::uint16_t kBg = 0x0024;
constexpr std::uint16_t kPanel = 0x08A9;
constexpr std::uint16_t kPanelBright = 0x1130;
constexpr std::uint16_t kCyan = 0x07FF;
constexpr std::uint16_t kCyanDark = 0x03B1;
constexpr std::uint16_t kMagenta = 0xF81F;
constexpr std::uint16_t kPurple = 0x801F;
constexpr std::uint16_t kLime = 0xA7F2;
constexpr std::uint16_t kOrange = 0xFD20;
constexpr std::uint16_t kRed = 0xF986;
constexpr std::uint16_t kInk = 0xFFFF;
constexpr std::uint16_t kMuted = 0x7C1F;
constexpr std::uint16_t kRule = 0x21B0;

struct RobotAction {
    const char* label;
    const char* command;
    std::uint16_t color;
};

constexpr RobotAction kActions[] = {
    {"REST", "rest", kCyan},       {"STAND", "stand", kLime},
    {"WAVE", "wave", kMagenta},   {"DANCE", "dance", kOrange},
    {"CUTE", "cute", kMagenta},   {"BOW", "bow", kCyan},
    {"SHAKE", "shake", kOrange},  {"SHRUG", "shrug", kPurple},
    {"POINT", "point", kLime},    {"SWIM", "swim", kCyan},
    {"PUSHUP", "pushup", kRed},   {"CRAB", "crab", kOrange},
    {"WORM", "worm", kLime},      {"FREAKY", "freaky", kPurple},
    {"DEAD", "dead", kRed},
};
constexpr int kActionCount = sizeof(kActions) / sizeof(kActions[0]);

enum class Screen : std::uint8_t {
    Connecting,
    Discovering,
    Home,
    Wifi,
    Password,
    Launch,
};

struct WifiNetwork {
    String ssid;
    std::int8_t rssi = -127;
    bool secured = true;
};

M5Canvas canvas(&M5Cardputer.Display);
bool ready = false;
bool dirty = true;
Screen screen = Screen::Connecting;

String statusLine = "BOOTING";
String wifiNotice;
String joiningSsid;
String joiningPassword;
bool joiningDefault = true;
std::uint32_t joinStartedMs = 0;

std::vector<WifiNetwork> wifiNetworks;
int selectedWifi = 0;
bool wifiScanning = false;
std::uint32_t scanStartedMs = 0;
String wifiTargetSsid;
String passwordDraft;

bool mdnsStarted = false;
bool discoveryAttempted = false;
std::uint32_t discoveryStartedMs = 0;
IPAddress robotIp;
bool robotOnline = false;
String discoverySource;
String robotCommand;
String robotFace;
std::uint32_t lastStatusPollMs = 0;
std::uint8_t statusFailures = 0;

int selectedAction = 2;
std::uint32_t launchStartedMs = 0;
bool launchSent = false;
bool launchSucceeded = false;

bool movementActive = false;
char activeMovementKey = '\0';
String movementName;

std::uint32_t lastDrawMs = 0;

String shortened(const String& value, std::size_t maximum)
{
    if (value.length() <= maximum) return value;
    if (maximum < 2) return value.substring(0, maximum);
    return value.substring(0, maximum - 1) + "~";
}

bool validIp(const IPAddress& ip)
{
    return ip != IPAddress(0, 0, 0, 0) && ip != IPAddress(255, 255, 255, 255);
}

String jsonString(const String& body, const char* key)
{
    const String needle = String("\"") + key + "\":\"";
    int start = body.indexOf(needle);
    if (start < 0) return "";
    start += needle.length();
    const int end = body.indexOf('"', start);
    return end > start ? body.substring(start, end) : String("");
}

void drawFooter(const char* text)
{
    canvas.drawFastHLine(0, 121, kScreenWidth, kRule);
    canvas.fillRect(0, 122, kScreenWidth, 13, kBg);
    canvas.setTextFont(1);
    canvas.setTextColor(kMuted, kBg);
    canvas.setCursor(4, 125);
    canvas.print(text);
}

void drawHeader(const char* title)
{
    canvas.fillRect(0, 0, kScreenWidth, 16, kPanel);
    canvas.drawFastHLine(0, 15, kScreenWidth, kCyanDark);
    canvas.fillRect(0, 0, 3, 16, kMagenta);
    canvas.setTextFont(1);
    canvas.setTextColor(kInk, kPanel);
    canvas.setCursor(8, 4);
    canvas.print(title);
    const bool wifi = WiFi.status() == WL_CONNECTED;
    canvas.fillCircle(211, 8, 3, wifi ? kCyan : kRed);
    canvas.fillCircle(226, 8, 3, robotOnline ? kLime : kOrange);
    canvas.setTextColor(kMuted, kPanel);
    canvas.setCursor(197, 4);
    canvas.print("W");
    canvas.setCursor(216, 4);
    canvas.print("R");
}

void drawRobot(int x, int y, float phase, std::uint16_t accent)
{
    const int bob = static_cast<int>(std::sin(phase) * 2.0f);
    const int swing = static_cast<int>(std::sin(phase * 1.7f) * 5.0f);
    y += bob;
    canvas.drawRoundRect(x + 8, y + 8, 43, 24, 5, accent);
    canvas.fillRoundRect(x + 13, y + 12, 33, 15, 3, kPanelBright);
    canvas.fillRect(x + 20, y + 17, 4, 3, accent);
    canvas.fillRect(x + 35, y + 17, 4, 3, accent);
    canvas.drawFastHLine(x + 26, y + 23, 7, accent);
    canvas.drawLine(x + 12, y + 30, x + 7 + swing, y + 43, accent);
    canvas.drawLine(x + 46, y + 30, x + 51 - swing, y + 43, accent);
    canvas.drawLine(x + 20, y + 31, x + 18 - swing, y + 46, accent);
    canvas.drawLine(x + 39, y + 31, x + 41 + swing, y + 46, accent);
    canvas.fillCircle(x + 7 + swing, y + 43, 2, accent);
    canvas.fillCircle(x + 51 - swing, y + 43, 2, accent);
    canvas.fillCircle(x + 18 - swing, y + 46, 2, accent);
    canvas.fillCircle(x + 41 + swing, y + 46, 2, accent);
    canvas.drawLine(x + 3, y + 14, x + 8, y + 18, kMagenta);
    canvas.drawLine(x + 55, y + 14, x + 50, y + 18, kMagenta);
}

void drawGrid(std::uint32_t elapsed, std::uint16_t color)
{
    const int horizon = 91;
    for (int x = -80; x <= 320; x += 24) {
        canvas.drawLine(120, horizon, x, 121, kRule);
    }
    const int shift = (elapsed / 35) % 10;
    for (int y = horizon + shift; y < 121; y += 10) {
        canvas.drawFastHLine(0, y, kScreenWidth, y > 110 ? color : kRule);
    }
}

void drawStatusPanel(const char* heading, const String& main,
                     const String& detail, bool animate)
{
    drawHeader("SESAME // LINK");
    canvas.fillRoundRect(8, 28, 224, 77, 5, kPanel);
    canvas.drawRoundRect(8, 28, 224, 77, 5, kCyanDark);
    canvas.fillRect(8, 28, 4, 77, kMagenta);
    canvas.setTextColor(kCyan, kPanel);
    canvas.setCursor(20, 40);
    canvas.print(heading);
    canvas.setTextColor(kInk, kPanel);
    canvas.setCursor(20, 58);
    canvas.print(shortened(main, 32));
    canvas.setTextColor(kMuted, kPanel);
    canvas.setCursor(20, 74);
    canvas.print(shortened(detail, 32));
    if (animate) {
        const int lit = 1 + (millis() / 120) % 10;
        for (int block = 0; block < 10; ++block) {
            const int x = 20 + block * 18;
            canvas.drawRect(x, 91, 13, 5, kRule);
            if (block < lit) canvas.fillRect(x + 1, 92, 11, 3, kCyan);
        }
    }
}

void drawConnecting()
{
    canvas.fillScreen(kBg);
    drawStatusPanel(joiningDefault ? "DEFAULT LINK" : "JOINING NETWORK",
                    joiningSsid,
                    joiningDefault ? "Sesame direct control" : "Waiting for DHCP",
                    true);
    drawFooter("N NETWORKS                 PLEASE WAIT");
}

void drawDiscovering()
{
    canvas.fillScreen(kBg);
    drawStatusPanel("SEARCHING FOR ROBOT", "sesame-robot.local",
                    statusLine, !discoveryAttempted);
    drawFooter(discoveryAttempted
                   ? "R RETRY  N NETWORKS       ESC HOME"
                   : "mDNS FIRST / AP FALLBACK");
}

void drawHome()
{
    canvas.fillScreen(kBg);
    drawHeader("SESAME // REMOTE");

    canvas.fillRoundRect(3, 21, 66, 94, 4, kPanel);
    canvas.drawRoundRect(3, 21, 66, 94, 4, robotOnline ? kCyanDark : kRed);
    drawRobot(7, 31, millis() * 0.008f,
              robotOnline ? kCyan : kOrange);
    canvas.setTextFont(1);
    canvas.setTextColor(robotOnline ? kLime : kOrange, kPanel);
    canvas.setCursor(9, 83);
    canvas.print(robotOnline ? "ROBOT ONLINE" : "ROBOT OFFLINE");
    canvas.setTextColor(kMuted, kPanel);
    canvas.setCursor(9, 94);
    canvas.print(shortened(discoverySource.length() ? discoverySource : "NO ROUTE", 10));
    canvas.setCursor(9, 104);
    canvas.print(shortened(robotFace.length() ? robotFace : statusLine, 10));

    constexpr int visible = 5;
    int start = selectedAction - visible / 2;
    if (start < 0) start = 0;
    if (start > kActionCount - visible) start = kActionCount - visible;
    for (int row = 0; row < visible; ++row) {
        const int index = start + row;
        const int y = 21 + row * 19;
        const bool selected = index == selectedAction;
        const auto& action = kActions[index];
        const std::uint16_t background = selected ? kPanelBright : kBg;
        if (selected) {
            canvas.fillRoundRect(73, y, 164, 17, 3, background);
            canvas.fillRect(73, y, 4, 17, action.color);
        } else {
            canvas.drawFastHLine(91, y + 17, 144, kRule);
        }
        canvas.setTextColor(selected ? action.color : kMuted, background);
        canvas.setCursor(81, y + 5);
        if (index + 1 < 10) canvas.print('0');
        canvas.print(index + 1);
        canvas.setTextColor(kInk, background);
        canvas.setCursor(108, y + 5);
        canvas.print(action.label);
        if (selected) {
            canvas.setTextColor(kCyan, background);
            canvas.setCursor(218, y + 5);
            canvas.print('>');
        }
    }
    if (movementActive) {
        canvas.fillRect(3, 108, 66, 7, kPurple);
        canvas.setTextColor(kInk, kPurple);
        canvas.setCursor(8, 108);
        canvas.print(shortened(movementName, 9));
    }
    drawFooter("^v POSE  ENTER GO  MOVE:HOLD  N WIFI");
}

void drawWifi()
{
    canvas.fillScreen(kBg);
    drawHeader("SESAME // WIFI");
    if (wifiScanning) {
        drawStatusPanel("SCANNING 2.4 GHZ", "Nearby networks",
                        wifiNotice.length() ? wifiNotice : "Asynchronous scan", true);
        drawFooter("PLEASE WAIT                 ` CANCEL");
        return;
    }
    canvas.setTextColor(wifiNotice.length() ? kOrange : kMuted, kBg);
    canvas.setCursor(5, 20);
    canvas.print(shortened(wifiNotice.length() ? wifiNotice
                                               : String("NETWORKS / ") + wifiNetworks.size(),
                            36));
    if (wifiNetworks.empty()) {
        canvas.fillRoundRect(8, 42, 224, 54, 4, kPanel);
        canvas.setTextColor(kOrange, kPanel);
        canvas.setCursor(20, 54);
        canvas.print("NO NETWORKS FOUND");
        canvas.setTextColor(kMuted, kPanel);
        canvas.setCursor(20, 72);
        canvas.print("R scan / D use Sesame default");
        drawFooter("R SCAN  D DEFAULT            ` BACK");
        return;
    }

    constexpr int visible = 5;
    const int count = static_cast<int>(wifiNetworks.size());
    int start = selectedWifi - visible / 2;
    if (start < 0) start = 0;
    if (start > count - visible) start = std::max(0, count - visible);
    const int end = std::min(count, start + visible);
    for (int index = start; index < end; ++index) {
        const int row = index - start;
        const int y = 30 + row * 18;
        const bool selected = index == selectedWifi;
        const auto& network = wifiNetworks[index];
        const std::uint16_t background = selected ? kPanelBright : kBg;
        if (selected) {
            canvas.fillRoundRect(2, y, 235, 17, 2, background);
            canvas.fillRect(2, y, 4, 17, kCyan);
        } else {
            canvas.drawFastHLine(25, y + 17, 210, kRule);
        }
        const int bars = network.rssi > -55 ? 4 : network.rssi > -65 ? 3
                       : network.rssi > -75 ? 2 : 1;
        for (int bar = 0; bar < 4; ++bar) {
            const int height = 3 + bar * 3;
            if (bar < bars)
                canvas.fillRect(9 + bar * 4, y + 14 - height, 2, height,
                                selected ? kInk : kCyan);
            else
                canvas.drawRect(9 + bar * 4, y + 14 - height, 2, height, kRule);
        }
        canvas.setTextColor(kInk, background);
        canvas.setCursor(31, y + 2);
        canvas.print(shortened(network.ssid, 27));
        canvas.setTextColor(selected ? kInk : kMuted, background);
        canvas.setCursor(188, y + 2);
        canvas.printf("%d", static_cast<int>(network.rssi));
        canvas.setCursor(222, y + 2);
        canvas.print(network.secured ? '*' : ' ');
    }
    drawFooter("^v SELECT  ENTER JOIN  R SCAN  D DEFAULT");
}

void drawPassword()
{
    canvas.fillScreen(kBg);
    drawHeader("SESAME // WIFI KEY");
    canvas.fillRoundRect(8, 29, 224, 78, 5, kPanel);
    canvas.drawRoundRect(8, 29, 224, 78, 5, kMagenta);
    canvas.fillRect(8, 29, 4, 78, kMagenta);
    canvas.setTextColor(kMagenta, kPanel);
    canvas.setCursor(20, 42);
    canvas.print(shortened(wifiTargetSsid, 32));
    canvas.setTextColor(kMuted, kPanel);
    canvas.setCursor(20, 60);
    canvas.print("PASSWORD (RAM ONLY)");
    canvas.fillRoundRect(18, 75, 204, 20, 3, kBg);
    canvas.setTextColor(kInk, kBg);
    canvas.setCursor(25, 82);
    const int shown = std::min(28, static_cast<int>(passwordDraft.length()));
    for (int i = 0; i < shown; ++i) canvas.print('*');
    canvas.setTextColor(kCyan, kBg);
    canvas.print('_');
    canvas.setTextColor(kMuted, kPanel);
    canvas.setCursor(176, 99);
    canvas.printf("%u/63", static_cast<unsigned>(passwordDraft.length()));
    drawFooter("ENTER JOIN  DEL ERASE          ` BACK");
}

void drawLaunch()
{
    const std::uint32_t elapsed = millis() - launchStartedMs;
    const auto& action = kActions[selectedAction];
    canvas.fillScreen(kBg);
    drawGrid(elapsed, action.color);

    const int radius = std::min(48, static_cast<int>(elapsed / 16));
    for (int ring = 0; ring < 3; ++ring) {
        const int r = radius - ring * 11;
        if (r > 2) canvas.drawCircle(120, 63, r, ring == 0 ? action.color : kRule);
    }
    for (int i = 0; i < 18; ++i) {
        const float angle = i * 0.349066f + elapsed * 0.0016f;
        const float distance = 12.0f + ((elapsed / 8 + i * 13) % 55);
        const int x = 120 + static_cast<int>(std::cos(angle) * distance);
        const int y = 62 + static_cast<int>(std::sin(angle) * distance * 0.55f);
        canvas.fillRect(x, y, 2, 2, i % 3 ? action.color : kMagenta);
    }
    drawRobot(91, 40, elapsed * 0.018f, action.color);

    canvas.fillRoundRect(48, 17, 144, 17, 3, kPanel);
    canvas.setTextColor(action.color, kPanel);
    canvas.setCursor(58, 22);
    canvas.print(launchSent ? (launchSucceeded ? "COMMAND ACCEPTED" : "LINK FAILED")
                            : "CHOREOGRAPHY SYNC");
    canvas.fillRoundRect(62, 101, 116, 16, 3, kPanel);
    canvas.setTextColor(kInk, kPanel);
    canvas.setCursor(79, 106);
    canvas.print(action.label);
    const int progress = launchSent ? 112 : std::min(112, static_cast<int>(elapsed * 112 / 950));
    canvas.drawRect(64, 119, 112, 4, kRule);
    if (progress > 0) canvas.fillRect(64, 119, progress, 4,
                                      launchSent && !launchSucceeded ? kRed : action.color);
    drawFooter("SPACE / GO = STOP");
}

void draw()
{
    switch (screen) {
        case Screen::Connecting: drawConnecting(); break;
        case Screen::Discovering: drawDiscovering(); break;
        case Screen::Home: drawHome(); break;
        case Screen::Wifi: drawWifi(); break;
        case Screen::Password: drawPassword(); break;
        case Screen::Launch: drawLaunch(); break;
    }
    canvas.pushSprite(0, 0);
    dirty = false;
    lastDrawMs = millis();
}

bool requestStatus(const IPAddress& address)
{
    if (!validIp(address) || WiFi.status() != WL_CONNECTED) return false;
    WiFiClient client;
    HTTPClient http;
    http.setConnectTimeout(650);
    http.setTimeout(1100);
    if (!http.begin(client, address.toString(), 80, "/api/status")) return false;
    const int code = http.GET();
    const String body = code == HTTP_CODE_OK ? http.getString() : String("");
    http.end();
    if (code != HTTP_CODE_OK || body.indexOf("\"currentCommand\"") < 0) return false;
    robotCommand = jsonString(body, "currentCommand");
    robotFace = jsonString(body, "currentFace");
    return true;
}

bool postCommand(const char* command)
{
    if (!robotOnline || !validIp(robotIp) || WiFi.status() != WL_CONNECTED) {
        statusLine = "ROBOT OFFLINE";
        return false;
    }
    WiFiClient client;
    HTTPClient http;
    http.setConnectTimeout(650);
    http.setTimeout(1200);
    if (!http.begin(client, robotIp.toString(), 80, "/api/command")) {
        statusLine = "HTTP START FAILED";
        return false;
    }
    http.addHeader("Content-Type", "application/json");
    const String body = String("{\"command\":\"") + command + "\"}";
    const int code = http.POST(body);
    http.end();
    const bool ok = code >= 200 && code < 300;
    if (ok) {
        robotCommand = command;
        statusLine = String("SENT ") + command;
        statusFailures = 0;
    } else {
        statusLine = String("HTTP ") + code;
        if (++statusFailures >= 2) robotOnline = false;
    }
    dirty = true;
    return ok;
}

void stopRobot()
{
    movementActive = false;
    activeMovementKey = '\0';
    launchSent = true;
    if (robotOnline) {
        const bool stopped = postCommand("stop");
        statusLine = stopped ? "STOPPED" : "STOP SEND FAILED";
    } else {
        statusLine = "STOP: NO ROBOT LINK";
    }
    if (screen == Screen::Launch) screen = Screen::Home;
    dirty = true;
}

void beginDiscovery()
{
    if (mdnsStarted) {
        MDNS.end();
        mdnsStarted = false;
    }
    robotOnline = false;
    robotIp = IPAddress();
    discoverySource = "mDNS";
    discoveryAttempted = false;
    discoveryStartedMs = millis();
    statusLine = "QUERYING mDNS";
    screen = Screen::Discovering;
    dirty = true;
}

void finishDiscovery(const IPAddress& address, const char* source)
{
    robotIp = address;
    robotOnline = true;
    discoverySource = source;
    statusLine = String(source) + " " + address.toString();
    statusFailures = 0;
    lastStatusPollMs = millis();
    screen = Screen::Home;
    dirty = true;
}

void serviceDiscovery()
{
    if (screen != Screen::Discovering || discoveryAttempted ||
        WiFi.status() != WL_CONNECTED || millis() - discoveryStartedMs < 250) {
        return;
    }
    discoveryAttempted = true;
    dirty = true;
    mdnsStarted = MDNS.begin(kControllerHostname);
    IPAddress resolved;
    if (mdnsStarted) resolved = MDNS.queryHost(kRobotMdnsHost, 1400);
    if (validIp(resolved) && requestStatus(resolved)) {
        finishDiscovery(resolved, "mDNS");
        return;
    }

    if (WiFi.SSID() == kDefaultSsid) {
        const IPAddress gateway = WiFi.gatewayIP();
        if (validIp(gateway) && requestStatus(gateway)) {
            finishDiscovery(gateway, "AP GATEWAY");
            return;
        }
        const IPAddress defaultIp(192, 168, 4, 1);
        if (gateway != defaultIp && requestStatus(defaultIp)) {
            finishDiscovery(defaultIp, "AP FALLBACK");
            return;
        }
    }
    statusLine = "ROBOT NOT FOUND";
    discoverySource = "mDNS MISS";
    dirty = true;
}

void connectWifi(const String& ssid, const String& password, bool isDefault)
{
    if (mdnsStarted) {
        MDNS.end();
        mdnsStarted = false;
    }
    WiFi.scanDelete();
    wifiScanning = false;
    joiningSsid = ssid;
    joiningPassword = password;
    joiningDefault = isDefault;
    joinStartedMs = millis();
    statusLine = "CONNECTING";
    screen = Screen::Connecting;
    WiFi.setAutoReconnect(true);
    WiFi.disconnect(false, false);
    delay(30);
    WiFi.begin(ssid.c_str(), password.c_str());
    dirty = true;
}

void startWifiScan(const String& notice = "")
{
    wifiNetworks.clear();
    selectedWifi = 0;
    wifiNotice = notice;
    WiFi.scanDelete();
    const int result = WiFi.scanNetworks(true, false);
    wifiScanning = result != WIFI_SCAN_FAILED;
    if (!wifiScanning) wifiNotice = "SCAN BUSY - R RETRY";
    scanStartedMs = millis();
    screen = Screen::Wifi;
    dirty = true;
}

void serviceConnection()
{
    if (screen != Screen::Connecting) return;
    if (WiFi.status() == WL_CONNECTED && WiFi.SSID() == joiningSsid) {
        statusLine = "WIFI " + WiFi.localIP().toString();
        beginDiscovery();
        return;
    }
    const wl_status_t value = WiFi.status();
    const bool terminal = millis() - joinStartedMs > 1500 &&
                          (value == WL_CONNECT_FAILED || value == WL_NO_SSID_AVAIL);
    if (!terminal && millis() - joinStartedMs < kWifiTimeoutMs) return;
    WiFi.setAutoReconnect(false);
    WiFi.disconnect(false, false);
    startWifiScan(joiningDefault ? "SESAME AP NOT FOUND" : "JOIN FAILED");
}

void serviceWifiScan()
{
    if (screen != Screen::Wifi || !wifiScanning) return;
    const int found = WiFi.scanComplete();
    if (found == WIFI_SCAN_RUNNING) {
        if (millis() - scanStartedMs > 12000) {
            WiFi.scanDelete();
            wifiScanning = false;
            wifiNotice = "SCAN TIMED OUT";
            dirty = true;
        }
        return;
    }
    wifiScanning = false;
    if (found < 0) {
        wifiNotice = "SCAN FAILED";
        dirty = true;
        return;
    }
    for (int index = 0; index < found; ++index) {
        const String ssid = WiFi.SSID(index);
        if (!ssid.length()) continue;
        const int rssi = WiFi.RSSI(index);
        bool merged = false;
        for (auto& network : wifiNetworks) {
            if (network.ssid != ssid) continue;
            if (rssi > network.rssi) network.rssi = static_cast<std::int8_t>(rssi);
            merged = true;
            break;
        }
        if (merged) continue;
        WifiNetwork network;
        network.ssid = ssid;
        network.rssi = static_cast<std::int8_t>(constrain(rssi, -127, 0));
        network.secured = WiFi.encryptionType(index) != WIFI_AUTH_OPEN;
        wifiNetworks.push_back(network);
    }
    std::sort(wifiNetworks.begin(), wifiNetworks.end(),
              [](const WifiNetwork& left, const WifiNetwork& right) {
                  return left.rssi > right.rssi;
              });
    if (wifiNetworks.size() > kMaxWifiNetworks) wifiNetworks.resize(kMaxWifiNetworks);
    for (std::size_t index = 0; index < wifiNetworks.size(); ++index) {
        if (wifiNetworks[index].ssid == kDefaultSsid) {
            selectedWifi = static_cast<int>(index);
            break;
        }
    }
    WiFi.scanDelete();
    if (wifiNetworks.empty()) wifiNotice = "NO NETWORKS FOUND";
    dirty = true;
}

void chooseWifi()
{
    if (wifiNetworks.empty()) return;
    const auto& network = wifiNetworks[selectedWifi];
    wifiTargetSsid = network.ssid;
    passwordDraft = "";
    if (network.ssid == kDefaultSsid) {
        connectWifi(network.ssid, kDefaultPassword, true);
    } else if (!network.secured) {
        connectWifi(network.ssid, "", false);
    } else {
        screen = Screen::Password;
        dirty = true;
    }
}

void startLaunch()
{
    if (!robotOnline) {
        statusLine = "ROBOT OFFLINE - R FIND";
        dirty = true;
        return;
    }
    launchStartedMs = millis();
    launchSent = false;
    launchSucceeded = false;
    screen = Screen::Launch;
    dirty = true;
}

void serviceLaunch()
{
    if (screen != Screen::Launch) return;
    const std::uint32_t elapsed = millis() - launchStartedMs;
    if (!launchSent && elapsed >= 950) {
        launchSent = true;
        launchSucceeded = postCommand(kActions[selectedAction].command);
        dirty = true;
    }
    if (launchSent && elapsed >= 1800) {
        screen = Screen::Home;
        dirty = true;
    }
}

const char* movementForKey(char key)
{
    switch (key) {
        case 'w': return "forward";
        case 'a': return "left";
        case 's': return "backward";
        case 'd': return "right";
        default: return nullptr;
    }
}

char movementKeyForInput(char key, bool fn)
{
    if (!fn) return key;

    // Cardputer ADV prints its arrows on Fn + ; , . /. Mirror WASD while
    // leaving the unmodified punctuation keys available for pose navigation.
    switch (key) {
        case ';': return 'w';
        case ',': return 'a';
        case '.': return 's';
        case '/': return 'd';
        default: return key;
    }
}

template <typename KeysState>
bool movementKeyHeld(const KeysState& keys, char expected)
{
    for (char key : keys.word) {
        if (key >= 'A' && key <= 'Z') {
            key = static_cast<char>(key - 'A' + 'a');
        }
        if (movementKeyForInput(key, keys.fn) == expected) return true;
    }
    return false;
}

void startMovement(char key)
{
    const char* command = movementForKey(key);
    if (!command || movementActive || !robotOnline) return;
    if (postCommand(command)) {
        movementActive = true;
        activeMovementKey = key;
        movementName = String("HOLD ") + command;
    }
    dirty = true;
}

template <typename KeysState>
char inputCharacter(const KeysState& keys)
{
    if (keys.enter) return '\n';
    if (keys.del) return '\b';
    if (keys.space) return ' ';
    if (!keys.word.empty()) return keys.word.front();
    return '\0';
}

void serviceInput()
{
    if (M5Cardputer.BtnA.wasPressed()) {
        stopRobot();
        return;
    }
    if (!M5Cardputer.Keyboard.isChange()) return;
    const auto& keys = M5Cardputer.Keyboard.keysState();
    if (!M5Cardputer.Keyboard.isPressed()) {
        if (movementActive) stopRobot();
        return;
    }
    char key = inputCharacter(keys);
    if (key >= 'A' && key <= 'Z') key = static_cast<char>(key - 'A' + 'a');

    if (screen == Screen::Password) {
        if (key == '`') {
            passwordDraft = "";
            screen = Screen::Wifi;
        } else if (key == '\n') {
            connectWifi(wifiTargetSsid, passwordDraft, false);
        } else if (key == '\b') {
            if (passwordDraft.length()) passwordDraft.remove(passwordDraft.length() - 1);
        } else if (key >= 32 && key < 127 && passwordDraft.length() < 63) {
            passwordDraft += inputCharacter(keys);
        }
        dirty = true;
        return;
    }

    if (keys.space) {
        stopRobot();
        return;
    }

    if (screen == Screen::Home) {
        if (movementActive) {
            if (!movementKeyHeld(keys, activeMovementKey)) stopRobot();
            return;
        }
        const char movementKey = movementKeyForInput(key, keys.fn);
        if (movementForKey(movementKey)) startMovement(movementKey);
        else if (key == ';') selectedAction = (selectedAction + kActionCount - 1) % kActionCount;
        else if (key == '.') selectedAction = (selectedAction + 1) % kActionCount;
        else if (key == ',') selectedAction = std::max(0, selectedAction - 5);
        else if (key == '/') selectedAction = std::min(kActionCount - 1, selectedAction + 5);
        else if (key == '\n') startLaunch();
        else if (key == 'n') startWifiScan();
        else if (key == 'r') beginDiscovery();
    } else if (screen == Screen::Wifi) {
        const int count = static_cast<int>(wifiNetworks.size());
        if (key == '`') {
            WiFi.scanDelete();
            wifiScanning = false;
            if (robotOnline) {
                screen = Screen::Home;
            } else if (WiFi.status() == WL_CONNECTED) {
                beginDiscovery();
            } else {
                connectWifi(kDefaultSsid, kDefaultPassword, true);
            }
        } else if (key == 'r') {
            startWifiScan();
        } else if (key == 'd') {
            connectWifi(kDefaultSsid, kDefaultPassword, true);
        } else if (key == ';' && count) {
            selectedWifi = selectedWifi > 0 ? selectedWifi - 1 : count - 1;
        } else if (key == '.' && count) {
            selectedWifi = (selectedWifi + 1) % count;
        } else if (key == '\n') {
            chooseWifi();
        }
    } else if (screen == Screen::Connecting) {
        if (key == 'n') startWifiScan();
    } else if (screen == Screen::Discovering) {
        if (key == 'r') beginDiscovery();
        else if (key == 'n') startWifiScan();
        else if (key == '`') screen = Screen::Home;
    } else if (screen == Screen::Launch && key == '`') {
        screen = Screen::Home;
    }
    dirty = true;
}

void serviceStatus()
{
    if (screen != Screen::Home || !robotOnline || movementActive ||
        millis() - lastStatusPollMs < kStatusPollMs) {
        return;
    }
    lastStatusPollMs = millis();
    if (requestStatus(robotIp)) {
        statusFailures = 0;
    } else if (++statusFailures >= 2) {
        robotOnline = false;
        statusLine = "ROBOT LINK LOST - R FIND";
    }
    dirty = true;
}

void fail(const char* message)
{
    M5Cardputer.Display.fillScreen(TFT_BLACK);
    M5Cardputer.Display.setTextColor(TFT_RED, TFT_BLACK);
    M5Cardputer.Display.setTextFont(1);
    M5Cardputer.Display.setCursor(8, 20);
    M5Cardputer.Display.println(message);
    Serial.println(message);
}

}  // namespace

void setup()
{
    Serial.begin(115200);
    auto config = M5.config();
    config.internal_mic = false;
    config.internal_spk = false;
    M5Cardputer.begin(config, true);
    M5Cardputer.Display.setRotation(1);
    M5Cardputer.Display.setBrightness(160);
    canvas.setColorDepth(16);
    if (canvas.createSprite(kScreenWidth, kScreenHeight) == nullptr) {
        fail("Unable to allocate display canvas");
        return;
    }

    const auto board = M5.getBoard();
    if (board != m5::board_t::board_M5CardputerADV &&
        board != m5::board_t::board_M5Cardputer) {
        fail("Cardputer / Cardputer ADV required");
        return;
    }

    WiFi.persistent(false);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    WiFi.setHostname(kControllerHostname);
    ready = true;
    Serial.printf("%s ready; board=%d heap=%u\n", kBuildIdentity,
                  static_cast<int>(board), ESP.getFreeHeap());
    connectWifi(kDefaultSsid, kDefaultPassword, true);
    draw();
}

void loop()
{
    if (!ready) {
        delay(50);
        return;
    }
    M5Cardputer.update();
    serviceInput();
    serviceConnection();
    serviceWifiScan();
    serviceDiscovery();
    serviceLaunch();
    serviceStatus();

    const std::uint32_t period =
        screen == Screen::Launch ? 33
        : (screen == Screen::Connecting || screen == Screen::Discovering ||
           (screen == Screen::Wifi && wifiScanning) || screen == Screen::Home)
              ? 100
              : 250;
    if (dirty || millis() - lastDrawMs >= period) draw();
    delay(3);
}
