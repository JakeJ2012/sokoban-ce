#include <tice.h>
#include <graphx.h>
#include <keypadc.h>
#include <debug.h>
#include <fileioc.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <sys/power.h>
#include "gfx/gfx.h"

#define MAX_LEVEL (20)
#define LEVEL_W (19)
#define LEVEL_H (16)
#define LEVEL_SIZE (LEVEL_W * LEVEL_H)
#define MAX_BOX_CT (30)

#define MIN(a, b) ({         \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a < _b ? _a : _b;      \
})

#define MAX(a, b) ({         \
    __typeof__(a) _a = (a); \
    __typeof__(b) _b = (b); \
    _a > _b ? _a : _b;      \
})

typedef enum screen
{
    SCREEN_TITLE = 1,
    SCREEN_HELP = 2,
    SCREEN_LEVEL_SELECT = 3,
    SCREEN_GAME = 4,
    SCREEN_DEBUG = 5,
    SCREEN_LEVEL_COMPLETE = 6
} screen;

typedef enum
{
    UP,
    RIGHT,
    DOWN,
    LEFT
} direction;

typedef struct rawLevelData
{
    int levelNum;
    char levelData[LEVEL_SIZE];
} rawLevelData;

typedef struct boxPos
{
    int x;
    int y;
} boxPos;

typedef struct levelInstance
{
    int levelNum;
    char staticLevelData[LEVEL_SIZE];
    boxPos boxes[MAX_BOX_CT];
    int boxCount;
    int playerX;
    int playerY;
    int viewportX;
    int viewportY;
    int isPaused;
    int pauseSelection;

} levelInstance;

typedef struct state
{
    int screen;
    int lastScreen;
    int titleSelected;
    int running;
    int levelSelected;
    int frame;
    int dbgByteOffset;
    int lastLoadError;
    int isTeacherMode;
    int teacherModeTick;
    rawLevelData *currentLevelRawData;
    levelInstance *currentLevelParsedData;
} state;

// core functions
void tickTitle(uint8_t keys[8], uint8_t lastKeys[8], state *gameState);
void tickHelpScreen(uint8_t keys[8], uint8_t lastKeys[8], state *gameState);
void tickLevelSelect(uint8_t keys[8], uint8_t lastKeys[8], state *gameState);
void tickGameLoop(uint8_t keys[8], uint8_t lastKeys[8], state *gameState);
void tickDebug(uint8_t keys[8], uint8_t lastKeys[8], state *gameState);
void tickTeacherMode(uint8_t keys[8], uint8_t lastKeys[8], state *gameState);
void tickMainGameLoop(uint8_t keys[8], uint8_t lastKeys[8], state *gameState);
void tickLevelComplete(uint8_t keys[8], uint8_t lastKeys[8], state *gameState);

// less important

int loadLevel(int level, rawLevelData *out);
void parseLevel(rawLevelData *raw, levelInstance *out);

int isPressed(uint8_t keys[8], int index, int mask);
int isNewPress(uint8_t keys[8], uint8_t lastKeys[8], int index, int mask);
int isHeldPress(uint8_t keys[8], uint8_t lastKeys[8], int index, int mask);
void binToStr(unsigned char value, char dest[9]);
void gameProcessing(uint8_t keys[8], uint8_t lastKeys[8], state *gameState);
void gameRendering(state *gameState);
int playerCanMove(levelInstance *curr, int dir);
bool boxCanMove(levelInstance *curr, int i, int dir);
void pushBox(levelInstance *curr, int i, int dir);
bool checkIsSolved(levelInstance *curr);
void getShifts(int *outX, int *outY, int dir);
void fillScreenWithTile(gfx_sprite_t * tileType);

/* loadLevel return codes (>0 success, <0 errors) */

typedef enum loadErrors
{
    LOAD_OK = 1,
    LOAD_ERR_NO_HANDLE = -1,
    LOAD_ERR_INVALID_LEVEL = -2,
    LOAD_ERR_BAD_SIZE = -3,
    LOAD_ERR_READ = -4
} loadErrors;

int main(void)
{
    uint8_t keys[8] = {0};
    uint8_t lastKeys[8] = {0};

    gfx_Begin();
    gfx_SetDrawBuffer();
    gfx_SetPalette(global_palette, sizeof_global_palette, 0);

    /*for (int i = 0; i < 256; i++) {
        uint16_t color = gfx_palette[i];

        dbg_sprintf(dbgout, "Palette[%d]: 0x%04X\n", i, color);
    }*/

    state gameState;

    gameState.screen = SCREEN_TITLE;
    gameState.lastScreen = SCREEN_TITLE;
    gameState.titleSelected = 0;
    gameState.levelSelected = 1;
    gameState.running = 1;
    gameState.frame = 0;
    gameState.dbgByteOffset = 0;
    gameState.lastLoadError = 0;
    gameState.isTeacherMode = 0;

    rawLevelData currentLoadedLevel;
    levelInstance level;

    gameState.currentLevelParsedData = &level;
    gameState.currentLevelRawData = &currentLoadedLevel;
    level.isPaused = 0;
    level.pauseSelection = 0;
    currentLoadedLevel.levelNum = -1;
    memset(currentLoadedLevel.levelData, 0, sizeof(currentLoadedLevel.levelData));

    while (gameState.running)
    {
        kb_Scan();
        for (int i = 0; i < 8; i++)
        {
            lastKeys[i] = keys[i];
        }

        for (int i = 0; i < 8; i++)
        {
            keys[i] = kb_Data[i];
        }

        int currentScreen = gameState.screen;

        if (!gameState.isTeacherMode)
        {
            switch (currentScreen)
            {
            case SCREEN_TITLE:
                tickTitle(keys, lastKeys, &gameState);
                break;
            case SCREEN_HELP:
                tickHelpScreen(keys, lastKeys, &gameState);
                break;
            case SCREEN_LEVEL_SELECT:
                tickLevelSelect(keys, lastKeys, &gameState);
                break;
            case SCREEN_GAME:
                tickGameLoop(keys, lastKeys, &gameState);
                break;
            case SCREEN_DEBUG:
                tickDebug(keys, lastKeys, &gameState);
                break;
            case SCREEN_LEVEL_COMPLETE:
                tickLevelComplete(keys, lastKeys, &gameState);
                break;
            }
        }
        else
        {
            tickTeacherMode(keys, lastKeys, &gameState);
        }

        if (isNewPress(keys, lastKeys, 4, kb_Cos))
        {
            gameState.isTeacherMode = 1;
        }

        gfx_SwapDraw();

        gameState.lastScreen = currentScreen;
        gameState.frame = (gameState.frame + 1) % 2;
    }

    gfx_End();
    return 0;
}

void tickTitle(uint8_t keys[8], uint8_t lastKeys[8], state *gameState)
{
    fillScreenWithTile(brick);

    gfx_ScaledTransparentSprite_NoClip(title, 40, 30, 2, 2);

    const void *buttons[3] = {
        play_button,
        help_button,
        exit_button,
    };
    const void *hoverButtons[3] = {
        play_button_hovered,
        help_button_hovered,
        exit_button_hovered,
    };

    for (int i = 0; i < 3; i++)
    {
        const void *sprite = (gameState->titleSelected == i) ? hoverButtons[i] : buttons[i];
        gfx_TransparentSprite(sprite, 140, 100 + 30 * i);
    }

    gameState->titleSelected -= isNewPress(keys, lastKeys, 7, kb_Up) - isNewPress(keys, lastKeys, 7, kb_Down);

    /*if (gameState->titleSelected < 0)
    {
        gameState->titleSelected = 2;
    }

    if (gameState->titleSelected > 2)
    {
        gameState->titleSelected = 0;
    }*/

    gameState->titleSelected = ((gameState->titleSelected % 3) + 3) % 3;

    if (isNewPress(keys, lastKeys, 6, kb_Clear))
    {
        gameState->running = 0;
    }

    if ((isNewPress(keys, lastKeys, 6, kb_Enter) || isNewPress(keys, lastKeys, 1, kb_2nd)) && gameState->titleSelected == 1)
    {
        gameState->screen = SCREEN_HELP;
    }

    if ((isNewPress(keys, lastKeys, 6, kb_Enter) || isNewPress(keys, lastKeys, 1, kb_2nd)) && gameState->titleSelected == 2)
    {
        gameState->running = 0;
    }

    if ((isNewPress(keys, lastKeys, 6, kb_Enter) || isNewPress(keys, lastKeys, 1, kb_2nd)) && gameState->titleSelected == 0)
    {
        gameState->screen = SCREEN_LEVEL_SELECT;
    }

    if (isNewPress(keys, lastKeys, 1, kb_Mode))
    {
        gameState->screen = SCREEN_DEBUG;
    }
}

void tickHelpScreen(uint8_t keys[8], uint8_t lastKeys[8], state *gameState)
{
    fillScreenWithTile(brick);


    gfx_SetTextBGColor(0);
    gfx_SetTextFGColor(2);
    gfx_SetTextTransparentColor(0);
    gfx_SetTextScale(2, 2);

    int w = gfx_GetStringWidth("Help");
    gfx_PrintStringXY("Help", 160-w/2, 30);



    gfx_PrintStringXY("How To Play", 10, 70);

    gfx_PrintStringXY("Controls", 10, 120);

    gfx_SetTextScale(1, 1);
    gfx_PrintStringXY("Push the boxes onto the goal tiles.", 10, 90);
    gfx_PrintStringXY("The goal tiles have a red dot.", 10, 100);
    gfx_PrintStringXY("You cannot pull boxes, you can only push.", 10, 110);

    gfx_PrintStringXY("Use arrows, 2nd, enter, and clear to navigate.", 10, 140);
    gfx_PrintStringXY("Use arrows to move player, and 2nd + arrows", 10, 150);
    gfx_PrintStringXY("to scroll the viewport.", 10, 160);
    gfx_PrintStringXY("Use mode to bring up pause menu,", 10, 170);
    gfx_PrintStringXY("where you will be able to", 10, 180);
    gfx_PrintStringXY("reset, exit, or resume.", 10, 190);
    gfx_PrintStringXY("Levels from:", 10, 200);
    gfx_PrintStringXY("https://github.com/davidjoffe/sokoban/", 10, 210);
    gfx_PrintStringXY("[cos] to enter teacher mode, clear exits.", 10, 220);

    if (isNewPress(keys, lastKeys, 6, kb_Clear))
    {
        gameState->screen = SCREEN_TITLE;
    }
}

void tickLevelSelect(uint8_t keys[8], uint8_t lastKeys[8], state *gameState)
{
    fillScreenWithTile(brick);


    int w = gfx_GetStringWidth("Level Select");

    gfx_SetTextBGColor(0);
    gfx_SetTextFGColor(2);
    gfx_SetTextTransparentColor(0);
    gfx_SetTextScale(2, 2);

    gfx_PrintStringXY("Level Select", 160 - w / 2, 30);

    char levelNum[3];
    snprintf(levelNum, sizeof(levelNum), "%d", gameState->levelSelected);

    w = gfx_GetStringWidth(levelNum);

    gfx_PrintStringXY(levelNum, 160 - w / 2, 100);

    gfx_TransparentSprite(up_arrow, 150, 85);

    gfx_TransparentSprite(down_arrow, 150, 120);

    if (isNewPress(keys, lastKeys, 6, kb_Clear))
    {
        gameState->levelSelected = 1;
        gameState->screen = SCREEN_TITLE;
    }

    if (isNewPress(keys, lastKeys, 6, kb_Enter) || isNewPress(keys, lastKeys, 1, kb_2nd))
    {
        gameState->currentLevelParsedData->isPaused = 0;
        gameState->currentLevelParsedData->pauseSelection = 0;
        gameState->screen = SCREEN_GAME;
    }

    if (isNewPress(keys, lastKeys, 7, kb_Down))
    {
        gameState->levelSelected--;
    }

    if (isNewPress(keys, lastKeys, 7, kb_Up))
    {
        gameState->levelSelected++;
    }

    gameState->levelSelected--; // make it 0-indexed temporarily

    gameState->levelSelected = ((gameState->levelSelected % MAX_LEVEL) + MAX_LEVEL) % MAX_LEVEL;

    gameState->levelSelected++; // fix it
}

void tickGameLoop(uint8_t keys[8], uint8_t lastKeys[8], state *gameState)
{


    if (gameState->lastScreen == SCREEN_LEVEL_SELECT)
    {
        int res = loadLevel(gameState->levelSelected, gameState->currentLevelRawData);
        gameState->lastLoadError = res;
        if (res == LOAD_OK)
        {
            parseLevel(gameState->currentLevelRawData, gameState->currentLevelParsedData);
        }
    }

    if (gameState->lastLoadError == LOAD_OK && gameState->currentLevelRawData->levelNum == gameState->levelSelected)
    {
        tickMainGameLoop(keys, lastKeys, gameState);
    }
    else
    {
        fillScreenWithTile(brick);

        gfx_SetTextFGColor(2);
        gfx_SetTextBGColor(0);
        gfx_SetTextTransparentColor(0);
        gfx_SetTextScale(1, 1);

        const char *msg;
        switch (gameState->lastLoadError)
        {
        case LOAD_ERR_NO_HANDLE:
            msg = "Level AppVar not found";
            break;
        case LOAD_ERR_INVALID_LEVEL:
            msg = "Invalid level number";
            break;
        case LOAD_ERR_BAD_SIZE:
            msg = "AppVar too small for level";
            break;
        case LOAD_ERR_READ:
            msg = "Error reading level data";
            break;
        default:
            msg = "Unknown load error";
            break;
        }
        int w = gfx_GetStringWidth(msg);
        gfx_PrintStringXY(msg, 160 - w / 2, 100);

        if (isNewPress(keys, lastKeys, 6, kb_Clear)) {
            gameState -> screen = SCREEN_LEVEL_SELECT;
        }
    }
}

void tickDebug(uint8_t keys[8], uint8_t lastKeys[8], state *gameState)
{

    gfx_FillScreen(1);
    gfx_SetTextScale(1, 1);
    gfx_SetTextTransparentColor(0);
    gfx_SetTextBGColor(0);
    gfx_SetTextFGColor(2);
    gfx_PrintStringXY("DEBUG: [MODE] to exit", 1, 1);
    // yes I know this runs every frame (sorry)
    uint8_t levelDataVar = ti_Open("SOKOLVLS", "r");
    if (levelDataVar)
    {
        gfx_PrintStringXY("Level data AppVar located. Preview:", 0, 10);

        char size[20];
        snprintf(size, sizeof(size), "Appvar Size: %d", ti_GetSize(levelDataVar));

        gfx_PrintStringXY(size, 0, 20);

        for (int i = 0; i < 15; i++)
        {
            ti_Seek(i + gameState->dbgByteOffset, SEEK_SET, levelDataVar);
            char byte = (char)ti_GetC(levelDataVar);
            char byteStr[2] = {byte, '\0'};
            gfx_PrintStringXY(byteStr, i * 20, 30);
            if (byte == '#')
            {
                gfx_Sprite(brick, i * 20, 50);
            }
            else if (byte == ' ')
            {
                gfx_Sprite(floor, i * 20, 50);
            }
            else if (byte == '.')
            {
                gfx_Sprite(goal, i * 20, 50);
            }
            else if (byte == '@')
            {
                gfx_Sprite(floor, i * 20, 50);
                gfx_TransparentSprite(player, i * 20, 70);
            }
            else if (byte == '+')
            {
                gfx_Sprite(goal, i * 20, 50);
                gfx_TransparentSprite(player, i * 20, 70);
            }
            else if (byte == '$')
            {
                gfx_Sprite(floor, i * 20, 50);
                gfx_Sprite(box, i * 20, 70);
            }
            else if (byte == '*')
            {
                gfx_Sprite(goal, i * 20, 50);
                gfx_Sprite(box, i * 20, 70);
            }
        }

        gameState->dbgByteOffset += (isNewPress(keys, lastKeys, 7, kb_Right) - isHeldPress(keys, lastKeys, 7, kb_Right)) && gameState->frame;

        gameState->dbgByteOffset += LEVEL_W * (isNewPress(keys, lastKeys, 7, kb_Right) - isHeldPress(keys, lastKeys, 7, kb_Right)) && gameState->frame;

        if (gameState->dbgByteOffset > ti_GetSize(levelDataVar) - 15)
        {
            gameState->dbgByteOffset = ti_GetSize(levelDataVar) - 15;
        }

        gameState->dbgByteOffset = MAX(gameState->dbgByteOffset, 0);

        ti_Close(levelDataVar);
    }
    else
    {
        gfx_PrintStringXY("Level data AppVar not found.", 0, 10);
    }

    char bin[9];
    for (int i = 0; i < 8; i++)
    {
        binToStr((unsigned char)keys[i], bin);

        gfx_PrintStringXY(bin, 0, 160 + 10 * i);
    }

    gfx_PrintStringXY("Palette:", 0, 100);
    for (int y = 0; y < 8; y++)
    {
        for (int x = 0; x < 32; x++)
        {
            gfx_SetColor(x + 32 * y);
            gfx_FillRectangle(x * 5, 115 + 5 * y, 5, 5);
        }
    }

    if (isNewPress(keys, lastKeys, 1, kb_Mode))
    {
        gameState->screen = SCREEN_TITLE;
    }
}

int isNewPress(uint8_t keys[8], uint8_t lastKeys[8], int index, int mask)
{
    return (keys[index] & mask) && !(lastKeys[index] & mask);
}

int isPressed(uint8_t keys[8], int index, int mask)
{
    return (keys[index] & mask);
}

int isHeldPress(uint8_t keys[8], uint8_t lastKeys[8], int index, int mask)
{
    return (keys[index] & mask) && (lastKeys[index] & mask);
}

void binToStr(unsigned char value, char dest[9])
{
    for (int i = 7; i >= 0; i--)
    {
        dest[7 - i] = (value & (1 << i)) ? '1' : '0';
    }
    dest[8] = '\0';
}

int loadLevel(int level, rawLevelData *out)
{
    if (!out)
        return LOAD_ERR_INVALID_LEVEL;

    uint8_t handle = ti_Open("SOKOLVLS", "r");
    if (!handle)
        return LOAD_ERR_NO_HANDLE;

    size_t fileSize = ti_GetSize(handle);
    if (level <= 0 || level > MAX_LEVEL)
    {
        ti_Close(handle);
        return LOAD_ERR_INVALID_LEVEL;
    }

    if (fileSize < (size_t)level * LEVEL_SIZE)
    {
        ti_Close(handle);
        return LOAD_ERR_BAD_SIZE;
    }

    ti_Seek((level - 1) * LEVEL_SIZE, SEEK_SET, handle);
    size_t read = ti_Read(out->levelData, 1, LEVEL_SIZE, handle);
    ti_Close(handle);

    if (read != LEVEL_SIZE)
    {
        return LOAD_ERR_READ;
    }

    out->levelNum = level;
    return LOAD_OK;
}

void parseLevel(rawLevelData *raw, levelInstance *out)
{
    memset(out, 0, sizeof(*out));

    out->levelNum = raw->levelNum;
    for (int x = 0; x < LEVEL_W; x++)
    {
        for (int y = 0; y < LEVEL_H; y++)
        {
            int i = y * LEVEL_W + x;
            switch (raw->levelData[i])
            {
            case ' ':
                out->staticLevelData[i] = ' ';
                break;
            case '#':
                out->staticLevelData[i] = '#';
                break;
            case '.':
                out->staticLevelData[i] = '.';
                break;
            case '@':
                out->staticLevelData[i] = ' ';
                out->playerX = x;
                out->playerY = y;
                break;
            case '+':
                out->staticLevelData[i] = '.';
                out->playerX = x;
                out->playerY = y;
                break;
            case '$':
                out->staticLevelData[i] = ' ';
                if (out->boxCount < MAX_BOX_CT)
                {
                    out->boxes[(out->boxCount)++] = (boxPos){.x = x, .y = y};
                }
                break;
            case '*':
                out->staticLevelData[i] = '.';
                if (out->boxCount < MAX_BOX_CT)
                {
                    out->boxes[(out->boxCount)++] = (boxPos){.x = x, .y = y};
                }
                break;
            }
        }
    }
}

void tickTeacherMode(uint8_t keys[8], uint8_t lastKeys[8], state *gameState)
{
    gfx_FillScreen(30);

    gameState->teacherModeTick = (gameState->teacherModeTick + 1) % 44;

    if (boot_BatteryCharging())
    {
        gfx_Sprite(home_screen_right_charging, 65, 0);
    }
    else
    {
        gfx_Sprite(home_screen_right, 65, 0);
    }

    if (gameState->teacherModeTick < 22)
    {
        gfx_Sprite(home_no_cursor, 0, 0);
    }
    else
    {
        gfx_Sprite(home_with_cursor, 0, 0);
    }

    if (isNewPress(keys, lastKeys, 6, kb_Clear))
    {
        gameState->isTeacherMode = 0;
    }
}

void tickMainGameLoop(uint8_t keys[8], uint8_t lastKeys[8], state *gameState)
{
    gameProcessing(keys, lastKeys, gameState);
    gameRendering(gameState);
}

void gameProcessing(uint8_t keys[8], uint8_t lastKeys[8], state *gameState)
{
    if (!gameState->currentLevelParsedData->isPaused)
    {
        gameState->currentLevelParsedData->viewportX += ((isPressed(keys, 7, kb_Right) && isPressed(keys, 1, kb_2nd)) - (isPressed(keys, 7, kb_Left) && isPressed(keys, 1, kb_2nd))) * gameState->frame;

        gameState->currentLevelParsedData->viewportY += ((isPressed(keys, 7, kb_Down) && isPressed(keys, 1, kb_2nd)) - (isPressed(keys, 7, kb_Up) && isPressed(keys, 1, kb_2nd))) * gameState->frame;

        gameState->currentLevelParsedData->viewportX = MAX(-1 * LEVEL_W / 2, MIN(gameState->currentLevelParsedData->viewportX, LEVEL_W / 2));

        gameState->currentLevelParsedData->viewportY = MAX(-1 * LEVEL_H / 2, MIN(gameState->currentLevelParsedData->viewportY, LEVEL_H / 2));

        bool is2nd = isPressed(keys, 1, kb_2nd);

        uint8_t dirKeys[4] = {kb_Up, kb_Right, kb_Down, kb_Left};

        for (int dir = UP; dir < 4; dir++)
        {
            if (!is2nd && isNewPress(keys, lastKeys, 7, dirKeys[dir]))
            {
                int moveCode = playerCanMove(gameState->currentLevelParsedData, dir);
                if (moveCode > 0)
                {
                    int xShift = 0;
                    int yShift = 0;

                    getShifts(&xShift, &yShift, dir);

                    if (moveCode == 1)
                    {
                        gameState->currentLevelParsedData->playerX += xShift;
                        gameState->currentLevelParsedData->playerY += yShift;
                    }
                    else if (moveCode >= 2)
                    {
                        int obstructingBoxIndex = moveCode - 2;
                        bool obstructingBoxCanMove = boxCanMove(gameState->currentLevelParsedData, obstructingBoxIndex, dir);
                        if (obstructingBoxCanMove)
                        {
                            pushBox(gameState->currentLevelParsedData, obstructingBoxIndex, dir);
                            gameState->currentLevelParsedData->playerX += xShift;
                            gameState->currentLevelParsedData->playerY += yShift;
                        }
                    }
                }
            }
        }

        bool isSolved = checkIsSolved(gameState->currentLevelParsedData);

        if (isSolved)
        {
            gameState->screen = SCREEN_LEVEL_COMPLETE;
        }
        else if (isNewPress(keys, lastKeys, 1, kb_Mode))
        {
            gameState->currentLevelParsedData->isPaused = 1;
        }
    }
    else
    {
        if (isNewPress(keys, lastKeys, 7, kb_Up))
        {
            gameState->currentLevelParsedData->pauseSelection--;
        }

        if (isNewPress(keys, lastKeys, 7, kb_Down))
        {
            gameState->currentLevelParsedData->pauseSelection++;
        }

        if (gameState->currentLevelParsedData->pauseSelection < 0)
        {
            gameState->currentLevelParsedData->pauseSelection = 2;
        }
        else if (gameState->currentLevelParsedData->pauseSelection > 2)
        {
            gameState->currentLevelParsedData->pauseSelection = 0;
        }

        if (isNewPress(keys, lastKeys, 1, kb_2nd) || isNewPress(keys, lastKeys, 6, kb_Enter))
        {
            switch (gameState->currentLevelParsedData->pauseSelection)
            {
            case 0:
                parseLevel(gameState->currentLevelRawData, gameState->currentLevelParsedData);
                break;
            case 1:
                gameState->screen = SCREEN_LEVEL_SELECT;
                break;
            case 2:
                gameState->currentLevelParsedData->isPaused = 0;
                gameState->currentLevelParsedData->pauseSelection = 0;
                break;
            default:
                break;
            }
        }
    }
}

void gameRendering(state *gameState)
{
    fillScreenWithTile(floor);


    if (!gameState->currentLevelParsedData->isPaused)
    {

        for (int x = 0; x < LEVEL_W; x++)
        {
            for (int y = 0; y < LEVEL_H; y++)
            {
                switch (gameState->currentLevelParsedData->staticLevelData[y * LEVEL_W + x])
                {
                case '#':
                    gfx_Sprite(brick, x * 20 - gameState->currentLevelParsedData->viewportX * 20, y * 20 - gameState->currentLevelParsedData->viewportY * 20);
                    break;
                case ' ':
                    gfx_Sprite(floor, x * 20 - gameState->currentLevelParsedData->viewportX * 20, y * 20 - gameState->currentLevelParsedData->viewportY * 20);
                    break;
                case '.':
                    gfx_Sprite(goal, x * 20 - gameState->currentLevelParsedData->viewportX * 20, y * 20 - gameState->currentLevelParsedData->viewportY * 20);
                    break;
                }
            }
        }

        for (int i = 0; i < gameState->currentLevelParsedData->boxCount; i++)
        {
            int x = gameState->currentLevelParsedData->boxes[i].x;
            int y = gameState->currentLevelParsedData->boxes[i].y;

            gfx_Sprite(box, x * 20 - gameState->currentLevelParsedData->viewportX * 20, y * 20 - gameState->currentLevelParsedData->viewportY * 20);
        }

        int x = gameState->currentLevelParsedData->playerX;
        int y = gameState->currentLevelParsedData->playerY;

        gfx_TransparentSprite(player, x * 20 - gameState->currentLevelParsedData->viewportX * 20, y * 20 - gameState->currentLevelParsedData->viewportY * 20);
    }
    else
    {

        gfx_SetColor(1);
        gfx_FillRectangle(130, 120 + 15 * gameState->currentLevelParsedData->pauseSelection, 60, 10);

        gfx_SetTextScale(2, 2);
        gfx_SetTextTransparentColor(0);
        gfx_SetTextBGColor(0);
        gfx_SetTextFGColor(2);

        char s[30];
        strlcpy(s, "Paused", 30);
        int w = gfx_GetStringWidth(s);
        gfx_PrintStringXY(s, 160 - w / 2, 80);

        gfx_SetTextScale(1, 1);

        strlcpy(s, "Reset", 30);
        w = gfx_GetStringWidth(s);
        gfx_PrintStringXY(s, 160 - w / 2, 120);

        strlcpy(s, "Exit", 30);
        w = gfx_GetStringWidth(s);
        gfx_PrintStringXY(s, 160 - w / 2, 135);

        strlcpy(s, "Resume", 30);
        w = gfx_GetStringWidth(s);
        gfx_PrintStringXY(s, 160 - w / 2, 150);
    }
}

void tickLevelComplete(uint8_t keys[8], uint8_t lastKeys[8], state *gameState)
{
    fillScreenWithTile(brick);


    gfx_SetTextScale(2, 2);
    gfx_SetTextFGColor(2);
    gfx_SetTextBGColor(0);
    gfx_SetTextTransparentColor(0);
    int w = gfx_GetStringWidth("Level Complete!");
    gfx_PrintStringXY("Level Complete!", GFX_LCD_WIDTH / 2 - w / 2, 100);

    if (isNewPress(keys, lastKeys, 6, kb_Clear) || isNewPress(keys, lastKeys, 1, kb_2nd) || isNewPress(keys, lastKeys, 1, kb_Enter))
    {
        gameState->screen = SCREEN_LEVEL_SELECT;
    }
}

int playerCanMove(levelInstance *curr, int dir)
{
    int playerX = curr->playerX;
    int playerY = curr->playerY;
    int xOffset;
    int yOffset;

    getShifts(&xOffset, &yOffset, dir);

    if (playerY + yOffset < 0 || playerY + yOffset >= LEVEL_H)
    {
        return 0;
    }

    if (playerX + xOffset < 0 || playerX + xOffset >= LEVEL_W)
    {
        return 0;
    }

    char at = curr->staticLevelData[(playerY + yOffset) * LEVEL_W + (playerX + xOffset)];

    bool boxBlocking = false;
    int j;

    for (j = 0; j < curr->boxCount; j++)
    {
        if (curr->boxes[j].x == playerX + xOffset && curr->boxes[j].y == playerY + yOffset)
        {
            boxBlocking = true;
            break;
        }
    }

    if (at == ' ' || at == '.')
    {
        if (boxBlocking)
        {
            return 2 + j;
        }
        return 1;
    }

    return 0;
}

bool boxCanMove(levelInstance *curr, int i, int dir)
{
    int boxX = curr->boxes[i].x;
    int boxY = curr->boxes[i].y;
    int xOffset;
    int yOffset;

    getShifts(&xOffset, &yOffset, dir);


    if (boxY + yOffset < 0 || boxY + yOffset >= LEVEL_H)
    {
        return 0;
    }

    if (boxX + xOffset < 0 || boxX + xOffset >= LEVEL_W)
    {
        return 0;
    }

    char at = curr->staticLevelData[(boxY + yOffset) * LEVEL_W + (boxX + xOffset)];

    bool boxBlocking = false;

    for (int j = 0; j < curr->boxCount; j++)
    {
        if (j != i && curr->boxes[j].x == boxX + xOffset && curr->boxes[j].y == boxY + yOffset)
        {
            boxBlocking = true;
            break;
        }
    }

    if ((at == ' ' || at == '.') && !boxBlocking)
    {
        return 1;
    }

    return 0;
}

void pushBox(levelInstance *curr, int i, int dir)
{
    int xOffset;
    int yOffset;

    getShifts(&xOffset, &yOffset, dir);

    curr->boxes[i].x += xOffset;
    curr->boxes[i].y += yOffset;
}

bool checkIsSolved(levelInstance *curr)
{
    for (int i = 0; i < curr->boxCount; i++)
    {
        if (curr->staticLevelData[curr->boxes[i].y * LEVEL_W + curr->boxes[i].x] != '.')
        {
            return false;
        }
    }

    return true;
}

void getShifts(int *outX, int *outY, int dir)
{
    switch (dir)
    {
    case UP:
        *outX = 0;
        *outY = -1;
        break;
    case RIGHT:
        *outX = 1;
        *outY = 0;
        break;
    case DOWN:
        *outX = 0;
        *outY = 1;
        break;
    case LEFT:
        *outX = -1;
        *outY = 0;
        break;
    default:
        *outX = 0;
        *outY = 0;
    }
} 

void fillScreenWithTile(gfx_sprite_t * tileType) {
    gfx_FillScreen(1);
    for (int x = 0; x < LCD_WIDTH/20; x++)
    {
        for (int y = 0; y < LCD_HEIGHT/20; y++)
        {
            gfx_Sprite(tileType, x * 20, y * 20);
        }
    }
}
