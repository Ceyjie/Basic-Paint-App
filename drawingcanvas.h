#ifndef DRAWINGCANVAS_H
#define DRAWINGCANVAS_H

#include <SDL2/SDL.h>
#include <vector>
#include <deque>
#include <string>
#include <map>

class DrawingCanvas {
public:
    DrawingCanvas(int width, int height);
    ~DrawingCanvas();

    void clear();
    void setColor(Uint8 r, Uint8 g, Uint8 b);
    void setSize(int size);
    void toggleEraser();
    void toggleFill();
    void toggleBackground();
    void startStroke(int x, int y, int fingerId);
    void continueStroke(int x, int y, int fingerId);
    void endStroke(int fingerId);
    void floodFill(int x, int y);
    void undo();
    void redo();
    bool save(const std::string& filename);
    bool load(const std::string& filename);

    SDL_Surface* getSurface() const { return canvas; }
    int getPenSize() const { return penSize; }
    bool isEraserMode() const { return eraserMode; }
    bool isFillMode() const { return fillMode; }
    Uint32 getCurrentColor() const { return currentColor; }

private:
    int width, height;
    SDL_Surface* canvas;
    Uint32 currentColor;
    Uint32 backgroundColor;
    int penSize;
    int minSize, maxSize;
    bool eraserMode, fillMode;
    std::map<int, SDL_Point> activeStrokes;   // fingerId -> last position
    std::deque<SDL_Surface*> undoStack;
    std::deque<SDL_Surface*> redoStack;
    int maxUndo = 20;

    void drawPoint(int x, int y);
    void drawCircle(int cx, int cy, int radius, Uint32 color);
    void drawThickLine(int x1, int y1, int x2, int y2);
    void pushState();
    void restoreState(SDL_Surface* surf);
    Uint32 getPixel(int x, int y);
    void setPixel(int x, int y, Uint32 color);
};

#endif
