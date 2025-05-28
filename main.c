// cbuild: -I./libs/raylib/include -L./libs/raylib/lib
// cbuild: -lraylib -lm

#include "raylib.h"

int main()
{
    InitWindow(1800, 1200, "GCC Build");

    while (!WindowShouldClose())
    {
        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("Hello, World!", 10, 10, 20, DARKGRAY);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
