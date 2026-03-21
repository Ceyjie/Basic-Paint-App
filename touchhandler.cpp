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

static const int RAW_THRESHOLD = 500;   // Filter out raw coordinates below this value

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
    touchXMin = touchYMin = 0;
    touchXMax = touchYMax = 4095;   // fallback values
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
    if (abs_x) {
        touchXMin = abs_x->minimum;
        touchXMax = abs_x->maximum;
    }
    if (abs_y) {
        touchYMin = abs_y->minimum;
        touchYMax = abs_y->maximum;
    }
    std::cout << "Touch range: " << touchXMin << "-" << touchXMax
              << " x " << touchYMin << "-" << touchYMax << std::endl;
    return true;
}

void TouchHandler::processEvents(std::vector<SDL_Event>& events) {
    if (!dev) return;

    struct input_event ev;
    int slot = 0;
    // Temporary storage for one frame: slot -> {x, y, xSet, ySet}
    struct SlotFrame {
        int x = 0, y = 0;
        bool xSet = false, ySet = false;
    };
    std::map<int, SlotFrame> frameSlots;

    while (libevdev_has_event_pending(dev)) {
        int rc = libevdev_next_event(dev, LIBEVDEV_READ_FLAG_NORMAL, &ev);
        if (rc == -EAGAIN) break;
        if (rc < 0) continue;

        if (ev.type == EV_ABS) {
            if (ev.code == ABS_MT_SLOT) {
                slot = ev.value;
            } else if (ev.code == ABS_MT_POSITION_X) {
                frameSlots[slot].x = ev.value;
                frameSlots[slot].xSet = true;
            } else if (ev.code == ABS_MT_POSITION_Y) {
                frameSlots[slot].y = ev.value;
                frameSlots[slot].ySet = true;
            }
            // If you want to use pressure, add ABS_MT_PRESSURE here
        } else if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
            // Build a map of slots that have complete valid coordinates
            std::map<int, SDL_Point> validSlots;

            for (auto& [slot, sf] : frameSlots) {
                if (sf.xSet && sf.ySet) {
                    int rawX = sf.x;
                    int rawY = sf.y;
                    // Filter out coordinates that are too low (likely noise)
                    if (rawX > RAW_THRESHOLD && rawY > RAW_THRESHOLD) {
                        // Map to screen coordinates using the device's actual min/max
                        int screenX = (rawX - touchXMin) * screenW / (touchXMax - touchXMin);
                        int screenY = (rawY - touchYMin) * screenH / (touchYMax - touchYMin);
                        applyCalibration(screenX, screenY);
                        // Clamp to screen bounds
                        screenX = std::max(0, std::min(screenW - 1, screenX));
                        screenY = std::max(0, std::min(screenH - 1, screenY));
                        validSlots[slot] = {screenX, screenY};
                    }
                }
            }

            // Generate events for new / moved touches
            for (auto& [slot, pt] : validSlots) {
                auto it = currentSlots.find(slot);
                if (it == currentSlots.end()) {
                    // New touch
                    generateTouchEvent(SDL_FINGERDOWN, slot, pt.x, pt.y, events);
                    currentSlots[slot] = pt;
                } else if (it->second.x != pt.x || it->second.y != pt.y) {
                    // Moved touch
                    generateTouchEvent(SDL_FINGERMOTION, slot, pt.x, pt.y, events);
                    it->second = pt;
                }
            }

            // Generate up events for slots that disappeared
            std::vector<int> toErase;
            for (auto& [slot, _] : currentSlots) {
                if (validSlots.find(slot) == validSlots.end()) {
                    generateTouchEvent(SDL_FINGERUP, slot, 0, 0, events);
                    toErase.push_back(slot);
                }
            }
            for (int slot : toErase) currentSlots.erase(slot);

            // Clear frame data for next frame
            frameSlots.clear();
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
