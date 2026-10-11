#ifndef __IKUAI_CLIENT_H__
#define __IKUAI_CLIENT_H__

#include <Arduino.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>

class IKuaiClient {
public:
    IKuaiClient();
    void setConfig(const String &host, uint16_t port, const String &user, const String &pass);
    bool isConfigured() const;
    bool updateSpeed(double &rx_kb, double &tx_kb);

private:
    String _host;
    uint16_t _port;
    String _user;
    String _pass;
    String _sess_cookie;
    unsigned long _last_login_time;

    bool login();
};

#endif // __IKUAI_CLIENT_H__
