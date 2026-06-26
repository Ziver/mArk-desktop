/**
 * Web Settings — serves a local task provider management page on port 80.
 * Visit http://<device-ip>/ from any phone on the same WiFi.
 */

#ifndef WEB_SETTINGS_H
#define WEB_SETTINGS_H

#include <WebServer.h>
#include <WiFi.h>
#include "task_store.h"

static WebServer webServer(80);

static String wsPage() {
    String h =
        "<!DOCTYPE html><html><head>"
        "<meta charset='utf-8'>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<title>Daily Scroll Settings</title>"
        "<style>"
        "body{font-family:-apple-system,sans-serif;max-width:480px;margin:0 auto;"
             "padding:16px;background:#f5f0e8;color:#2c2c2c}"
        "h1{font-size:22px;margin:0 0 4px}"
        "p.sub{color:#8a8a7a;font-size:13px;margin:0 0 16px}"
        ".card{background:#fff;border-radius:10px;padding:12px 16px;margin-bottom:10px}"
        ".row{display:flex;justify-content:space-between;align-items:center}"
        ".name{font-weight:600;font-size:15px}"
        ".type{font-size:11px;color:#888;text-transform:uppercase;margin-top:2px}"
        ".url{font-size:11px;color:#aaa;margin-top:2px;word-break:break-all}"
        "form.df{margin:0}"
        ".btn{background:#e8742a;color:#fff;border:none;border-radius:8px;"
             "padding:8px 14px;font-size:14px;cursor:pointer}"
        ".btn-del{background:#e0ddd9;color:#555}"
        "label{display:block;font-size:13px;font-weight:600;margin:12px 0 4px}"
        "input,select{width:100%;padding:10px;border:1px solid #d0cecc;border-radius:8px;"
                    "font-size:14px;box-sizing:border-box;background:#fff}"
        ".hint{font-size:12px;color:#8a8a7a;margin:4px 0 0}"
        ".submit{background:#e8742a;color:#fff;border:none;width:100%;padding:12px;"
               "border-radius:8px;font-size:15px;font-weight:600;cursor:pointer;margin-top:16px}"
        "h2{font-size:16px;margin:16px 0 8px}"
        "</style></head><body>"
        "<h1>Daily Scroll</h1>"
        "<p class='sub'>Manage your task providers below.</p>";

    // ── Current providers ──
    h += "<h2>Active Providers</h2>";
    if (task_provider_count == 0) {
        h += "<div class='card'><p style='color:#888;margin:0'>No task providers added yet.</p></div>";
    }
    for (int i = 0; i < task_provider_count; i++) {
        h += "<div class='card'><div class='row'>";
        h += "<div><div class='name'>" + String(task_providers[i].name) + "</div>";
        h += "<div class='type'>" + String(task_providers[i].type) + "</div>";
        h += "<div class='url'>" + String(task_providers[i].url) + "</div></div>";
        h += "<form class='df' method='POST' action='/delete'>"
             "<input type='hidden' name='idx' value='" + String(i) + "'>"
             "<button class='btn btn-del' type='submit'>Remove</button>"
             "</form>";
        h += "</div></div>";
    }

    // ── Add provider form ──
    h += "<h2>Add Task Provider</h2>"
         "<div class='card'><form method='POST' action='/add'>"
         "<label>Type</label>"
         "<select name='type' id='tp' onchange='upd()'>"
         "<option value='gcal'>Google Calendar (API)</option>"
         "<option value='ical'>iCal / ICS URL</option>"
         "<option value='vikunja'>Vikunja Tasks (API)</option>"
         "</select>"
         "<label>Display Name</label>"
         "<input type='text' name='name' placeholder='Work Tasks' required>"
         "<label id='ul'>Google Calendar ID / Email</label>"
         "<input type='text' name='url' id='ui' placeholder='you@gmail.com' required>"
         "<p class='hint' id='uh'>Your Google Calendar ID — usually your Gmail address. "
         "Find it in Google Calendar settings under &ldquo;Integrate calendar&rdquo;.</p>"
         "<div id='tg' style='display:none;'>"
         "<label>API Token</label>"
         "<input type='password' name='token' id='ti' placeholder='tk_...'>"
         "</div>"
         "<button class='submit' type='submit'>Add Provider</button>"
         "</form></div>";

    // ── Device Settings ──
    h += "<h2>Device Settings</h2>"
         "<div class='card'><form method='POST' action='/settings'>"
         "<label>Screen Timeout (minutes)</label>"
         "<input type='number' name='timeout' min='0' max='1440' value='" + String(taskStoreGetScreenTimeout()) + "' required>"
         "<p class='hint'>Minutes of inactivity before the screen turns off. Set to 0 to disable timeout.</p>"
         "<label>Appearance Theme</label>"
         "<select name='dark_mode'>"
         "<option value='0'" + String(taskStoreGetDarkMode() ? "" : " selected") + ">Light Theme</option>"
         "<option value='1'" + String(taskStoreGetDarkMode() ? " selected" : "") + ">Dark Theme</option>"
         "</select>"
         "<p class='hint'>Choose Light Mode or Dark Mode theme for the display.</p>"
         "<button class='submit' type='submit'>Save Settings</button>"
         "</form></div>";

    h += "<script>"
         "function upd(){"
         "var t=document.getElementById('tp').value,"
             "l=document.getElementById('ul'),"
             "u=document.getElementById('ui'),"
             "h=document.getElementById('uh'),"
             "g=document.getElementById('tg'),"
             "i=document.getElementById('ti');"
         "if(t==='ical'){"
         "l.textContent='iCal URL';"
         "u.placeholder='https://calendar.google.com/calendar/ical/...';"
         "h.innerHTML='Paste the full .ics URL. In Google Calendar: Settings &rarr; "
                      "your provider &rarr; &ldquo;Secret address in iCal format&rdquo;.';"
         "g.style.display='none';"
         "i.required=false;"
         "}else if(t==='vikunja'){"
         "l.textContent='Vikunja Base URL';"
         "u.placeholder='https://vikunja.example.com';"
         "h.textContent='The base URL of your self-hosted Vikunja instance (no trailing slash).';"
         "g.style.display='block';"
         "i.required=true;"
         "}else{"
         "l.textContent='Google Calendar ID / Email';"
         "u.placeholder='you@gmail.com';"
         "h.textContent='Your Google Calendar ID — usually your Gmail address.';"
         "g.style.display='none';"
         "i.required=false;"
         "}}"
         "</script>"
         "</body></html>";
    return h;
}

static void wsRoot()   { webServer.send(200, "text/html", wsPage()); }

static void wsAdd() {
    String type  = webServer.arg("type");
    String name  = webServer.arg("name");
    String url   = webServer.arg("url");
    String token = webServer.arg("token");
    name.trim(); url.trim(); token.trim();
    if (name.length() > 0 && url.length() > 0)
        taskStoreAdd(type.c_str(), name.c_str(), url.c_str(), token.c_str());
    webServer.sendHeader("Location", "/");
    webServer.send(302, "text/plain", "");
}

static void wsDelete() {
    if (webServer.hasArg("idx"))
        taskStoreDelete(webServer.arg("idx").toInt());
    webServer.sendHeader("Location", "/");
    webServer.send(302, "text/plain", "");
}

static void wsSettings() {
    if (webServer.hasArg("timeout")) {
        int timeout = webServer.arg("timeout").toInt();
        if (timeout < 0) timeout = 0;
        taskStoreSetScreenTimeout(timeout);
        Serial.printf("[Web] Saved screen timeout: %d min\n", timeout);
    }
    if (webServer.hasArg("dark_mode")) {
        bool dark = webServer.arg("dark_mode").toInt() == 1;
        bool old_dark = taskStoreGetDarkMode();
        if (dark != old_dark) {
            taskStoreSetDarkMode(dark);
            ui_dark_mode = dark;
            Serial.printf("[Web] Saved theme preference: %s\n", dark ? "DARK" : "LIGHT");

            // Rebuild UI immediately to reflect the theme change
            uiRebuild();
        }
    }
    webServer.sendHeader("Location", "/");
    webServer.send(302, "text/plain", "");
}

void webSettingsSetup() {
    webServer.on("/",       HTTP_GET,  wsRoot);
    webServer.on("/add",    HTTP_POST, wsAdd);
    webServer.on("/delete", HTTP_POST, wsDelete);
    webServer.on("/settings", HTTP_POST, wsSettings);
    webServer.begin();
    Serial.printf("[Web] Settings at http://%s/\n", WiFi.localIP().toString().c_str());
}

void webSettingsLoop() {
    webServer.handleClient();
}

#endif // WEB_SETTINGS_H
