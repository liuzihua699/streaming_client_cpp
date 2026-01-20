#pragma once
#include "network/TcpClient.h"
#include "rtsp/RtspSplitter.h"
#include "rtsp/RtpPacket.h"
#include "util/RingBuffer.h"
#include <sstream>
#include <iostream>
#include <map>
#include <openssl/md5.h>

class RtspClient : public TcpClient {
public:
    using Ptr = std::shared_ptr<RtspClient>;
    using RingType = RingBuffer<RtpPacket::Ptr>;

    void play(const std::string& url) {
        parseUrl(url);
        _splitter.setOnResponse([this](const std::string& r) { onRtspResponse(r); });
        _splitter.setOnRtp([this](const char* d, size_t l, int t) { onRtpPacket(d, l, t); });
        connect(_host, _port);
    }

    RingType::Ptr getRing() { return _ring; }
    void setOnPlayResult(std::function<void(bool, const std::string&)> cb) { _on_result = cb; }

protected:
    void onConnect() override { sendOptions(); }
    void onRecv(const char* data, size_t len) override { _splitter.input(data, len); }
    void onDisconnect() override { if (_on_result) _on_result(false, "Disconnected"); }

private:
    void parseUrl(const std::string& url) {
        _url = url;

        size_t pos = url.find("://");
        std::string schema = url.substr(0, pos);
        std::string rest = url.substr(pos + 3);

        pos = rest.find('/');
        std::string authority = (pos != std::string::npos) ? rest.substr(0, pos) : rest;
        std::string path = (pos != std::string::npos) ? rest.substr(pos) : "";

        pos = authority.find('@');
        std::string host_port;
        if (pos != std::string::npos) {
            std::string userinfo = authority.substr(0, pos);
            host_port = authority.substr(pos + 1);
            size_t c = userinfo.find(':');
            if (c != std::string::npos) {
                _user = userinfo.substr(0, c);
                _password = userinfo.substr(c + 1);
            }
        } else {
            host_port = authority;
        }

        pos = host_port.find(':');
        _host = (pos != std::string::npos) ? host_port.substr(0, pos) : host_port;
        _port = (pos != std::string::npos) ? std::stoi(host_port.substr(pos + 1)) : 554;

        _play_url = schema + "://" + host_port + path;
    }

    static std::string md5(const std::string& str) {
        unsigned char digest[16];
        MD5((unsigned char*)str.c_str(), str.size(), digest);
        char hex[33];
        for (int i = 0; i < 16; i++) sprintf(hex + i*2, "%02x", digest[i]);
        return hex;
    }

    static std::string escapeString(const std::string& s) {
        std::string result;
        for (char c : s) {
            if (c == '\r') {
                result += "\\r";
            } else if (c == '\n') {
                result += "\\n\n";  // 换行后真换一行方便看
            } else {
                result += c;
            }
        }
        return result;
    }

    bool handleAuthenticationFailure(const std::string& params) {
        if (!_realm.empty()) return false;

        char realm[256] = {0}, nonce[256] = {0};
        if (sscanf(params.c_str(), "Digest realm=\"%[^\"]\", nonce=\"%[^\"]\"", realm, nonce) == 2) {
            _realm = realm;
            _nonce = nonce;
            return true;
        }
        if (sscanf(params.c_str(), "Basic realm=\"%[^\"]\"", realm) == 1) {
            _realm = realm;
            return true;
        }
        return false;
    }

    std::string makeAuthHeader(const std::string& method, const std::string& uri) {
        if (_realm.empty() || _user.empty()) return "";

        if (!_nonce.empty()) {
            std::string ha1 = md5(_user + ":" + _realm + ":" + _password);
            std::string ha2 = md5(method + ":" + uri);
            std::string response = md5(ha1 + ":" + _nonce + ":" + ha2);

            std::ostringstream ss;
            ss << "Digest username=\"" << _user << "\", "
               << "realm=\"" << _realm << "\", "
               << "nonce=\"" << _nonce << "\", "
               << "uri=\"" << uri << "\", "
               << "response=\"" << response << "\"";
            return ss.str();
        }
        return "";
    }

    void sendRequest(const std::string& method, const std::string& url,
                     const std::map<std::string, std::string>& extra = {}) {
        std::ostringstream ss;
        ss << method << " " << url << " RTSP/1.0\r\n";
        ss << "CSeq: " << ++_cseq << "\r\n";
        ss << "User-Agent: SimplePlayer\r\n";
        if (!_session.empty()) ss << "Session: " << _session << "\r\n";

        std::string auth = makeAuthHeader(method, url);
        if (!auth.empty()) ss << "Authorization: " << auth << "\r\n";

        for (auto& h : extra) ss << h.first << ": " << h.second << "\r\n";
        ss << "\r\n";

        std::string req = ss.str();
        fprintf(stderr, ">>> SEND (%zu bytes):\n%s\n", req.size(), escapeString(req).c_str());
        send(req);
    }

    void sendOptions()  { _state = OPTIONS;  sendRequest("OPTIONS", _play_url); }
    void sendDescribe() { _state = DESCRIBE; sendRequest("DESCRIBE", _play_url, {{"Accept", "application/sdp"}}); }
    void sendSetup()    { _state = SETUP;    sendRequest("SETUP", _control, {{"Transport", "RTP/AVP/TCP;unicast;interleaved=0-1"}}); }
    void sendPlay()     { _state = PLAY;     sendRequest("PLAY", _play_url, {{"Range", "npt=0.000-"}}); }

    void onRtspResponse(const std::string& resp) {
        fprintf(stderr, "<<< RECV (%zu bytes):\n%s\n", resp.size(), escapeString(resp).c_str());        int status = 0;
        sscanf(resp.c_str(), "RTSP/1.0 %d", &status);

        std::string auth_info;
        size_t pos = resp.find("WWW-Authenticate:");
        if (pos != std::string::npos) {
            size_t end = resp.find("\r\n", pos);
            auth_info = resp.substr(pos + 18, end - pos - 18);
        }

        if (status == 401) {
            if (handleAuthenticationFailure(auth_info)) {
                switch (_state) {
                    case OPTIONS:  sendOptions();  break;
                    case DESCRIBE: sendDescribe(); break;
                    case SETUP:    sendSetup();    break;
                    case PLAY:     sendPlay();     break;
                }
                return;
            }
            if (_on_result) _on_result(false, "Auth failed");
            shutdown();
            return;
        }

        if (status != 200) {
            if (_on_result) _on_result(false, "RTSP " + std::to_string(status));
            shutdown();
            return;
        }

        pos = resp.find("Session:");
        if (pos != std::string::npos) {
            size_t start = pos + 8;
            while (start < resp.size() && resp[start] == ' ') start++;
            size_t end = resp.find_first_of(";\r\n", start);
            _session = resp.substr(start, end - start);
        }

        switch (_state) {
            case OPTIONS:  sendDescribe(); break;
            case DESCRIBE: parseSdp(resp); sendSetup(); break;
            case SETUP:    sendPlay(); break;
            case PLAY:
                _splitter.enableRtp(true);
                if (_on_result) _on_result(true, "OK");
                break;
        }
    }

    void parseSdp(const std::string& resp) {
        // 找Content-Base或使用_play_url作为基础
        std::string base = _play_url;
        size_t pos = resp.find("Content-Base:");
        if (pos != std::string::npos) {
            size_t start = pos + 13;
            while (start < resp.size() && resp[start] == ' ') start++;
            size_t end = resp.find_first_of("\r\n", start);
            base = resp.substr(start, end - start);
            if (!base.empty() && base.back() == '/') base.pop_back();
        }

        // 找视频track的control（m=video之后的a=control）
        pos = resp.find("m=video");
        if (pos == std::string::npos) {
            pos = resp.find("m=audio");  // 没视频找音频
        }

        if (pos != std::string::npos) {
            size_t ctrl_pos = resp.find("a=control:", pos);
            if (ctrl_pos != std::string::npos) {
                size_t start = ctrl_pos + 10;
                size_t end = resp.find_first_of("\r\n", start);
                std::string ctrl = resp.substr(start, end - start);

                if (ctrl.find("rtsp://") == 0) {
                    _control = ctrl;
                } else if (ctrl == "*") {
                    _control = base;
                } else if (ctrl[0] == '/') {
                    // 绝对路径
                    size_t p = base.find("://");
                    std::string hostpart = base.substr(0, p + 3);
                    std::string rest = base.substr(p + 3);
                    p = rest.find('/');
                    _control = hostpart + rest.substr(0, p) + ctrl;
                } else {
                    // 相对路径
                    _control = base + "/" + ctrl;
                }
            } else {
                _control = base;
            }
        } else {
            _control = base;
        }

        std::cout << "Control URL: " << _control << std::endl;
    }

    void onRtpPacket(const char* data, size_t len, int track) {
        auto pkt = RtpPacket::parse(data, len);
        if (pkt) _ring->write(pkt, pkt->isKeyFrame());
    }

private:
    enum State { INIT, OPTIONS, DESCRIBE, SETUP, PLAY } _state = INIT;

    std::string _url;
    std::string _play_url;
    std::string _host;
    std::string _user;
    std::string _password;
    std::string _session;
    std::string _control;
    std::string _realm;
    std::string _nonce;
    uint16_t _port = 554;
    int _cseq = 0;

    RtspSplitter _splitter;
    RingType::Ptr _ring = std::make_shared<RingType>();
    std::function<void(bool, const std::string&)> _on_result;
};