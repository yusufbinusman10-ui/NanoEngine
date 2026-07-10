#include "raylib.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// CELL_SIZE is now fixed. Resizing the window changes how many grid
// cells fit on screen (gridWidth/gridHeight), not how big each cell is.
// That's what lets sand fall further when the window gets taller.
#define CELL_SIZE 3

// Upper bound on simulation size (covers windows up to ~2700x1800).
// Bump these if you need bigger windows.
#define MAX_GRID_WIDTH   900
#define MAX_GRID_HEIGHT  600

#define CHUNK_SIZE 16
#define MAX_CHUNKS_X (MAX_GRID_WIDTH  / CHUNK_SIZE)
#define MAX_CHUNKS_Y (MAX_GRID_HEIGHT / CHUNK_SIZE)

typedef enum {
    FLAG_NONE       = 0,
    FLAG_GRAVITY    = 1 << 0,
    FLAG_SLIDE      = 1 << 1,
    FLAG_LIQUID     = 1 << 2,
    FLAG_BUOYANT    = 1 << 3,
    FLAG_CONSUMABLE = 1 << 4,
    FLAG_STATIC     = 1 << 5,
    FLAG_CORROSIVE  = 1 << 6
} MatFlags;

typedef enum {
    ID_EMPTY = 0,
    ID_SAND,
    ID_WATER,
    ID_GAS,
    ID_FIRE,
    ID_WALL,
    ID_ACID,
    ID_OIL
} MatID;

typedef struct {
    unsigned char id;
    unsigned char flags;
    bool updated;
    Color color;
} Grain;

// Arrays are allocated at MAX size. At any given moment only the
// [0..gridWidth) x [0..gridHeight) sub-region is simulated/drawn.
// Data outside that region is preserved, not wiped, so shrinking then
// regrowing the window brings old sand back.
static Grain grid[MAX_GRID_WIDTH][MAX_GRID_HEIGHT];
static Color pixelBuffer[MAX_GRID_WIDTH * MAX_GRID_HEIGHT];
static unsigned char chunkState[MAX_CHUNKS_X][MAX_CHUNKS_Y];

static int gridWidth  = 0;
static int gridHeight = 0;
static int chunksX    = 0;
static int chunksY    = 0;

void RecomputeGridDims(void) {
    int newW = GetScreenWidth()  / CELL_SIZE;
    int newH = GetScreenHeight() / CELL_SIZE;

    if (newW < 1) newW = 1;
    if (newH < 1) newH = 1;
    if (newW > MAX_GRID_WIDTH)  newW = MAX_GRID_WIDTH;
    if (newH > MAX_GRID_HEIGHT) newH = MAX_GRID_HEIGHT;

    gridWidth  = newW;
    gridHeight = newH;
    chunksX = (gridWidth  + CHUNK_SIZE - 1) / CHUNK_SIZE;
    chunksY = (gridHeight + CHUNK_SIZE - 1) / CHUNK_SIZE;
    if (chunksX > MAX_CHUNKS_X) chunksX = MAX_CHUNKS_X;
    if (chunksY > MAX_CHUNKS_Y) chunksY = MAX_CHUNKS_Y;

    // Wake every chunk in the newly active area so nothing sits frozen
    // (e.g. sand that's now got new empty space below it after growing).
    for (int cy = 0; cy < chunksY; cy++)
        for (int cx = 0; cx < chunksX; cx++)
            chunkState[cx][cy] = 2;
}

void ForceWake(int x, int y) {
    if (x < 0 || x >= gridWidth || y < 0 || y >= gridHeight) return;
    int cx = x / CHUNK_SIZE;
    int cy = y / CHUNK_SIZE;
    chunkState[cx][cy] = 2;

    if (x % CHUNK_SIZE == 0 && cx > 0)                       chunkState[cx - 1][cy] = 2;
    if (x % CHUNK_SIZE == CHUNK_SIZE - 1 && cx < chunksX - 1) chunkState[cx + 1][cy] = 2;
    if (y % CHUNK_SIZE == 0 && cy > 0)                       chunkState[cx][cy - 1] = 2;
    if (y % CHUNK_SIZE == CHUNK_SIZE - 1 && cy < chunksY - 1) chunkState[cx][cy + 1] = 2;
}

void SaveSandboxState(const char *filename) {
    FILE *file = fopen(filename, "wb");
    if (!file) return;
    unsigned short ver = 0x5602; // bumped: now always stores the MAX-size buffer
    fwrite(&ver, sizeof(unsigned short), 1, file);
    fwrite(grid, sizeof(Grain), MAX_GRID_WIDTH * MAX_GRID_HEIGHT, file);
    fclose(file);
}

void LoadSandboxState(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) return;
    unsigned short ver = 0;
    fread(&ver, sizeof(unsigned short), 1, file);
    if (ver == 0x5602) {
        fread(grid, sizeof(Grain), MAX_GRID_WIDTH * MAX_GRID_HEIGHT, file);
    }
    fclose(file);

    for (int cy = 0; cy < MAX_CHUNKS_Y; cy++)
        for (int cx = 0; cx < MAX_CHUNKS_X; cx++)
            chunkState[cx][cy] = 2;
}

int main(void) {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(400 * CELL_SIZE, 224 * CELL_SIZE, "sandbox tool");

    LoadSandboxState("sandbox\\sandbox.sav");
    RecomputeGridDims();
    SetTargetFPS(60);

    // Texture is allocated at MAX size once; we only draw the active
    // gridWidth x gridHeight sub-rect of it each frame, so no need to
    // recreate the texture on resize.
    Image img = {
        .data   = pixelBuffer,
        .width  = MAX_GRID_WIDTH,
        .height = MAX_GRID_HEIGHT,
        .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
        .mipmaps = 1
    };
    Texture2D tex = LoadTextureFromImage(img);

    int  activeTool = ID_SAND;
    float hue       = 0.0f;
    bool eraserMode = false;

    while (!WindowShouldClose()) {
        if (IsWindowResized()) {
            RecomputeGridDims();
        }

        if (IsKeyPressed(KEY_ONE))   { activeTool = ID_SAND;  eraserMode = false; }
        if (IsKeyPressed(KEY_TWO))   { activeTool = ID_WATER; eraserMode = false; }
        if (IsKeyPressed(KEY_THREE)) { activeTool = ID_GAS;   eraserMode = false; }
        if (IsKeyPressed(KEY_FOUR))  { activeTool = ID_FIRE;  eraserMode = false; }
        if (IsKeyPressed(KEY_FIVE))  { activeTool = ID_ACID;  eraserMode = false; }
        if (IsKeyPressed(KEY_SIX))   { activeTool = ID_OIL;   eraserMode = false; }
        if (IsKeyPressed(KEY_SEVEN)) { eraserMode = true; }

        if (IsKeyPressed(KEY_S)) {
            SaveSandboxState("data\\sandbox.sav");
            Image snap = {
                .data   = pixelBuffer,
                .width  = gridWidth,
                .height = gridHeight,
                .format = PIXELFORMAT_UNCOMPRESSED_R8G8B8A8,
                .mipmaps = 1
            };
            ExportImage(snap, "your_images\\sandbox_render.png");
        }

        if (IsKeyPressed(KEY_L)) {
            LoadSandboxState("data\\sandbox.sav");
            RecomputeGridDims();
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) || IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) {
            Vector2 m = GetMousePosition();
            int mx = (int)(m.x / CELL_SIZE);
            int my = (int)(m.y / CELL_SIZE);

            int currentTool = IsMouseButtonDown(MOUSE_BUTTON_RIGHT) ? ID_WALL : activeTool;
            int brushSize   = 4;

            for (int dx = -brushSize; dx <= brushSize; dx++) {
                for (int dy = -brushSize; dy <= brushSize; dy++) {
                    int nx = mx + dx;
                    int ny = my + dy;
                    if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;

                    if (eraserMode) {
                        grid[nx][ny] = (Grain){0};
                    } else if (currentTool == ID_WALL) {
                        grid[nx][ny] = (Grain){
                            ID_WALL,
                            FLAG_STATIC,
                            false,
                            (Color){100, 100, 105, 255}
                        };
                    } else if (grid[nx][ny].id == ID_EMPTY) {
                        if (currentTool == ID_SAND) {
                            grid[nx][ny] = (Grain){
                                ID_SAND,
                                FLAG_GRAVITY | FLAG_SLIDE,
                                false,
                                ColorFromHSV(hue, 0.7f, 0.85f)
                            };
                        } else if (currentTool == ID_WATER) {
                            grid[nx][ny] = (Grain){
                                ID_WATER,
                                FLAG_GRAVITY | FLAG_SLIDE | FLAG_LIQUID,
                                false,
                                (Color){40, 140, 240, 255}
                            };
                        } else if (currentTool == ID_GAS) {
                            grid[nx][ny] = (Grain){
                                ID_GAS,
                                FLAG_BUOYANT | FLAG_CONSUMABLE,
                                false,
                                (Color){180, 180, 220, 140}
                            };
                        } else if (currentTool == ID_FIRE) {
                            grid[nx][ny] = (Grain){
                                ID_FIRE,
                                FLAG_BUOYANT,
                                false,
                                (Color){255, 90, 15, 255}
                            };
                        } else if (currentTool == ID_ACID) {
                            grid[nx][ny] = (Grain){
                                ID_ACID,
                                FLAG_GRAVITY | FLAG_SLIDE | FLAG_LIQUID | FLAG_CORROSIVE,
                                false,
                                (Color){50, 230, 50, 255}
                            };
                        } else if (currentTool == ID_OIL) {
                            grid[nx][ny] = (Grain){
                                ID_OIL,
                                FLAG_GRAVITY | FLAG_SLIDE | FLAG_LIQUID | FLAG_CONSUMABLE,
                                false,
                                (Color){25, 20, 35, 255}
                            };
                        }
                    }
                    ForceWake(nx, ny);
                }
            }

            hue += 1.2f;
            if (hue >= 360.0f) hue = 0.0f;
        }

        if (IsKeyPressed(KEY_C)) {
            memset(grid, 0, sizeof(grid));
            memset(chunkState, 0, sizeof(chunkState));
        }

        for (int y = 0; y < gridHeight; y++)
            for (int x = 0; x < gridWidth; x++)
                grid[x][y].updated = false;

        for (int cy = chunksY - 1; cy >= 0; cy--) {
            for (int cx = 0; cx < chunksX; cx++) {
                if (chunkState[cx][cy] == 0) continue;

                chunkState[cx][cy] = 0;

                int startY = (cy + 1) * CHUNK_SIZE - 1;
                if (startY >= gridHeight) startY = gridHeight - 1;
                int endY   = cy * CHUNK_SIZE;

                int startX = cx * CHUNK_SIZE;
                int endX   = (cx + 1) * CHUNK_SIZE;
                if (endX > gridWidth) endX = gridWidth;

                for (int y = startY; y >= endY; y--) {
                    int scanDir = (GetRandomValue(0, 1) == 0) ? 1 : -1;

                    for (int x = startX; x < endX; x++) {
                        Grain p = grid[x][y];
                        if (p.id == ID_EMPTY || (p.flags & FLAG_STATIC) || p.updated) continue;

                        int dir = scanDir;

                        if (p.id == ID_FIRE) {
                            bool extinguished = false;
                            for (int ox = -1; ox <= 1; ox++) {
                                for (int oy = -1; oy <= 1; oy++) {
                                    int nx = x + ox;
                                    int ny = y + oy;
                                    if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;

                                    if (grid[nx][ny].id == ID_WATER) {
                                        grid[nx][ny] = (Grain){
                                            ID_GAS,
                                            FLAG_BUOYANT | FLAG_CONSUMABLE,
                                            true,
                                            (Color){200, 200, 240, 150}
                                        };
                                        grid[x][y] = (Grain){0};
                                        extinguished = true;
                                        ForceWake(nx, ny);
                                        ForceWake(x, y);
                                        break;
                                    } else if (grid[nx][ny].id == ID_OIL) {
                                        int radius = 5;
                                        for (int ex = -radius; ex <= radius; ex++) {
                                            for (int ey = -radius; ey <= radius; ey++) {
                                                int bx = nx + ex;
                                                int by = ny + ey;
                                                if (bx < 0 || bx >= gridWidth || by < 0 || by >= gridHeight) continue;

                                                if (grid[bx][by].id != ID_WALL) {
                                                    if (GetRandomValue(0, 2) == 0) {
                                                        grid[bx][by] = (Grain){
                                                            ID_FIRE,
                                                            FLAG_BUOYANT,
                                                            true,
                                                            (Color){255, GetRandomValue(60, 180), 20, 255}
                                                        };
                                                    } else {
                                                        grid[bx][by] = (Grain){
                                                            ID_GAS,
                                                            FLAG_BUOYANT | FLAG_CONSUMABLE,
                                                            true,
                                                            (Color){100, 80, 110, 180}
                                                        };
                                                    }
                                                    ForceWake(bx, by);
                                                }
                                            }
                                        }
                                        extinguished = true;
                                        break;
                                    } else if (grid[nx][ny].flags & FLAG_CONSUMABLE) {
                                        grid[nx][ny] = (Grain){
                                            ID_FIRE,
                                            FLAG_BUOYANT,
                                            true,
                                            (Color){255, 120, 20, 255}
                                        };
                                        ForceWake(nx, ny);
                                    }
                                }
                                if (extinguished) break;
                            }
                            if (extinguished) continue;
                            if (GetRandomValue(0, 8) == 4) {
                                grid[x][y] = (Grain){0};
                                ForceWake(x, y);
                                continue;
                            }
                        }

                        if (p.id == ID_GAS && y <= 2) {
                            if (GetRandomValue(0, 5) == 2) {
                                grid[x][y] = (Grain){
                                    ID_WATER,
                                    FLAG_GRAVITY | FLAG_SLIDE | FLAG_LIQUID,
                                    true,
                                    (Color){40, 140, 240, 255}
                                };
                                ForceWake(x, y);
                                continue;
                            }
                        }

                        if (p.flags & FLAG_CORROSIVE) {
                            bool corroded = false;
                            for (int ox = -1; ox <= 1; ox++) {
                                for (int oy = -1; oy <= 1; oy++) {
                                    int nx = x + ox;
                                    int ny = y + oy;
                                    if (nx < 0 || nx >= gridWidth || ny < 0 || ny >= gridHeight) continue;

                                    if (grid[nx][ny].id == ID_WALL) {
                                        grid[nx][ny] = (Grain){0};
                                        corroded = true;
                                        ForceWake(nx, ny);
                                    }
                                }
                            }
                            if (corroded) {
                                grid[x][y] = (Grain){0};
                                ForceWake(x, y);
                                continue;
                            }
                        }

                        if (p.flags & FLAG_GRAVITY) {
                            if (y + 1 < gridHeight && grid[x][y + 1].id == ID_EMPTY) {
                                grid[x][y + 1] = p;
                                grid[x][y + 1].updated = true;
                                grid[x][y] = (Grain){0};
                                ForceWake(x, y);
                                ForceWake(x, y + 1);
                                continue;
                            }
                        }

                        if (p.flags & FLAG_SLIDE) {
                            if (y + 1 < gridHeight) {
                                if (x + dir >= 0 && x + dir < gridWidth &&
                                    grid[x + dir][y + 1].id == ID_EMPTY) {
                                    grid[x + dir][y + 1] = p;
                                    grid[x + dir][y + 1].updated = true;
                                    grid[x][y] = (Grain){0};
                                    ForceWake(x, y);
                                    ForceWake(x + dir, y + 1);
                                    continue;
                                }
                                if (x - dir >= 0 && x - dir < gridWidth &&
                                    grid[x - dir][y + 1].id == ID_EMPTY) {
                                    grid[x - dir][y + 1] = p;
                                    grid[x - dir][y + 1].updated = true;
                                    grid[x][y] = (Grain){0};
                                    ForceWake(x, y);
                                    ForceWake(x - dir, y + 1);
                                    continue;
                                }
                            }
                        }

                        if (p.flags & FLAG_LIQUID) {
                            if (x + dir >= 0 && x + dir < gridWidth &&
                                grid[x + dir][y].id == ID_EMPTY) {
                                grid[x + dir][y] = p;
                                grid[x + dir][y].updated = true;
                                grid[x][y] = (Grain){0};
                                ForceWake(x, y);
                                ForceWake(x + dir, y);
                                continue;
                            }
                            if (x - dir >= 0 && x - dir < gridWidth &&
                                grid[x - dir][y].id == ID_EMPTY) {
                                grid[x - dir][y] = p;
                                grid[x - dir][y].updated = true;
                                grid[x][y] = (Grain){0};
                                ForceWake(x, y);
                                ForceWake(x - dir, y);
                                continue;
                            }
                        }

                        if (p.flags & FLAG_BUOYANT) {
                            if (y - 1 >= 0 && grid[x][y - 1].id == ID_EMPTY) {
                                grid[x][y - 1] = p;
                                grid[x][y - 1].updated = true;
                                grid[x][y] = (Grain){0};
                                ForceWake(x, y);
                                ForceWake(x, y - 1);
                                continue;
                            }
                            if (y - 1 >= 0 &&
                                x + dir >= 0 && x + dir < gridWidth &&
                                grid[x + dir][y - 1].id == ID_EMPTY) {
                                grid[x + dir][y - 1] = p;
                                grid[x + dir][y - 1].updated = true;
                                grid[x][y] = (Grain){0};
                                ForceWake(x, y);
                                ForceWake(x + dir, y - 1);
                                continue;
                            }
                        }
                    }
                }
            }
        }

        for (int cy = 0; cy < chunksY; cy++)
            for (int cx = 0; cx < chunksX; cx++)
                if (chunkState[cx][cy] == 2) chunkState[cx][cy] = 1;

        
        Color bg = (Color){8, 8, 12, 255};
        for (int y = 0; y < gridHeight; y++) {
            for (int x = 0; x < gridWidth; x++) {
                int idx = y * MAX_GRID_WIDTH + x;
                pixelBuffer[idx] =
                    (grid[x][y].id != ID_EMPTY)
                        ? grid[x][y].color
                        : (chunkState[x / CHUNK_SIZE][y / CHUNK_SIZE]
                            ? (Color){12, 16, 28, 255}
                            : bg);
            }
        }

        UpdateTexture(tex, pixelBuffer);

        BeginDrawing();
        ClearBackground(bg);

        DrawTexturePro(
            tex,
            (Rectangle){0, 0, (float)gridWidth, (float)gridHeight},
            (Rectangle){0, 0, (float)(gridWidth * CELL_SIZE), (float)(gridHeight * CELL_SIZE)},
            (Vector2){0, 0},
            0.0f,
            WHITE
        );

        DrawRectangle(10, 10, 360, 120, ColorAlpha(BLACK, 0.50f));
        DrawRectangleLines(10, 10, 360, 120, DARKGRAY);

        DrawText(
            TextFormat("TOOL ACTIVE: %s | FPS: %d | GRID: %dx%d",
                eraserMode ? "ERASER" :
                (activeTool == ID_SAND  ? "SAND"  :
                 activeTool == ID_WATER ? "WATER" :
                 activeTool == ID_GAS   ? "GAS"   :
                 activeTool == ID_FIRE  ? "FIRE"  :
                 activeTool == ID_ACID  ? "ACID"  : "OIL"),
                GetFPS(), gridWidth, gridHeight),
            20, 20, 11, GREEN
        );

        DrawText("Keys 1-6: Materials", 20, 40, 11, RAYWHITE);
        DrawText("Key 7: Eraser. C: clear", 20, 55, 11, RED);
        DrawText("Right-Click: Paint Walls", 20, 70, 11, SKYBLUE);
        DrawText("S: Save + PNG snapshot", 20, 92, 11, GOLD);
        DrawText("L: Load last state", 20, 107, 11, GOLD);

        EndDrawing();
    }
    SaveSandboxState("sandbox\\sandbox.sav");
    UnloadTexture(tex);
    CloseWindow();
    return 0;
}