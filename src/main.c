#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tgmath.h>
#include <time.h>

#include "raylib.h"
#include "raymath.h"

#define UNGENERATED (-1)
#define UNREVEALED 0
#define REVEALED 1
#define FLAG 2
#define QUESTION_UNREVEALED 3
#define QUESTION_REVEALED 4
#define BOMB 5
#define BOMB_RED 6
#define BOMB_CROSS 7
#define NUM_TILE(num) (7 + num)

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

//----------------------------------------------------------------------------------
// Global Variables Definition
//----------------------------------------------------------------------------------
int screenWidth = 960;
int screenHeight = 600;

Font gameFont;
int hudFontSize = 56;
int hudFontSpacing = 6;

int buttonSelected;

Color BGGRAY = (Color){128,128,128,255};

char gameStage = 2; //-1-> lost, 0->not started, 1->Started, 2->menu 3->victory

Vector2 mousePos;

struct Vector2 hudPos = {40, 120};
struct Vector2 hudSize = {0, 0};
struct Vector2 startPos = {40, 120};
struct Vector2 gameSize = {0, 0};

int textureSize = 16;

struct Vector2 textureOrigin;
struct Vector2 tileLenVec;
int tileLen;

int textureCount = 16;

struct Vector2 textureRect = {16, 16 };
struct Rectangle sprites[16];

struct Grid {
    int h, w, len; //height, width, length
    char* tiles; //Tiles to be rendered
    char* map; //Map of numbers and bombs
    int* tilesScanned; //Array to help with tile scanning/revealing
    int* scanQue;
    int quePos;
    int bombCount, bombsFlagged, flagCount, tilesRevealed;
};

// minesweeper map
struct Grid grid;

Texture2D spriteSheet;

int textureRows = 2;
int textureColumns = 8;

int h = 12;
int w = 16;
int len;

//int bombCount = 40;

int surroundingTileAddresses[9];
char surroundingTiles[9];

int scanSize = 5;

//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
void UpdateDrawFrame(void);     // Update and Draw one frame
void InitMap(struct Grid* gp);
void ShuffleMap(struct Grid* gp, int tileRevealed);
void GenMap(struct Grid* gp);
int PixelToGrid(struct Grid* gp, Vector2 origin, Vector2 mousePos);
int TileInBounds(struct Grid* gp, int tile);
int XYInBounds(struct Grid* gp, int tileX, int tileY);
int GetSurroundingTiles(struct Grid* gp, int tile, int* surroundingTileAddresses, char* surroundingTiles);
int GetSurroundingBombCount(struct Grid* gp, int tile);
int RevealTile(struct Grid* gp, int gridPos);
void FlagTile(struct Grid* gp, int gridPos);
void RevealEmptyTiles(struct Grid* gp, int gridPos);
int DrawHud(Vector2 mousePos);
void LoseGame(struct Grid* gp, int gridPos);


//----------------------------------------------------------------------------------
// Main Entry Point
//----------------------------------------------------------------------------------
int main() {

    // Initialization
    //--------------------------------------------------------------------------------------
    InitWindow(screenWidth, screenHeight, "Minesweeper");
    spriteSheet = LoadTexture("../resources/spriteSheet.png");

    gameFont = gameFont = LoadFont("../resources/fonts/alpha_beta.png");

    //SetRandomSeed((unsigned int)time(NULL));
    srand((unsigned int)time(NULL)); //Comment out for predictable maps

    len = h*w;

    for (int i = 0; i < textureCount; i++) {
        sprites[i].x = ((int)(i % 8) * 16);
        sprites[i].y = ((int)(i / 8) * 16);
        sprites[i].height = textureSize;
        sprites[i].width = textureSize;
    }

    textureOrigin = (Vector2){textureSize / 2, textureSize / 2};
    tileLen = textureSize * 2;
    tileLenVec = (Vector2){tileLen, tileLen};

    gameSize.x = (int)(w * tileLen);
    gameSize.y = (int)(h * tileLen);

    startPos.x = 6 + (screenWidth - gameSize.x) / 2;

    hudPos.x = startPos.x - (12 + 200);
    hudPos.y = startPos.y-100;

    hudSize.x = gameSize.x + (9 + 200*2);
    hudSize.y = 68;

    char tiles[len];
    char map[len];
    int tilesScanned[scanSize*len];
    int scanQue[scanSize*len];

    grid.h = h;
    grid.w = w;
    grid.len = len;

    grid.tiles = &tiles[0];
    grid.map = &map[0];
    grid.tilesScanned = &tilesScanned[0];
    grid.scanQue = &scanQue[0];
    grid.quePos = 0;


    InitMap(&grid);



#if defined(PLATFORM_WEB)
    emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
#else
    SetTargetFPS(60);   // Set our game to run at 60 frames-per-second
    //--------------------------------------------------------------------------------------

    // Main game loop
    while (!WindowShouldClose())    // Detect window close button or ESC key
    {
        UpdateDrawFrame();
    }
#endif

    // De-Initialization
    //--------------------------------------------------------------------------------------
    CloseWindow();        // Close window and OpenGL context
    //--------------------------------------------------------------------------------------

    return 0;
}

void UpdateDrawFrame(void)
{
    // Update

    mousePos = GetMousePosition();

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        int gridPos = PixelToGrid(&grid, startPos, Vector2Add(mousePos, (Vector2){6, 6}));
        //printf("gridPos: %d\n", gridPos);

        if (gameStage == 0 && gridPos != -1) {
            gameStage = 1;
            ShuffleMap(&grid, gridPos);
            GenMap(&grid);
        }
        else if (gameStage == 1) {

        }

        if (gameStage == 1 || gameStage == 0) {
            int revealed = RevealTile(&grid, gridPos);
            if (revealed == 1 && grid.map[gridPos] == REVEALED) {
                RevealEmptyTiles(&grid, gridPos);
            } else if (revealed == -1) {
                LoseGame(&grid, gridPos);
            }
        }
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        int gridPos = PixelToGrid(&grid, startPos, Vector2Add(mousePos, (Vector2){6, 6}));
        FlagTile(&grid, gridPos);
    }

    if (grid.len - grid.bombCount == grid.tilesRevealed) {
        gameStage = 3;
    }

    // Draw
    //----------------------------------------------------------------------------------
    BeginDrawing();

        ClearBackground(RAYWHITE);

        //Draw hud and get cursor overlap
        buttonSelected = DrawHud(mousePos);

        if (gameStage != 2) { //Game

            //Rectangle around game field
            DrawRectangle((int)startPos.x - 14, (int)startPos.y - 14, (int)gameSize.x + 14, (int)gameSize.y + 14, LIGHTGRAY);

            // Bottom/Right line around game field
            DrawRectangle((int)startPos.x - 8, (int)startPos.y - 8, (int)gameSize.x + 2, (int)gameSize.y + 2, BGGRAY);

            //Render grid
            for (int i = 0; i < grid.len; i++) {
                DrawTexturePro(spriteSheet, sprites[grid.tiles[i]], (Rectangle){(i % grid.w) * tileLen + startPos.x, (int)(i / grid.w) * tileLen + startPos.y, tileLen, tileLen}, textureOrigin, 0, WHITE);
            }
        }
        else { //Menu

        }
    EndDrawing();
    //----------------------------------------------------------------------------------

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {

        switch (buttonSelected) {
            case 1: {
                gameStage = 2;
                break;
            }
            case 2: {
                gameStage = 0;
                InitMap(&grid);
                break;
            }
        }
    }
}

void InitMap(struct Grid* gp) {

    printf("InitMap ran\n");

    //bomb gen
    gp->bombCount = (int)(2.35 * sqrt(gp->len));
    //gp->bombCount = 5;


    grid.bombsFlagged = 0;
    grid.flagCount = 0;
    grid.tilesRevealed = 0;

    for (int i = 0; i < gp->len; i++) {
        gp->tiles[i] = UNREVEALED;
        gp->map[i] = REVEALED;
        gp->scanQue[i] = -1;
        gp->tilesScanned[i] = -1;

    }

    for (int i = 0; i < gp->bombCount; i++) {
        gp->map[i] = BOMB;
    }
}


void ShuffleMap(struct Grid* gp, int tileRevealed) {

    printf("ShuffleMap ran\n");

    int tileX = tileRevealed % gp->w;
    int tileY = tileRevealed / gp->w;


    int tr, tl, bl, br;
    tr = XYInBounds(gp, tileX+1, tileY+1);
    tl = XYInBounds(gp, tileX-1, tileY+1);
    bl = XYInBounds(gp, tileX-1, tileY-1);
    br = XYInBounds(gp, tileX+1, tileY-1);

    int edgeCount = tr + tl + bl + br;
    int o = 10; //offset
    if (edgeCount == 1) {
        o = 7;
    }
    if (edgeCount == 2) {
        o = 5;
    }

    for (int i = gp->len - o; i > 0; i--) {
        int r = rand() % i;
        char t = gp->map[r];
        gp->map[r] = gp->map[i];
        gp->map[i] = t;
    }

    int surrounding_tiles[9] = {tileRevealed, (tileRevealed - gp->w) - 1, (tileRevealed - gp->w),
        (tileRevealed - gp->w) + 1, tileRevealed - 1, tileRevealed + 1,
        (tileRevealed + gp->w) - 1, (tileRevealed + gp->w), (tileRevealed + gp->w) + 1};

    for (int i = 0; i < 9; i++) {
        if (TileInBounds(gp, surrounding_tiles[i]) == 1) {
            char t = gp->map[len - (i+1)];
            gp->map[len - (i+1)] = gp->map[surrounding_tiles[i]];
            gp->map[surrounding_tiles[i]] = t;
        }
    }
}

void GenMap(struct Grid* gp) {
    for (int i = 0; i < gp->len; i++) {
        if (gp->map[i] != BOMB) {
            int bombCount = GetSurroundingBombCount(gp, i);
            if (bombCount != 0) {
                gp->map[i] = NUM_TILE(bombCount);
            }
        } else {

        }
    }
}

int PixelToGrid(struct Grid* gp, Vector2 origin, Vector2 mousePos) {

    Vector2 pos = Vector2Subtract(mousePos, origin);

    if (pos.x < 0 || pos.y < 0) {
        return -1;
    }

    int gridX = pos.x / tileLen;
    int gridY = pos.y / tileLen;

    if (gridX >= gp->w || gridY >= gp->h) {
        return -1;
    }

    return gridX + gridY * gp->w;
}

int TileInBounds(struct Grid* gp, int tile) {
    /*
    int tileX = tile % gp->w;
    int tileY = tile / gp->w;

    return XYInBounds(gp, tileX, tileY);
    */
    if (tile >= 0 && tile < gp->len) {
        return 1;
    }
    return 0;
}

int XYInBounds(struct Grid* gp, int tileX, int tileY) {

    if (tileX >= 0 && tileY >= 0 && tileX < gp->w && tileY < gp->h) {
        return 1;
    }
    return 0;
}

int GetSurroundingTiles(struct Grid* gp, int tile, int* surroundingTileAddresses, char* surroundingTiles) {
    int tileX = tile % gp->w;
    int tileY = tile / gp->w;

    printf("fetched surr. tiles. pos x:%d, y:%d\n", tileX, tileY);
    int x, y;
    int pos;

    int bombCount = 0;

    for (int i = 0; i < 9; i++) {
        x = -1 + (i % 3);
        y = -1 + (i / 3);
        if (XYInBounds(gp, tileX + x, tileY + y) == 1) {
            //gp->tiles[tile + x + (y * gp->w)] = FLAG; // DEBUG
            pos = tile + x + (y * gp->w);
            *(surroundingTileAddresses++) = pos;
            *(surroundingTiles++) = gp->map[pos];
            if (gp->map[pos] == BOMB) {
                ++bombCount;
            }
        } else {
            *(surroundingTileAddresses++) = -1;
            *(surroundingTiles++) = -1;
        }
    }

    return bombCount;
}

int GetSurroundingBombCount(struct Grid* gp, int tile) {
    int tileX = tile % gp->w;
    int tileY = tile / gp->w;

    printf("pos x:%d, y:%d\n", tileX, tileY);
    int x, y;
    int pos;

    int bombCount = 0;

    for (int i = 0; i < 9; i++) {
        x = -1 + (i % 3);
        y = -1 + (i / 3);
        if (XYInBounds(gp, tileX + x, tileY + y) == 1) {
            pos = tile + x + (y * gp->w);
            if (gp->map[pos] == BOMB) {
                ++bombCount;
            }
        }
    }

    return bombCount;
}

int RevealTile(struct Grid* gp, int gridPos) {
    if (gp->tiles[gridPos] == UNREVEALED) {
        if (gp->map[gridPos] == BOMB) {
            return -1;
        } else {
            gp->tiles[gridPos] = gp->map[gridPos];
            gp->tilesRevealed += 1;
            return 1;
        }

    }
    return 0;
}

void FlagTile(struct Grid* gp, int gridPos) {
    if (gp->tiles[gridPos] == FLAG) {
        gp->tiles[gridPos] = UNREVEALED;
        gp->flagCount -= 1;
        if (gp->map[gridPos] == BOMB) {
            gp->bombsFlagged -= 1;
        }
    }
    else if (gp->tiles[gridPos] == UNREVEALED) {
        gp->tiles[gridPos] = FLAG;
        gp->flagCount += 1;
        if (gp->map[gridPos] == BOMB) {
            gp->bombsFlagged += 1;
        }
    }
}

//This function is so wierd. I give up
void RevealEmptyTiles(struct Grid* gp, int gridPos) {

    int adjacentBombCount;


    char loop = 10;


    gp->quePos = 0;
    gp->scanQue[0] = gridPos;
    gp->tilesScanned[gridPos] = gridPos;

    for (int i = 1; i < scanSize*gp->len && loop > 0;) {
        if (i > gp->quePos) {
            adjacentBombCount = GetSurroundingTiles(&grid, gp->scanQue[gp->quePos++], &surroundingTileAddresses[0], &surroundingTiles[0]);
            loop -= 1;
            if (adjacentBombCount == 0) {
                for (int j = 0; j < 9; j++) {
                    int scanPos = surroundingTileAddresses[j];
                    if (scanPos != -1 && gp->tilesScanned[scanPos] != gridPos) {
                        loop = 10;

                        gp->tilesScanned[scanPos] = gridPos;
                        gp->scanQue[i++] = scanPos;

                        //gp->tiles[scanPos] = gp->map[scanPos];
                        RevealTile(gp, scanPos);

                        printf("tile revealed. x: %d y: %d\n", scanPos % gp->w, scanPos / gp->w);
                    }
                }
            }
            printf("i: %d\n", i);
        } else {
            loop = 0;
        }
    }
}

int DrawHud(Vector2 mousePos) {
    //Hud bar
    DrawRectangle((int)hudPos.x, (int)hudPos.y, (int)hudSize.x, (int)hudSize.y, LIGHTGRAY);

    Vector2 textPos1 = {hudPos.x+hudFontSpacing*2, hudPos.y+6};

    Vector2 textLen1 = MeasureTextEx(gameFont, "MENU", hudFontSize, hudFontSpacing);

    Vector2 textPos2 = {hudPos.x + hudFontSize+hudFontSpacing + textLen1.x, hudPos.y+6};

    Vector2 textLen2 = MeasureTextEx(gameFont, "START", hudFontSize, hudFontSpacing);




    char minesLeft[20] = "Minesweeper";

    if (gameStage != 2)

    sprintf(minesLeft, "%d Mines Left", grid.bombCount - grid.flagCount);

    Vector2 textLen3 = MeasureTextEx(gameFont, minesLeft, hudFontSize, hudFontSpacing);

    Vector2 textPos3 = {hudPos.x+hudSize.x-(2*hudFontSpacing+textLen3.x), hudPos.y+6};

    char endText[32] = "";

    if (gameStage == -1) {
        strcpy(endText, "Boom! You Lose");
    } else if (gameStage == 3) {
        strcpy(endText, "Congrats! You win");
    }

    Vector2 textLen4 = MeasureTextEx(gameFont, endText, hudFontSize, hudFontSpacing);

    Vector2 textPos4 = {(screenWidth - textLen4.x) / 2, 520};

    Vector2 relativePos1 = Vector2Subtract(mousePos, textPos1);
    Vector2 relativePos2 = Vector2Subtract(mousePos, textPos2);
    int cursorPos = -1;
    if (relativePos1.x > 0 && relativePos1.y > 0 && relativePos1.x < textLen1.x && relativePos1.y < textLen1.y) {
        cursorPos = 1;
    }
    else if (relativePos2.x > 0 && relativePos2.y > 0 && relativePos2.x < textLen2.x && relativePos2.y < textLen2.y) {
        cursorPos = 2;
    }

    char secondText[6] = "";

    if (gameStage == 0 || gameStage == 2) {
        strcpy(secondText, "START");
    } else {
        strcpy(secondText, "RESET");
    }

    if (cursorPos == 1) {
        DrawTextEx(gameFont, "MENU", textPos1, hudFontSize, hudFontSpacing , DARKGRAY);
    } else {
        DrawTextEx(gameFont, "MENU", textPos1, hudFontSize, hudFontSpacing , BLACK);

    }

    if (cursorPos == 2) {
        DrawTextEx(gameFont, secondText, textPos2, hudFontSize, hudFontSpacing , DARKGRAY);
    } else {
        DrawTextEx(gameFont, secondText, textPos2, hudFontSize, hudFontSpacing , BLACK);
    }

    DrawTextEx(gameFont, minesLeft, textPos3, hudFontSize, hudFontSpacing , BLACK);

    DrawTextEx(gameFont, endText, textPos4, hudFontSize, hudFontSpacing , BLACK);

    Vector2 creditLen = MeasureTextEx(gameFont, "Eren Kural", 18, 2);

    Vector2 creditPos = {screenWidth - (creditLen.x + 8), screenHeight - (creditLen.y + 8)};

    //DrawText("Eren Kural", screenWidth - 100, screenHeight - 24, 18, BLACK);
    DrawTextEx(gameFont, "Eren Kural", creditPos, 18, 2, BLACK);

    return cursorPos;
}
void LoseGame(struct Grid* gp, int gridPos) {
    gameStage = -1;
    for (int i = 0; i < gp->len; i++) {
        if (gp->map[i] == BOMB && gp->tiles[i] != FLAG) {
            gp->tiles[i] = BOMB;
        } else if (gp->map[i] != BOMB && gp->tiles[i] == FLAG) {
            gp->tiles[i] = BOMB_CROSS;
        }
    }
    gp->tiles[gridPos] = BOMB_RED;
}