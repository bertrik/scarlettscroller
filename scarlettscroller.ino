#include <stdint.h>

#include "cmdproc.h"
#include "editline.h"

#include "framebuffer.h"
#include "leddriver.h"

#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiManager.h>
#include <Arduino.h>
#include <WiFiUdp.h>

#define print Serial.printf

static WiFiManager wifiManager;
static WiFiUDP udpServer;

static char esp_id[64];
static char editline[120];
static uint8_t framebuffer[LED_HEIGHT][LED_WIDTH];
static volatile uint32_t frame_counter = 0;

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
        print("All red\n");
        draw_fill(255);
        break;
    case 2:
        print("Top half\n");
        memset(framebuffer, 0, sizeof(framebuffer));
        memset(framebuffer, 1, sizeof(framebuffer) / 2);
        break;
    case 3:
        print("Left half\n");
        for (int y = 0; y < 7; y++) {
            for (int x = 0; x < 80; x++) {
                framebuffer[y][x] = (x < 40) ? 1 : 0;
            }
        }
        break;
    case 4:
        print("Blokjes\n");
        for (int y = 0; y < 7; y++) {
            for (int x = 0; x < 80; x++) {
                framebuffer[y][x] = (x + y) & 1;
            }
        }
        break;
    default:
        print("Unhandled pattern %d\n", pat);
        return CMD_ARG;
    }

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
    { "pat", do_pat, "<pattern> Set pattern"},
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

// vsync callback
static void IRAM_ATTR vsync(int frame_nr)
{
    led_write_framebuffer(framebuffer);
    frame_counter = frame_nr;
}

void setup(void)
{
    led_init(vsync);

    // get ESP id
    snprintf(esp_id, sizeof(esp_id), "esp-ledsign-%06x", ESP.getChipId());
    Serial.begin(115200);
    print("\n%s\n", esp_id);

    EditInit(editline, sizeof(editline));

//    wifiManager.autoConnect(esp_id);

    led_enable();
}

void loop(void)
{
    // parse command line
    bool haveLine = false;
    if (Serial.available()) {
        char c;
        haveLine = EditLine(Serial.read(), &c);
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
//    MDNS.update();
}
