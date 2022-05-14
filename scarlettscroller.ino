#include <stdint.h>

#include "cmdproc.h"
#include "editline.h"

#include "framebuffer.h"
#include "leddriver.h"
#include "draw.h"

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include <Arduino.h>
#include <WiFiUdp.h>

#include <ArduinoOTA.h>

#define print Serial.printf
#define UDP_PORT    8888

#define MY_NTP_SERVER "nl.pool.ntp.org"
#define MY_TZ "CET-1CEST,M3.5.0/02,M10.5.0/03"

static WiFiManager wifiManager;
static WiFiUDP udpServer;
static WiFiUDP textServer;
static uint8_t udpframe[LED_HEIGHT * LED_WIDTH];

static char espid[64];
static char editline[120];
static uint8_t framebuffer[LED_HEIGHT][LED_WIDTH];
static volatile uint32_t frame_counter = 0;

static int scrolling = 0;
static char scroll_buf[1501];
static int scroll_pos = 0;

static int do_pix(int argc, char *argv[])
{
    if (argc < 4) {
        return CMD_ARG;
    }
    int x = atoi(argv[1]);
    int y = atoi(argv[2]);
    uint8_t c = atoi(argv[3]);
    if ((x >= 80) || (y >= 7)) {
        return CMD_ARG;
    }
    framebuffer[y][x] = c;
    return CMD_OK;
}

static void draw_fill(uint8_t c)
{
    memset(framebuffer, c, sizeof(framebuffer));
}

static int do_pat(int argc, char *argv[])
{
    if (argc < 2) {
        return CMD_ARG;
    }
    int pat = atoi(argv[1]);

    switch (pat) {
    case 0:
        print("All clear\n");
        draw_fill(0);
        break;
    case 1:
        print("All on\n");
        draw_fill(255);
        break;
    case 2:
        print("Every other pixel\n");
        for (int y = 0; y < 7; y++) {
            for (int x = 0; x < 80; x++) {
                framebuffer[y][x] = (x + y) & 1 ? 255 : 0;
            }
        }
        break;
    case 3:
        print("Horizontal gradient\n");
        for (int y = 0; y < 7; y++) {
            for (int x = 0; x < 80; x++) {
                framebuffer[y][x] = map(x, 0, 80, 0, 256);
            }
        }
        break;
    default:
        print("Unhandled pattern %d\n", pat);
        return CMD_ARG;
    }

    return CMD_OK;
}

static int do_text(int argc, char *argv[])
{
    if (argc < 2) {
        return CMD_ARG;
    }

    draw_text(argv[1], 0, 255, 0);
    return CMD_OK;
}

static int do_fps(int argc, char *argv[])
{
    print("Measuring ...");
    uint32_t count = frame_counter;
    delay(1000);
    int fps = frame_counter - count;
    print("FPS = %d\n", fps);
    return CMD_OK;
}

static int do_enable(int argc, char *argv[])
{
    bool enable = true;
    if (argc > 1) {
        enable = atoi(argv[1]) != 0;
    }
    if (enable) {
        led_enable();
    } else {
        led_disable();
    }
    return CMD_OK;
}

static int do_reboot(int argc, char *argv[])
{
    led_disable();
    ESP.restart();
    return CMD_OK;
}

static int do_help(int argc, char *argv[]);
static const cmd_t commands[] = {
    { "pix", do_pix, "<col> <row> [intensity] Set pixel" },
    { "pat", do_pat, "<pattern> Set pattern" },
    { "text", do_text, "<text> Draw test" },
    { "fps", do_fps, "Show FPS" },
    { "enable", do_enable, "[0|1] Enable/disable" },
    { "reboot", do_reboot, "Reboot" },
    { "help", do_help, "Show help" },
    { NULL, NULL, NULL }
};

static void show_help(const cmd_t * cmds)
{
    for (const cmd_t * cmd = cmds; cmd->cmd != NULL; cmd++) {
        print("%10s: %s\n", cmd->name, cmd->help);
    }
}

static int do_help(int argc, char *argv[])
{
    show_help(commands);
    return CMD_OK;
}

static void draw_time(void)
{
    time_t now;
    char timestr[16];

    time(&now);
    struct tm * info = localtime (&now);
    sprintf(timestr, "%02d:%02d:%02d", info->tm_hour, info->tm_min, info->tm_sec);

    // draw fancy background
    draw_clear();
    for (int x = 0; x < 18; x++) {
        uint8_t c = (255 - 12 * x);
        draw_vline(x, c);
        draw_vline(LED_WIDTH - 1 - x, c);
    }

    // draw time
    draw_text(timestr, 19, 255, 0);
}

// vsync callback
static void IRAM_ATTR vsync(int frame_nr)
{
    led_write_framebuffer(framebuffer);
    frame_counter = frame_nr;
}

void setup(void)
{
    led_init(vsync);

    snprintf(espid, sizeof(espid), "scarlett-%06x", ESP.getChipId());
    Serial.begin(115200);
    print("\n%s\n", espid);

    EditInit(editline, sizeof(editline));
    draw_init((uint8_t *) framebuffer, 2.2);
    draw_flip(true, true);

    wifiManager.autoConnect(espid);
    configTime(MY_TZ, MY_NTP_SERVER);
    draw_text(WiFi.localIP().toString().c_str(), 0, 255, 0);
    udpServer.begin(UDP_PORT);
    MDNS.begin(espid);
    MDNS.addService("grayscale", "udp", UDP_PORT);

    textServer.begin(5000);
    MDNS.addService("text", "udp", 5000);

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
        for (int x = 0; x < LED_WIDTH; x++) {
            uint8_t c;
            if ((x == 0) || (x == 79)) {
                c = 255;
            } else {
                int l = map(progress, 0, total, 0, 80);
                c = (x < l) ? 128 : 0;
            }
            draw_pixel(x, 0, 0);
            draw_pixel(x, 1, 255);
            draw_pixel(x, 2, c);
            draw_pixel(x, 3, c);
            draw_pixel(x, 4, c);
            draw_pixel(x, 5, 255);
            draw_pixel(x, 6, 0);
        }
    });
    ArduinoOTA.onEnd([](){draw_clear();});
    ArduinoOTA.begin();

    led_enable();
}

void loop(void)
{
    static int scroll_tick_last = -1;
    static unsigned long last_activity = 0;

    unsigned long ms = millis();

    // handle currently scrolling text with priority
    if (scrolling > 0) {
        int scroll_tick = millis() / 20;
        if (scroll_tick != scroll_tick_last) {
            scroll_tick_last = scroll_tick;
            if (draw_text(scroll_buf, scroll_pos, 255, 0) < 0) {
                scroll_pos = LED_WIDTH;
                scrolling--;
            } else {
                scroll_pos--;
            }
        }
    } else {
        // handle incoming UDP text
        int udpSize = textServer.parsePacket();
        if (udpSize > 0) {
            size_t len = textServer.read((uint8_t *) udpframe, sizeof(udpframe));
            if (len < sizeof(scroll_buf)) {
                memcpy(scroll_buf, udpframe, len);
                scroll_buf[len] = 0;
                scrolling = 3;
                scroll_pos = LED_WIDTH;
            }
        }

        // handle incoming UDP frame
        udpSize = udpServer.parsePacket();
        if (udpSize > 0) {
            int len = udpServer.read((uint8_t *) udpframe, sizeof(udpframe));
            if (len == (LED_HEIGHT * LED_WIDTH)) {
                for (int i = 0; i < len; i++) {
                    draw_pixel(i % LED_WIDTH, i / LED_WIDTH, udpframe[i]);
                }
            }
            last_activity = ms;
        }

        // show time when idle for a while
        if ((ms - last_activity) > 60000) {
            static int sec_last = 0;
            int sec = ms / 1000;
            if (sec != sec_last) {
                sec_last = sec;
                draw_time();
            }
        }
    }

    // parse command line
    if (Serial.available()) {
        char c = Serial.read();
        bool haveLine = EditLine(c, &c);
        Serial.write(c);
        if (haveLine) {
            int result = cmd_process(commands, editline);
            switch (result) {
            case CMD_OK:
                print("OK\n");
                break;
            case CMD_NO_CMD:
                break;
            case CMD_UNKNOWN:
                print("Unknown command, available commands:\n");
                show_help(commands);
                break;
            case CMD_ARG:
                print("Invalid argument(s)\n");
                break;
            default:
                print("%d\n", result);
                break;
            }
            print(">");
        }
    }
    // network update
    MDNS.update();
    ArduinoOTA.handle();
}
