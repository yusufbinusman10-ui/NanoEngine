#include "raylib.h"
#include <cstdlib>
int main(){
    InitWindow(500,200,"loading nanoweb");
    int width = 0;
    int loader = 0;
    bool one = true;
    Color color = GREEN;
    while (!WindowShouldClose()){
        BeginDrawing();
        ClearBackground(BLACK);
        if (one){
        width+=1;
        }
        DrawText("nano loader...", 150,50,20, RAYWHITE);
        DrawRectangle(45,95,width+10,40, GRAY);
        DrawRectangle(50,100,width,30, color);
        if (width >= 400){
            for (int x = 0; x < 100;x++){BeginDrawing(), DrawRectangle(45,95,width+10,40, BLACK), EndDrawing();}
            width = 0;
            loader+=1;
        }
        if (loader >= 10){
            one = false;
            system("start web_start.bat");
            CloseWindow();
            return 0;
        }
        DrawText("super nano",200,150,10, SKYBLUE);
        EndDrawing();
    }
    CloseWindow();
    return 0;
}