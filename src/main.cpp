#include <QApplication>
#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSlider>
#include <QLineEdit>
#include <QScrollArea>
#include <QScrollBar>
#include <QRegularExpression>
#include <QMouseEvent>
#include <QKeyEvent>
#include <QResizeEvent>
#include <QPainter>
#include <QTimer>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>

#include <QtGui/qwindow.h>
#include <QtGui/QWindow>
#include <QtGui/QScreen>
#include <QtGui/QGuiApplication>

#include <httplib.h>
#include <nlohmann/json.hpp>

#include <iostream>
#include <string>
#include <vector>
#include <deque>
#include <thread>
#include <chrono>
#include <regex>
#include <set>
#include <map>
#include <fstream>
#include <atomic>
#include <mutex>
#include <filesystem>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
    #include <windows.h>
    #include <winsock2.h>
    #include <ws2tcpip.h>
    #include <winhttp.h>
    #pragma comment(lib, "ws2_32.lib")
    #pragma comment(lib, "winhttp.lib")
#else
    #include <curl/curl.h>
    #include <sys/socket.h>
    #include <arpa/inet.h>
    #include <netdb.h>
    #include <unistd.h>
#endif

using json = nlohmann::json;
namespace fs = std::filesystem;

static constexpr size_t MAX_QUEUE_SIZE     = 1000;
static constexpr size_t MAX_MESSAGE_LOG    = 1000;
static constexpr size_t MAX_SEEN_YT_IDS    = 2000;

struct ChatItem {
    uint64_t id;
    std::string platform;
    std::string user;
    std::string text;
    std::string msgId;
};

// =============================================================
// UTILIDADES YOUTUBE
// =============================================================
static bool isValidVideoId(const std::string& s) {
    if (s.size() != 11) return false;
    for (char c : s) {
        if (!std::isalnum((unsigned char)c) && c != '_' && c != '-') return false;
    }
    return true;
}

static std::string extractVideoId(const std::string& url) {
    const std::vector<std::string> patterns = {
        "youtube.com/watch?v=",
        "youtube.com/live/",
        "youtube.com/shorts/",
        "youtube.com/embed/",
        "youtube.com/v/",
        "youtu.be/"
    };
    for (const auto& p : patterns) {
        size_t pos = url.find(p);
        if (pos != std::string::npos) {
            size_t start = pos + p.size();
            if (start + 11 <= url.size()) {
                std::string id = url.substr(start, 11);
                if (isValidVideoId(id)) return id;
            }
        }
    }
    size_t vPos = url.find("v=");
    if (vPos != std::string::npos) {
        size_t start = vPos + 2;
        if (start + 11 <= url.size()) {
            std::string id = url.substr(start, 11);
            if (isValidVideoId(id)) return id;
        }
    }
    if (isValidVideoId(url)) return url;
    return "";
}

// Fallback: extraer videoId del HTML de la pagina final (sigue redirects de /@canal/live)
static std::string extractVideoIdFromHtml(const std::string& html) {
    // Metodo 1: og:url (canonico, apunta al video real)
    size_t ogPos = html.find("<meta property=\"og:url\" content=\"");
    if (ogPos != std::string::npos) {
        size_t start = ogPos + 34;
        size_t end = html.find('"', start);
        if (end != std::string::npos) {
            std::string ogUrl = html.substr(start, end - start);
            size_t vp = ogUrl.find("v=");
            if (vp != std::string::npos && vp + 2 + 11 <= ogUrl.size()) {
                std::string id = ogUrl.substr(vp + 2, 11);
                if (isValidVideoId(id)) return id;
            }
            size_t lp = ogUrl.find("/live/");
            if (lp != std::string::npos && lp + 6 + 11 <= ogUrl.size()) {
                std::string id = ogUrl.substr(lp + 6, 11);
                if (isValidVideoId(id)) return id;
            }
        }
    }
    // Metodo 2: "videoId":"..." (menos confiable)
    size_t pos = html.find("\"videoId\":\"");
    if (pos != std::string::npos && pos + 11 + 11 <= html.size()) {
        std::string cand = html.substr(pos + 11, 11);
        if (isValidVideoId(cand)) return cand;
    }
    return "";
}

// =============================================================
// PLANTILLA WEB TRANSPARENTE PARA OBS STUDIO
// =============================================================
const std::string OBS_OVERLAY_HTML = R"html(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <title>NativeChats OBS Overlay</title>
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    :root { --obs-font-size: 20px; --obs-line-spacing: 13px; }
    html, body {
      background: transparent !important;
      color: #fff;
      height: 100vh; width: 100vw;
      overflow: hidden;
      display: flex; flex-direction: column; justify-content: flex-end;
      font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Helvetica, Arial, sans-serif;
      font-size: var(--obs-font-size);
      line-height: 1.4;
      user-select: none;
    }
    #chat-list {
      width: 100%;
      display: flex; flex-direction: column; justify-content: flex-end;
      gap: var(--obs-line-spacing);
      padding: 10px;
    }
    @keyframes slideInBounce {
      0% { opacity: 0; transform: translateX(40px); }
      70% { opacity: 1; transform: translateX(-3px); }
      100% { opacity: 1; transform: translateX(0); }
    }
    .msg-item {
      animation: slideInBounce 0.28s cubic-bezier(0.175, 0.885, 0.32, 1.275) forwards;
      word-break: break-word;
      width: 100%;
      text-shadow: 1px 1px 2px #000, -1px -1px 2px #000, 1px -1px 2px #000, -1px 1px 2px #000, 0 0 3px #000;
    }
    .badge {
      font-weight: bold;
      font-size: 0.7em;
      padding: 2px 6px;
      border-radius: 3px;
      margin-right: 6px;
      display: inline-block;
      vertical-align: middle;
      line-height: 1;
      text-shadow: none;
    }
    .badge-tw { background: #9146FF; color: #fff; }
    .badge-yt { background: #FF0000; color: #fff; }
    .user { color: #58a6ff; font-weight: bold; margin-right: 4px; }
    .text { color: #ffffff; }
    .text a { color: #58a6ff; text-decoration: underline; }
    .custom-emote { height: 1.3em; vertical-align: middle; margin: 0 2px; display: inline-block; }
  </style>
</head>
<body>
  <div id="chat-list"></div>
  <script>
    let maxVisibleMessages = 6;
    let pacingIntervalMs = 1500;
    const twQueue = [];
    const ytQueue = [];
    let lastPlatformServed = "YT";
    const MAX_QUEUE = 1000;
    const list = document.getElementById('chat-list');

    function scheduleNextMessage() {
      let data = null;
      if (twQueue.length > 0 && ytQueue.length > 0) {
        if (lastPlatformServed === "TW") { data = ytQueue.shift(); lastPlatformServed = "YT"; }
        else { data = twQueue.shift(); lastPlatformServed = "TW"; }
      } else if (twQueue.length > 0) { data = twQueue.shift(); lastPlatformServed = "TW"; }
      else if (ytQueue.length > 0) { data = ytQueue.shift(); lastPlatformServed = "YT"; }
      if (data) renderSingleMessage(data);
      setTimeout(scheduleNextMessage, pacingIntervalMs);
    }
    setTimeout(scheduleNextMessage, pacingIntervalMs);

    function renderSingleMessage(data) {
      const item = document.createElement('div');
      item.className = 'msg-item';
      const badgeClass = data.platform === 'TW' ? 'badge-tw' : 'badge-yt';
      item.innerHTML = `
        <span class="badge ${badgeClass}">${data.platform}</span>
        <span class="user">${data.user}:</span>
        <span class="text">${data.text}</span>
      `;
      list.appendChild(item);
      while (list.children.length > maxVisibleMessages) list.removeChild(list.firstChild);
    }

    let lastMsgId = -1;
    let bootstrapped = false;

    async function bootstrap() {
      try {
        const r = await fetch('/api/last-id');
        if (r.ok) { const j = await r.json(); lastMsgId = j.id || 0; bootstrapped = true; }
      } catch (e) {}
      if (!bootstrapped) setTimeout(bootstrap, 500); else fetchMessages();
    }
    bootstrap();

    async function fetchMessages() {
      try {
        const res = await fetch('/api/messages?since=' + lastMsgId);
        if (res.ok) {
          const msgs = await res.json();
          for (const m of msgs) {
            if (m.id > lastMsgId) lastMsgId = m.id;
            if (m.platform === 'TW') { twQueue.push(m); if (twQueue.length > MAX_QUEUE) twQueue.shift(); }
            else { ytQueue.push(m); if (ytQueue.length > MAX_QUEUE) ytQueue.shift(); }
          }
        }
      } catch (e) {}
      setTimeout(fetchMessages, 120);
    }

    async function syncObsSettings() {
      try {
        const r = await fetch('/api/config');
        if (r.ok) {
          const cfg = await r.json();
          if (cfg.maxObsMessages) maxVisibleMessages = cfg.maxObsMessages;
          if (cfg.pacingInterval) pacingIntervalMs = cfg.pacingInterval;
          if (cfg.fontSize) document.documentElement.style.setProperty('--obs-font-size', cfg.fontSize + 'px');
          if (cfg.lineSpacing) document.documentElement.style.setProperty('--obs-line-spacing', cfg.lineSpacing + 'px');
        }
      } catch(e) {}
      setTimeout(syncObsSettings, 250);
    }
    syncObsSettings();
  </script>
</body>
</html>
)html";

// ==========================================
// BACKEND MULTIHILO
// ==========================================
class ChatBackend : public QObject {
    Q_OBJECT
public:
    std::string twitchChannel = "";
    std::string youtubeUrl    = "";
    std::atomic<bool> running{true};
    std::atomic<uint64_t> configVersion{1};
    std::mutex configMutex;

    std::atomic<int> pacingInterval{1500};
    std::atomic<int> fontSize{20};
    std::atomic<int> lineSpacing{13};
    std::atomic<int> maxObsMessages{6};

    std::atomic<bool> twitchConnected{false};
    std::atomic<bool> youtubeConnected{false};

    std::deque<ChatItem> messageLog;
    std::mutex messageMutex;
    std::atomic<uint64_t> nextId{1};

    httplib::Server svr;
    std::string cacheDir;

    std::map<std::string, std::string> globalEmotes;

    ChatBackend() {
#ifdef _WIN32
        char tempPath[MAX_PATH];
        GetTempPathA(MAX_PATH, tempPath);
        std::string tPath = tempPath;
        std::replace(tPath.begin(), tPath.end(), '\\', '/');
        if (!tPath.empty() && tPath.back() != '/') tPath += '/';
        cacheDir = tPath + "nativechats_emotes/";
#else
        cacheDir = "/tmp/nativechats_emotes/";
#endif
        try { fs::create_directories(cacheDir); } catch (...) {}

        globalEmotes["catJAM"] = "https://cdn.betterttv.net/emote/5f1b0186cf6d2144653d2970/2x";
        globalEmotes["PepeLaugh"] = "https://cdn.betterttv.net/emote/5c548025009a2e73916b3a37/2x";
        globalEmotes["KEKW"] = "https://cdn.betterttv.net/emote/5e9c6c187e090362f8b0b9e8/2x";
        globalEmotes["MonkaS"] = "https://cdn.betterttv.net/emote/56e9f494fff3cc5c35e5287e/2x";
        globalEmotes["monkaW"] = "https://cdn.betterttv.net/emote/59e5f928e4695b28d61e479d/2x";
        globalEmotes["LUL"] = "https://static-cdn.jtvnw.net/emoticons/v2/425618/default/dark/2.0";
        globalEmotes["PogChamp"] = "https://static-cdn.jtvnw.net/emoticons/v2/305954156/default/dark/2.0";
        globalEmotes["Kappa"] = "https://static-cdn.jtvnw.net/emoticons/v2/25/default/dark/2.0";
        globalEmotes["Aware"] = "https://cdn.7tv.app/emote/6145dc24e93d8b5a8d46db1d/2x.webp";
        globalEmotes["COGGERS"] = "https://cdn.betterttv.net/emote/5af84b9e766736477265a6f2/2x";
        globalEmotes["BatChest"] = "https://cdn.betterttv.net/emote/5ba5a8946ee0c2049d5c3127/2x";
        globalEmotes["GIGACHAD"] = "https://cdn.7tv.app/emote/60ae3e542597ab5215c0e1ae/2x.webp";
        globalEmotes["Clueless"] = "https://cdn.7tv.app/emote/613d5267b2d4c0628e937d57/2x.webp";
        globalEmotes["ResidentSleeper"] = "https://static-cdn.jtvnw.net/emoticons/v2/245/default/dark/2.0";
        globalEmotes["BibleThump"] = "https://static-cdn.jtvnw.net/emoticons/v2/86/default/dark/2.0";
        globalEmotes["xqcL"] = "https://static-cdn.jtvnw.net/emoticons/v2/114836/default/dark/2.0";
        globalEmotes["DuckerZ"] = "https://cdn.betterttv.net/emote/5b47a07054f24f4e72ea952d/2x";
    }

    void start() {
        std::thread([this]() { twitchLoop(); }).detach();
        std::thread([this]() { youtubeLoop(); }).detach();
        std::thread([this]() { httpLoop(); }).detach();
    }

    void stop() {
        running.store(false);
        configVersion.fetch_add(1);
        svr.stop();
    }

    void pushMessage(const std::string& platform, const std::string& user, const std::string& text, const std::string& msgId = "") {
        ChatItem item;
        item.id = nextId.fetch_add(1);
        item.platform = platform;
        item.user = user;
        item.text = text;
        item.msgId = msgId;

        {
            std::lock_guard<std::mutex> lock(messageMutex);
            messageLog.push_back(item);
            if (messageLog.size() > MAX_MESSAGE_LOG) messageLog.pop_front();
        }

        emit messageReceived(item.id, QString::fromStdString(platform), QString::fromStdString(user),
                             QString::fromStdString(text), QString::fromStdString(msgId));
    }

    std::string getCachedEmoteFilename(const std::string& imgUrl) {
        if (imgUrl.empty()) return "";
        std::size_t hash = std::hash<std::string>{}(imgUrl);
        std::string filename = std::to_string(hash) + ".png";
        std::string fullPath = cacheDir + filename;

        if (fs::exists(fullPath)) return filename;

        std::string data = httpGet(imgUrl);
        if (!data.empty()) {
            std::ofstream out(fullPath, std::ios::binary);
            if (out.is_open()) {
                out.write(data.data(), data.size());
                return filename;
            }
        }
        return "";
    }

signals:
    void messageReceived(quint64 id, QString platform, QString user, QString text, QString msgId);
    void statusChanged(bool twConnected, bool ytConnected);

private:
    static size_t CurlWrite(void* contents, size_t size, size_t nmemb, void* userp) {
        ((std::string*)userp)->append((char*)contents, size * nmemb);
        return size * nmemb;
    }

    std::string httpGet(const std::string& url) {
#ifdef _WIN32
        std::string response = "";
        URL_COMPONENTS urlComp{};
        urlComp.dwStructSize = sizeof(urlComp);
        wchar_t hostName[256] = {0};
        wchar_t urlPath[2048] = {0};
        urlComp.lpszHostName = hostName;
        urlComp.dwHostNameLength = 256;
        urlComp.lpszUrlPath = urlPath;
        urlComp.dwUrlPathLength = 2048;

        std::wstring wurl(url.begin(), url.end());
        if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &urlComp)) return "";

        HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/124.0.0.0 Safari/537.36",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
        if (!hSession) return "";
        HINTERNET hConnect = WinHttpConnect(hSession, urlComp.lpszHostName, urlComp.nPort, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }
        DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlComp.lpszUrlPath, NULL, NULL, NULL, flags);
        if (hRequest) {
            DWORD opt = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
            WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &opt, sizeof(opt));
            LPCWSTR headers = L"Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\nAccept-Encoding: identity\r\n";
            WinHttpAddRequestHeaders(hRequest, headers, -1L, WINHTTP_ADDREQ_FLAG_ADD);
            if (WinHttpSendRequest(hRequest, NULL, 0, NULL, 0, 0, 0) && WinHttpReceiveResponse(hRequest, NULL)) {
                DWORD dwSize = 0, dwDownloaded = 0;
                do {
                    dwSize = 0;
                    if (!WinHttpQueryDataAvailable(hRequest, &dwSize) || dwSize == 0) break;
                    std::string buf(dwSize, '\0');
                    if (WinHttpReadData(hRequest, &buf[0], dwSize, &dwDownloaded)) {
                        buf.resize(dwDownloaded);
                        response += buf;
                    }
                } while (dwSize > 0);
            }
            WinHttpCloseHandle(hRequest);
        }
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return response;
#else
        CURL* curl = curl_easy_init();
        if (!curl) return "";
        std::string response = "";
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWrite);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64) Chrome/124.0.0.0 Safari/537.36");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
        return response;
#endif
    }

    std::string httpPostJson(const std::string& url, const std::string& body) {
#ifdef _WIN32
        std::string response = "";
        URL_COMPONENTS urlComp{};
        urlComp.dwStructSize = sizeof(urlComp);
        wchar_t hostName[256] = {0};
        wchar_t urlPath[2048] = {0};
        urlComp.lpszHostName = hostName;
        urlComp.dwHostNameLength = 256;
        urlComp.lpszUrlPath = urlPath;
        urlComp.dwUrlPathLength = 2048;

        std::wstring wurl(url.begin(), url.end());
        if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &urlComp)) return "";

        HINTERNET hSession = WinHttpOpen(L"Mozilla/5.0 (Windows NT 10.0; Win64; x64) Chrome/124.0.0.0 Safari/537.36",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, NULL, NULL, 0);
        if (!hSession) return "";
        HINTERNET hConnect = WinHttpConnect(hSession, urlComp.lpszHostName, urlComp.nPort, 0);
        if (!hConnect) { WinHttpCloseHandle(hSession); return ""; }
        DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
        HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"POST", urlComp.lpszUrlPath, NULL, NULL, NULL, flags);
        if (hRequest) {
            DWORD opt = WINHTTP_OPTION_REDIRECT_POLICY_ALWAYS;
            WinHttpSetOption(hRequest, WINHTTP_OPTION_REDIRECT_POLICY, &opt, sizeof(opt));
            LPCWSTR headers = L"Content-Type: application/json\r\nAccept: application/json\r\n";
            WinHttpAddRequestHeaders(hRequest, headers, -1L, WINHTTP_ADDREQ_FLAG_ADD);
            if (WinHttpSendRequest(hRequest, NULL, 0, (LPVOID)body.data(), (DWORD)body.size(), (DWORD)body.size(), 0)
                && WinHttpReceiveResponse(hRequest, NULL)) {
                DWORD dwSize = 0, dwDownloaded = 0;
                do {
                    dwSize = 0;
                    if (!WinHttpQueryDataAvailable(hRequest, &dwSize) || dwSize == 0) break;
                    std::string buf(dwSize, '\0');
                    if (WinHttpReadData(hRequest, &buf[0], dwSize, &dwDownloaded)) {
                        buf.resize(dwDownloaded);
                        response += buf;
                    }
                } while (dwSize > 0);
            }
            WinHttpCloseHandle(hRequest);
        }
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return response;
#else
        CURL* curl = curl_easy_init();
        if (!curl) return "";
        std::string response = "";
        struct curl_slist* headers = NULL;
        headers = curl_slist_append(headers, "Content-Type: application/json");
        curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWrite);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64) Chrome/124.0.0.0 Safari/537.36");
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POST, 1L);
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)body.size());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
        curl_easy_perform(curl);
        curl_slist_free_all(headers);
        curl_easy_cleanup(curl);
        return response;
#endif
    }

    std::string processTwitchEmotes(const std::string& rawMsg, const std::string& tags) {
        std::string msg = rawMsg;
        struct EmoteSpan { size_t start; size_t end; std::string id; };
        std::vector<EmoteSpan> spans;

        size_t emoPos = tags.find("emotes=");
        if (emoPos != std::string::npos) {
            emoPos += 7;
            size_t endEmo = tags.find(';', emoPos);
            std::string emoData = tags.substr(emoPos, (endEmo == std::string::npos ? tags.size() : endEmo) - emoPos);
            if (!emoData.empty()) {
                std::istringstream stream(emoData);
                std::string token;
                while (std::getline(stream, token, '/')) {
                    size_t colon = token.find(':');
                    if (colon != std::string::npos) {
                        std::string emoteId = token.substr(0, colon);
                        std::string occurrences = token.substr(colon + 1);
                        std::istringstream occStream(occurrences);
                        std::string occ;
                        while (std::getline(occStream, occ, ',')) {
                            size_t dash = occ.find('-');
                            if (dash != std::string::npos) {
                                try {
                                    size_t s = std::stoull(occ.substr(0, dash));
                                    size_t e = std::stoull(occ.substr(dash + 1));
                                    spans.push_back({s, e, emoteId});
                                } catch (...) {}
                            }
                        }
                    }
                }
            }
        }

        std::sort(spans.begin(), spans.end(), [](const EmoteSpan& a, const EmoteSpan& b) {
            return a.start > b.start;
        });

        for (const auto& sp : spans) {
            if (sp.start < msg.size() && sp.end < msg.size() && sp.start <= sp.end) {
                std::string url = "https://static-cdn.jtvnw.net/emoticons/v2/" + sp.id + "/default/dark/2.0";
                std::string emoteFile = getCachedEmoteFilename(url);
                if (!emoteFile.empty()) {
                    std::string imgTag = " <img src=\"/emotes/" + emoteFile + "\" height=\"18\" style=\"vertical-align:middle;\"> ";
                    msg.replace(sp.start, sp.end - sp.start + 1, imgTag);
                }
            }
        }

        std::istringstream iss(msg);
        std::string word;
        std::string finalMsg = "";
        while (iss >> word) {
            if (word.rfind("<img", 0) == 0) { finalMsg += word + " "; continue; }
            auto it = globalEmotes.find(word);
            if (it != globalEmotes.end()) {
                std::string emoteFile = getCachedEmoteFilename(it->second);
                if (!emoteFile.empty()) {
                    finalMsg += "<img src=\"/emotes/" + emoteFile + "\" height=\"18\" style=\"vertical-align:middle;\"> ";
                } else {
                    finalMsg += word + " ";
                }
            } else {
                finalMsg += word + " ";
            }
        }
        if (!finalMsg.empty() && finalMsg.back() == ' ') finalMsg.pop_back();
        return finalMsg.empty() ? msg : finalMsg;
    }

    void twitchLoop() {
        uint64_t lastVer = 0;
        std::string channel = "";

        while (running.load()) {
            uint64_t curVer = configVersion.load();
            if (curVer != lastVer) {
                lastVer = curVer;
                std::lock_guard<std::mutex> lock(configMutex);
                channel = twitchChannel;
            }

            if (!channel.empty() && channel[0] == '#') channel.erase(0, 1);
            channel.erase(0, channel.find_first_not_of(" \t\r\n"));
            if (!channel.empty()) channel.erase(channel.find_last_not_of(" \t\r\n") + 1);
            std::transform(channel.begin(), channel.end(), channel.begin(), ::tolower);

            if (channel.empty()) {
                twitchConnected.store(false);
                emit statusChanged(false, youtubeConnected.load());
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }

            struct addrinfo hints{}, *res;
            hints.ai_family = AF_INET;
            hints.ai_socktype = SOCK_STREAM;

            if (getaddrinfo("irc.chat.twitch.tv", "6667", &hints, &res) != 0) {
                twitchConnected.store(false);
                emit statusChanged(false, youtubeConnected.load());
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }

            int sock = socket(res->ai_family, res->ai_socktype, res->ai_protocol);
            if (::connect(sock, res->ai_addr, res->ai_addrlen) != 0) {
                twitchConnected.store(false);
                emit statusChanged(false, youtubeConnected.load());
                freeaddrinfo(res);
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }
            freeaddrinfo(res);

            std::string login = "CAP REQ :twitch.tv/tags twitch.tv/commands\r\n"
                                "PASS oauth:justinfan12345\r\n"
                                "NICK justinfan12345\r\n"
                                "JOIN #" + channel + "\r\n";
            send(sock, login.c_str(), login.length(), 0);
            twitchConnected.store(true);
            emit statusChanged(true, youtubeConnected.load());
            std::cout << "[Twitch] Conectado a #" << channel << std::endl;

            std::string tcpBuffer = "";
            char buffer[4096];

            while (running.load() && configVersion.load() == lastVer) {
#ifdef _WIN32
                DWORD timeout = 1000;
                setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&timeout, sizeof timeout);
#else
                struct timeval tv;
                tv.tv_sec = 1; tv.tv_usec = 0;
                setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, (const char*)&tv, sizeof tv);
#endif
                int bytes = recv(sock, buffer, sizeof(buffer) - 1, 0);
                if (bytes <= 0) continue;
                buffer[bytes] = '\0';
                tcpBuffer.append(buffer, bytes);

                size_t pos = 0;
                while ((pos = tcpBuffer.find('\n')) != std::string::npos) {
                    std::string line = tcpBuffer.substr(0, pos);
                    tcpBuffer.erase(0, pos + 1);
                    if (!line.empty() && line.back() == '\r') line.pop_back();
                    if (line.empty()) continue;

                    if (line.rfind("PING", 0) == 0) {
                        std::string pong = "PONG :tmi.twitch.tv\r\n";
                        send(sock, pong.c_str(), pong.length(), 0);
                        continue;
                    }

                    std::string tags = "", msgLine = line, msgId = "";
                    if (msgLine[0] == '@') {
                        size_t space = msgLine.find(' ');
                        if (space != std::string::npos) {
                            tags = msgLine.substr(1, space - 1);
                            msgLine = msgLine.substr(space + 1);
                            size_t idPos = tags.find("id=");
                            if (idPos != std::string::npos) {
                                idPos += 3;
                                size_t endId = tags.find(';', idPos);
                                msgId = tags.substr(idPos, (endId == std::string::npos ? tags.size() : endId) - idPos);
                            }
                        }
                    }

                    if (msgLine.find("PRIVMSG") != std::string::npos) {
                        size_t exMark = msgLine.find('!');
                        size_t msgStart = msgLine.find(" :", 1);
                        if (exMark != std::string::npos && msgStart != std::string::npos) {
                            std::string user = msgLine.substr(1, exMark - 1);
                            std::string rawMessage = msgLine.substr(msgStart + 2);

                            if (rawMessage.size() >= 8 && rawMessage.substr(0, 8) == "\x01" "ACTION ") {
                                rawMessage = rawMessage.substr(8);
                                if (!rawMessage.empty() && rawMessage.back() == '\x01') rawMessage.pop_back();
                            }

                            std::string formattedMsg = processTwitchEmotes(rawMessage, tags);
                            pushMessage("TW", user, formattedMsg, msgId);
                        }
                    }
                }
            }

            twitchConnected.store(false);
            emit statusChanged(false, youtubeConnected.load());

#ifdef _WIN32
            closesocket(sock);
#else
            close(sock);
#endif
        }
    }

    void youtubeLoop() {
        std::set<std::string> seenIds;
        std::deque<std::string> seenOrder;
        std::string videoId = "";
        std::string continuation = "";
        std::string apiKey = "";
        uint64_t lastVer = 0;
        std::string currentUrl = "";

        auto rememberId = [&](const std::string& id) {
            if (seenIds.insert(id).second) {
                seenOrder.push_back(id);
                if (seenOrder.size() > MAX_SEEN_YT_IDS) {
                    seenIds.erase(seenOrder.front());
                    seenOrder.pop_front();
                }
            }
        };

        auto processActions = [&](json& actions) {
            for (auto& action : actions) {
                if (!action.contains("addChatItemAction")) continue;
                auto& item = action["addChatItemAction"]["item"];
                if (!item.contains("liveChatTextMessageRenderer")) continue;
                auto& msg = item["liveChatTextMessageRenderer"];
                if (!msg.contains("id")) continue;
                std::string id = msg["id"].get<std::string>();
                if (seenIds.count(id)) continue;
                rememberId(id);

                std::string author = "?";
                if (msg.contains("authorName") && msg["authorName"].contains("simpleText")) {
                    author = msg["authorName"]["simpleText"].get<std::string>();
                }

                std::string text = "";
                if (msg.contains("message") && msg["message"].contains("runs")) {
                    for (auto& run : msg["message"]["runs"]) {
                        if (run.contains("text")) {
                            text += run["text"].get<std::string>();
                        } else if (run.contains("emoji")) {
                            auto& emo = run["emoji"];
                            if (emo.contains("image") && emo["image"].contains("thumbnails") && !emo["image"]["thumbnails"].empty()) {
                                std::string imgUrl = emo["image"]["thumbnails"][0]["url"].get<std::string>();
                                std::string emoteFile = getCachedEmoteFilename(imgUrl);
                                if (!emoteFile.empty()) {
                                    text += " <img src=\"/emotes/" + emoteFile + "\" height=\"18\" style=\"vertical-align:middle;\"> ";
                                } else if (emo.contains("shortcuts") && !emo["shortcuts"].empty()) {
                                    text += " " + emo["shortcuts"][0].get<std::string>() + " ";
                                }
                            } else if (emo.contains("shortcuts") && !emo["shortcuts"].empty()) {
                                std::string sc = emo["shortcuts"][0].get<std::string>();
                                if (sc == ":hollow_red_circle:" || sc == ":red_circle:") text += "⭕";
                                else if (sc == ":backhand_index_pointing_right:" || sc == ":point_right:") text += "👉";
                                else if (sc == ":star_struck:") text += "🤩";
                                else text += " " + sc + " ";
                            }
                        }
                    }
                }
                if (!text.empty()) pushMessage("YT", author, text, id);
            }
        };

        while (running.load()) {
            uint64_t ver = configVersion.load();
            if (ver != lastVer) {
                lastVer = ver;
                {
                    std::lock_guard<std::mutex> lock(configMutex);
                    currentUrl = youtubeUrl;
                }
                videoId.clear();
                continuation.clear();
                apiKey.clear();
                seenIds.clear();
                seenOrder.clear();
                youtubeConnected.store(false);
                emit statusChanged(twitchConnected.load(), false);
            }

            if (videoId.empty() && !currentUrl.empty()) {
                videoId = extractVideoId(currentUrl);

                if (videoId.empty()) {
                    std::string html = httpGet(currentUrl);
                    if (!html.empty()) {
                        videoId = extractVideoIdFromHtml(html);
                    }
                }

                if (videoId.empty()) {
                    std::cerr << "[YT] No se pudo extraer videoId de: " << currentUrl << std::endl;
                    youtubeConnected.store(false);
                    emit statusChanged(twitchConnected.load(), false);
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    continue;
                }

                std::cout << "[YouTube] Directo detectado: " << videoId << std::endl;

                std::string chatHtml = httpGet("https://www.youtube.com/live_chat?v=" + videoId + "&is_popout=1");
                if (chatHtml.empty()) {
                    std::cerr << "[YT] No se pudo cargar live_chat" << std::endl;
                    std::this_thread::sleep_for(std::chrono::seconds(3));
                    continue;
                }

                // === API KEY (publica, embebida en cada pagina de YouTube) ===
                size_t kp = chatHtml.find("\"INNERTUBE_API_KEY\":\"");
                if (kp != std::string::npos) {
                    kp += 21;
                    size_t ke = chatHtml.find('"', kp);
                    if (ke != std::string::npos) apiKey = chatHtml.substr(kp, ke - kp);
                }

                // === CONTINUATION: metodo 1, parsear ytInitialData ===
                size_t jsonStart = chatHtml.find("ytInitialData");
                if (jsonStart != std::string::npos) {
                    size_t braceStart = chatHtml.find('{', jsonStart);
                    size_t scriptEnd = chatHtml.find("</script>", jsonStart);
                    if (braceStart != std::string::npos && scriptEnd != std::string::npos) {
                        size_t braceEnd = chatHtml.rfind('}', scriptEnd);
                        if (braceEnd != std::string::npos && braceEnd > braceStart) {
                            try {
                                auto data = json::parse(chatHtml.substr(braceStart, braceEnd - braceStart + 1));
                                if (data.contains("contents") && data["contents"].contains("liveChatRenderer")) {
                                    auto& lcr = data["contents"]["liveChatRenderer"];
                                    if (lcr.contains("continuations") && !lcr["continuations"].empty()) {
                                        auto& c = lcr["continuations"][0];
                                        if (c.contains("reloadContinuationData") && c["reloadContinuationData"].contains("continuation")) {
                                            continuation = c["reloadContinuationData"]["continuation"].get<std::string>();
                                        } else if (c.contains("invalidationContinuationData") && c["invalidationContinuationData"].contains("continuation")) {
                                            continuation = c["invalidationContinuationData"]["continuation"].get<std::string>();
                                        } else if (c.contains("timedContinuationData") && c["timedContinuationData"].contains("continuation")) {
                                            continuation = c["timedContinuationData"]["continuation"].get<std::string>();
                                        }
                                    }
                                    if (lcr.contains("actions")) {
                                        processActions(lcr["actions"]);
                                    }
                                }
                            } catch (const std::exception& e) {
                                std::cerr << "[YT] Error parseando ytInitialData: " << e.what() << std::endl;
                            }
                        }
                    }
                }

                // === CONTINUATION: metodo 2, scan de HTML por patron 0of... ===
                if (continuation.empty()) {
                    size_t scanPos = 0;
                    int intentos = 0;
                    while ((scanPos = chatHtml.find("\"continuation\":\"", scanPos)) != std::string::npos && intentos < 50) {
                        scanPos += 16;
                        size_t ce = chatHtml.find('"', scanPos);
                        if (ce == std::string::npos) break;
                        std::string cand = chatHtml.substr(scanPos, ce - scanPos);
                        if (cand.size() > 30 && cand.rfind("0of", 0) == 0) {
                            continuation = cand;
                            std::cerr << "[YT] Continuation hallado por fallback HTML" << std::endl;
                            break;
                        }
                        scanPos = ce + 1;
                        intentos++;
                    }
                }

                if (apiKey.empty() || continuation.empty()) {
                    std::cerr << "[YT] Faltan apiKey o continuation (apiKey_vacia=" << apiKey.empty()
                              << ", continuation_vacia=" << continuation.empty() << ")" << std::endl;
                    youtubeConnected.store(false);
                    emit statusChanged(twitchConnected.load(), false);
                    std::this_thread::sleep_for(std::chrono::seconds(5));
                    continue;
                }

                youtubeConnected.store(true);
                emit statusChanged(twitchConnected.load(), true);
            }

            if (continuation.empty() || apiKey.empty()) {
                std::this_thread::sleep_for(std::chrono::seconds(1));
                continue;
            }

            std::string url = "https://www.youtube.com/youtubei/v1/live_chat/get_live_chat?key=" + apiKey;
            std::string body = std::string("{\"context\":{\"client\":{\"clientName\":\"WEB\",\"clientVersion\":\"2.20240101.00.00\"}},\"continuation\":\"") + continuation + "\"}";
            std::string resp = httpPostJson(url, body);

            if (resp.empty()) {
                std::cerr << "[YT] Respuesta vacia del endpoint get_live_chat" << std::endl;
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }

            try {
                auto j = json::parse(resp);
                if (j.contains("continuationContents") && j["continuationContents"].contains("liveChatContinuation")) {
                    auto& lcc = j["continuationContents"]["liveChatContinuation"];

                    if (lcc.contains("continuations") && !lcc["continuations"].empty()) {
                        auto& c = lcc["continuations"][0];
                        if (c.contains("invalidationContinuationData")) {
                            continuation = c["invalidationContinuationData"]["continuation"].get<std::string>();
                        } else if (c.contains("timedContinuationData")) {
                            continuation = c["timedContinuationData"]["continuation"].get<std::string>();
                        } else if (c.contains("liveChatReplayContinuationData")) {
                            continuation = c["liveChatReplayContinuationData"]["continuation"].get<std::string>();
                        }
                    }

                    if (lcc.contains("actions")) {
                        auto& actions = lcc["actions"];
                        processActions(actions);
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "[YT] Error parseando continuation response: " << e.what() << std::endl;
                continuation.clear();
                apiKey.clear();
                videoId.clear();
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
        }
    }

    void httpLoop() {
        svr.Get("/", [](const httplib::Request&, httplib::Response& res) {
            res.set_content(OBS_OVERLAY_HTML, "text/html; charset=utf-8");
        });

        svr.Get(R"(/emotes/(.+))", [this](const httplib::Request& req, httplib::Response& res) {
            std::string filename = req.matches[1];
            std::string fullPath = cacheDir + filename;
            if (fs::exists(fullPath)) {
                std::ifstream file(fullPath, std::ios::binary);
                if (file.is_open()) {
                    std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
                    res.set_content(content, "image/png");
                    return;
                }
            }
            res.status = 404;
        });

        svr.Get("/api/messages", [this](const httplib::Request& req, httplib::Response& res) {
            uint64_t sinceId = 0;
            if (req.has_param("since")) {
                try { sinceId = std::stoull(req.get_param_value("since")); } catch (...) {}
            }
            json arr = json::array();
            {
                std::lock_guard<std::mutex> lock(messageMutex);
                for (const auto& m : messageLog) {
                    if (m.id > sinceId) {
                        arr.push_back({{"id", m.id}, {"platform", m.platform}, {"user", m.user}, {"text", m.text}});
                    }
                }
            }
            res.set_content(arr.dump(), "application/json; charset=utf-8");
        });

        svr.Get("/api/last-id", [this](const httplib::Request&, httplib::Response& res) {
            uint64_t last = 0;
            {
                std::lock_guard<std::mutex> lock(messageMutex);
                if (!messageLog.empty()) last = messageLog.back().id;
            }
            json j;
            j["id"] = last;
            res.set_content(j.dump(), "application/json; charset=utf-8");
        });

        svr.Get("/api/config", [this](const httplib::Request&, httplib::Response& res) {
            json j;
            {
                std::lock_guard<std::mutex> lock(configMutex);
                j["pacingInterval"] = pacingInterval.load();
                j["fontSize"] = fontSize.load();
                j["lineSpacing"] = lineSpacing.load();
                j["maxObsMessages"] = maxObsMessages.load();
            }
            res.set_content(j.dump(), "application/json; charset=utf-8");
        });

        std::cout << "\n[OBS] Servidor web para OBS disponible en: http://localhost:8080\n";
        svr.listen("0.0.0.0", 8080);
    }
};

// =============================================================
// VENTANA FLOTANTE
// =============================================================
class OverlayWindow : public QWidget {
    Q_OBJECT
public:
    OverlayWindow(ChatBackend* backend, QWidget *parent = nullptr)
        : QWidget(parent), m_backend(backend) {

#ifdef _WIN32
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
#else
        setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
#endif
        setAttribute(Qt::WA_TranslucentBackground, true);
        setMouseTracking(true);
        setMinimumSize(280, 180);
        resize(460, 420);

        auto *mainLayout = new QVBoxLayout(this);
        mainLayout->setContentsMargins(8, 8, 8, 8);
        mainLayout->setSpacing(6);

        m_topBarWidget = new QWidget(this);
        auto *topBar = new QHBoxLayout(m_topBarWidget);
        topBar->setContentsMargins(0, 0, 0, 0);

        auto *title = new QLabel("NATIVECHATS");
        title->setStyleSheet("color: #888; font-weight: bold; font-size: 11px; letter-spacing: 0.5px;");

        m_scrollDownBtn = new QPushButton("⬇ Nuevos mensajes (Ir al fondo)");
        m_scrollDownBtn->setStyleSheet("background: #0d6efd; color: white; border: none; border-radius: 4px; padding: 2px 8px; font-size: 11px; font-weight: bold;");
        m_scrollDownBtn->hide();
        connect(m_scrollDownBtn, &QPushButton::clicked, this, &OverlayWindow::resumeAndScrollBottom);

        m_statusWidget = new QWidget(this);
        auto *statusLayout = new QHBoxLayout(m_statusWidget);
        statusLayout->setContentsMargins(0, 0, 0, 0);
        statusLayout->setSpacing(5);

        m_twDot = new QLabel("● TW");
        m_twDot->setStyleSheet("color: #e74c3c; font-size: 10px; font-weight: bold;");
        m_ytDot = new QLabel("● YT");
        m_ytDot->setStyleSheet("color: #e74c3c; font-size: 10px; font-weight: bold;");

        statusLayout->addWidget(m_twDot);
        statusLayout->addWidget(m_ytDot);

        auto *settingsBtn = new QPushButton("⚙ Ajustes (F1)");
        settingsBtn->setStyleSheet("background: #2b2b2b; color: #ddd; border: 1px solid #444; border-radius: 4px; padding: 2px 8px; font-size: 11px;");
        connect(settingsBtn, &QPushButton::clicked, this, &OverlayWindow::toggleSettings);

        auto *closeBtn = new QPushButton("✕");
        closeBtn->setFixedSize(22, 22);
        closeBtn->setStyleSheet("background: #c0392b; color: white; border: none; border-radius: 4px; font-weight: bold; font-size: 11px;");
        connect(closeBtn, &QPushButton::clicked, qApp, &QApplication::quit);

        topBar->addWidget(title);
        topBar->addWidget(m_scrollDownBtn);
        topBar->addStretch();
        topBar->addWidget(m_statusWidget);
        topBar->addWidget(settingsBtn);
        topBar->addWidget(closeBtn);
        mainLayout->addWidget(m_topBarWidget);

        m_configWidget = new QWidget(this);
        auto *cfgLayout = new QVBoxLayout(m_configWidget);
        cfgLayout->setContentsMargins(10, 8, 10, 8);
        cfgLayout->setSpacing(6);
        m_configWidget->setStyleSheet("background: #181818; border-radius: 6px; border: 1px solid #333;");
        m_configWidget->hide();

        m_twInput = new QLineEdit(QString::fromStdString(m_backend->twitchChannel));
        m_twInput->setPlaceholderText("ej: canal_twitch");
        m_twInput->setStyleSheet("background: #2b2b2b; color: white; padding: 4px; border: 1px solid #444; border-radius: 3px; font-size: 12px;");
        auto *twLabel = new QLabel("Canal de Twitch:");
        twLabel->setStyleSheet("color: #f1c40f; font-weight: bold; font-size: 11px; border: none;");

        m_ytInput = new QLineEdit(QString::fromStdString(m_backend->youtubeUrl));
        m_ytInput->setPlaceholderText("https://www.youtube.com/watch?v=ID  o  https://youtu.be/ID  (tambien /@canal/live si esta en directo)");
        m_ytInput->setStyleSheet("background: #2b2b2b; color: white; padding: 4px; border: 1px solid #444; border-radius: 3px; font-size: 12px;");
        auto *ytLabel = new QLabel("URL de YouTube Live:");
        ytLabel->setStyleSheet("color: #f1c40f; font-weight: bold; font-size: 11px; border: none;");

        auto *opLabel = new QLabel("Opacidad del fondo (0% = cristal):");
        opLabel->setStyleSheet("color: #58a6ff; font-weight: bold; font-size: 11px; border: none;");
        auto *opSlider = new QSlider(Qt::Horizontal);
        opSlider->setRange(0, 100);
        opSlider->setValue(static_cast<int>(m_bgAlpha * 100));
        connect(opSlider, &QSlider::valueChanged, this, [this](int v) {
            m_bgAlpha = v / 100.0;
            update();
        });

        m_paceLabel = new QLabel(QString("Pausa entre mensajes: %1 s").arg(m_pacingInterval / 1000.0, 0, 'f', 1));
        m_paceLabel->setStyleSheet("color: #58a6ff; font-weight: bold; font-size: 11px; border: none;");
        auto *paceSlider = new QSlider(Qt::Horizontal);
        paceSlider->setRange(0, 3000);
        paceSlider->setSingleStep(100);
        paceSlider->setPageStep(500);
        paceSlider->setTickPosition(QSlider::TicksBelow);
        paceSlider->setTickInterval(500);
        paceSlider->setValue(m_pacingInterval);
        connect(paceSlider, &QSlider::valueChanged, this, [this](int v) {
            m_pacingInterval = std::max(50, v);
            m_backend->pacingInterval.store(m_pacingInterval);
            m_paceLabel->setText(QString("Pausa entre mensajes: %1 s").arg(m_pacingInterval / 1000.0, 0, 'f', 1));
            m_pacingTimer->setInterval(m_pacingInterval);
            m_pacingTimer->start(m_pacingInterval);
        });

        auto *spLabel = new QLabel("Separación entre mensajes:");
        spLabel->setStyleSheet("color: #58a6ff; font-weight: bold; font-size: 11px; border: none;");
        auto *spSlider = new QSlider(Qt::Horizontal);
        spSlider->setRange(2, 24);
        spSlider->setValue(m_lineSpacing);
        connect(spSlider, &QSlider::valueChanged, this, [this](int v) {
            m_lineSpacing = v;
            m_backend->lineSpacing.store(v);
            m_chatLayout->setSpacing(m_lineSpacing);
        });

        auto *fsLabel = new QLabel("Tamaño de letra:");
        fsLabel->setStyleSheet("color: #58a6ff; font-weight: bold; font-size: 11px; border: none;");
        auto *fsSlider = new QSlider(Qt::Horizontal);
        fsSlider->setRange(12, 28);
        fsSlider->setValue(m_fontSize);
        connect(fsSlider, &QSlider::valueChanged, this, [this](int v) {
            m_fontSize = v;
            m_backend->fontSize.store(v);
        });

        m_obsMsgLabel = new QLabel(QString("Mensajes visibles en OBS: %1").arg(m_maxObsMessages));
        m_obsMsgLabel->setStyleSheet("color: #58a6ff; font-weight: bold; font-size: 11px; border: none;");
        auto *obsMsgSlider = new QSlider(Qt::Horizontal);
        obsMsgSlider->setRange(3, 15);
        obsMsgSlider->setValue(m_maxObsMessages);
        connect(obsMsgSlider, &QSlider::valueChanged, this, [this](int v) {
            m_maxObsMessages = v;
            m_backend->maxObsMessages.store(v);
            m_obsMsgLabel->setText(QString("Mensajes visibles en OBS: %1").arg(v));
        });

        auto *btnBox = new QHBoxLayout();
        auto *saveBtn = new QPushButton("Guardar y Aplicar");
        saveBtn->setStyleSheet("background: #0d6efd; color: white; font-weight: bold; padding: 5px 12px; border: none; border-radius: 3px; font-size: 12px;");
        connect(saveBtn, &QPushButton::clicked, this, &OverlayWindow::saveConfig);

        auto *hideBtn = new QPushButton("Ocultar");
        hideBtn->setStyleSheet("background: #444; color: white; padding: 5px 12px; border: none; border-radius: 3px; font-size: 12px;");
        connect(hideBtn, &QPushButton::clicked, this, &OverlayWindow::toggleSettings);

        btnBox->addWidget(saveBtn);
        btnBox->addWidget(hideBtn);

        cfgLayout->addWidget(twLabel);
        cfgLayout->addWidget(m_twInput);
        cfgLayout->addWidget(ytLabel);
        cfgLayout->addWidget(m_ytInput);
        cfgLayout->addWidget(opLabel);
        cfgLayout->addWidget(opSlider);
        cfgLayout->addWidget(m_paceLabel);
        cfgLayout->addWidget(paceSlider);
        cfgLayout->addWidget(spLabel);
        cfgLayout->addWidget(spSlider);
        cfgLayout->addWidget(fsLabel);
        cfgLayout->addWidget(fsSlider);
        cfgLayout->addWidget(m_obsMsgLabel);
        cfgLayout->addWidget(obsMsgSlider);
        cfgLayout->addLayout(btnBox);
        mainLayout->addWidget(m_configWidget);

        m_scrollArea = new QScrollArea(this);
        m_scrollArea->setWidgetResizable(true);
        m_scrollArea->setStyleSheet("background: transparent; border: none;");
        m_scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        m_chatContainer = new QWidget();
        m_chatContainer->setStyleSheet("background: transparent;");
        m_chatLayout = new QVBoxLayout(m_chatContainer);
        m_chatLayout->setContentsMargins(0, 0, 0, 0);
        m_chatLayout->setSpacing(m_lineSpacing);
        m_chatLayout->addStretch();

        m_scrollArea->setWidget(m_chatContainer);
        mainLayout->addWidget(m_scrollArea, 1);

        connect(m_scrollArea->verticalScrollBar(), &QScrollBar::valueChanged, this, [this](int value) {
            int maxVal = m_scrollArea->verticalScrollBar()->maximum();
            bool atBottom = (maxVal - value <= 40);
            if (!atBottom) {
                m_isPaused = true;
                m_scrollDownBtn->show();
            } else {
                if (m_isPaused) resumeAndScrollBottom();
            }
        });

        connect(m_backend, &ChatBackend::messageReceived, this, &OverlayWindow::enqueueMessage);
        connect(m_backend, &ChatBackend::statusChanged, this, &OverlayWindow::updateStatusLeds);

        m_pacingTimer = new QTimer(this);
        connect(m_pacingTimer, &QTimer::timeout, this, &OverlayWindow::processPacingQueue);
        m_pacingTimer->start(m_pacingInterval);

        loadConfigFile();
    }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.fillRect(rect(), QColor::fromRgbF(0.04, 0.04, 0.04, m_bgAlpha));
    }

    void resizeEvent(QResizeEvent *event) override {
        QWidget::resizeEvent(event);
        if (m_scrollArea && m_chatContainer && m_scrollArea->viewport()) {
            int w = m_scrollArea->viewport()->width();
            if (w > 50) m_chatContainer->setFixedWidth(w);
        }
    }

    Qt::Edges getResizeEdges(const QPoint& p) {
        const int B = 10;
        Qt::Edges edges = Qt::Edges();
        if (p.x() <= B) edges |= Qt::LeftEdge;
        if (p.x() >= width() - B) edges |= Qt::RightEdge;
        if (p.y() <= B) edges |= Qt::TopEdge;
        if (p.y() >= height() - B) edges |= Qt::BottomEdge;
        return edges;
    }

    void mousePressEvent(QMouseEvent *event) override {
        if (event->button() == Qt::LeftButton) {
            Qt::Edges edges = getResizeEdges(event->pos());
            if (edges != Qt::Edges() && window()->windowHandle()) {
                window()->windowHandle()->startSystemResize(edges);
                event->accept();
                return;
            } else if (m_topBarWidget->geometry().contains(event->pos()) && window()->windowHandle()) {
                window()->windowHandle()->startSystemMove();
                event->accept();
                return;
            }
        }
        QWidget::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override {
        Qt::Edges edges = getResizeEdges(event->pos());
        if ((edges & Qt::LeftEdge && edges & Qt::TopEdge) || (edges & Qt::RightEdge && edges & Qt::BottomEdge)) {
            setCursor(Qt::SizeFDiagCursor);
        } else if ((edges & Qt::RightEdge && edges & Qt::TopEdge) || (edges & Qt::LeftEdge && edges & Qt::BottomEdge)) {
            setCursor(Qt::SizeBDiagCursor);
        } else if (edges & (Qt::LeftEdge | Qt::RightEdge)) {
            setCursor(Qt::SizeHorCursor);
        } else if (edges & (Qt::TopEdge | Qt::BottomEdge)) {
            setCursor(Qt::SizeVerCursor);
        } else {
            setCursor(Qt::ArrowCursor);
        }
        QWidget::mouseMoveEvent(event);
    }

    void keyPressEvent(QKeyEvent *event) override {
        if (event->key() == Qt::Key_F1) {
            toggleSettings();
            event->accept();
        } else {
            QWidget::keyPressEvent(event);
        }
    }

private slots:
    void toggleSettings() {
        m_configWidget->setVisible(!m_configWidget->isVisible());
    }

    void updateStatusLeds(bool twConnected, bool ytConnected) {
        m_twDot->setStyleSheet(twConnected
            ? "color: #2ecc71; font-size: 10px; font-weight: bold;"
            : "color: #e74c3c; font-size: 10px; font-weight: bold;");
        m_ytDot->setStyleSheet(ytConnected
            ? "color: #2ecc71; font-size: 10px; font-weight: bold;"
            : "color: #e74c3c; font-size: 10px; font-weight: bold;");
    }

    void resumeAndScrollBottom() {
        m_isPaused = false;
        m_scrollDownBtn->hide();
        scrollToBottom();
    }

    void scrollToBottom() {
        if (m_scrollArea->verticalScrollBar()) {
            m_scrollArea->verticalScrollBar()->setValue(m_scrollArea->verticalScrollBar()->maximum());
        }
    }

    void enqueueMessage(quint64 id, QString platform, QString user, QString text, QString msgId) {
        Q_UNUSED(id);
        Q_UNUSED(msgId);
        if (platform == "TW") {
            m_twQueue.push_back({platform, user, text});
            if (m_twQueue.size() > MAX_QUEUE_SIZE) m_twQueue.pop_front();
        } else {
            m_ytQueue.push_back({platform, user, text});
            if (m_ytQueue.size() > MAX_QUEUE_SIZE) m_ytQueue.pop_front();
        }
    }

    void processPacingQueue() {
        size_t totalPending = m_twQueue.size() + m_ytQueue.size();

        if (m_isPaused || totalPending == 0) {
            if (m_isPaused && totalPending > 0) {
                m_scrollDownBtn->setText(QString("⬇ %1 nuevos (Ir al fondo)").arg(totalPending));
                m_scrollDownBtn->show();
            }
            return;
        }

        QueueItem item;
        if (!m_twQueue.empty() && !m_ytQueue.empty()) {
            if (m_lastPlatformServed == "TW") {
                item = m_ytQueue.front();
                m_ytQueue.pop_front();
                m_lastPlatformServed = "YT";
            } else {
                item = m_twQueue.front();
                m_twQueue.pop_front();
                m_lastPlatformServed = "TW";
            }
        } else if (!m_twQueue.empty()) {
            item = m_twQueue.front();
            m_twQueue.pop_front();
            m_lastPlatformServed = "TW";
        } else if (!m_ytQueue.empty()) {
            item = m_ytQueue.front();
            m_ytQueue.pop_front();
            m_lastPlatformServed = "YT";
        }

        auto *label = new QLabel(m_chatContainer);
        label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        label->setWordWrap(true);
        label->setTextFormat(Qt::RichText);

        QString badgeColor = (item.platform == "TW") ? "#9146FF" : "#FF0000";
        QString userColor  = (item.platform == "TW") ? "#c4a7e7" : "#ff7b72";

        QString localHtmlText = item.text;
        localHtmlText.replace(QStringLiteral("src=\"/emotes/"), QString("src=\"file:///%1").arg(QString::fromStdString(m_backend->cacheDir)));

        // Calculo: badge 70% del tamano del mensaje, minimo 10px. Usuario al 100% igual que el texto.
        int badgePx = std::max(10, m_fontSize * 7 / 10);

        QString html = QString(
            "<div style='font-size:%1px; line-height:1.35;'>"
            "<span style='background:%2; color:#ffffff; font-weight:bold; font-size:%3px; "
            "padding:2px 6px; border-radius:3px; line-height:1;'>%4</span>"
            "&nbsp;<span style='color:%5; font-weight:bold;'>%6:</span>&nbsp;"
            "<span style='color:#ffffff;'>%7</span>"
            "</div>"
        ).arg(
            QString::number(m_fontSize),        // %1 - base
            badgeColor,                          // %2
            QString::number(badgePx),            // %3 - badge px
            item.platform,                       // %4 - texto TW/YT
            userColor,                           // %5
            item.user.toHtmlEscaped(),           // %6
            formatLinks(localHtmlText)           // %7
        );

        label->setText(html);
        label->setStyleSheet("background: transparent;");

        auto *effect = new QGraphicsOpacityEffect(label);
        label->setGraphicsEffect(effect);
        auto *anim = new QPropertyAnimation(effect, "opacity");
        anim->setDuration(200);
        anim->setStartValue(0.0);
        anim->setEndValue(1.0);

        m_chatLayout->addWidget(label);
        anim->start(QAbstractAnimation::DeleteWhenStopped);

        QTimer::singleShot(15, this, &OverlayWindow::scrollToBottom);

        if (m_chatLayout->count() > 405) {
            auto *w = m_chatLayout->itemAt(1)->widget();
            if (w) w->deleteLater();
        }
    }

    void saveConfig() {
        {
            std::lock_guard<std::mutex> lock(m_backend->configMutex);
            m_backend->twitchChannel = m_twInput->text().toStdString();
            m_backend->youtubeUrl = m_ytInput->text().toStdString();
        }
        m_backend->configVersion.fetch_add(1);

        json j;
        j["twitch"] = m_twInput->text().toStdString();
        j["youtube"] = m_ytInput->text().toStdString();
        j["opacity"] = m_bgAlpha;
        j["pacingInterval"] = m_pacingInterval;
        j["lineSpacing"] = m_lineSpacing;
        j["fontSize"] = m_fontSize;
        j["maxObsMessages"] = m_maxObsMessages;
        std::ofstream f("config.json");
        if (f.is_open()) f << j.dump(4);

        m_configWidget->hide();
    }

private:
    struct QueueItem { QString platform; QString user; QString text; };
    std::deque<QueueItem> m_twQueue;
    std::deque<QueueItem> m_ytQueue;
    std::string m_lastPlatformServed = "YT";

    ChatBackend* m_backend;
    QWidget* m_topBarWidget;
    QWidget* m_configWidget;
    QWidget* m_statusWidget;
    QLabel* m_twDot;
    QLabel* m_ytDot;
    QLabel* m_paceLabel;
    QLabel* m_obsMsgLabel;
    QPushButton* m_scrollDownBtn;
    QLineEdit* m_twInput;
    QLineEdit* m_ytInput;
    QScrollArea* m_scrollArea;
    QWidget* m_chatContainer;
    QVBoxLayout* m_chatLayout;
    QTimer* m_pacingTimer;

    double m_bgAlpha = 0.5;
    int m_lineSpacing = 13;
    int m_fontSize = 20;
    int m_pacingInterval = 1500;
    int m_maxObsMessages = 6;
    bool m_isPaused = false;

    QString formatLinks(const QString& text) {
        QString result = "";
        static QRegularExpression tagOrUrlRegex(QStringLiteral("(<img[^>]+>)|(https?://[^\\s<]+)"));
        int lastPos = 0;
        auto matchIt = tagOrUrlRegex.globalMatch(text);
        while (matchIt.hasNext()) {
            auto match = matchIt.next();
            if (match.capturedStart() > lastPos) {
                result += text.mid(lastPos, match.capturedStart() - lastPos).toHtmlEscaped();
            }
            if (match.captured(1).length() > 0) {
                result += match.captured(1);
            } else if (match.captured(2).length() > 0) {
                QString url = match.captured(2);
                result += QStringLiteral("<a href=\"%1\" style=\"color:#58a6ff; text-decoration:underline;\">%1</a>").arg(url.toHtmlEscaped());
            }
            lastPos = match.capturedEnd();
        }
        if (lastPos < text.length()) {
            result += text.mid(lastPos).toHtmlEscaped();
        }
        return result;
    }

    void loadConfigFile() {
        std::ifstream f("config.json");
        if (f.is_open()) {
            try {
                json j;
                f >> j;
                if (j.contains("twitch")) {
                    m_backend->twitchChannel = j["twitch"].get<std::string>();
                    m_twInput->setText(QString::fromStdString(m_backend->twitchChannel));
                }
                if (j.contains("youtube")) {
                    m_backend->youtubeUrl = j["youtube"].get<std::string>();
                    m_ytInput->setText(QString::fromStdString(m_backend->youtubeUrl));
                }
                if (j.contains("opacity")) { m_bgAlpha = j["opacity"].get<double>(); update(); }
                if (j.contains("pacingInterval")) {
                    m_pacingInterval = j["pacingInterval"].get<int>();
                    m_backend->pacingInterval.store(m_pacingInterval);
                    m_paceLabel->setText(QString("Pausa entre mensajes: %1 s").arg(m_pacingInterval / 1000.0, 0, 'f', 1));
                    m_pacingTimer->setInterval(m_pacingInterval);
                }
                if (j.contains("lineSpacing")) {
                    m_lineSpacing = j["lineSpacing"].get<int>();
                    m_backend->lineSpacing.store(m_lineSpacing);
                    m_chatLayout->setSpacing(m_lineSpacing);
                }
                if (j.contains("fontSize")) {
                    m_fontSize = j["fontSize"].get<int>();
                    m_backend->fontSize.store(m_fontSize);
                }
                if (j.contains("maxObsMessages")) {
                    m_maxObsMessages = j["maxObsMessages"].get<int>();
                    m_backend->maxObsMessages.store(m_maxObsMessages);
                    if (m_obsMsgLabel) m_obsMsgLabel->setText(QString("Mensajes visibles en OBS: %1").arg(m_maxObsMessages));
                }
            } catch (const std::exception& e) {
                std::cerr << "[Config] Error cargando config.json: " << e.what() << std::endl;
            }
        }
    }
};

#include "main.moc"

int main(int argc, char *argv[]) {
#ifdef _WIN32
    WSADATA wsaData;
    WSAStartup(MAKEWORD(2, 2), &wsaData);
#endif

    QApplication app(argc, argv);

    ChatBackend backend;
    backend.start();

    OverlayWindow window(&backend);
    window.show();

    int ret = app.exec();

    backend.stop();

#ifdef _WIN32
    WSACleanup();
#endif
    return ret;
}
