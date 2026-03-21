#ifndef TOUCHHANDLER_H
#define TOUCHHANDLER_H

#include <SDL2/SDL.h>
#include <vector>
#include <map>

struct libevdev;

class TouchHandler {
public:
    TouchHandler(int screenWidth, int screenHeight);
    ~TouchHandler();

    bool init();
    void processEvents(std::vector<SDL_Event>& events);
    void setCalibration(int xOffset, int yOffset);
    void getCalibration(int& xOffset, int& yOffset) const;

private:
    int screenW, screenH;
    int calibrationX, calibrationY;
    int touchXMax, touchYMax;
    struct libevdev* dev;
    int fd;
    std::map<int, SDL_Point> currentSlots;   // slot -> raw coordinates
    void applyCalibration(int& x, int& y);
    void generateTouchEvent(int type, int fingerId, int x, int y, std::vector<SDL_Event>& events);
};

#endif
