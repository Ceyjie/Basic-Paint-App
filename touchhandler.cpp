#include "touchhandler.h"
#include <libevdev/libevdev.h>
#include <fcntl.h>
#include <unistd.h>
#include <linux/input.h>
#include <sys/select.h>
#include <cerrno>
#include <iostream>
#include <dirent.h>
#include <cstring>

static std::string findTouchDevice() {
    DIR* dir = opendir("/dev/input");
    if (!dir) return "";
    struct dirent* entry;
    std::string result;
    while ((entry = readdir(dir)) != nullptr) {
        if (strncmp(entry->d_name, "event", 5) != 0) continue;
        std::string path = "/dev/input/" + std::string(entry->d_name);
        int fd = open(path.c_str(), O_RDONLY);
        if (fd < 0) continue;
        struct libevdev* dev = nullptr;
        if (libevdev_new_from_fd(fd, &dev) == 0) {
            const char* name = libevdev_get_name(dev);
            std::cout << "Found input device: " << name << " at " << path << std::endl;
            if (name && (strstr(name, "Touch") || strstr(name, "p403") || strstr(name, "Virtual Ink"))) {
                result = path;
                libevdev_free(dev);
                close(fd);
                break;
            }
            libevdev_free(dev);
        }
        close(fd);
    }
    closedir(dir);
    return result;
}

TouchHandler::TouchHandler(int w, int h) : screenW(w), screenH(h) {
    calibrationX = calibrationY = 0;
    touchXMax = 4095;
    touchYMax = 4095;
    dev = nullptr;
    fd = -1;
}

TouchHandler::~TouchHandler() {
    if (dev) libevdev_free(dev);
    if (fd != -1) close(fd);
}

bool TouchHandler::init() {
    std::string devicePath = findTouchDevice();
    if (devicePath.empty()) {
        std::cerr << "No touch device found.\n";
        return false;
    }
    std::cout << "Using touch device: " << devicePath << std::endl;
    fd = open(devicePath.c_str(), O_RDONLY | O_NONBLOCK);
    if (fd < 0) return false;

    if (libevdev_new_from_fd(fd, &dev) < 0) {
        close(fd);
        fd = -1;
        return false;
    }

    const struct input_absinfo* abs_x = libevdev_get_abs_info(dev, ABS_MT_POSITION_X);
    const struct input_absinfo* abs_y = libevdev_get_abs_info(dev, ABS_MT_POSITION_Y);
    if (abs_x) touchXMax = abs_x->maximum;
    if (abs_y) touchYMax = abs_y->maximum;
    std::cout << "Touch range: " << touchXMax << "x" << touchYMax << std::endl;
    return true;
}

void TouchHandler::processEvents(std::vector<SDL_Event>& events) {
    if (!dev) return;

    struct input_event ev;
    int slot = 0;
    std::map<int, SDL_Point> newSlots;

    while (libevdev_has_event_pending(dev)) {
        int rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
        if (rc == -EAGAIN) break;
        if (rc < 0) continue;

        if (ev.type == EV_ABS) {
            if (ev.code == ABS_MT_SLOT) {
                slot = ev.value;
            } else if (ev.code == ABS_MT_POSITION_X) {
                newSlots[slot].x = ev.value;
            } else if (ev.code == ABS_MT_POSITION_Y) {
                newSlots[slot].y = ev.value;
            }
        } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
            // Process new frame
            for (auto& [slot, raw] : newSlots) {
                int x = raw.x * screenW / touchXMax;
                int y = raw.y * screenH / touchYMax;
                applyCalibration(x, y);
                std::cout << "Raw: " << raw.x << "," << raw.y << " -> Screen: " << x << "," << y << std::endl;
                if (currentSlots.find(slot) == currentSlots.end()) {
                    generateTouchEvent(SDL_FINGERDOWN, slot, x, y, events);
                } else {
                    auto& prev = currentSlots[slot];
                    if (prev.x != x || prev.y != y) {
                        generateTouchEvent(SDL_FINGERMOTION, slot, x, y, events);
                    }
                }
                currentSlots[slot] = {x, y};
            }

            // Detect releases
            std::vector<int> toErase;
            for (auto& [slot, _] : currentSlots) {
                if (newSlots.find(slot) == newSlots.end()) {
                    generateTouchEvent(SDL_FINGERUP, slot, 0, 0, events);
                    toErase.push_back(slot);
                }
            }
            for (int slot : toErase) currentSlots.erase(slot);

            newSlots.clear();
        }
    }
}

void TouchHandler::applyCalibration(int& x, int& y) {
    x += calibrationX;
    y += calibrationY;
    x = std::max(0, std::min(screenW - 1, x));
    y = std::max(0, std::min(screenH - 1, y));
}

void TouchHandler::generateTouchEvent(int type, int fingerId, int x, int y, std::vector<SDL_Event>& events) {
    SDL_Event e;
    e.type = type;
    e.tfinger.fingerId = fingerId;
    e.tfinger.x = x / (float)screenW;
    e.tfinger.y = y / (float)screenH;
    e.tfinger.dx = 0;
    e.tfinger.dy = 0;
    events.push_back(e);
}

void TouchHandler::setCalibration(int xOffset, int yOffset) {
    calibrationX = xOffset;
    calibrationY = yOffset;
}

void TouchHandler::getCalibration(int& xOffset, int& yOffset) const {
    xOffset = calibrationX;
    yOffset = calibrationY;
}
