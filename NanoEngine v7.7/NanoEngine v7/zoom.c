#include "raylib.h"
#include <math.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#define MAX_HISTORY 10000
const char* GetNewestImagePath(const char* directoryPath, char* outPath) {
    FilePathList files = LoadDirectoryFiles(directoryPath);
    time_t newestTime = 0;
    int newestIndex = -1;
    for (unsigned int i = 0; i < files.count; i++) {
        if (IsFileExtension(files.paths[i], ".png")  || 
            IsFileExtension(files.paths[i], ".jpg")  || 
            IsFileExtension(files.paths[i], ".jpeg") || 
            IsFileExtension(files.paths[i], ".bmp")) {
            struct stat attrib;
            if (stat(files.paths[i], &attrib) == 0) {
                if (attrib.st_mtime > newestTime) {
                    newestTime = attrib.st_mtime;
                    newestIndex = i;
                }
            }
        }
    }
    if (newestIndex != -1) {
        strcpy(outPath, files.paths[newestIndex]);
        UnloadDirectoryFiles(files);
        return outPath;
    }
    UnloadDirectoryFiles(files);
    return NULL;
}
typedef struct {
    Vector2 start;
    Vector2 control;
    Vector2 end;
    Color color;
    float thickness;
} TraceLine;
TraceLine lines[500];
int lineCount = 0; 
RenderTexture2D undoStack[MAX_HISTORY];
int undoCount = 0;
RenderTexture2D redoStack[MAX_HISTORY];
int redoCount = 0;
void ClearRedoStack() {
    for (int i = 0; i < redoCount; i++) {
        UnloadRenderTexture(redoStack[i]);
    }
    redoCount = 0;
}
void SaveUndoState(RenderTexture2D canvas) {
    if (undoCount >= MAX_HISTORY) {
        UnloadRenderTexture(undoStack[0]); 
        for (int i = 1; i < MAX_HISTORY; i++) {
            undoStack[i - 1] = undoStack[i];
        }
        undoCount = MAX_HISTORY - 1;
    }
    undoStack[undoCount] = LoadRenderTexture(canvas.texture.width, canvas.texture.height);
    BeginTextureMode(undoStack[undoCount]);
    ClearBackground(BLACK);
    DrawTextureRec(canvas.texture, (Rectangle){ 0, 0, (float)canvas.texture.width, (float)-canvas.texture.height }, (Vector2){ 0, 0 }, WHITE);
    EndTextureMode();
    undoCount++;
    ClearRedoStack();
}
void PerformUndo(RenderTexture2D *canvas) {
    if (undoCount <= 0) return;
    if (redoCount >= MAX_HISTORY) {
        UnloadRenderTexture(redoStack[0]); 
        for (int i = 1; i < MAX_HISTORY; i++) {
            redoStack[i - 1] = redoStack[i];
        }
        redoCount = MAX_HISTORY - 1;
    }
    redoStack[redoCount] = LoadRenderTexture(canvas->texture.width, canvas->texture.height);
    BeginTextureMode(redoStack[redoCount]);
    ClearBackground(BLACK);
    DrawTextureRec(canvas->texture, (Rectangle){ 0, 0, (float)canvas->texture.width, (float)-canvas->texture.height }, (Vector2){ 0, 0 }, WHITE);
    EndTextureMode();
    redoCount++;
    undoCount--;
    BeginTextureMode(*canvas);
    ClearBackground(BLACK);
    DrawTextureRec(undoStack[undoCount].texture, (Rectangle){ 0, 0, (float)undoStack[undoCount].texture.width, (float)-undoStack[undoCount].texture.height }, (Vector2){ 0, 0 }, WHITE);
    EndTextureMode();
    UnloadRenderTexture(undoStack[undoCount]);
}
void PerformRedo(RenderTexture2D *canvas) {
    if (redoCount <= 0) return;
    if (undoCount >= MAX_HISTORY) {
        UnloadRenderTexture(undoStack[0]); 
        for (int i = 1; i < MAX_HISTORY; i++) {
            undoStack[i - 1] = undoStack[i];
        }
        undoCount = MAX_HISTORY - 1;
    }
    undoStack[undoCount] = LoadRenderTexture(canvas->texture.width, canvas->texture.height);
    BeginTextureMode(undoStack[undoCount]);
    ClearBackground(BLACK);
    DrawTextureRec(canvas->texture, (Rectangle){ 0, 0, (float)canvas->texture.width, (float)-canvas->texture.height }, (Vector2){ 0, 0 }, WHITE);
    EndTextureMode();
    undoCount++;
    redoCount--;
    BeginTextureMode(*canvas);
    ClearBackground(BLACK);
    DrawTextureRec(redoStack[redoCount].texture, (Rectangle){ 0, 0, (float)redoStack[redoCount].texture.width, (float)-redoStack[redoCount].texture.height }, (Vector2){ 0, 0 }, WHITE);
    EndTextureMode();
    UnloadRenderTexture(redoStack[redoCount]);
}
void DrawCustomBezierQuad(Vector2 p0, Vector2 p2, Vector2 p1, float thickness, Color color) {
    int segments = 24; 
    Vector2 prevPoint = p0;
    for (int i = 1; i <= segments; i++) {
        float t = (float)i / (float)segments;
        float a = (1.0f - t) * (1.0f - t);
        float b = 2.0f * (1.0f - t) * t;
        float c = t * t;
        Vector2 nextPoint = {
            a * p0.x + b * p1.x + c * p2.x,
            a * p0.y + b * p1.y + c * p2.y
        };
        DrawLineEx(prevPoint, nextPoint, thickness, color);
        prevPoint = nextPoint;
    }
}

void FloodFillCanvas(RenderTexture2D *canvas, int startX, int startY, Color fillCol) {
    Image canvasImg = LoadImageFromTexture(canvas->texture);
    ImageFlipVertical(&canvasImg);
    Color targetCol = GetImageColor(canvasImg, startX, startY);
    if (ColorToInt(targetCol) == ColorToInt(fillCol)) {
        UnloadImage(canvasImg);
        return;
    }
    int width = canvasImg.width;
    int height = canvasImg.height;
    int maxCells = width * height;
    Vector2 *stack = (Vector2 *)malloc(maxCells * sizeof(Vector2));
    if (!stack) { UnloadImage(canvasImg); return; }
    int stackPtr = 0;
    stack[stackPtr++] = (Vector2){ (float)startX, (float)startY };
    while (stackPtr > 0) {
        Vector2 p = stack[--stackPtr];
        int px = (int)p.x, py = (int)p.y;
        if (px >= 0 && px < width && py >= 0 && py < height) {
            Color currCol = GetImageColor(canvasImg, px, py);
            if (ColorToInt(currCol) == ColorToInt(targetCol)) {
                ImageDrawPixel(&canvasImg, px, py, fillCol);
                if (stackPtr < (maxCells - 4)) {
                    stack[stackPtr++] = (Vector2){ (float)px + 1, (float)py };
                    stack[stackPtr++] = (Vector2){ (float)px - 1, (float)py };
                    stack[stackPtr++] = (Vector2){ (float)px, (float)py + 1 };
                    stack[stackPtr++] = (Vector2){ (float)px, (float)py - 1 };
                }
            }
        }
    }
    Texture2D updatedTex = LoadTextureFromImage(canvasImg);
    BeginTextureMode(*canvas);
    DrawTexture(updatedTex, 0, 0, WHITE);
    EndTextureMode();
    UnloadTexture(updatedTex);
    UnloadImage(canvasImg);
    free(stack); 
}
int main(int argc, char const *argv[]) {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    bool htmi = false; 
    bool sizes = false;
    bool circle = true; 
    const int w = 1200; 
    const int h = 650;
    bool traceActive = false; 
    bool fillModeActive = false; 
    bool mode = false; 
    bool pens = false; 
    bool downloads = false;
    bool more = false; 
    bool mode3D = false; 
    Color color2 = BLANK; 
    float hue = 0.0f; 
    bool rainbowMode = false;
    bool mirrorMode = false;
    InitWindow(w, h, "nanozoom");
    bool selectModeActive = false;
    bool showSelectionMenu = false;
    Vector2 selectStart = {0};
    Vector2 selectEnd = {0};
    Vector2 menuPos = {0};
    SetTargetFPS(60); 
    Color boxcolor = DARKGRAY; 
    Color box2color = BLUE;
    Color tc = BLACK;
    int count = 0; 
    const char *pen = "o"; 
    int size = 50; 
    Color color = RED; 
    int m = GetCurrentMonitor(); 
    int monitorWidth = GetMonitorWidth(m);
    int monitorHeight = GetMonitorHeight(m);
    SetWindowPosition((monitorWidth - w) / 2, (monitorHeight - h) / 2);
    int pen_size = 20;
    Vector2 lastMousePos = { 0.0f, 0.0f };
    bool isFirstStrokeFrame = true;
    RenderTexture2D canvas = LoadRenderTexture(1150, 616);
    SetTextureFilter(canvas.texture, TEXTURE_FILTER_BILINEAR);
    int canvasCenterX = canvas.texture.width / 2; 
    BeginTextureMode(canvas);
    ClearBackground(BLACK);
    EndTextureMode();
    int clickState = 0;
    Vector2 traceStart = { 0, 0 };
    Vector2 traceControl = { 0, 0 };
    Camera2D camera = { 0 };
    camera.target = (Vector2){ canvas.texture.width / 2.0f, canvas.texture.height / 2.0f };
    camera.offset = (Vector2){ (w - 50) / 2.0f + 50, (h - 34) / 2.0f + 34 };
    camera.rotation = 0.0f;
    camera.zoom = 1.0f;
    const char* dirPath = "./zoom"; 
    char currentPath[512] = { 0 };
    char checkPath[512] = { 0 };
    char currentImagePath[512] = { 0 };
    char checkImagePath[512] = { 0 };
    Texture2D loadedImageTexture = { 0 };
    bool hasLoadedImage = false;
    Texture2D texture = { 0 };
    if (GetNewestImagePath(dirPath, currentImagePath) != NULL) {
        loadedImageTexture = LoadTexture(currentImagePath);
        if (loadedImageTexture.id > 0) {
            hasLoadedImage = true;
            BeginTextureMode(canvas);
                DrawTexture(loadedImageTexture, 0, 0, WHITE);
            EndTextureMode();
        }
    }
    while (!WindowShouldClose()) {
        if (texture.id > 0) {
                DrawTexture(texture, 10, 10, WHITE);
        }
        bool UI_Menu_Open = (downloads || more || pens || mode || sizes);
        int checkx = GetMouseX();
        int checky = GetMouseY(); 
        int x = GetMouseX();
        int y = GetMouseY(); 
        Vector2 mPos = GetMousePosition();
        Vector2 worldMouse = GetScreenToWorld2D(mPos, camera);
        if (IsKeyPressed(KEY_ESCAPE)) {
            if (more || pens || mode || sizes || downloads) { 
                more = false; pens = false; mode = false; sizes = false; downloads = false;
            } else if (mode3D) {
                mode3D = false; 
            } else {
                break; 
            }
        }
        if (IsKeyDown(KEY_LEFT_CONTROL) || IsKeyDown(KEY_RIGHT_CONTROL)) {
            if (IsKeyPressed(KEY_Z)) { if (!mode3D) PerformUndo(&canvas); }
            if (IsKeyPressed(KEY_Y)) { if (!mode3D) PerformRedo(&canvas); }
        }
        if (IsKeyPressed(KEY_THREE)) mode3D = !mode3D;
        if (IsKeyPressed(KEY_ONE)) { color = DARKGRAY; }
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), (Rectangle){0,600,50,50})) { color = DARKGRAY; }
        if (!mode3D) {
            float wheel = GetMouseWheelMove();
            if (wheel != 0) {
                pen_size += (int)wheel;
                if (pen_size < 1) pen_size = 1; 
            }
            if (IsKeyDown(KEY_ENTER)) {
                SaveUndoState(canvas);
                BeginTextureMode(canvas);
                ClearBackground(BLACK); 
                EndTextureMode();
                lineCount = 0;
            }
            if (IsKeyDown(KEY_B)) color = BLACK; 
            if (IsKeyDown(KEY_O)) color = ORANGE; 
            if (IsKeyDown(KEY_R)) color = RED;
            if (IsKeyDown(KEY_G)) color = GREEN; 
            if (IsKeyDown(KEY_Y)) color = YELLOW; 
            if (IsKeyDown(KEY_Q)) color = PURPLE; 
            if (IsKeyDown(KEY_P)) color = PINK;
            if (IsKeyDown(KEY_W)) color = RAYWHITE; 
            if (IsKeyDown(KEY_H)) color = DARKBROWN; 
            if (IsKeyDown(KEY_F)) color = GRAY; 
            if (IsKeyDown(KEY_L)) color = BLUE; 
            if (IsKeyDown(KEY_C)) color = SKYBLUE; 
            if (IsKeyDown(KEY_D)) { pen = "."; circle = false; }
            if (IsKeyDown(KEY_J)) { pen = "#"; circle = false; }
            if (IsKeyDown(KEY_M)) { pen = "+"; circle = false; }
            if (IsKeyDown(KEY_Z)) { pen = "="; circle = false; }
            if (IsKeyDown(KEY_E)) { pen = "-"; circle = false; }
            if (IsKeyDown(KEY_X)) { pen = "*"; circle = false; } 
            if (IsKeyDown(KEY_V)) { pen = "x"; circle = false; } 
            if (IsKeyDown(KEY_S)) { pen = "%"; circle = false; }
            if (IsKeyDown(KEY_O)) { pen = "o"; circle = false; } 
            if (IsKeyPressed(KEY_I)) { traceActive = false; fillModeActive = false; clickState = 0; }
            if (IsKeyPressed(KEY_T)) { traceActive = true;  fillModeActive = false; clickState = 0; } 
            if (IsKeyPressed(KEY_A)) { fillModeActive = true; traceActive = false; clickState = 0; }
            if (IsKeyPressed(KEY_K)) rainbowMode = !rainbowMode;   
            if (rainbowMode) {
                hue += 1.0f;
                if (hue > 360.0f) hue = 0.0f;
                color = ColorFromHSV(hue, 1.0f, 1.0f);
            }
            if (IsFileDropped()) {
                FilePathList droppedFiles = LoadDroppedFiles();
                if (droppedFiles.count > 0) {
                    SaveUndoState(canvas);
                    Texture2D pick = LoadTexture(droppedFiles.paths[0]);
                    BeginTextureMode(canvas);
                    DrawTexture(pick, 0, 0, WHITE); 
                    EndTextureMode();
                    UnloadTexture(pick);
                }
                UnloadDroppedFiles(droppedFiles);
            }
        }
        bool isInsideCanvas = (x > 50 && y > 34 && x < 1200 && y < 650);
        bool clickInterceptedByMenu = false;
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            if (downloads) {
                clickInterceptedByMenu = true;
                bool triggerSave = false; 
                const char* ext = "png";
                if (CheckCollisionPointRec(mPos, (Rectangle){910,40,200,40})) downloads = false;
                if (CheckCollisionPointRec(mPos, (Rectangle){910,90,200,40}))  { triggerSave = true; ext = "png"; }
                if (CheckCollisionPointRec(mPos, (Rectangle){910,140,200,40})) { triggerSave = true; ext = "jpeg"; }
                if (CheckCollisionPointRec(mPos, (Rectangle){910,190,200,40})) { triggerSave = true; ext = "jpg"; }
                if (CheckCollisionPointRec(mPos, (Rectangle){910,240,200,40})) { triggerSave = true; ext = "bmp"; }
                if (triggerSave) {
                    system("start saved_message.exe\"");
                    count++;
                    Image screenshot = LoadImageFromTexture(canvas.texture);
                    ImageFlipVertical(&screenshot); 
                    if (color2.a != 0) ImageColorReplace(&screenshot, color2, BLANK);
                    ExportImage(screenshot, TextFormat("your_images\\image(%d).%s", count, ext));
                    UnloadImage(screenshot);
                }
            } else if (more) {
                clickInterceptedByMenu = true;
                if (CheckCollisionPointRec(mPos, (Rectangle){910,60,150,40})) more = false;
                if (CheckCollisionPointRec(mPos, (Rectangle){910,110,480,40})) color2 = RAYWHITE;
                if (CheckCollisionPointRec(mPos, (Rectangle){910,160,480,40})) color2 = GRAY;
                if (CheckCollisionPointRec(mPos, (Rectangle){910,210,480,40})) color2 = BLACK;
                if (CheckCollisionPointRec(mPos, (Rectangle){910,260,480,40})) color2 = DARKBROWN;
                if (CheckCollisionPointRec(mPos, (Rectangle){910,310,480,40})) color2 = color;
                if (CheckCollisionPointRec(mPos, (Rectangle){910,360,70,50})) system("start picks\"");
            } else if (pens) {
                clickInterceptedByMenu = true;
                for(int k = 0; k < 8; k++) {
                    if (CheckCollisionPointRec(mPos, (Rectangle){950, (float)(20 + (k * 50)), 200, 40})) {
                        if (k == 0) pens = false;             
                        if (k == 1) { circle = true; }        
                        if (k == 2) { pen = "."; circle = false; }
                        if (k == 3) { pen = "#"; circle = false; }
                        if (k == 4) { pen = "+"; circle = false; }
                        if (k == 5) { pen = "x"; circle = false; }
                        if (k == 6) { pen = "*"; circle = false; }
                        if (k == 7) { pen = "o"; circle = false; }
                    }
                }
            } else if (mode) {
                clickInterceptedByMenu = true;
                if (CheckCollisionPointRec(mPos, (Rectangle){910,40,200,40})) { mode = false; }
                if (CheckCollisionPointRec(mPos, (Rectangle){910,90,200,40})) rainbowMode = true;
                if (CheckCollisionPointRec(mPos, (Rectangle){910,140,200,40})) rainbowMode = false;
                if (CheckCollisionPointRec(mPos, (Rectangle){910,190,200,40})) mirrorMode = true;
                if (CheckCollisionPointRec(mPos, (Rectangle){910,240,200,40})) mirrorMode = false;
                if (CheckCollisionPointRec(mPos, (Rectangle){910,290,200,40})) { traceActive = !traceActive; fillModeActive = false; clickState = 0; mode = false; }
                if (CheckCollisionPointRec(mPos, (Rectangle){910,340,200,40})) { fillModeActive = !fillModeActive; traceActive = false; clickState = 0; mode = false; }
                if (CheckCollisionPointRec(mPos, (Rectangle){910,390,200,40})) { mode3D = !mode3D; mode = false; }
            } else if (sizes) {
                clickInterceptedByMenu = true;
                if (CheckCollisionPointRec(mPos, (Rectangle){915,55,50,50})) pen_size += 1;
                if (CheckCollisionPointRec(mPos, (Rectangle){915,110,50,50})) pen_size -= 1;
                if (CheckCollisionPointRec(mPos, (Rectangle){915,165,50,50})) sizes = false;
            }   
            if (!clickInterceptedByMenu) {
                if (CheckCollisionPointRec(mPos, (Rectangle){0,0,(float)size,(float)size})) { color = BLACK; clickInterceptedByMenu = true; }
                if (CheckCollisionPointRec(mPos, (Rectangle){0,50,(float)size,(float)size})) { color = DARKBROWN; clickInterceptedByMenu = true; }
                if (CheckCollisionPointRec(mPos, (Rectangle){0,100,(float)size,(float)size})) { color = GRAY; clickInterceptedByMenu = true; }
                if (CheckCollisionPointRec(mPos, (Rectangle){0,150,(float)size,(float)size})) { color = RAYWHITE; clickInterceptedByMenu = true; }
                if (CheckCollisionPointRec(mPos, (Rectangle){0,200,(float)size,(float)size})) { color = SKYBLUE; clickInterceptedByMenu = true; }
                if (CheckCollisionPointRec(mPos, (Rectangle){0,250,(float)size,(float)size})) { color = BLUE; clickInterceptedByMenu = true; }
                if (CheckCollisionPointRec(mPos, (Rectangle){0,300,(float)size,(float)size})) { color = YELLOW; clickInterceptedByMenu = true; }
                if (CheckCollisionPointRec(mPos, (Rectangle){0,350,(float)size,(float)size})) { color = GREEN; clickInterceptedByMenu = true; }
                if (CheckCollisionPointRec(mPos, (Rectangle){0,400,(float)size,(float)size})) { color = PINK; clickInterceptedByMenu = true; }
                if (CheckCollisionPointRec(mPos, (Rectangle){0,450,(float)size,(float)size})) { color = PURPLE; clickInterceptedByMenu = true; }
                if (CheckCollisionPointRec(mPos, (Rectangle){0,500,(float)size,(float)size})) { color = RED; clickInterceptedByMenu = true; }
                if (CheckCollisionPointRec(mPos, (Rectangle){0,550,(float)size,(float)size})) { color = ORANGE; clickInterceptedByMenu = true; }
                if (CheckCollisionPointRec(mPos, (Rectangle){50, 0, 50, 30})) { system("start \"\" \"%USERPROFILE%\\Desktop\""); clickInterceptedByMenu = true; }
                if (CheckCollisionPointRec(mPos, (Rectangle){105,0,50,30})) { SaveUndoState(canvas); BeginTextureMode(canvas); ClearBackground(BLACK); EndTextureMode(); lineCount = 0; clickInterceptedByMenu = true; }
                if (CheckCollisionPointRec(mPos, (Rectangle){165,0,50,30})) { downloads = true; more = false; pens = false; mode = false; sizes = false; clickInterceptedByMenu = true; }
                if (CheckCollisionPointRec(mPos, (Rectangle){225,0,70,30})) { more = true; downloads = false; pens = false; mode = false; sizes = false; clickInterceptedByMenu = true; }
                if (CheckCollisionPointRec(mPos, (Rectangle){305,0,50,30})) { pens = true; downloads = false; more = false; mode = false; sizes = false; clickInterceptedByMenu = true; }
                if (CheckCollisionPointRec(mPos, (Rectangle){365,0,50,30})) { mode = true; downloads = false; more = false; pens = false; sizes = false; clickInterceptedByMenu = true; }
                if (CheckCollisionPointRec(mPos, (Rectangle){425,0,50,30})) { sizes = true; downloads = false; more = false; pens = false; mode = false; clickInterceptedByMenu = true; }
                if (CheckCollisionPointRec(mPos, (Rectangle){485,0,50,30})) { if (!mode3D) PerformUndo(&canvas); clickInterceptedByMenu = true; }
                if (CheckCollisionPointRec(mPos, (Rectangle){540,0,50,30})) { if (!mode3D) PerformRedo(&canvas); clickInterceptedByMenu = true; }
                if (CheckCollisionPointRec(mPos, (Rectangle){600,0,75,30})) { downloads = false; more = false; pens = false; mode = false; sizes = false; clickInterceptedByMenu = true; htmi = true; }
                if (CheckCollisionPointRec(mPos, (Rectangle){685,0,95,30})) { system("start your_images"); }
            }
        }
        if (!clickInterceptedByMenu && !UI_Menu_Open && !htmi) {
            if (fillModeActive) {
                if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && isInsideCanvas) {
                    if (!selectModeActive) {
                        selectModeActive = true;
                        selectStart = mPos;
                        showSelectionMenu = false;
                    } else {
                        showSelectionMenu = true;
                        selectEnd = mPos;
                        menuPos = mPos;
                        selectModeActive = false;
                    }
                }
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && isInsideCanvas) {
                    SaveUndoState(canvas);
                    FloodFillCanvas(&canvas, (int)worldMouse.x - 50, (int)worldMouse.y - 34, color);
                }
            } else if (traceActive) {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && isInsideCanvas) {
                    if (clickState == 0) { traceStart = mPos; clickState = 1; } 
                    else if (clickState == 1) {
                        if (lineCount < 500) {
                            SaveUndoState(canvas);
                            lines[lineCount] = (TraceLine){ .start = traceStart, .control = traceStart, .end = mPos, .color = color, .thickness = (float)pen_size };
                            Vector2 localStart = { traceStart.x - 50, traceStart.y - 34 };
                            Vector2 localEnd = { mPos.x - 50, mPos.y - 34 };
                            BeginTextureMode(canvas);
                            DrawLineEx(localStart, localEnd, (float)pen_size, color);
                            if (mirrorMode) {
                                Vector2 mStart = { (float)canvasCenterX - (localStart.x - (float)canvasCenterX), localStart.y };
                                Vector2 mEnd = { (float)canvasCenterX - (localEnd.x - (float)canvasCenterX), localEnd.y };
                                DrawLineEx(mStart, mEnd, (float)pen_size, color);
                            }
                            EndTextureMode();
                            lineCount++;
                        }
                        clickState = 0; 
                    } else if (clickState == 2) {
                        if (lineCount < 500) {
                            SaveUndoState(canvas);
                            lines[lineCount] = (TraceLine){ .start = traceStart, .control = traceControl, .end = mPos, .color = color, .thickness = (float)pen_size };
                            Vector2 localStart = { traceStart.x - 50, traceStart.y - 34 };
                            Vector2 localControl = { traceControl.x - 50, traceControl.y - 34 };
                            Vector2 localEnd = { mPos.x - 50, mPos.y - 34 };
                            BeginTextureMode(canvas);
                            DrawCustomBezierQuad(localStart, localEnd, localControl, (float)pen_size, color);
                            if (mirrorMode) {
                                Vector2 mStart = { (float)canvasCenterX - (localStart.x - (float)canvasCenterX), localStart.y };
                                Vector2 mControl = { (float)canvasCenterX - (localControl.x - (float)canvasCenterX), localControl.y };
                                Vector2 mEnd = { (float)canvasCenterX - (localEnd.x - (float)canvasCenterX), localEnd.y };
                                DrawCustomBezierQuad(mStart, mEnd, mControl, (float)pen_size, color);
                            }
                            EndTextureMode();
                            lineCount++;
                        }
                        clickState = 0; 
                    }
                }
                if (IsMouseButtonPressed(MOUSE_RIGHT_BUTTON) && isInsideCanvas) {
                    if (clickState == 1) { traceControl = mPos; clickState = 2; }
                }
            } else {
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && isInsideCanvas) {
                    if (isFirstStrokeFrame) { 
                        SaveUndoState(canvas); 
                        lastMousePos = worldMouse; 
                        isFirstStrokeFrame = false; 
                    }
                    Vector2 localStart = { lastMousePos.x - 50.0f, lastMousePos.y - 34.0f };
                    Vector2 localEnd = { worldMouse.x - 50.0f, worldMouse.y - 34.0f };
                    BeginTextureMode(canvas);
                        float dx = localEnd.x - localStart.x;
                        float dy = localEnd.y - localStart.y;
                        float distance = sqrtf(dx * dx + dy * dy);
                        float stepSize = (circle) ? fmaxf(0.1f, (float)pen_size * 0.05f) : fmaxf(1.0f, (float)pen_size * 0.1f);
                        int steps = (distance > 0) ? (int)(distance / stepSize) : 1;
                        for (int i = 0; i <= steps; i++) {
                            float t = (float)i / (float)steps;
                            int stepX = (int)(localStart.x + dx * t);
                            int stepY = (int)(localStart.y + dy * t);

                            if (circle) {
                                DrawCircle(stepX, stepY, (float)pen_size, color);
                            } else {
                                DrawText(pen, stepX - (pen_size / 4), stepY - (pen_size / 2), pen_size, color); 
                            }

                            if (mirrorMode) {
                                int mirrorX = canvasCenterX - (stepX - canvasCenterX);
                                if (circle) {
                                    DrawCircle(mirrorX, stepY, (float)pen_size, color);
                                } else {
                                    DrawText(pen, mirrorX - (pen_size / 4), stepY - (pen_size / 2), pen_size, color);
                                }
                            }
                        }
                    EndTextureMode();
                    lastMousePos = worldMouse;
                }
            }
        }
        if (!IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
            isFirstStrokeFrame = true;
        }
        BeginDrawing();
        ClearBackground(DARKGREEN);
        if (mode3D) {
            DrawRectangle(50, 34, 1150, 616, (Color){ 235, 225, 205, 255 });
            for (int fi = 50; fi < 1200; fi += 12) DrawLine(fi, 34, fi, 650, (Color){0,0,0,15});
            for (int fj = 34; fj < 650; fj += 12)  DrawLine(50, fj, 1200, fj, (Color){0,0,0,15});
            
            Image img = LoadImageFromTexture(canvas.texture);
            ImageFlipVertical(&img);
            int step = 4; 
            for (int yCoord = 0; yCoord < img.height; yCoord += step) {
                for (int xCoord = 0; xCoord < img.width; xCoord += step) {
                    Color pixCol = GetImageColor(img, xCoord, yCoord);
                    if (pixCol.r > 12 || pixCol.g > 12 || pixCol.b > 12) {
                        int drawX = xCoord + 50;
                        int drawY = yCoord + 34;
                        int shift = ((xCoord + yCoord) % 2 == 0) ? 3 : -3;
                        DrawLineEx((Vector2){(float)drawX - shift, (float)drawY - 2}, (Vector2){(float)drawX + shift, (float)drawY + 4}, 3.0f, (Color){0, 0, 0, 45});
                        DrawLineEx((Vector2){(float)drawX - shift, (float)drawY - 3}, (Vector2){(float)drawX + shift, (float)drawY + 3}, 3.0f, pixCol);
                        DrawLineEx((Vector2){(float)drawX - shift, (float)drawY - 3}, (Vector2){(float)drawX, (float)drawY}, 1.5f, (Color){255, 255, 255, 40});
                    }
                }
            }
            UnloadImage(img);
            DrawRectangle(50, 34, 1150, 35, ColorAlpha(BLACK, 0.7f));
            DrawText("WILCOM TRUEVIEW MODE (2D STITCH EMBROIDERY)", 70, 42, 16, GOLD);
        } else {
            BeginMode2D(camera); 
                ClearBackground(DARKGRAY);
                DrawTextureRec(canvas.texture, (Rectangle){ 0, 0, 1150, -616 }, (Vector2){ 50, 34 }, WHITE);
            EndMode2D();
            int displayX = (int)worldMouse.x - 50;
            int displayY = (int)worldMouse.y - 34;
            DrawText(TextFormat("x: %d, y: %d | Zoom: %d%%", displayX, displayY, (int)(camera.zoom * 100)), 60, 40, 20, RAYWHITE);

            if (x > 50 && y > 34 && x < 1200 && y < 650 && !UI_Menu_Open) {
                DrawRectangle(x, 34, 2, 616, DARKGRAY);
                DrawRectangle(50, y, 1150, 2, DARKGRAY);
            }
            if (!mode3D) {
                Vector2 mouseWorldBeforeZoom = GetScreenToWorld2D(GetMousePosition(), camera);
                bool zoomAltered = false;
                if (IsKeyDown(KEY_PAGE_UP) || IsKeyDown(KEY_KP_ADD)) {
                    camera.zoom += 0.03f;
                    if (camera.zoom > 10000.0f) camera.zoom = 10000.0f;
                    zoomAltered = true;
                }
                if (IsKeyDown(KEY_PAGE_DOWN) || IsKeyDown(KEY_KP_SUBTRACT)) {
                    camera.zoom -= 0.03f;
                    if (camera.zoom < 0.1f) camera.zoom = 0.1f;
                    zoomAltered = true;
                }
                if (zoomAltered) {
                    camera.offset = GetMousePosition();
                    camera.target = mouseWorldBeforeZoom;
                }
                if (IsKeyPressed(KEY_ZERO)) {
                    camera.zoom = 1.0f;
                    camera.offset = (Vector2){ (w - 50) / 2.0f + 50, (h - 34) / 2.0f + 34 };
                    camera.target = (Vector2){ canvas.texture.width / 2.0f, canvas.texture.height / 2.0f };
                }
                if (IsMouseButtonDown(MOUSE_BUTTON_MIDDLE)) {
                    Vector2 delta = GetMouseDelta();
                    camera.target.x -= delta.x / camera.zoom;
                    camera.target.y -= delta.y / camera.zoom;
                }
            }

            if (htmi) {
                if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mPos, (Rectangle){600,130,70 ,50})) {
                    htmi = false;
                } 
                DrawRectangle(590,0,300,300, GRAY); 
                DrawRectangle(600,130,70,50, DARKGRAY);
                DrawText("close", 605,135,20, BLACK); 
                DrawRectangle(600, 40, 200, 20, RED);
                int mx = GetMouseX(); if (mx >= 780) {mx = 780;} if (mx <= 600) {mx = 600;}
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), (Rectangle){600, 40, 200, 20})){ DrawRectangle(mx,35,30,30, DARKGRAY);} 
                DrawRectangle(600, 70, 200, 20, GREEN);
                int mxx = GetMouseX();  if (mxx >= 780) {mxx = 780;} if (mxx <= 600) {mxx = 600;} 
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), (Rectangle){600, 70, 200, 20})){DrawRectangle(mxx,65,30,30, DARKGRAY);}
                DrawRectangle(600, 100, 200, 20, BLUE);
                int mxxx = GetMouseX();  if (mxxx >= 780) {mxxx = 780;} if (mxxx <= 600) {mxxx = 600;} 
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), (Rectangle){600, 100, 200, 20})){DrawRectangle(mxxx,95,30,30, DARKGRAY);}
                int displayR = (int)(r * 255);
                int displayG = (int)(g * 255);
                int displayB = (int)(b * 255);
                DrawText(TextFormat("R: %d  G: %d  B: %d", displayR, displayG, displayB), 600, 200, 20, BLACK);
                if (IsMouseButtonDown(MOUSE_LEFT_BUTTON)) {
                    Vector2 mouse = GetMousePosition();
                    if (CheckCollisionPointRec(mouse, (Rectangle){600, 40, 200, 20})) { r = (mouse.x - 600) / 200.0f; r = fmaxf(0.0f, fminf(1.0f, r)); }
                    if (CheckCollisionPointRec(mouse, (Rectangle){600, 70, 200, 20})) { g = (mouse.x - 600) / 200.0f; g = fmaxf(0.0f, fminf(1.0f, g)); }
                    if (CheckCollisionPointRec(mouse, (Rectangle){600, 100, 200, 20})) { b = (mouse.x - 600) / 200.0f; b = fmaxf(0.0f, fminf(1.0f, b)); }
                    color = (Color){ (unsigned char)(r * 255), (unsigned char)(g * 255), (unsigned char)(b * 255), 255 };
                }
                const char *color_right_now = "";
                if (displayR == 0 && displayG == 0 && displayB == 0) {color_right_now = "black";}
                if (displayR == 1 && displayG == 1 && displayB == 1) {color_right_now = "all most black";}
                if (displayR == 2 && displayG == 2 && displayB == 2) {color_right_now = "all most black";}
                if (displayR == 3 && displayG == 3 && displayB == 3) {color_right_now = "all most black";}
                if (displayR == 4 && displayG == 4 && displayB == 4) {color_right_now = "all most black";}
                if (displayR == 5 && displayG == 5 && displayB == 5) {color_right_now = "all most gray";}
                if (displayR == 6 && displayG == 6 && displayB == 6) {color_right_now = "all most gray";}
                if (displayR == 7 && displayG == 7 && displayB == 7) {color_right_now = "all most gray";}
                if (displayR == 8 && displayG == 8 && displayB == 8) {color_right_now = "all most gray";}
                if (displayR == 9 && displayG == 9 && displayB == 9) {color_right_now = "all most gray";}
                if (displayR == 10 && displayG == 10 && displayB == 10) {color_right_now = "super dark gray";}
                if (displayR == 11 && displayG == 11 && displayB == 11) {color_right_now = "super dark gray";}
                if (displayR == 12 && displayG == 12 && displayB == 12) {color_right_now = "super dark gray";}
                if (displayR == 13 && displayG == 13 && displayB == 13) {color_right_now = "super dark gray";}
                if (displayR == 14 && displayG == 14 && displayB == 14) {color_right_now = "super dark gray";}
                if (displayR == 15 && displayG == 15 && displayB == 15) {color_right_now = "super dark gray";}
                if (displayR == 16 && displayG == 16 && displayB == 16) {color_right_now = "super dark gray";}
                if (displayR == 17 && displayG == 17 && displayB == 17) {color_right_now = "super dark gray";}
                if (displayR == 18 && displayG == 18 && displayB == 18) {color_right_now = "super dark gray";}
                if (displayR == 19 && displayG == 19 && displayB == 19) {color_right_now = "super dark gray";}
                if (displayR == 20 && displayG == 20 && displayB == 20) {color_right_now = "super dark gray";}
                if (displayR == 21 && displayG == 21 && displayB == 21) {color_right_now = "super dark gray";}
                if (displayR == 22 && displayG == 22 && displayB == 22) {color_right_now = "super dark gray";}
                if (displayR == 23 && displayG == 23 && displayB == 23) {color_right_now = "super dark gray";}
                if (displayR == 24 && displayG == 24 && displayB == 24) {color_right_now = "super dark gray";}
                if (displayR == 25 && displayG == 25 && displayB == 25) {color_right_now = "super dark gray";}
                if (displayR == 26 && displayG == 26 && displayB == 26) {color_right_now = "super dark gray";}
                if (displayR == 27 && displayG == 27 && displayB == 27) {color_right_now = "super dark gray";}
                if (displayR == 28 && displayG == 28 && displayB == 28) {color_right_now = "super dark gray";}
                if (displayR == 29 && displayG == 29 && displayB == 29) {color_right_now = "super dark gray";}
                if (displayR == 30 && displayG == 30 && displayB == 30) {color_right_now = "super dark gray";}
                if (displayR == 40 && displayG == 40 && displayB == 40) {color_right_now = "super dark gray";}
                if (displayR == 41 && displayG == 41 && displayB == 41) {color_right_now = "super dark gray";}
                if (displayR == 42 && displayG == 42 && displayB == 42) {color_right_now = "super dark gray";}
                if (displayR == 43 && displayG == 43 && displayB == 43) {color_right_now = "super dark gray";}
                if (displayR == 44 && displayG == 44 && displayB == 44) {color_right_now = "super dark gray";}
                if (displayR == 45 && displayG == 45 && displayB == 45) {color_right_now = "super dark gray";}
                if (displayR == 46 && displayG == 46 && displayB == 46) {color_right_now = "super dark gray";}
                if (displayR == 47 && displayG == 47 && displayB == 47) {color_right_now = "super dark gray";}
                if (displayR == 48 && displayG == 48 && displayB == 48) {color_right_now = "super dark gray";}
                if (displayR == 49 && displayG == 49 && displayB == 49) {color_right_now = "super dark gray";}
                if (displayR == 50 && displayG == 50 && displayB == 50) {color_right_now = "super dark gray";}
                if (displayR == 51 && displayG == 51 && displayB == 51) {color_right_now = "super dark gray";}
                if (displayR == 52 && displayG == 52 && displayB == 52) {color_right_now = "super dark gray";}
                if (displayR == 53 && displayG == 53 && displayB == 53) {color_right_now = "super dark gray";}
                if (displayR == 54 && displayG == 54 && displayB == 54) {color_right_now = "super dark gray";}
                if (displayR == 55 && displayG == 55 && displayB == 55) {color_right_now = "super dark gray";}
                if (displayR == 56 && displayG == 56 && displayB == 56) {color_right_now = "super dark gray";}
                if (displayR == 57 && displayG == 57 && displayB == 57) {color_right_now = "super dark gray";}
                if (displayR == 58 && displayG == 58 && displayB == 58) {color_right_now = "super dark gray";}
                if (displayR == 59 && displayG == 59 && displayB == 59) {color_right_now = "super dark gray";}
                if (displayR == 60 && displayG == 60 && displayB == 60) {color_right_now = "super dark gray";}
                if (displayR == 61 && displayG == 61 && displayB == 61) {color_right_now = "super dark gray";}
                if (displayR == 62 && displayG == 62 && displayB == 62) {color_right_now = "dark gray";}
                if (displayR == 63 && displayG == 63 && displayB == 63) {color_right_now = "dark gray";}
                if (displayR == 64 && displayG == 64 && displayB == 64) {color_right_now = "dark gray";}
                if (displayR == 65 && displayG == 65 && displayB == 65) {color_right_now = "dark gray";}
                if (displayR == 66 && displayG == 66 && displayB == 66) {color_right_now = "dark gray";}
                if (displayR == 67 && displayG == 67 && displayB == 67) {color_right_now = "dark gray";}
                if (displayR == 68 && displayG == 68 && displayB == 68) {color_right_now = "dark gray";}
                if (displayR == 69 && displayG == 69 && displayB == 69) {color_right_now = "dark gray";}
                if (displayR == 70 && displayG == 70 && displayB == 70) {color_right_now = "dark gray";}
                if (displayR == 71 && displayG == 71 && displayB == 71) {color_right_now = "dark gray";}
                if (displayR == 72 && displayG == 72 && displayB == 72) {color_right_now = "dark gray";}
                if (displayR == 73 && displayG == 73 && displayB == 73) {color_right_now = "dark gray";}
                if (displayR == 74 && displayG == 74 && displayB == 74) {color_right_now = "dark gray";}
                if (displayR == 75 && displayG == 75 && displayB == 75) {color_right_now = "dark gray";}
                if (displayR == 76 && displayG == 76 && displayB == 76) {color_right_now = "dark gray";}
                if (displayR == 77 && displayG == 77 && displayB == 77) {color_right_now = "dark gray";}
                if (displayR == 78 && displayG == 78 && displayB == 78) {color_right_now = "dark gray";}
                if (displayR == 79 && displayG == 79 && displayB == 79) {color_right_now = "dark gray";}
                if (displayR == 80 && displayG == 80 && displayB == 80) {color_right_now = "dark gray";}
                if (displayR == 81 && displayG == 81 && displayB == 81) {color_right_now = "dark gray";}
                if (displayR == 82 && displayG == 82 && displayB == 82) {color_right_now = "dark gray";}
                if (displayR == 83 && displayG == 83 && displayB == 83) {color_right_now = "dark gray";}
                if (displayR == 84 && displayG == 84 && displayB == 84) {color_right_now = "dark gray";}
                if (displayR == 85 && displayG == 85 && displayB == 85) {color_right_now = "dark gray";}
                if (displayR == 86 && displayG == 86 && displayB == 86) {color_right_now = "dark gray";}
                if (displayR == 87 && displayG == 87 && displayB == 87) {color_right_now = "dark gray";}
                if (displayR == 88 && displayG == 88 && displayB == 88) {color_right_now = "dark gray";}
                if (displayR == 89 && displayG == 89 && displayB == 89) {color_right_now = "dark gray";}
                if (displayR == 90 && displayG == 90 && displayB == 90) {color_right_now = "dark gray";}
                if (displayR == 91 && displayG == 91 && displayB == 91) {color_right_now = "dark gray";}
                if (displayR == 92 && displayG == 92 && displayB == 92) {color_right_now = "dark gray";}
                if (displayR == 93 && displayG == 93 && displayB == 93) {color_right_now = "dark gray";}
                if (displayR == 94 && displayG == 94 && displayB == 94) {color_right_now = "dark gray";}
                if (displayR == 95 && displayG == 95 && displayB == 95) {color_right_now = "dark gray";}
                if (displayR == 96 && displayG == 96 && displayB == 96) {color_right_now = "dark gray";}
                if (displayR == 97 && displayG == 97 && displayB == 97) {color_right_now = "dark gray";}
                if (displayR == 98 && displayG == 98 && displayB == 98) {color_right_now = "dark gray";}
                if (displayR == 99 && displayG == 99 && displayB == 99) {color_right_now = "dark gray";}
                if (displayR == 100 && displayG == 100 && displayB == 100) {color_right_now = "dark gray";}
                if (displayR == 101 && displayG == 101 && displayB == 101) {color_right_now = "dark gray";}
                if (displayR == 102 && displayG == 102 && displayB == 102) {color_right_now = "dark gray";}
                if (displayR == 103 && displayG == 103 && displayB == 103) {color_right_now = "dark gray";}
                if (displayR == 104 && displayG == 104 && displayB == 104) {color_right_now = "dark gray";}
                if (displayR == 105 && displayG == 105 && displayB == 105) {color_right_now = "dark gray";}
                if (displayR == 106 && displayG == 106 && displayB == 106) {color_right_now = "dark gray";}
                if (displayR == 107 && displayG == 107 && displayB == 107) {color_right_now = "dark gray";}
                if (displayR == 108 && displayG == 108 && displayB == 108) {color_right_now = "dark gray";}
                if (displayR == 109 && displayG == 109 && displayB == 109) {color_right_now = "dark gray";}
                if (displayR == 110 && displayG == 110 && displayB == 110) {color_right_now = "dark gray";}
                if (displayR == 111 && displayG == 111 && displayB == 111) {color_right_now = "dark gray";}
                if (displayR == 112 && displayG == 112 && displayB == 112) {color_right_now = "dark gray";}
                if (displayR == 113 && displayG == 113 && displayB == 113) {color_right_now = "dark gray";}
                if (displayR == 114 && displayG == 114 && displayB == 114) {color_right_now = "dark gray";}
                if (displayR == 115 && displayG == 115 && displayB == 115) {color_right_now = "dark gray";}
                if (displayR == 116 && displayG == 116 && displayB == 116) {color_right_now = "dark gray";}
                if (displayR == 117 && displayG == 117 && displayB == 117) {color_right_now = "dark gray";}
                if (displayR == 118 && displayG == 118 && displayB == 118) {color_right_now = "dark gray";}
                if (displayR == 119 && displayG == 119 && displayB == 119) {color_right_now = "dark gray";}
                if (displayR == 120 && displayG == 120 && displayB == 120) {color_right_now = "dark gray";}
                if (displayR == 121 && displayG == 121 && displayB == 121) {color_right_now = "dark gray";}
                if (displayR == 122 && displayG == 122 && displayB == 122) {color_right_now = "dark gray";}
                if (displayR == 123 && displayG == 123 && displayB == 123) {color_right_now = "dark gray";}
                if (displayR == 124 && displayG == 124 && displayB == 124) {color_right_now = "dark gray";}
                if (displayR == 125 && displayG == 125 && displayB == 125) {color_right_now = "dark gray";}
                if (displayR == 126 && displayG == 126 && displayB == 126) {color_right_now = "dark gray";}
                if (displayR == 127 && displayG == 127 && displayB == 127) {color_right_now = "dark gray";}
                if (displayR == 128 && displayG == 128 && displayB == 128) {color_right_now = "dark gray";}
                if (displayR == 129 && displayG == 129 && displayB == 129) {color_right_now = "dark gray";}
                if (displayR == 130 && displayG == 130 && displayB == 130) {color_right_now = "dark gray";}
                if (displayR == 131 && displayG == 131 && displayB == 131) {color_right_now = "dark gray";}
                if (displayR == 132 && displayG == 132 && displayB == 132) {color_right_now = "dark gray";}
                if (displayR == 133 && displayG == 133 && displayB == 133) {color_right_now = "all most gray";}
                if (displayR == 134 && displayG == 134 && displayB == 134) {color_right_now = "all most gray";}
                if (displayR == 135 && displayG == 135 && displayB == 135) {color_right_now = "all most gray";}
                if (displayR == 136 && displayG == 136 && displayB == 136) {color_right_now = "all most gray";}
                if (displayR == 137 && displayG == 137 && displayB == 137) {color_right_now = "all most gray";}
                if (displayR == 138 && displayG == 138 && displayB == 138) {color_right_now = "all most gray";}
                if (displayR == 139 && displayG == 139 && displayB == 139) {color_right_now = "all most gray";}
                if (displayR == 140 && displayG == 140 && displayB == 140) {color_right_now = "all most gray";}
                if (displayR == 141 && displayG == 141 && displayB == 141) {color_right_now = "all most gray";}
                if (displayR == 142 && displayG == 142 && displayB == 142) {color_right_now = "all most gray";}
                if (displayR == 143 && displayG == 143 && displayB == 143) {color_right_now = "all most gray";}
                if (displayR == 144 && displayG == 144 && displayB == 144) {color_right_now = "all most gray";}
                if (displayR == 145 && displayG == 145 && displayB == 145) {color_right_now = "gray";}
                if (displayR == 146 && displayG == 146 && displayB == 146) {color_right_now = "gray";}
                if (displayR == 147 && displayG == 147 && displayB == 147) {color_right_now = "gray";}
                if (displayR == 148 && displayG == 148 && displayB == 148) {color_right_now = "gray";}
                if (displayR == 149 && displayG == 149 && displayB == 149) {color_right_now = "gray";}
                if (displayR == 150 && displayG == 150 && displayB == 150) {color_right_now = "gray";}
                if (displayR == 151 && displayG == 151 && displayB == 151) {color_right_now = "gray";}
                if (displayR == 152 && displayG == 152 && displayB == 152) {color_right_now = "gray";}
                if (displayR == 153 && displayG == 153 && displayB == 153) {color_right_now = "gray";}
                if (displayR == 154 && displayG == 154 && displayB == 154) {color_right_now = "gray";}
                if (displayR == 155 && displayG == 155 && displayB == 155) {color_right_now = "gray";}
                if (displayR == 156 && displayG == 156 && displayB == 156) {color_right_now = "gray";}
                if (displayR == 157 && displayG == 157 && displayB == 157) {color_right_now = "gray";}
                if (displayR == 158 && displayG == 158 && displayB == 158) {color_right_now = "gray";}
                if (displayR == 159 && displayG == 159 && displayB == 159) {color_right_now = "gray";}
                if (displayR == 160 && displayG == 160 && displayB == 160) {color_right_now = "gray";}
                if (displayR == 160 && displayG == 160 && displayB == 160) {color_right_now = "gray";}
                if (displayR == 161 && displayG == 161 && displayB == 161) {color_right_now = "gray";}
                if (displayR == 162 && displayG == 162 && displayB == 162) {color_right_now = "gray";}
                if (displayR == 163 && displayG == 163 && displayB == 163) {color_right_now = "gray";}
                if (displayR == 164 && displayG == 164 && displayB == 164) {color_right_now = "all most light gray";}
                if (displayR == 165 && displayG == 165 && displayB == 165) {color_right_now = "all most light gray";}
                if (displayR == 166 && displayG == 166 && displayB == 166) {color_right_now = "all most light gray";}
                if (displayR == 167 && displayG == 167 && displayB == 167) {color_right_now = "all most light gray";}
                if (displayR == 168 && displayG == 168 && displayB == 168) {color_right_now = "all most light gray";}
                if (displayR == 169 && displayG == 169 && displayB == 169) {color_right_now = "all most light gray";}
                if (displayR == 170 && displayG == 170 && displayB == 170) {color_right_now = "all most light gray";}
                if (displayR == 171 && displayG == 171 && displayB == 171) {color_right_now = "all most light gray";}
                if (displayR == 172 && displayG == 172 && displayB == 172) {color_right_now = "all most light gray";}
                if (displayR == 173 && displayG == 173 && displayB == 173) {color_right_now = "all most light gray";}
                if (displayR == 174 && displayG == 174 && displayB == 174) {color_right_now = "all most light gray";}
                if (displayR == 175 && displayG == 175 && displayB == 175) {color_right_now = "all most light gray";}
                if (displayR == 176 && displayG == 176 && displayB == 176) {color_right_now = "all most light gray";}
                if (displayR == 177 && displayG == 177 && displayB == 177) {color_right_now = "all most light gray";}
                if (displayR == 178 && displayG == 178 && displayB == 178) {color_right_now = "light gray";}
                if (displayR == 179 && displayG == 179 && displayB == 179) {color_right_now = "light gray";}
                if (displayR == 180 && displayG == 180 && displayB == 180) {color_right_now = "light gray";}
                if (displayR == 181 && displayG == 181 && displayB == 181) {color_right_now = "light gray";}
                if (displayR == 182 && displayG == 182 && displayB == 182) {color_right_now = "light gray";}
                if (displayR == 183 && displayG == 183 && displayB == 183) {color_right_now = "light gray";}
                if (displayR == 184 && displayG == 184 && displayB == 184) {color_right_now = "light gray";}
                if (displayR == 185 && displayG == 185 && displayB == 185) {color_right_now = "light gray";}
                if (displayR == 186 && displayG == 186 && displayB == 186) {color_right_now = "light gray";}
                if (displayR == 187 && displayG == 187 && displayB == 187) {color_right_now = "light gray";}
                if (displayR == 188 && displayG == 188 && displayB == 188) {color_right_now = "light gray";}
                if (displayR == 189 && displayG == 189 && displayB == 189) {color_right_now = "light gray";}
                if (displayR == 190 && displayG == 190 && displayB == 190) {color_right_now = "light gray";}
                if (displayR == 191 && displayG == 191 && displayB == 191) {color_right_now = "light gray";}
                if (displayR == 192 && displayG == 192 && displayB == 192) {color_right_now = "light gray";}
                if (displayR == 193 && displayG == 193 && displayB == 193) {color_right_now = "light gray";}
                if (displayR == 194 && displayG == 194 && displayB == 194) {color_right_now = "light gray";}
                if (displayR == 195 && displayG == 195 && displayB == 195) {color_right_now = "light gray";}
                if (displayR == 196 && displayG == 196 && displayB == 196) {color_right_now = "light gray";}
                if (displayR == 197 && displayG == 197 && displayB == 197) {color_right_now = "light gray";}
                if (displayR == 198 && displayG == 198 && displayB == 198) {color_right_now = "light gray";}
                if (displayR == 199 && displayG == 199 && displayB == 199) {color_right_now = "light gray";}
                if (displayR == 200 && displayG == 200 && displayB == 200) {color_right_now = "light gray";}
                if (displayR == 201 && displayG == 201 && displayB == 201) {color_right_now = "light gray";}
                if (displayR == 202 && displayG == 202 && displayB == 202) {color_right_now = "light gray";}
                if (displayR == 203 && displayG == 203 && displayB == 203) {color_right_now = "light gray";}
                if (displayR == 204 && displayG == 204 && displayB == 204) {color_right_now = "light gray";}
                if (displayR == 205 && displayG == 205 && displayB == 205) {color_right_now = "super light gray";}
                if (displayR == 206 && displayG == 206 && displayB == 206) {color_right_now = "super light gray";}
                if (displayR == 207 && displayG == 207 && displayB == 207) {color_right_now = "super light gray";}
                if (displayR == 208 && displayG == 208 && displayB == 208) {color_right_now = "super light gray";}
                if (displayR == 209 && displayG == 209 && displayB == 209) {color_right_now = "super light gray";}
                if (displayR == 210 && displayG == 210 && displayB == 210) {color_right_now = "super light gray";}
                if (displayR == 211 && displayG == 211 && displayB == 211) {color_right_now = "super light gray";}
                if (displayR == 212 && displayG == 212 && displayB == 212) {color_right_now = "super light gray";}
                if (displayR == 213 && displayG == 213 && displayB == 213) {color_right_now = "super light gray";}
                if (displayR == 214 && displayG == 214 && displayB == 214) {color_right_now = "super light gray";}
                if (displayR == 215 && displayG == 215 && displayB == 215) {color_right_now = "super light gray";}
                if (displayR == 216 && displayG == 216 && displayB == 216) {color_right_now = "super light gray";}
                if (displayR == 217 && displayG == 217 && displayB == 217) {color_right_now = "super light gray";}
                if (displayR == 218 && displayG == 218 && displayB == 218) {color_right_now = "super light gray";}
                if (displayR == 219 && displayG == 219 && displayB == 219) {color_right_now = "super light gray";}
                if (displayR == 220 && displayG == 220 && displayB == 220) {color_right_now = "super light gray";}
                if (displayR == 221 && displayG == 221 && displayB == 221) {color_right_now = "super light gray";}
                if (displayR == 222 && displayG == 222 && displayB == 222) {color_right_now = "super light gray";}
                if (displayR == 223 && displayG == 223 && displayB == 223) {color_right_now = "super light gray";}
                if (displayR == 224 && displayG == 224 && displayB == 224) {color_right_now = "super light gray";}
                if (displayR == 225 && displayG == 225 && displayB == 225) {color_right_now = "super light gray";}
                if (displayR == 226 && displayG == 226 && displayB == 226) {color_right_now = "super light gray";}
                if (displayR == 227 && displayG == 227 && displayB == 227) {color_right_now = "super light gray";}
                if (displayR == 228 && displayG == 228 && displayB == 228) {color_right_now = "super light gray";}
                if (displayR == 229 && displayG == 229 && displayB == 229) {color_right_now = "super light gray";}
                if (displayR == 230 && displayG == 230 && displayB == 230) {color_right_now = "white";}
                if (displayR == 231 && displayG == 231 && displayB == 231) {color_right_now = "white";}
                if (displayR == 232 && displayG == 232 && displayB == 232) {color_right_now = "white";}
                if (displayR == 233 && displayG == 233 && displayB == 233) {color_right_now = "white";}
                if (displayR == 233 && displayG == 233 && displayB == 233) {color_right_now = "white";}
                if (displayR == 234 && displayG == 234 && displayB == 234) {color_right_now = "white";}
                if (displayR == 235 && displayG == 235 && displayB == 235) {color_right_now = "white";}
                if (displayR == 236 && displayG == 236 && displayB == 236) {color_right_now = "white";}
                if (displayR == 237 && displayG == 237 && displayB == 237) {color_right_now = "white";}
                if (displayR == 238 && displayG == 238 && displayB == 238) {color_right_now = "white";}
                if (displayR == 239 && displayG == 239 && displayB == 239) {color_right_now = "white";}
                if (displayR == 240 && displayG == 240 && displayB == 240) {color_right_now = "white";}
                if (displayR == 241 && displayG == 241 && displayB == 241) {color_right_now = "white";}
                if (displayR == 242 && displayG == 242 && displayB == 242) {color_right_now = "white";}
                if (displayR == 243 && displayG == 243 && displayB == 243) {color_right_now = "white";}
                if (displayR == 244 && displayG == 244 && displayB == 244) {color_right_now = "white";}
                if (displayR == 245 && displayG == 245 && displayB == 245) {color_right_now = "white";}
                if (displayR == 246 && displayG == 246 && displayB == 246) {color_right_now = "white";}
                if (displayR == 247 && displayG == 247 && displayB == 247) {color_right_now = "white";}
                if (displayR == 248 && displayG == 248 && displayB == 248) {color_right_now = "white";}
                if (displayR == 249 && displayG == 249 && displayB == 249) {color_right_now = "white";}
                if (displayR == 250 && displayG == 250 && displayB == 250) {color_right_now = "white";}
                if (displayR == 251 && displayG == 251 && displayB == 251) {color_right_now = "white";}
                if (displayR == 252 && displayG == 252 && displayB == 252) {color_right_now = "white";}
                if (displayR == 253 && displayG == 253 && displayB == 253) {color_right_now = "white";}
                if (displayR == 254 && displayG == 254 && displayB == 254) {color_right_now = "white";}
                if (displayR == 255 && displayG == 255 && displayB == 255) {color_right_now = "white";}
                if (displayR == 0 && displayG == 1 && displayB == 0) {color_right_now = "black";}
                if (displayR == 0 && displayG == 2 && displayB == 0) {color_right_now = "black";}
                if (displayR == 0 && displayG == 3 && displayB == 0) {color_right_now = "black";}
                if (displayR == 0 && displayG == 4 && displayB == 0) {color_right_now = "black";}
                if (displayR == 0 && displayG == 5 && displayB == 0) {color_right_now = "black";}
                if (displayR == 0 && displayG == 6 && displayB == 0) {color_right_now = "black";}
                if (displayR == 0 && displayG == 7 && displayB == 0) {color_right_now = "black";}
                if (displayR == 0 && displayG == 8 && displayB == 0) {color_right_now = "black";}
                if (displayR == 0 && displayG == 9 && displayB == 0) {color_right_now = "black";}
                if (displayR == 0 && displayG == 10 && displayB == 0) {color_right_now = "black";}
                if (displayR == 0 && displayG == 11 && displayB == 0) {color_right_now = "black";}
                if (displayR == 0 && displayG == 12 && displayB == 0) {color_right_now = "black";}
                if (displayR == 0 && displayG == 13 && displayB == 0) {color_right_now = "black";}
                if (displayR == 0 && displayG == 14 && displayB == 0) {color_right_now = "black";}
                if (displayR == 0 && displayG == 15 && displayB == 0) {color_right_now = "black";}
                if (displayR == 0 && displayG == 16 && displayB == 0) {color_right_now = "black";}
                if (displayR == 0 && displayG == 17 && displayB == 0) {color_right_now = "black";}
                if (displayR == 0 && displayG == 18 && displayB == 0) {color_right_now = "black";}
                if (displayR == 0 && displayG == 19 && displayB == 0) {color_right_now = "black";}
                if (displayR == 0 && displayG == 20 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 21 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 22 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 23 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 24 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 25 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 26 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 27 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 28 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 29 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 30 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 31 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 32 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 33 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 34 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 35 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 36 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 37 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 38 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 39 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 40 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 41 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 42 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 43 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 44 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 45 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 46 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 47 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 48 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG == 49 && displayB == 0) {color_right_now = "super dark green";}
                if (displayR == 0 && displayG >= 50 && displayB == 0) {color_right_now = "dark green";}
                if (displayR == 0 && displayG >= 90 && displayB == 0) {color_right_now = "green";}
                if (displayR == 0 && displayG >= 170 && displayB == 0) {color_right_now = "light green";}
                if (displayR == 0 && displayG >= 220 && displayB == 0) {color_right_now = "super light green";}
                if (displayR >= 0 && displayG == 0 && displayB == 0) {color_right_now = "black";}
                if (displayR >= 30 && displayG == 0 && displayB == 0) {color_right_now = "super dark red";}
                if (displayR >= 50 && displayG == 0 && displayB == 0) {color_right_now = "dark red";}
                if (displayR >= 150 && displayG == 0 && displayB == 0) {color_right_now = "red";}
                if (displayR >= 220 && displayG == 0 && displayB == 0) {color_right_now = "light red";}
                if (displayR == 0 && displayG == 0 && displayB >= 0) {color_right_now = "black";}
                if (displayR == 0 && displayG == 0 && displayB >= 30) {color_right_now = "super dark blue";}
                if (displayR == 0 && displayG == 0 && displayB >= 50) {color_right_now = "dark blue";}
                if (displayR == 0 && displayG == 0 && displayB >= 150) {color_right_now = "blue";}
                if (displayR == 0 && displayG == 0 && displayB >= 220) {color_right_now = "light blue";}
                if (displayR <= 255 && displayR >= 220 && displayG <= 150 && displayG >= 100 && displayB <= 255 && displayB >= 220) {color_right_now = "pink";}
                if (displayR <= 255 && displayR >= 220 && displayG <= 80 && displayG >= 20 && displayB <= 255 && displayB >= 220) {color_right_now = "PURPLE";}
                if (displayR <= 255 && displayR >= 220 && displayG <= 200 && displayG >= 150 && displayB == 0) {color_right_now = "ORANGE";}
                if (displayR <= 110 && displayR >= 101 && displayG <= 80 && displayG >= 67 && displayB <= 40 && displayB >= 33) {color_right_now = "BROWN";}
                DrawText(TextFormat(color_right_now), 600,230,20, BLACK);
            }
            if (traceActive) {
                float cX = (float)canvasCenterX + 50.0f; 
                if (clickState == 1) {
                    DrawLineEx(traceStart, mPos, (float)pen_size, ColorAlpha(color, 0.6f));
                    if (mirrorMode) {
                        Vector2 mStart = { cX - (traceStart.x - cX), traceStart.y };
                        Vector2 mEnd = { cX - (mPos.x - cX), mPos.y };
                        DrawLineEx(mStart, mEnd, (float)pen_size, ColorAlpha(color, 0.6f));
                    }
                } else if (clickState == 2) {
                    DrawCustomBezierQuad(traceStart, mPos, traceControl, (float)pen_size, ColorAlpha(color, 0.6f));
                    if (mirrorMode) {
                        Vector2 mStart = { cX - (traceStart.x - cX), traceStart.y };
                        Vector2 mControl = { cX - (traceControl.x - cX), traceControl.y };
                        Vector2 mEnd = { cX - (mPos.x - cX), mPos.y };
                        DrawCustomBezierQuad(mStart, mEnd, mControl, (float)pen_size, ColorAlpha(color, 0.6f));
                    }
                }
            }
        }
        DrawRectangle(0,0,50,650, DARKGREEN);
        for(int idx=0; idx<12; idx++) {
            Color itemColor = (idx==0)?BLACK:(idx==1)?DARKBROWN:(idx==2)?GRAY:(idx==3)?RAYWHITE:(idx==4)?SKYBLUE:(idx==5)?BLUE:(idx==6)?YELLOW:(idx==7)?GREEN:(idx==8)?PINK:(idx==9)?PURPLE:(idx==10)?RED:ORANGE;
            DrawRectangle(0, idx*50, size-2, size-2, itemColor);
        }
        DrawRectangle(0, 600, 48, 48, DARKGRAY);
        DrawRectangle(50, 0, 1150, 34, DARKGREEN);
        DrawRectangle(50, 0, 50, 30, GRAY);
        DrawText("file", 55, 5, 20, BLACK);
        DrawRectangle(105, 0, 50, 30, ORANGE);
        DrawText("del", 110, 10, 20, BLACK);
        DrawRectangle(165, 0, 50, 30, ORANGE);
        DrawText("save", 170, 5, 18, BLACK);
        DrawRectangle(225, 0, 70, 30, RED);
        DrawText("more", 230, 10, 10, BLACK);
        DrawRectangle(305, 0, 50, 30, GREEN);
        DrawText("pens", 310, 5, 15, BLACK);
        DrawRectangle(365, 0, 50, 30, GREEN);  
        DrawText("modes", 370, 5, 15, BLACK);
        DrawRectangle(425, 0, 50, 30, GREEN);
        DrawText("size", 430, 0, 20, BLACK);  
        DrawRectangle(685,0,95,30, GRAY); 
        DrawText("my_stuff",690,5,20, BLACK); 
        DrawRectangle(600, 0, 75, 30, GRAY); 
        DrawText("colors", 605,5,20, BLACK); 
        DrawRectangle(485, 0, 50, 30, LIGHTGRAY);
        DrawText("undo", 490, 5, 15, BLACK);
        DrawRectangle(540, 0, 50, 30, LIGHTGRAY); 
        DrawText("redo", 545, 5, 15, BLACK);        
        const char* activeToolText = "Tool: Brush (I)";
        if (traceActive) activeToolText = "Tool: Trace (T)";
        else if (fillModeActive) activeToolText = "Tool: Fill (A)";
        DrawRectangle(950, 615, 230, 30, ColorAlpha(BLACK, 0.6f));
        DrawText(activeToolText, 960, 620, 20, RAYWHITE);
        if (downloads) {
            DrawRectangle(900, 20, 500, 500, GRAY);
            DrawRectangle(910, 40, 200, 40, SKYBLUE);   
            DrawText("close", 920, 45, 30, BLACK);
            DrawRectangle(910, 90, 200, 40, ORANGE);    
            DrawText("save png", 920, 95, 25, BLACK);
            DrawRectangle(910, 140, 200, 40, ORANGE);   
            DrawText("save jpeg", 920, 145, 25, BLACK);
            DrawRectangle(910, 190, 200, 40, ORANGE);   
            DrawText("save jpg", 920, 195, 25, BLACK);
            DrawRectangle(910, 240, 200, 40, ORANGE);   
            DrawText("save bmp", 920, 245, 25, BLACK);
        }
        if (selectModeActive) {
            Vector2 current = GetMousePosition();
            DrawRectangleLines(selectStart.x, selectStart.y, current.x - selectStart.x, current.y - selectStart.y, YELLOW);
        }
        if (showSelectionMenu) {
            DrawRectangle(menuPos.x, menuPos.y, 100, 80, DARKGRAY);
            DrawText("Copy", menuPos.x + 10, menuPos.y + 10, 20, WHITE);
            DrawText("Delete", menuPos.x + 10, menuPos.y + 40, 20, WHITE);
            if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) { showSelectionMenu = false; } 
        } else if (more) {
            DrawRectangle(900, 20, 480, 500, boxcolor);
            DrawRectangle(910, 60, 150, 40, box2color);         
            DrawText("Close", 920, 70, 20, tc);
            DrawRectangle(910, 110, 480, 40, box2color);        
            DrawText("remove white", 925, 120, 20, tc);
            DrawRectangle(910, 160, 480, 40, box2color);        
            DrawText("remove GRAY", 925, 170, 20, tc);
            DrawRectangle(910, 210, 480, 40, box2color);        
            DrawText("remove black", 925, 220, 20, tc);
            DrawRectangle(910, 260, 480, 40, box2color);        
            DrawText("remove DARKBROWN", 925, 270, 20, tc);
            DrawRectangle(910, 310, 480, 40, box2color);       
            DrawText("remove selected color", 920, 320, 20, BLACK);
            DrawRectangle(910, 360, 70, 50, GRAY);              
            DrawText("lib", 925, 370, 30, BLACK);
        } else if (pens) {
            DrawRectangle(900, 20, 480, 500, GRAY);
            for(int k=0; k<8; k++) {
                DrawRectangle(950, (float)(20+(k*50)), 200, 40, SKYBLUE);
                DrawText(k==0?"close":k==1?"Circle brush":k==2?"Dot brush":k==3?"Hash brush":k==4?"Plus brush":k==5?"X-Cross brush":k==6?"Star brush":"Percent brush", 960, 25+(k*50), 22, BLACK);
            }
        } else if (mode) {
            DrawRectangle(900, 20, 500, 500, GRAY);
            DrawRectangle(910, 40, 200, 40, SKYBLUE);    
            DrawText("close", 920, 45, 30, BLACK);
            DrawRectangle(910, 90, 200, 40, SKYBLUE);    
            DrawText("rainbow pen", 920, 95, 30, BLACK);
            DrawRectangle(910, 140, 200, 40, SKYBLUE);   
            DrawText("rainbow off", 920, 145, 30, BLACK);
            DrawRectangle(910, 190, 200, 40, SKYBLUE);   
            DrawText("mirror mode", 920, 195, 30, BLACK);
            DrawRectangle(910, 240, 200, 40, SKYBLUE);   
            DrawText("mirror off", 920, 245, 30, BLACK);    
            Color traceBtnColor = traceActive ? DARKGREEN : RED;
            DrawRectangle(910, 290, 200, 40, traceBtnColor);     
            DrawText("trace tool", 920, 295, 30, BLACK);
            Color fillBtnColor = fillModeActive ? GREEN : SKYBLUE;
            const char* fillText = fillModeActive ? "fill tool: on" : "fill tool: off";
            DrawRectangle(910, 340, 200, 40, fillBtnColor);     
            DrawText(fillText, 920, 345, 25, BLACK);
            DrawRectangle(910, 390, 200, 40, GOLD);      
            DrawText("3D Cloth", 920, 395, 30, BLACK);
        } else if (sizes) {
            DrawRectangle(910, 50, 300, 200, GRAY);
            DrawRectangle(915, 55, 50, 50, SKYBLUE);    
            DrawText("^", 930, 60, 40, BLACK);
            DrawRectangle(915, 110, 50, 50, SKYBLUE);   
            DrawText("v", 930, 115, 40, BLACK);
            DrawRectangle(915, 165, 50, 50, SKYBLUE);   
            DrawText("x", 930, 170, 40, BLACK);
            DrawRectangle(975,120, 200,30, SKYBLUE);    
            DrawText(TextFormat("size:%d",pen_size), 980,125,20, BLACK);
        }
        EndDrawing();
    } 
    for (int i = 0; i < undoCount; i++) UnloadRenderTexture(undoStack[i]);
    for (int i = 0; i < redoCount; i++) UnloadRenderTexture(redoStack[i]);
    UnloadRenderTexture(canvas);
    CloseWindow();
    return 0;
}