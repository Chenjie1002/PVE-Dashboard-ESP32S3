#ifndef __WEB_PORTAL_H__
#define __WEB_PORTAL_H__

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "ConfigManager.h"

class WebPortal {
public:
    WebPortal();
    void begin();
    void beginStaServer();
    void handleClient();
    bool isRebootPending() const { return _reboot_pending; }
    bool isApMode() const { return _is_ap_mode; }

private:
    WebServer _server;
    DNSServer _dns_server;
    bool _is_ap_mode;
    bool _reboot_pending;
    unsigned long _reboot_time;
    String _scanned_ssids;

    void scanNetworks();
    void handleRoot();
    void handleSave();
    void handleCaptivePortal();
};

#endif // __WEB_PORTAL_H__
