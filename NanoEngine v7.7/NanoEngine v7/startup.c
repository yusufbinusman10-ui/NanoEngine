#include "raylib.h"
#include <cstdlib>

int main(){
    InitWindow(500,200,"loading nano");
    SetTargetFPS(200);
    Texture2D loader = LoadTexture("loader\\load.png");
    int count = 100;
    int stop = 400;
    int get = 0;
    int run = 0;
    float speed = 2;
    int space = 10;
    while (!WindowShouldClose()){
        BeginDrawing();
        DrawText("nano loader...", 150,50,20, RAYWHITE);
        DrawRectangle(100,75,5,45, DARKGRAY);
        DrawRectangle(count - 0, 75, loader.width + 5, loader.height + 10, DARKGRAY);
        DrawTexture(loader, count,80, RAYWHITE);
        DrawText("super nano",200, 150, 10, RAYWHITE);
        if (count <= stop) {count+=speed;}
        if (count  >= stop){stop-=loader.width;count=100;get+=1;}
        if (get >= 8) {get=0;count=100;stop = 400;run+=1;}
        if (run >= 2) {get=0;count=0;stop=0;system("start nanoexplorer.exe");run=0;UnloadTexture(loader);CloseWindow();return 0;}
        EndDrawing();
    }
    UnloadTexture(loader);
    CloseWindow();
    return 0;
}