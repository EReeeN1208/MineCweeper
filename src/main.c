#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <tgmath.h>
#include <time.h>

#include <libgen.h>
#include <limits.h>
#include <unistd.h>

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
int screenHeight = 720;

Font gameFont;
int hudFontSize = 56;
int hudFontSpacing = 6;

int sp = 10; //selection padding

int buttonSelected;

Color BGGRAY = (Color){128,128,128,255};

char gameStage = 2; //-1-> lost, 0->not started, 1->Started, 2->menu 3->victory
char difficulty = 1; // 0 -> easy, 1 -> medium, 2 -> hard, 3 -> custom

Vector2 mousePos;

//struct Vector2 hudPos = {40, 120};
//struct Vector2 hudSize = {0, 0};
Vector2 startPos = {40, 120};
Vector2 gameSize = {0, 0};

int textureSize = 16;

Vector2 textureOrigin;
Vector2 tileLenVec;
int tileLen;

int textureCount = 16;

Vector2 textureRect = {16, 16 };
Rectangle sprites[16];

struct Grid {
    int h, w, len; //height, width, length
    char* tiles; //Tiles to be rendered
    char* map; //Map of numbers and bombs
    int* tilesScanned; //Array to help with tile scanning/revealing
    int* scanQue;
    int quePos;
    int bombCount, bombsFlagged, flagCount, tilesRevealed;
};

struct Setting {
    int h, w, bombCount;
    char difficulty;
};

struct Text {
    char* text;
    Color color;
    Vector2 size;
    Vector2 pos;
    int fontSize;
    int fontSpacing;
    Rectangle collisionBox;
};

struct Hud {
    Color hudColor;
    Vector2 hudSize;
    Vector2 hudPos;
    struct Text* texts;
};

struct Menu {
    Vector2 menuPos;
    struct Text* texts;
};

// minesweeper map/hud/menu init
struct Grid grid;
struct Hud hud;
struct Menu menu;

// Difficulty settings
struct Setting easy;
struct Setting medium;
struct Setting hard;

Texture2D spriteSheet;

int textureRows = 2;
int textureColumns = 8;

int h = 12;
int w = 16;
int len;
int maxLen;

//int bombCount = 40;

int surroundingTileAddresses[9];
char surroundingTiles[9];

int scanSize = 5;

//UI STUFF
Vector2 textLenEasy;
Vector2 textLenMedium;
Vector2 textLenHard;
Vector2 textLenCustom;

Vector2 textPosEasy;
Vector2 textPosMedium;
Vector2 textPosHard;
Vector2 textPosCustom;
//----------------------------------------------------------------------------------
// Module Functions Declaration
//----------------------------------------------------------------------------------
void UpdateDrawFrame(void);     // Update and Draw one frame
char* GetResourcePath(void);
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
void InitUI(struct Hud* hudp, struct Menu* menup);
int DrawUI(Vector2 mousePos, struct Hud* hudp, struct Menu* menup);
void RecalculateTextSize(struct Text* textp);
void RecalculateTextCollisionBox(struct Text* textp);
Rectangle RectangleFromVector2(Vector2* pos, Vector2* size);
void DrawTextFromStruct(struct Text* textp);
void DrawTextFromStructColor(struct Text* textp, Color color);
void LoseGame(struct Grid* gp, int gridPos);
void InitDifficulty(void);
char UpdateDifficulty(struct Grid* gp, struct Setting* setting);


//----------------------------------------------------------------------------------
// Main Entry Point
//----------------------------------------------------------------------------------
int main() {

    // Initialization
    //--------------------------------------------------------------------------------------
    InitWindow(screenWidth, screenHeight, "Minesweeper");

    spriteSheet = LoadTexture("resources/spriteSheet.png");

    gameFont = LoadFont("resources/fonts/alpha_beta.png");

    //SetRandomSeed((unsigned int)time(NULL));
    srand((unsigned int)time(NULL)); //Comment out for predictable/nonrandom maps

    InitDifficulty();

    len = h*w;

    maxLen = hard.h * hard.w;

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

    char tiles[maxLen];
    char map[maxLen];
    int tilesScanned[scanSize*maxLen];
    int scanQue[scanSize*maxLen];

    grid.tiles = &tiles[0];
    grid.map = &map[0];
    grid.tilesScanned = &tilesScanned[0];
    grid.scanQue = &scanQue[0];
    grid.quePos = 0;

    InitMap(&grid);

    difficulty = UpdateDifficulty(&grid, &medium);

    InitUI(&hud, &menu);


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


void UpdateDrawFrame(void) {
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

    if (grid.len - grid.bombCount < grid.tilesRevealed && grid.bombCount == grid.bombsFlagged) {
        gameStage = 3;
    }

    // Draw
    //----------------------------------------------------------------------------------
    BeginDrawing();

        ClearBackground(RAYWHITE);

        //Draw hud and get cursor overlap
        buttonSelected = DrawUI(mousePos, &hud, &menu);
        //buttonSelected = 0;

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
            case 3: {
                difficulty = UpdateDifficulty(&grid, &easy);
                break;
            }
            case 4: {
                difficulty = UpdateDifficulty(&grid, &medium);
                break;
            }
            case 5: {
                difficulty = UpdateDifficulty(&grid, &hard);
                break;
            }
        }
    }
}

// Initialise/Reset the map.
void InitMap(struct Grid* gp) {

    //printf("InitMap ran\n");

    //bomb gen
    //gp->bombCount = (int)(2.35 * sqrt(gp->len));
    //gp->bombCount = 5;

    grid.h = h;
    grid.w = w;
    grid.len = len;

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

// Shuffle the map after user clicks the first tile. The initial 3x3 tiles the user clicks are always 0.
void ShuffleMap(struct Grid* gp, int tileRevealed) {

    //printf("ShuffleMap ran\n");

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

// Fill the map array with number tiles according to the bombs.
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

// Convert a pixel on the map to the tile it corresponds to
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

// Check if a given tile index is within the len of the map.
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

// Check if given x and y are withing the grid.
int XYInBounds(struct Grid* gp, int tileX, int tileY) {

    if (tileX >= 0 && tileY >= 0 && tileX < gp->w && tileY < gp->h) {
        return 1;
    }
    return 0;
}

// Function to get address and values of surrounding tiles
int GetSurroundingTiles(struct Grid* gp, int tile, int* surroundingTileAddresses, char* surroundingTiles) {
    int tileX = tile % gp->w;
    int tileY = tile / gp->w;

    //printf("fetched surr. tiles. pos x:%d, y:%d\n", tileX, tileY);
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

// Function to return # of bombs surrounding tile
int GetSurroundingBombCount(struct Grid* gp, int tile) {
    int tileX = tile % gp->w;
    int tileY = tile / gp->w;

    //printf("pos x:%d, y:%d\n", tileX, tileY);
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

// Function to reveal a tile. Checks if new tile is bomb/not
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

// Function to flag/un-flag a tile
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

// Called when user reveals 0 tile to reveal all surrounding non-bomb tiles. Flood fill (?)
// This function is so wierd. I give up.
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

                        //printf("tile revealed. x: %d y: %d\n", scanPos % gp->w, scanPos / gp->w);
                    }
                }
            }
            printf("i: %d\n", i);
        } else {
            loop = 0;
        }
    }
}

// Called at the start to initialise ui.
void InitUI(struct Hud* hudp, struct Menu* menup) {

    printf("init ui start\n");

    hudp->hudPos = (Vector2){0, 0};
    hudp->hudSize = (Vector2){0, 0};

    menup->menuPos = (Vector2){0, 0};

    hudp->hudPos.x = 6 + (screenWidth - 16 * tileLen) / 2 - (12 + 200);
    hudp->hudPos.y = startPos.y-100;

    hudp->hudSize.x = 16 * tileLen + (9 + 200*2);
    hudp->hudSize.y = 68;


    menup->menuPos.x = hudp->hudPos.x;
    menup->menuPos.y = hudp->hudPos.y + 100;

    //Allocate Text Arrays
    hudp->texts = malloc(sizeof(struct Text) * 5);
    menup->texts = malloc(sizeof(struct Text) * 4);

    //Init Hud
    char hudText0[10];
    char hudText1[10];
    char hudText2[20];
    char hudText3[20];
    char hudText4[20];

    hudp->texts[0].text = malloc(sizeof(char)*strlen(hudText0));
    hudp->texts[1].text = malloc(sizeof(char)*strlen(hudText1));
    hudp->texts[2].text = malloc(sizeof(char)*strlen(hudText2));
    hudp->texts[3].text = malloc(sizeof(char)*strlen(hudText3));
    hudp->texts[4].text = malloc(sizeof(char)*strlen(hudText4));

    strcpy(hudp->texts[0].text, "MENU");
    strcpy(hudp->texts[1].text, "START");
    strcpy(hudp->texts[2].text, "Minesweeper");
    strcpy(hudp->texts[3].text, "");
    strcpy(hudp->texts[4].text, "Eren Kural");


    for (int i = 0; i < 4; i++) {
        hudp->texts[i].color = BLACK;
        hudp->texts[i].fontSize = hudFontSize;
        hudp->texts[i].fontSpacing = hudFontSpacing;
        hudp->texts[i].size = MeasureTextEx(gameFont, hudp->texts[i].text, hudp->texts[i].fontSize, hudp->texts[i].fontSpacing);
    }

    hudp->texts[0].pos = (Vector2){hudp->hudPos.x+hudFontSpacing*2, hudp->hudPos.y+6};

    hudp->texts[1].pos = (Vector2){hudp->hudPos.x + hudFontSize+hudFontSpacing + hudp->texts[0].size.x, hudp->hudPos.y+6};

    hudp->texts[2].pos = (Vector2){hudp->hudPos.x+hudp->hudSize.x-(2*hudFontSpacing+hudp->texts[2].size.x), hudp->hudPos.y+6};

    hudp->texts[3].pos = (Vector2){(screenWidth - hudp->texts[3].size.x) / 2, 640};

    hudp->texts[4].color = BLACK;
    hudp->texts[4].fontSize = 18;
    hudp->texts[4].fontSpacing = 2;
    hudp->texts[4].size = MeasureTextEx(gameFont, hudp->texts[4].text, hudp->texts[4].fontSize, hudp->texts[4].fontSpacing);
    hudp->texts[4].pos = (Vector2){screenWidth - (hudp->texts[4].size.x + 8), screenHeight - (hudp->texts[4].size.y + 8)};

    for (int i = 0; i < 5; i++) {
        hudp->texts[i].collisionBox = RectangleFromVector2(&hudp->texts[i].pos, &hudp->texts[i].size);
    }

    //Init Menu
    char menuText0[] = "Easy";
    char menuText1[] = "Medium";
    char menuText2[] = "Hard";
    char menuText3[] = "Custom - Coming Soon";

    menup->texts[0].text = malloc(sizeof(char)*strlen(menuText0));
    menup->texts[1].text = malloc(sizeof(char)*strlen(menuText1));
    menup->texts[2].text = malloc(sizeof(char)*strlen(menuText2));
    menup->texts[3].text = malloc(sizeof(char)*strlen(menuText3));

    strcpy(menup->texts[0].text, menuText0);
    strcpy(menup->texts[1].text, menuText1);
    strcpy(menup->texts[2].text, menuText2);
    strcpy(menup->texts[3].text, menuText3);

    for (int i = 0; i < 4; i++) {
        menup->texts[i].color = BLACK;
        menup->texts[i].fontSize = hudFontSize;
        menup->texts[i].fontSpacing = hudFontSpacing;
        menup->texts[i].size = MeasureTextEx(gameFont, menup->texts[i].text, menup->texts[i].fontSize, menup->texts[i].fontSpacing);
        menup->texts[i].pos = (Vector2){(screenWidth - menup->texts[i].size.x) / 2, menup->menuPos.y+80*(i+1)};
        menup->texts[i].collisionBox = RectangleFromVector2(&menup->texts[i].pos, &menup->texts[i].size);
    }
}


//Function to draw UI. Executed during drawing.
int DrawUI(Vector2 mousePos, struct Hud* hudp, struct Menu* menup) {

    DrawRectangle((int)hudp->hudPos.x, (int)hudp->hudPos.y, (int)hudp->hudSize.x, (int)hudp->hudSize.y, LIGHTGRAY);

    if (gameStage != 2) {
        sprintf(hudp->texts[2].text, "%d Mines Left", grid.bombCount - grid.flagCount);
    }

    RecalculateTextSize(&hudp->texts[2]);

    hudp->texts[2].pos = (Vector2){hudp->hudPos.x+hudp->hudSize.x-(2*hudFontSpacing+hudp->texts[2].size.x), hudp->hudPos.y+6};

    RecalculateTextCollisionBox(&hudp->texts[2]);

    if (gameStage == -1) {
        strcpy(hudp->texts[3].text, "Boom! You Lose");
    } else if (gameStage == 3) {
        strcpy(hudp->texts[3].text, "Congrats! You win");
    }

    if (gameStage == -1 || gameStage == 3) {
        RecalculateTextSize(&hudp->texts[3]);
        hudp->texts[3].pos = (Vector2){(screenWidth - hudp->texts[3].size.x) / 2, 640};
        // RecalculateTextCollisionBox(&hudp->texts[3]); // Not necessary
    }

    int cursorPos = -1;

    if (CheckCollisionPointRec(mousePos, hudp->texts[0].collisionBox)) {
        cursorPos = 1;
    }
    else if (CheckCollisionPointRec(mousePos, hudp->texts[1].collisionBox)) {
        cursorPos = 2;
    }

    if (gameStage == 2) {
        if (CheckCollisionPointRec(mousePos, menup->texts[0].collisionBox)) {
            cursorPos = 3;
        }
        else if (CheckCollisionPointRec(mousePos, menup->texts[1].collisionBox)) {
            cursorPos = 4;
        }
        else if (CheckCollisionPointRec(mousePos, menup->texts[2].collisionBox)) {
            cursorPos = 5;
        }
        else if (CheckCollisionPointRec(mousePos, menup->texts[3].collisionBox)) {
            cursorPos = 6;
        }
    }

    char randomStr[6];

    if (gameStage == 0 || gameStage == 2) {
        strcpy(randomStr, "START\0");
    } else {
        strcpy(randomStr, "RESET\0");
    }

    strcpy(hudp->texts[1].text, randomStr);


    if (gameStage == 2) {
        const Rectangle box = menup->texts[difficulty].collisionBox;
        DrawRectangle((int)box.x - sp, (int)box.y - sp, (int)box.width + 2*sp, (int)box.height + 2*sp, LIME);

        for (int i = 0; i < 4; i++) {
            if (i == (cursorPos - 3) && cursorPos != 6) {
                DrawTextFromStructColor(&menup->texts[i], DARKGRAY);
            } else {
                DrawTextFromStruct(&menup->texts[i]);
            }
        }
    }


    for (int i = 0; i < 2; i++) {
        if (i == (cursorPos - 1)) {
            DrawTextFromStructColor(&hudp->texts[i], DARKGRAY);
        } else {
            DrawTextFromStruct(&hudp->texts[i]);
        }
    }

    DrawTextFromStruct(&hudp->texts[2]);

    if (gameStage == -1 || gameStage == 3) {
        DrawTextFromStruct(&hudp->texts[3]);
    }

    DrawTextFromStruct(&hudp->texts[4]);

    return cursorPos;
}

// UI Helper functions
void RecalculateTextSize(struct Text* textp) {
    textp->size = MeasureTextEx(gameFont, textp->text, textp->fontSize, textp->fontSpacing);
}

void RecalculateTextCollisionBox(struct Text* textp) {
    textp->collisionBox = RectangleFromVector2(&textp->pos, &textp->size);
}

Rectangle RectangleFromVector2(Vector2* pos, Vector2* size) {
    return (Rectangle){pos->x, pos->y, size->x, size->y};
}

void DrawTextFromStruct(struct Text* textp) {
    DrawTextFromStructColor(textp, textp->color);
}

void DrawTextFromStructColor(struct Text* textp, Color color) {
    DrawTextEx(gameFont, textp->text, textp->pos, textp->fontSize, textp->fontSpacing, color);
}
//UI Helper functions end

// Executed when user reveals bomb
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

// Called at the start to initialise difficulty structs.
void InitDifficulty(void) {
    //set difficulties
    easy.h = 8;
    easy.w = 12;
    easy.bombCount = 20;
    easy.difficulty = 0;

    medium.h = 12;
    medium.w = 16;
    medium.bombCount = 36;
    medium.difficulty = 1;

    hard.h = 16;
    hard.w = 24;
    hard.bombCount = 80;
    hard.difficulty = 2;
}

// Called when difficulty is updated. Modifies grid accordingly.
char UpdateDifficulty(struct Grid* gp, struct Setting* setting) {
    h = gp->h = setting->h;
    w = gp->w = setting->w;
    gp->bombCount = setting->bombCount;

    len = h*w;
    gameSize.x = (int)(w * tileLen);
    gameSize.y = (int)(h * tileLen);

    startPos.x = 6 + (screenWidth - gameSize.x) / 2;

    grid.len = len;


    return setting->difficulty;
}