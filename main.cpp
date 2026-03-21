#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_ttf.h>
#include <iostream>
#include <cstdlib>
#include <string>
#include <map>
#include "drawingcanvas.h"
#include "touchhandler.h"

// Colors
const Uint32 COLOR_WHITE = SDL_MapRGB(SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888), 255, 255, 255);
const Uint32 COLOR_BLACK = SDL_MapRGB(SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888), 0, 0, 0);
const Uint32 COLOR_RED   = SDL_MapRGB(SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888), 255, 59, 48);
const Uint32 COLOR_GREEN = SDL_MapRGB(SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888), 40, 205, 65);
const Uint32 COLOR_BLUE  = SDL_MapRGB(SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888), 0, 122, 255);
const Uint32 COLOR_YELLOW= SDL_MapRGB(SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888), 255, 204, 0);
const Uint32 COLOR_PURPLE= SDL_MapRGB(SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888), 175, 82, 222);
const Uint32 COLOR_ORANGE= SDL_MapRGB(SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888), 255, 149, 0);
const Uint32 COLOR_PINK  = SDL_MapRGB(SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888), 255, 45, 85);
const Uint32 COLOR_CYAN  = SDL_MapRGB(SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888), 90, 200, 250);
const Uint32 COLOR_LIGHT_GRAY = SDL_MapRGB(SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888), 229, 229, 234);
const Uint32 COLOR_DARK_GRAY  = SDL_MapRGB(SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888), 44, 44, 46);
const Uint32 COLOR_TOOLBAR_BG = SDL_MapRGB(SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888), 240, 240, 245);
const Uint32 COLOR_BORDER     = SDL_MapRGB(SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888), 180, 180, 185);
const Uint32 COLOR_HIGHLIGHT  = SDL_MapRGB(SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888), 180, 200, 255);

struct Button {
    SDL_Rect rect;
    std::string type;
    int index;
};

class PiPaint {
public:
    PiPaint();
    ~PiPaint();
    void run();

private:
    int width, height;
    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Texture* canvasTexture;
    TTF_Font* fontSmall;
    TTF_Font* fontMedium;
    TTF_Font* fontLarge;

    DrawingCanvas canvas;
    TouchHandler touch;

    int toolbarHeight = 80;
    std::vector<Button> toolbarButtons;
    int penSize = 5;
    bool fillArmed = false;

    bool showOverlay = false;
    std::string overlayType;
    std::vector<std::string> overlayFiles;
    int overlayScroll = 0;
    int selectedIndex = -1;
    std::string filenameInput;
    int cursorPos = 0;
    bool cursorVisible = true;
    Uint32 cursorTimer = 0;

    bool browsingFolder = false;
    std::string currentBrowsePath;
    std::vector<std::string> subdirs;
    int browseScroll = 0;
    int selectedSubdir = -1;

    int activeFinger = -1;
    SDL_Point activeFingerPos;

    std::map<std::string, Uint32> lastClickTime;
    const Uint32 doubleClickThreshold = 300;

    void createToolbar();
    void updateCanvasTexture();
    void drawToolbar();
    void drawOverlay();
    void handleTouchDown(int fingerId, int x, int y);
    void handleTouchMove(int fingerId, int x, int y);
    void handleTouchUp(int fingerId);
    void handleMouseButtonDown(SDL_MouseButtonEvent& ev);
    void handleMouseMotion(SDL_MouseMotionEvent& ev);
    void handleMouseButtonUp(SDL_MouseButtonEvent& ev);
    void handleKeyboard(SDL_KeyboardEvent& ev);
    void executeToolAction(const std::string& type, int index = -1);

    void showSaveOverlay();
    void showLoadOverlay();
    void refreshFileList();
    std::string generateRandomFilename();
    void saveCurrentDrawing();
    void loadSelectedDrawing();

    void enterFolderBrowser();
    void refreshSubdirs();
    void goUp();
    void goHome();
    void goMedia();
    void selectCurrentFolder();

    void calibrate();
};

PiPaint::PiPaint() : canvas(1920, 1080), touch(1920, 1080) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    IMG_Init(IMG_INIT_PNG);
    TTF_Init();

    SDL_DisplayMode dm;
    SDL_GetCurrentDisplayMode(0, &dm);
    width = dm.w;
    height = dm.h;
    window = SDL_CreateWindow("Pi Paint", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                               width, height, SDL_WINDOW_FULLSCREEN_DESKTOP);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    canvasTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, width, height);
    fontSmall = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 20);
    fontMedium = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 28);
    fontLarge = TTF_OpenFont("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 40);
    if (!fontSmall) fontSmall = TTF_OpenFont("DejaVuSans.ttf", 20);

    touch.init();
    createToolbar();

    system("mkdir -p ~/pi-paint/drawings");
    currentBrowsePath = std::string(getenv("HOME")) + "/pi-paint/drawings";

    canvas.clear();
}

PiPaint::~PiPaint() {
    TTF_CloseFont(fontSmall);
    TTF_CloseFont(fontMedium);
    TTF_CloseFont(fontLarge);
    SDL_DestroyTexture(canvasTexture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    TTF_Quit();
    IMG_Quit();
    SDL_Quit();
}

void PiPaint::createToolbar() {
    int colorSize = 40, margin = 8;
    int y = (toolbarHeight - colorSize) / 2;
    int x = 10;
    const Uint32 colors[10] = {COLOR_BLACK, COLOR_RED, COLOR_GREEN, COLOR_BLUE, COLOR_YELLOW,
                               COLOR_PURPLE, COLOR_ORANGE, COLOR_PINK, COLOR_CYAN, COLOR_WHITE};
    for (int i = 0; i < 10; i++) {
        Button btn;
        btn.rect = {x, y, colorSize, colorSize};
        btn.type = "color";
        btn.index = i;
        toolbarButtons.push_back(btn);
        x += colorSize + margin;
    }

    int btnW = 60, btnH = 35;
    margin = 6;
    y = (toolbarHeight - btnH) / 2;
    struct { std::string label; std::string type; } textBtns[] = {
        {"Eraser", "eraser"},
        {"Fill", "fill"},
        {"Bg", "bg"},
        {"Undo", "undo"},
        {"Redo", "redo"},
        {"Clear", "clear"},
        {"Save", "save"},
        {"Load", "load"},
        {"Exit", "exit"}
    };
    for (auto& tb : textBtns) {
        Button btn;
        btn.rect = {x, y, btnW, btnH};
        btn.type = tb.type;
        toolbarButtons.push_back(btn);
        x += btnW + margin;
    }

    int sizeW = 35, sizeH = 35;
    margin = 5;
    y = (toolbarHeight - sizeH) / 2;

    Button minusBtn;
    minusBtn.rect = {x, y, sizeW, sizeH};
    minusBtn.type = "size_down";
    toolbarButtons.push_back(minusBtn);
    x += sizeW + margin;

    Button sizeLabel;
    sizeLabel.rect = {x, y, 45, sizeH};
    sizeLabel.type = "size_label";
    toolbarButtons.push_back(sizeLabel);
    x += 45 + margin;

    Button plusBtn;
    plusBtn.rect = {x, y, sizeW, sizeH};
    plusBtn.type = "size_up";
    toolbarButtons.push_back(plusBtn);
}

void PiPaint::updateCanvasTexture() {
    SDL_UpdateTexture(canvasTexture, nullptr, canvas.getSurface()->pixels, canvas.getSurface()->pitch);
}

void PiPaint::drawToolbar() {
    SDL_Rect toolbarBg = {0, 0, width, toolbarHeight};
    SDL_SetRenderDrawColor(renderer, 240, 240, 245, 255);
    SDL_RenderFillRect(renderer, &toolbarBg);
    SDL_SetRenderDrawColor(renderer, 180, 180, 185, 255);
    SDL_RenderDrawLine(renderer, 0, toolbarHeight, width, toolbarHeight);

    for (auto& btn : toolbarButtons) {
        if (btn.type == "color") {
            Uint32 col = 0;
            switch (btn.index) {
                case 0: col = COLOR_BLACK; break;
                case 1: col = COLOR_RED; break;
                case 2: col = COLOR_GREEN; break;
                case 3: col = COLOR_BLUE; break;
                case 4: col = COLOR_YELLOW; break;
                case 5: col = COLOR_PURPLE; break;
                case 6: col = COLOR_ORANGE; break;
                case 7: col = COLOR_PINK; break;
                case 8: col = COLOR_CYAN; break;
                case 9: col = COLOR_WHITE; break;
            }
            SDL_SetRenderDrawColor(renderer, (col>>16)&0xFF, (col>>8)&0xFF, col&0xFF, 255);
            SDL_RenderFillRect(renderer, &btn.rect);
            SDL_SetRenderDrawColor(renderer, 44,44,46,255);
            SDL_RenderDrawRect(renderer, &btn.rect);
            if (col == canvas.getCurrentColor() && !canvas.isEraserMode()) {
                SDL_Rect highlight = {btn.rect.x-3, btn.rect.y-3, btn.rect.w+6, btn.rect.h+6};
                SDL_SetRenderDrawColor(renderer, 0,122,255,255);
                SDL_RenderDrawRect(renderer, &highlight);
            }
        } else if (btn.type == "size_label") {
            char text[10];
            snprintf(text, sizeof(text), "%dpx", penSize);
            SDL_Surface* surf = TTF_RenderUTF8_Blended(fontSmall, text, {44,44,46,0});
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_Rect dst = {btn.rect.x + (btn.rect.w - surf->w)/2,
                            btn.rect.y + (btn.rect.h - surf->h)/2,
                            surf->w, surf->h};
            SDL_RenderCopy(renderer, tex, nullptr, &dst);
            SDL_FreeSurface(surf);
            SDL_DestroyTexture(tex);
        } else if (btn.type == "size_up") {
            SDL_SetRenderDrawColor(renderer, 200,200,200,255);
            SDL_RenderFillRect(renderer, &btn.rect);
            SDL_SetRenderDrawColor(renderer, 44,44,46,255);
            SDL_RenderDrawRect(renderer, &btn.rect);
            SDL_RenderDrawLine(renderer, btn.rect.x+btn.rect.w/2, btn.rect.y+10,
                               btn.rect.x+btn.rect.w/2, btn.rect.y+btn.rect.h-10);
            SDL_RenderDrawLine(renderer, btn.rect.x+10, btn.rect.y+btn.rect.h/2,
                               btn.rect.x+btn.rect.w-10, btn.rect.y+btn.rect.h/2);
        } else if (btn.type == "size_down") {
            SDL_SetRenderDrawColor(renderer, 200,200,200,255);
            SDL_RenderFillRect(renderer, &btn.rect);
            SDL_SetRenderDrawColor(renderer, 44,44,46,255);
            SDL_RenderDrawRect(renderer, &btn.rect);
            SDL_RenderDrawLine(renderer, btn.rect.x+10, btn.rect.y+btn.rect.h/2,
                               btn.rect.x+btn.rect.w-10, btn.rect.y+btn.rect.h/2);
        } else {
            const char* label = "";
            if (btn.type == "eraser") label = "Eraser";
            else if (btn.type == "fill") label = "Fill";
            else if (btn.type == "bg") label = "Bg";
            else if (btn.type == "undo") label = "Undo";
            else if (btn.type == "redo") label = "Redo";
            else if (btn.type == "clear") label = "Clear";
            else if (btn.type == "save") label = "Save";
            else if (btn.type == "load") label = "Load";
            else if (btn.type == "exit") label = "Exit";

            Uint32 bg = COLOR_LIGHT_GRAY;
            if (btn.type == "eraser" && canvas.isEraserMode()) bg = SDL_MapRGB(SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888), 200,220,255);
            if (btn.type == "fill" && fillArmed) bg = SDL_MapRGB(SDL_AllocFormat(SDL_PIXELFORMAT_ARGB8888), 200,255,200);
            SDL_SetRenderDrawColor(renderer, (bg>>16)&0xFF, (bg>>8)&0xFF, bg&0xFF, 255);
            SDL_RenderFillRect(renderer, &btn.rect);
            SDL_SetRenderDrawColor(renderer, 44,44,46,255);
            SDL_RenderDrawRect(renderer, &btn.rect);

            SDL_Surface* surf = TTF_RenderUTF8_Blended(fontSmall, label, {44,44,46,0});
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_Rect dst = {btn.rect.x + (btn.rect.w - surf->w)/2,
                            btn.rect.y + (btn.rect.h - surf->h)/2,
                            surf->w, surf->h};
            SDL_RenderCopy(renderer, tex, nullptr, &dst);
            SDL_FreeSurface(surf);
            SDL_DestroyTexture(tex);
        }
    }
}

void PiPaint::drawOverlay() {
    // Placeholder – implement later
}

void PiPaint::handleTouchDown(int fingerId, int x, int y) {
    std::cout << "handleTouchDown: finger=" << fingerId << " (" << x << "," << y << ")\n";
    if (activeFinger != -1 && fingerId != activeFinger) {
        std::cout << "  Ignoring, activeFinger=" << activeFinger << "\n";
        return;
    }

    for (auto& btn : toolbarButtons) {
        if (x >= btn.rect.x && x <= btn.rect.x+btn.rect.w && y >= btn.rect.y && y <= btn.rect.y+btn.rect.h) {
            if (btn.type == "color") {
                executeToolAction("color", btn.index);
            } else if (btn.type == "eraser" || btn.type == "fill" || btn.type == "bg") {
                Uint32 now = SDL_GetTicks();
                if (lastClickTime.count(btn.type) && (now - lastClickTime[btn.type]) < doubleClickThreshold) {
                    executeToolAction(btn.type);
                    lastClickTime.erase(btn.type);
                } else {
                    lastClickTime[btn.type] = now;
                }
            } else if (btn.type == "size_up" || btn.type == "size_down") {
                executeToolAction(btn.type);
            } else {
                executeToolAction(btn.type);
            }
            return;
        }
    }

    if (y > toolbarHeight) {
        if (fillArmed) {
            canvas.floodFill(x, y);
            fillArmed = false;
        } else {
            activeFinger = fingerId;
            activeFingerPos = {x, y};
            canvas.startStroke(x, y, fingerId);
        }
    }
}

void PiPaint::handleTouchMove(int fingerId, int x, int y) {
    if (fingerId == activeFinger && y > toolbarHeight) {
        canvas.continueStroke(x, y, fingerId);
        activeFingerPos = {x, y};
    }
}

void PiPaint::handleTouchUp(int fingerId) {
    std::cout << "handleTouchUp: finger=" << fingerId << "\n";
    if (fingerId == activeFinger) {
        canvas.endStroke(fingerId);
        activeFinger = -1;
    }
}

void PiPaint::handleMouseButtonDown(SDL_MouseButtonEvent& ev) {
    handleTouchDown(0, ev.x, ev.y);
}

void PiPaint::handleMouseMotion(SDL_MouseMotionEvent& ev) {
    if (activeFinger == 0 && ev.y > toolbarHeight) {
        canvas.continueStroke(ev.x, ev.y, 0);
        activeFingerPos = {ev.x, ev.y};
    }
}

void PiPaint::handleMouseButtonUp(SDL_MouseButtonEvent& ev) {
    if (activeFinger == 0) {
        canvas.endStroke(0);
        activeFinger = -1;
    }
}

void PiPaint::handleKeyboard(SDL_KeyboardEvent& ev) {
    if (ev.type == SDL_KEYDOWN) {
        if (ev.keysym.sym >= SDLK_1 && ev.keysym.sym <= SDLK_9) {
            int idx = ev.keysym.sym - SDLK_1;
            executeToolAction("color", idx);
        } else if (ev.keysym.sym == SDLK_0) {
            executeToolAction("color", 9);
        } else if (ev.keysym.sym == SDLK_e && (ev.keysym.mod & KMOD_CTRL)) {
            executeToolAction("eraser");
        } else if (ev.keysym.sym == SDLK_f && (ev.keysym.mod & KMOD_CTRL)) {
            executeToolAction("fill");
        } else if (ev.keysym.sym == SDLK_b && (ev.keysym.mod & KMOD_CTRL)) {
            executeToolAction("bg");
        } else if (ev.keysym.sym == SDLK_l && (ev.keysym.mod & KMOD_CTRL)) {
            executeToolAction("clear");
        } else if (ev.keysym.sym == SDLK_z && (ev.keysym.mod & KMOD_CTRL)) {
            canvas.undo();
        } else if (ev.keysym.sym == SDLK_y && (ev.keysym.mod & KMOD_CTRL)) {
            canvas.redo();
        } else if (ev.keysym.sym == SDLK_s && (ev.keysym.mod & KMOD_CTRL)) {
            showSaveOverlay();
        } else if (ev.keysym.sym == SDLK_o && (ev.keysym.mod & KMOD_CTRL)) {
            showLoadOverlay();
        } else if (ev.keysym.sym == SDLK_UP) {
            executeToolAction("size_up");
        } else if (ev.keysym.sym == SDLK_DOWN) {
            executeToolAction("size_down");
        } else if (ev.keysym.sym == SDLK_ESCAPE) {
            if (showOverlay) showOverlay = false;
            else exit(0);
        }
    }
}

void PiPaint::executeToolAction(const std::string& type, int index) {
    if (type == "color") {
        switch (index) {
            case 0: canvas.setColor(0,0,0); break;
            case 1: canvas.setColor(255,59,48); break;
            case 2: canvas.setColor(40,205,65); break;
            case 3: canvas.setColor(0,122,255); break;
            case 4: canvas.setColor(255,204,0); break;
            case 5: canvas.setColor(175,82,222); break;
            case 6: canvas.setColor(255,149,0); break;
            case 7: canvas.setColor(255,45,85); break;
            case 8: canvas.setColor(90,200,250); break;
            case 9: canvas.setColor(255,255,255); break;
        }
    } else if (type == "eraser") {
        canvas.toggleEraser();
    } else if (type == "fill") {
        canvas.toggleFill();
        fillArmed = canvas.isFillMode();
    } else if (type == "bg") {
        canvas.toggleBackground();
    } else if (type == "undo") {
        canvas.undo();
    } else if (type == "redo") {
        canvas.redo();
    } else if (type == "clear") {
        canvas.clear();
    } else if (type == "save") {
        showSaveOverlay();
    } else if (type == "load") {
        showLoadOverlay();
    } else if (type == "size_up") {
        penSize = std::min(50, penSize + 1);
        canvas.setSize(penSize);
    } else if (type == "size_down") {
        penSize = std::max(1, penSize - 1);
        canvas.setSize(penSize);
    } else if (type == "exit") {
        exit(0);
    }
}

void PiPaint::showSaveOverlay() {
    std::cout << "Save overlay (placeholder)\n";
}

void PiPaint::showLoadOverlay() {
    std::cout << "Load overlay (placeholder)\n";
}

void PiPaint::refreshFileList() {}
std::string PiPaint::generateRandomFilename() { return "drawing_" + std::to_string(time(nullptr)) + ".png"; }
void PiPaint::saveCurrentDrawing() {}
void PiPaint::loadSelectedDrawing() {}
void PiPaint::enterFolderBrowser() {}
void PiPaint::refreshSubdirs() {}
void PiPaint::goUp() {}
void PiPaint::goHome() {}
void PiPaint::goMedia() {}
void PiPaint::selectCurrentFolder() {}
void PiPaint::calibrate() {}

void PiPaint::run() {
    SDL_ShowCursor(SDL_ENABLE);
    SDL_Event e;
    Uint32 lastTick = SDL_GetTicks();
    while (true) {
        std::vector<SDL_Event> touchEvents;
        touch.processEvents(touchEvents);
        for (auto& ev : touchEvents) {
            if (ev.type == SDL_FINGERDOWN) {
                int x = ev.tfinger.x * width;
                int y = ev.tfinger.y * height;
                handleTouchDown(ev.tfinger.fingerId, x, y);
            } else if (ev.type == SDL_FINGERMOTION) {
                int x = ev.tfinger.x * width;
                int y = ev.tfinger.y * height;
                handleTouchMove(ev.tfinger.fingerId, x, y);
            } else if (ev.type == SDL_FINGERUP) {
                handleTouchUp(ev.tfinger.fingerId);
            }
        }

        while (SDL_PollEvent(&e)) {
            if (e.type == SDL_QUIT) return;
            else if (e.type == SDL_MOUSEBUTTONDOWN) handleMouseButtonDown(e.button);
            else if (e.type == SDL_MOUSEMOTION) handleMouseMotion(e.motion);
            else if (e.type == SDL_MOUSEBUTTONUP) handleMouseButtonUp(e.button);
            else if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP) handleKeyboard(e.key);
        }

        updateCanvasTexture();

        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, canvasTexture, nullptr, nullptr);
        drawToolbar();
        if (showOverlay) drawOverlay();

        if (activeFinger != -1 && activeFingerPos.y > toolbarHeight) {
            int r = penSize + 4;
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
            SDL_SetRenderDrawColor(renderer, 200,200,255,100);
            for (int dy = -r; dy <= r; dy++) {
                for (int dx = -r; dx <= r; dx++) {
                    if (dx*dx + dy*dy <= r*r) {
                        SDL_RenderDrawPoint(renderer, activeFingerPos.x+dx, activeFingerPos.y+dy);
                    }
                }
            }
            SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);
        }

        SDL_RenderPresent(renderer);

        Uint32 now = SDL_GetTicks();
        if (now - lastTick < 33) SDL_Delay(33 - (now - lastTick));
        lastTick = now;
    }
}

int main(int argc, char* argv[]) {
    PiPaint app;
    app.run();
    return 0;
}
