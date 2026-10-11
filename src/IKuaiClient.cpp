#include "IKuaiClient.h"
#include <MD5Builder.h>

IKuaiClient::IKuaiClient()
    : _port(80), _last_login_time(0) {
}

void IKuaiClient::setConfig(const String &host, uint16_t port, const String &user, const String &pass) {
    _host = host;
    _port = port > 0 ? port : 80;
    _user = user;
    _pass = pass;
    _sess_cookie = "";
    _last_login_time = 0;
}

bool IKuaiClient::isConfigured() const {
    return (_host.length() > 0 && _user.length() > 0);
}

bool IKuaiClient::login() {
    if (!isConfigured()) return false;

    WiFiClient client;
    client.setTimeout(2000);
    if (!client.connect(_host.c_str(), _port)) {
        Serial.printf("[iKuai] Failed to connect to %s:%d\n", _host.c_str(), _port);
        return false;
    }

    // 计算密码的 MD5 哈希
    MD5Builder md5;
    md5.begin();
    md5.add(_pass);
    md5.calculate();
    String pass_md5 = md5.toString();

    String body = "{\"username\":\"" + _user + "\",\"passwd\":\"" + pass_md5 + "\",\"remember_password\":\"\"}";
    
    String req = "POST /Action/login HTTP/1.1\r\n"
                 "Host: " + _host + ":" + String(_port) + "\r\n"
                 "Content-Type: application/json;charset=utf-8\r\n"
                 "Content-Length: " + String(body.length()) + "\r\n"
                 "Connection: close\r\n\r\n" + body;

    client.print(req);

    String raw_resp = client.readString();
    client.stop();

    if (raw_resp.indexOf("200 OK") == -1) {
        Serial.println("[iKuai] Login HTTP error: not 200 OK");
        return false;
    }

    String cookie_str = "";
    int cookie_idx = raw_resp.indexOf("Set-Cookie:");
    if (cookie_idx == -1) {
        cookie_idx = raw_resp.indexOf("set-cookie:");
    }
    if (cookie_idx != -1) {
        int val_start = cookie_idx + 11;
        int semi = raw_resp.indexOf(';', val_start);
        int newline = raw_resp.indexOf('\r', val_start);
        int end_cookie = semi;
        if (end_cookie == -1 || (newline != -1 && newline < end_cookie)) {
            end_cookie = newline;
        }
        if (end_cookie != -1) {
            cookie_str = raw_resp.substring(val_start, end_cookie);
        } else {
            cookie_str = raw_resp.substring(val_start);
        }
        cookie_str.trim();
    }

    int start = raw_resp.indexOf('{');
    int end = raw_resp.lastIndexOf('}');
    if (start == -1 || end == -1 || end < start) {
        Serial.println("[iKuai] Login response: No JSON found in body");
        return false;
    }

    DynamicJsonDocument doc(1024);
    DeserializationError err = deserializeJson(doc, raw_resp.substring(start, end + 1));

    if (err) {
        Serial.println("[iKuai] Login response JSON parse failed");
        return false;
    }

    int res = doc["Result"] | -1;
    if (res == 10000) {
        _sess_cookie = cookie_str;
        _last_login_time = millis();
        Serial.printf("[iKuai] Login success! Cookie: %s\n", _sess_cookie.c_str());
        return true;
    } else {
        const char *errmsg = doc["ErrMsg"] | "Unknown error";
        Serial.printf("[iKuai] Login rejected (%d): %s\n", res, errmsg);
        return false;
    }
}

bool IKuaiClient::updateSpeed(double &rx_kb, double &tx_kb) {
    if (!isConfigured()) return false;

    // 若未登录或超过 1 小时，重新登录刷新 Session
    if (_sess_cookie.length() == 0 || (millis() - _last_login_time > 3600000)) {
        if (!login()) return false;
    }

    WiFiClient client;
    client.setTimeout(1500);
    if (!client.connect(_host.c_str(), _port)) {
        Serial.println("[iKuai updateSpeed] Connect failed");
        return false;
    }

    String body = "{\"func_name\":\"monitor_lanip\",\"action\":\"show\",\"TYPE\":\"total\"}";
    String req = "POST /Action/call HTTP/1.1\r\n"
                 "Host: " + _host + ":" + String(_port) + "\r\n"
                 "Content-Type: application/json;charset=utf-8\r\n"
                 "Cookie: " + _sess_cookie + "\r\n"
                 "Content-Length: " + String(body.length()) + "\r\n"
                 "Connection: close\r\n\r\n" + body;

    client.print(req);

    String raw_resp = client.readString();
    client.stop();

    if (raw_resp.indexOf("200 OK") == -1) {
        Serial.println("[iKuai updateSpeed] HTTP error: not 200 OK");
        return false;
    }

    if (raw_resp.indexOf("\"Result\":10014") != -1) {
        Serial.println("[iKuai updateSpeed] Session expired (10014), clearing cookie");
        _sess_cookie = "";
        return false;
    }

    // 采用超轻量流式数值累加，杜绝 10KB JSON 导致 ESP32 内存耗尽
    uint64_t total_down_bytes = 0;
    uint64_t total_up_bytes = 0;

    int pos = 0;
    int resp_len = raw_resp.length();
    while (pos < resp_len) {
        int d_idx = raw_resp.indexOf("\"download\":", pos);
        int u_idx = raw_resp.indexOf("\"upload\":", pos);
        if (d_idx == -1 && u_idx == -1) break;

        if (d_idx != -1 && (u_idx == -1 || d_idx < u_idx)) {
            int num_start = d_idx + 11;
            while (num_start < resp_len && raw_resp[num_start] == ' ') num_start++;
            int num_end = num_start;
            while (num_end < resp_len && isDigit(raw_resp[num_end])) num_end++;
            if (num_end > num_start) {
                total_down_bytes += raw_resp.substring(num_start, num_end).toInt();
            }
            pos = num_end;
        } else if (u_idx != -1) {
            int num_start = u_idx + 9;
            while (num_start < resp_len && raw_resp[num_start] == ' ') num_start++;
            int num_end = num_start;
            while (num_end < resp_len && isDigit(raw_resp[num_end])) num_end++;
            if (num_end > num_start) {
                total_up_bytes += raw_resp.substring(num_start, num_end).toInt();
            }
            pos = num_end;
        }
    }

    rx_kb = (double)total_down_bytes / 1024.0; // 实时全网真实下行速度
    tx_kb = (double)total_up_bytes / 1024.0;   // 实时全网真实上行速度

    Serial.printf("[iKuai Real Speed] Down: %.1f KB/s | Up: %.1f KB/s\n", rx_kb, tx_kb);
    return true;
}
