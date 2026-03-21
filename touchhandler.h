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
    struct SlotState { int x, y; float pressure; };
    std::map<int, SlotState> currentSlots;   // slot -> screen coords + pressure
    int pressureMax;
    int currentSlot = 0;  // persists across processEvents calls — kernel omits ABS_MT_SLOT when unchanged
    void applyCalibration(int& x, int& y);
    void generateTouchEvent(int type, int fingerId, int x, int y, float pressure, std::vector<SDL_Event>& events);
};

#endif
