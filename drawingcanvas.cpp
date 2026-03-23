#include "drawingcanvas.h"
#include <SDL2/SDL_image.h>
#include <cstring>
#include <queue>
#include <algorithm>
#include <cmath>

DrawingCanvas::DrawingCanvas(int w, int h) : width(w), height(h) {
    canvas = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    currentColor = SDL_MapRGB(canvas->format, 0, 0, 0);
    backgroundColor = SDL_MapRGB(canvas->format, 255, 255, 255);
    penSize = 5;
    minSize = 1;
    maxSize = 50;
    eraserMode = false;
    fillMode = false;
    SDL_FillRect(canvas, nullptr, backgroundColor);
    pushState();
}

DrawingCanvas::~DrawingCanvas() {
    SDL_FreeSurface(canvas);
    for (auto s : undoStack) SDL_FreeSurface(s);
    for (auto s : redoStack) SDL_FreeSurface(s);
}

void DrawingCanvas::clear() {
    SDL_FillRect(canvas, nullptr, backgroundColor);
    pushState();
}

void DrawingCanvas::resetToBlank() {
    SDL_FillRect(canvas, nullptr, backgroundColor);
    for (auto s : undoStack) SDL_FreeSurface(s);
    for (auto s : redoStack) SDL_FreeSurface(s);
    undoStack.clear();
    redoStack.clear();
    pushState();
}

void DrawingCanvas::setColor(Uint8 r, Uint8 g, Uint8 b) {
    currentColor = SDL_MapRGB(canvas->format, r, g, b);
    eraserMode = false;
    fillMode = false;
}

void DrawingCanvas::setSize(int size) {
    penSize = std::max(minSize, std::min(maxSize, size));
}

void DrawingCanvas::toggleEraser() {
    eraserMode = !eraserMode;
    fillMode = false;
}

void DrawingCanvas::toggleFill() {
    fillMode = !fillMode;
    if (fillMode) eraserMode = false;
}

void DrawingCanvas::toggleBackground() {
    Uint32 oldBg = backgroundColor;
    backgroundColor = (oldBg == SDL_MapRGB(canvas->format, 255,255,255)) ?
                       SDL_MapRGB(canvas->format, 0,0,0) :
                       SDL_MapRGB(canvas->format, 255,255,255);
    Uint32* pixels = (Uint32*)canvas->pixels;
    int totalPixels = width * height;
    for (int i = 0; i < totalPixels; i++) {
        Uint32 pix = pixels[i];
        if (pix != oldBg) {
            Uint8 r = (pix >> 16) & 0xFF;
            Uint8 g = (pix >> 8) & 0xFF;
            Uint8 b = pix & 0xFF;
            pixels[i] = SDL_MapRGB(canvas->format, 255 - r, 255 - g, 255 - b);
        } else {
            pixels[i] = backgroundColor;
        }
    }
    pushState();
}

void DrawingCanvas::startStroke(int x, int y, int fingerId) {
    if (fillMode) {
        floodFill(x, y);
        fillMode = false;
        return;
    }
    activeStrokes.erase(fingerId);
    activeStrokes[fingerId] = {x, y};
    drawPoint(x, y);
}

void DrawingCanvas::continueStroke(int x, int y, int fingerId) {
    auto it = activeStrokes.find(fingerId);
    if (it == activeStrokes.end()) return;
    drawThickLine(it->second.x, it->second.y, x, y);
    it->second = {x, y};
}

void DrawingCanvas::endStroke(int fingerId) {
    if (activeStrokes.erase(fingerId) > 0) {
        pushState();
    }
}

void DrawingCanvas::drawPoint(int x, int y) {
    Uint32 color = eraserMode ? backgroundColor : currentColor;
    drawCircleAA(x, y, penSize, color);
}

void DrawingCanvas::drawCircle(int cx, int cy, int radius, Uint32 color) {
    int r2 = radius * radius;
    for (int y = -radius; y <= radius; y++) {
        int dx = (int)sqrt(r2 - y*y);
        int x1 = cx - dx;
        int x2 = cx + dx;
        for (int x = x1; x <= x2; x++) {
            if (x >= 0 && x < width && cy+y >= 0 && cy+y < height) {
                ((Uint32*)canvas->pixels)[(cy+y) * canvas->w + x] = color;
            }
        }
    }
}

void DrawingCanvas::blendPixel(int x, int y, Uint32 color, float alpha) {
    if (x < 0 || x >= width || y < 0 || y >= height || alpha <= 0.0f) return;
    if (alpha >= 1.0f) { setPixel(x, y, color); return; }

    Uint32* px = &((Uint32*)canvas->pixels)[y * canvas->w + x];
    Uint8 sr = (color >> 16) & 0xFF;
    Uint8 sg = (color >>  8) & 0xFF;
    Uint8 sb =  color        & 0xFF;
    Uint8 dr = (*px >> 16) & 0xFF;
    Uint8 dg = (*px >>  8) & 0xFF;
    Uint8 db =  *px        & 0xFF;

    Uint8 r = (Uint8)(sr * alpha + dr * (1.0f - alpha));
    Uint8 g = (Uint8)(sg * alpha + dg * (1.0f - alpha));
    Uint8 b = (Uint8)(sb * alpha + db * (1.0f - alpha));
    *px = SDL_MapRGB(canvas->format, r, g, b);
}

void DrawingCanvas::drawCircleAA(int cx, int cy, int radius, Uint32 color) {
    if (radius <= 1) { drawCircle(cx, cy, radius, color); return; }

    float r = (float)radius;
    int iy0 = std::max(0, cy - radius - 1);
    int iy1 = std::min(height - 1, cy + radius + 1);

    for (int y = iy0; y <= iy1; y++) {
        float dy = (float)(y - cy);
        float edgeDist = r - std::abs(dy);
        if (edgeDist < -1.0f) continue;

        float halfW = std::sqrt(std::max(0.0f, r*r - dy*dy));
        int x0 = (int)(cx - halfW);
        int x1 = (int)(cx + halfW);

        for (int x = std::max(0, x0); x <= std::min(width-1, x1); x++)
            setPixel(x, y, color);

        float leftFrac = (cx - halfW) - (float)x0;
        if (leftFrac > 0.0f)
            blendPixel(x0 - 1, y, color, leftFrac);

        float rightFrac = (float)(x1 + 1) - (cx + halfW);
        if (rightFrac > 0.0f)
            blendPixel(x1 + 1, y, color, rightFrac);
    }
}

void DrawingCanvas::drawThickLine(int x1, int y1, int x2, int y2) {
    int dx = abs(x2 - x1), dy = abs(y2 - y1);
    int sx = x1 < x2 ? 1 : -1, sy = y1 < y2 ? 1 : -1;
    int err = dx - dy;
    while (true) {
        drawPoint(x1, y1);
        if (x1 == x2 && y1 == y2) break;
        int e2 = err * 2;
        if (e2 > -dy) { err -= dy; x1 += sx; }
        if (e2 < dx) { err += dx; y1 += sy; }
    }
}

void DrawingCanvas::floodFill(int x, int y) {
    if (x < 0 || x >= width || y < 0 || y >= height) return;
    Uint32 target = getPixel(x, y);
    Uint32 fill = eraserMode ? backgroundColor : currentColor;
    if (target == fill) return;

    pushState();

    std::queue<SDL_Point> queue;
    queue.push({x, y});
    std::vector<std::vector<bool>> visited(width, std::vector<bool>(height, false));
    visited[x][y] = true;

    while (!queue.empty()) {
        SDL_Point p = queue.front(); queue.pop();
        setPixel(p.x, p.y, fill);

        for (int dx = -1; dx <= 1; dx++) {
            for (int dy = -1; dy <= 1; dy++) {
                if (dx == 0 && dy == 0) continue;
                if (dx != 0 && dy != 0) continue;
                int nx = p.x + dx, ny = p.y + dy;
                if (nx >= 0 && nx < width && ny >= 0 && ny < height && !visited[nx][ny]) {
                    if (getPixel(nx, ny) == target) {
                        visited[nx][ny] = true;
                        queue.push({nx, ny});
                    }
                }
            }
        }
    }
}

void DrawingCanvas::drawShapeLine(int x1, int y1, int x2, int y2) {
    pushState();
    drawThickLine(x1, y1, x2, y2);
    pushState();
}

void DrawingCanvas::drawShapeRect(int x1, int y1, int x2, int y2) {
    pushState();
    int left   = std::min(x1, x2);
    int right  = std::max(x1, x2);
    int top    = std::min(y1, y2);
    int bottom = std::max(y1, y2);
    drawThickLine(left,  top,    right, top);
    drawThickLine(right, top,    right, bottom);
    drawThickLine(right, bottom, left,  bottom);
    drawThickLine(left,  bottom, left,  top);
    pushState();
}

void DrawingCanvas::drawShapeEllipse(int cx, int cy, int rx, int ry) {
    if (rx <= 0 || ry <= 0) return;
    pushState();
    Uint32 color = eraserMode ? backgroundColor : currentColor;
    int steps = 2 * (rx + ry);
    for (int i = 0; i <= steps; i++) {
        float angle = 2.0f * M_PI * i / steps;
        int px = cx + (int)(rx * cosf(angle));
        int py = cy + (int)(ry * sinf(angle));
        drawCircleAA(px, py, penSize, color);
    }
    pushState();
}

void DrawingCanvas::undo() {
    if (undoStack.size() > 1) {
        redoStack.push_front(undoStack.front());
        undoStack.pop_front();
        restoreState(undoStack.front());
    }
}

void DrawingCanvas::redo() {
    if (!redoStack.empty()) {
        undoStack.push_front(redoStack.front());
        redoStack.pop_front();
        restoreState(undoStack.front());
    }
}

bool DrawingCanvas::save(const std::string& filename) {
    return IMG_SavePNG(canvas, filename.c_str()) == 0;
}

bool DrawingCanvas::load(const std::string& filename) {
    SDL_Surface* img = IMG_Load(filename.c_str());
    if (!img) return false;
    SDL_BlitScaled(img, nullptr, canvas, nullptr);
    SDL_FreeSurface(img);
    pushState();
    return true;
}

Uint32 DrawingCanvas::getPixel(int x, int y) {
    Uint32* pixels = (Uint32*)canvas->pixels;
    return pixels[y * canvas->w + x];
}

Uint32 DrawingCanvas::getPixelAt(int x, int y) {
    if (x < 0 || x >= width || y < 0 || y >= height) return 0;
    return getPixel(x, y);
}

void DrawingCanvas::setPixel(int x, int y, Uint32 color) {
    Uint32* pixels = (Uint32*)canvas->pixels;
    pixels[y * canvas->w + x] = color;
}

void DrawingCanvas::pushState() {
    SDL_Surface* copy = SDL_ConvertSurface(canvas, canvas->format, 0);
    undoStack.push_front(copy);
    if (undoStack.size() > maxUndo) {
        SDL_FreeSurface(undoStack.back());
        undoStack.pop_back();
    }
    while (!redoStack.empty()) {
        SDL_FreeSurface(redoStack.back());
        redoStack.pop_back();
    }
}

void DrawingCanvas::restoreState(SDL_Surface* surf) {
    SDL_BlitSurface(surf, nullptr, canvas, nullptr);
}
