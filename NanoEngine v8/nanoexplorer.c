#include "raylib.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>

#define MAX_FILES 200
#define GRID_START_X 220
#define GRID_START_Y 60
#define COL_WIDTH 170
#define ROW_HEIGHT 200

typedef struct {
    char name[256];
    char path[512];
    Rectangle bounds;
    Texture2D preview;
} FileCard;

FileCard files[MAX_FILES];
int fileCount = 0;
float scrollY = 0.0f;

void ScanAndOrganize() {
    fileCount = 0;
    struct dirent *entry;
    DIR *dp = opendir("your_images");
    if (dp == NULL) return;
    
    while ((entry = readdir(dp)) != NULL && fileCount < MAX_FILES) {
        if (strstr(entry->d_name, ".png") ||
            strstr(entry->d_name, ".bmp") ||
            strstr(entry->d_name, ".jpeg")||
            strstr(entry->d_name, ".jpg") ) {
            strcpy(files[fileCount].name, entry->d_name);
            sprintf(files[fileCount].path, "your_images/%s", entry->d_name);
            
            // Layout Calculation
            int col = fileCount % 4;
            int row = fileCount / 4;
            files[fileCount].bounds = (Rectangle){ GRID_START_X + (col * COL_WIDTH) + 20, GRID_START_Y + (row * ROW_HEIGHT) + 20, 150, 150 };
            
            Image img = LoadImage(files[fileCount].path);
            ImageResize(&img, 150, 150);
            files[fileCount].preview = LoadTextureFromImage(img);
            UnloadImage(img);
            
            fileCount++;
        }
    }
    closedir(dp);
}

void ProcessAndLaunch(int index) {
    FILE *src = fopen(files[index].path, "rb");
    FILE *dst = fopen("net/for.png", "wb");
    if (src && dst) {
        char buffer[4096];
        size_t n;
        while ((n = fread(buffer, 1, sizeof(buffer), src)) > 0) fwrite(buffer, 1, n, dst);
    }
    if (src) fclose(src);
    if (dst) fclose(dst);
    system("start nanopaint.exe");
}

int main() {
    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(1400, 700, "NanoExplorer");
    ScanAndOrganize();
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        // Handle Scrolling
        scrollY += GetMouseWheelMove() * 30.0f;
        if (scrollY > 0) scrollY = 0;

        // Handle Clicks
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            for (int i = 0; i < fileCount; i++) {
                Rectangle adjusted = { files[i].bounds.x, files[i].bounds.y + scrollY, 150, 150 };
                if (CheckCollisionPointRec(GetMousePosition(), adjusted)) {
                    ProcessAndLaunch(i);
                }
            }
        }
        
        BeginDrawing();
        ClearBackground((Color){40, 40, 40, 255}); // Dark Theme
        // Draw Sidebar
        DrawRectangle(0, 0, 200, GetScreenHeight(), (Color){30, 30, 30, 255});
        DrawText("NanoExplorer", 20, 20, 20, GRAY);
        
        // Draw Grid
        for (int i = 0; i < fileCount; i++) {
            float drawY = files[i].bounds.y + scrollY;
            if (drawY > -200 && drawY < 800) { // Simple Clipping
                DrawTexture(files[i].preview, files[i].bounds.x, drawY, WHITE);
                
                Rectangle card = { files[i].bounds.x, drawY, 150, 150 };
                DrawRectangleLinesEx(card, 2, (CheckCollisionPointRec(GetMousePosition(), card)) ? YELLOW : GRAY);
                DrawText(files[i].name, files[i].bounds.x, drawY + 155, 10, WHITE);
            }
        }
        DrawRectangle(25,50,100,30,BLACK);
        DrawRectangleLinesEx((Rectangle){25,50,100,30},2, (CheckCollisionPointRec(GetMousePosition(), (Rectangle){20,45,100,35})) ? YELLOW : GRAY);
        DrawText("+ New", 30,55,20, RAYWHITE);
        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(GetMousePosition(), (Rectangle){25,50,100,30})) {system("start start.bat");for(int i = 0; i < fileCount; i++) UnloadTexture(files[i].preview);CloseWindow();return 0;}
        EndDrawing();
    }
    
    for(int i = 0; i < fileCount; i++) UnloadTexture(files[i].preview);
    CloseWindow();
    return 0;
}