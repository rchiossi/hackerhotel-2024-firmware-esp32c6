#include "screen_battleship.h"
#include "application.h"
#include "badge_messages.h"
#include "bsp.h"
#include "esp_err.h"
#include "esp_log.h"
#include "events.h"
#include "freertos/FreeRTOS.h"
#include "freertos/portmacro.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "nvs.h"
#include "pax_codecs.h"
#include "pax_gfx.h"
#include "screen_home.h"
#include "screen_repertoire.h"
#include "screens.h"
#include "textedit.h"
#include <inttypes.h>
#include <stdio.h>
#include <esp_random.h>
#include <string.h>

extern uint8_t const caronl_png_start[] asm("_binary_caronl_png_start");
extern uint8_t const caronl_png_end[] asm("_binary_caronl_png_end");
extern uint8_t const caronv_png_start[] asm("_binary_caronv_png_start");
extern uint8_t const caronv_png_end[] asm("_binary_caronv_png_end");
extern uint8_t const carond_png_start[] asm("_binary_carond_png_start");
extern uint8_t const carond_png_end[] asm("_binary_carond_png_end");

#define water         0
#define boat          1
#define missedshot    2
#define boathit       3
#define boatdestroyed 4

#define playing 0
#define victory 1
#define defeat  2

#define player 0
#define ennemy 1

#define west      0
#define northwest 1
#define northeast 2
#define east      3
#define southeast 4
#define southwest 5

#define invalid -1

static char const * TAG                = "testscreen";
static char const   forfeitprompt[128] = "Do you want to exit and declare forfeit";


screen_t screen_battleship_splash(QueueHandle_t application_event_queue, QueueHandle_t keyboard_event_queue);
screen_t screen_battleship_placeships(
    QueueHandle_t application_event_queue,
    QueueHandle_t keyboard_event_queue,
    int           playerboard[20],
    int           _position[20],
    int           playership[6]
);
screen_t screen_battleship_battle(
    QueueHandle_t application_event_queue,
    QueueHandle_t keyboard_event_queue,
    int           playerboard[20],
    int           ennemyboard[20],
    int           _position[20],
    int*          victoryflag,
    int           playership[6],
    int           ennemyship[6],
    int           bodge
);
screen_t screen_battleship_victory(
    QueueHandle_t application_event_queue, QueueHandle_t keyboard_event_queue, int* victoryflag
);

uint8_t PostoCha(uint8_t _nb) {
    if (_nb > 1)
        _nb++;  // skips the hiodden letters
    if (_nb > 8)
        _nb++;
    if (_nb > 15)
        _nb++;
    if (_nb > 19)
        _nb++;
    if (_nb > 22)
        _nb++;
    if (_nb > 24)
        _nb++;
    return _nb;
}

void AddShiptoBuffer(int _shiplenght, int _shiporientation, int _x, int _y) {
    pax_buf_t* gfx = bsp_get_gfx_buffer();

    // horizontal ship coordonates orientation east
    int of[13][2] = {
        {-8, -4},
        {-8, 5},
        {-7, -5},
        {-7, 6},
        {-6, -6},
        {5 + 16 * (_shiplenght - 1), -6},
        {-6, 7},
        {5 + 16 * (_shiplenght - 1), 7},
        {5 + 16 * (_shiplenght - 1), -6},
        {11 + 16 * (_shiplenght - 1), 0},
        {5 + 16 * (_shiplenght - 1), 7},
        {11 + 16 * (_shiplenght - 1), 1},
        {16, 0}
    };

    // diagonal ship coordonates orientation southeasty
    int od[10][2] = {
        {8, 15},
        {-6, 5},
        {-8, -1},
        {-7, -4},
        {0, -9},
        {4, -8},
        {6, -6},
        {9 + 8 * (_shiplenght - 1), 2 + 15 * (_shiplenght - 1)},
        {6 + 8 * (_shiplenght - 1), 11 + 15 * (_shiplenght - 1)},
        {-3 + 8 * (_shiplenght - 1), 8 + 15 * (_shiplenght - 1)}
    };

    switch (_shiporientation) {
        case west:
        case east:
            if (_shiporientation == west)  // flip on y axis if west
                for (int i = 0; i < 13; i++) of[i][0] = -of[i][0];
            for (int i = 0; i < _shiplenght; i++) AddBlocktoBuffer(_x + i * of[12][0], _y);
            _y--;

            pax_draw_line(gfx, BLACK, _x + of[0][0], _y + of[0][1], _x + of[1][0], _y + of[1][1]);
            pax_set_pixel(gfx, BLACK, _x + of[2][0] - 1, _y + of[2][1]);
            pax_set_pixel(gfx, BLACK, _x + of[3][0] - 1, _y + of[3][1]);

            pax_draw_line(gfx, BLACK, _x + of[4][0], _y + of[4][1], _x + of[5][0], _y + of[5][1]);
            pax_draw_line(gfx, BLACK, _x + of[6][0], _y + of[6][1], _x + of[7][0], _y + of[7][1]);

            pax_draw_line(gfx, BLACK, _x + of[8][0], _y + of[8][1], _x + of[9][0], _y + of[9][1]);
            pax_draw_line(gfx, BLACK, _x + of[10][0], _y + of[10][1], _x + of[11][0], _y + of[11][1]);
            break;

        default:
            if (_shiporientation == southwest || _shiporientation == northwest)  // flip on y axis if west
                for (int i = 0; i < 10; i++) od[i][0] = -od[i][0];
            if (_shiporientation == northeast || _shiporientation == northwest)  // flip on x axis if north
                for (int i = 0; i < 10; i++) od[i][1] = -od[i][1];
            for (int i = 0; i < _shiplenght; i++) AddBlocktoBuffer(_x + i * od[0][0], _y + i * od[0][1]);

            for (int i = 1; i < 9; i++)
                pax_draw_line(gfx, BLACK, _x + od[i][0], _y + od[i][1], _x + od[i + 1][0], _y + od[i + 1][1]);
            pax_draw_line(gfx, BLACK, _x + od[9][0], _y + od[9][1], _x + od[1][0], _y + od[1][1]);  // last line from 0
                                                                                                    // to 8
            break;
    }
}

void Display_battleship_placeships(
    int playerboard[20], int _shipplaced, int _flagstart, int _position[20], int playership[6]
);
void Display_battleship_battle(
    int  playerboard[20],
    int  ennemyboard[20],
    char _nickname[nicknamelenght],
    char _ennemyname[nicknamelenght],
    int  _turn,
    int  _position[20],
    int  playership[6],
    int  ennemyship[6]
);
void debugboardstatus(int board[20]) {
    ESP_LOGE(TAG, "      %d", board[0]);
    ESP_LOGE(TAG, "    %d - %d", board[1], board[2]);
    ESP_LOGE(TAG, "  %d - %d - %d", board[3], board[4], board[5]);
    ESP_LOGE(TAG, "%d - %d - %d - %d", board[6], board[7], board[8], board[9]);
    ESP_LOGE(TAG, "%d - %d - %d - %d", board[10], board[11], board[12], board[13]);
    ESP_LOGE(TAG, "  %d - %d - %d", board[14], board[15], board[16]);
    ESP_LOGE(TAG, "    %d - %d", board[17], board[18]);
    ESP_LOGE(TAG, "      %d", board[19]);
}
void debugshipstatus(int ships[6]) {
    ESP_LOGE(TAG, "Ship 1: %d", ships[0]);
    ESP_LOGE(TAG, "Ship 2: %d - %d", ships[1], ships[2]);
    ESP_LOGE(TAG, "Ship 3: %d - %d - %d", ships[3], ships[4], ships[5]);
}

int CalculateMidPositionLongShip(int _ships[6]) {
    if (log)
        ESP_LOGE(TAG, "Enter CalculateMidPositionLongShip");
    int validlines[24][4] = {{0, 1, 3, 6},     {2, 4, 7, -1},    {5, 8, -1, -1},   {-1, -1, -1, -1},  //
                             {0, 2, 5, 9},     {1, 4, 8, -1},    {3, 7, -1, -1},   {-1, -1, -1, -1},  //
                             {-1, -1, -1, -1}, {1, 2, -1, -1},   {3, 4, 5, -1},    {6, 7, 8, 9},      //
                             {-1, -1, -1, -1}, {11, 14, -1, -1}, {12, 15, 17, -1}, {13, 16, 18, 19},  //
                             {10, 14, 17, 19}, {11, 15, 18, -1}, {12, 16, -1, -1}, {-1, -1, -1, -1},  //
                             {10, 11, 12, 13}, {14, 15, 16, -1}, {17, 18, -1, -1}, {-1, -1, -1, -1}};

    int streakvalid = 0;

    for (int j = 0; j < 24; j++) {
        streakvalid = 0;
        for (int i = 0; i < 4; i++) {
            // check for the line that contains front and back of the ship
            if (validlines[j][i] == _ships[5] || validlines[j][i] == _ships[3]) {
                streakvalid++;
            }
            if (streakvalid == 2) {
                if (validlines[j][0] == _ships[5] || validlines[j][0] == _ships[3]) {
                    if (log)
                        ESP_LOGE(TAG, "ship 4 is : %d", validlines[j][1]);
                    return validlines[j][1];

                } else {
                    if (log)
                        ESP_LOGE(TAG, "ship 4 is : %d", validlines[j][2]);
                    return validlines[j][2];
                }
            }
        }
    }
    return -1;
}

int CalculateShipOrientation(int _ships[6], int _shiplenght) {
    int validlines[24][4] = {{0, 1, 3, 6},     {2, 4, 7, -1},    {5, 8, -1, -1},   {-1, -1, -1, -1},  //
                             {0, 2, 5, 9},     {1, 4, 8, -1},    {3, 7, -1, -1},   {-1, -1, -1, -1},  //
                             {-1, -1, -1, -1}, {1, 2, -1, -1},   {3, 4, 5, -1},    {6, 7, 8, 9},      //
                             {-1, -1, -1, -1}, {11, 14, -1, -1}, {12, 15, 17, -1}, {13, 16, 18, 19},  //
                             {10, 14, 17, 19}, {11, 15, 18, -1}, {12, 16, -1, -1}, {-1, -1, -1, -1},  //
                             {10, 11, 12, 13}, {14, 15, 16, -1}, {17, 18, -1, -1}, {-1, -1, -1, -1}};
    int streakvalid       = 0;
    switch (_shiplenght) {
        case 2:  // returns the orientation of the 2 long ship
            for (int j = 0; j < 24; j++) {
                streakvalid = 0;
                for (int i = 0; i < 4; i++) {
                    // check for the line that contains front and back of the ship
                    if (validlines[j][i] == _ships[1] || validlines[j][i] == _ships[2]) {
                        streakvalid++;
                    }
                    if (streakvalid == 2) {
                        int orientation;
                        switch (j) {
                            case 0:
                            case 1:
                            case 2:
                            case 3:
                            case 12:
                            case 13:
                            case 14:
                            case 15:
                                orientation = southwest;
                                ESP_LOGE(TAG, "OG orientation is southwest");
                                break;
                            case 4:
                            case 5:
                            case 6:
                            case 7:
                            case 16:
                            case 17:
                            case 18:
                            case 19:
                                orientation = southeast;
                                ESP_LOGE(TAG, "OG orientation is southeast");
                                break;
                            case 8:
                            case 9:
                            case 10:
                            case 11:
                            case 20:
                            case 21:
                            case 22:
                            case 23:
                                orientation = east;
                                ESP_LOGE(TAG, "OG orientation is east");
                                break;
                        }
                        for (int k = 0; k < 4; k++)  // check if the back or the front is first in the line
                        {
                            // if front rotate 180 degree
                            if (validlines[j][i] == _ships[1]) {
                                ESP_LOGE(TAG, "orienation for 2 long ship is : %d", ((orientation + 3) % 6));
                                return ((orientation + 3) % 6);
                            } else if (validlines[j][i] == _ships[2]) {
                                ESP_LOGE(TAG, "orienation for 2 long ship is : %d", orientation);
                                return orientation;
                            }
                        }
                    }
                }
            }
            break;

        case 3:  // returns the orientation of the 3 long ship
            for (int j = 0; j < 24; j++) {
                streakvalid = 0;
                for (int i = 0; i < 4; i++) {
                    // check for the line that contains front and back of the ship
                    if (validlines[j][i] == _ships[5] || validlines[j][i] == _ships[3]) {
                        streakvalid++;
                    }
                    if (streakvalid == 2) {
                        int orientation;
                        switch (j) {
                            case 0:
                            case 1:
                            case 2:
                            case 3:
                            case 12:
                            case 13:
                            case 14:
                            case 15:
                                orientation = southwest;
                                ESP_LOGE(TAG, "OG orientation is southwest");
                                break;
                            case 4:
                            case 5:
                            case 6:
                            case 7:
                            case 16:
                            case 17:
                            case 18:
                            case 19:
                                orientation = southeast;
                                ESP_LOGE(TAG, "OG orientation is southeast");
                                break;
                            case 8:
                            case 9:
                            case 10:
                            case 11:
                            case 20:
                            case 21:
                            case 22:
                            case 23:
                                orientation = east;
                                ESP_LOGE(TAG, "OG orientation is east");
                                break;
                        }
                        for (int k = 0; k < 4; k++)  // check if the back or the front is first in the line
                        {
                            // if front rotate 180 degree
                            if (validlines[j][i] == _ships[3]) {
                                ESP_LOGE(TAG, "orienation for 3 long ship is : %d", ((orientation + 3) % 6));
                                return ((orientation + 3) % 6);
                            } else if (validlines[j][i] == _ships[5]) {
                                ESP_LOGE(TAG, "orienation for 3 long ship is : %d", orientation);
                                return orientation;
                            }
                        }
                    }
                }
            }
            break;
        default: break;
    }
    return -1;
}

// either return the possible blocks for the front of the ship, or the back
// _FB for which part of the boat to place, if front (first part) _FB = 1, back is 0
// _blockfrontship show the block off the front of the ship, only relevant if _FB = 0
int CheckforShipPlacement(int _ships[6], int _shipplaced, int _position[20]) {

    for (int i = 0; i < 20; i++) _position[i] = water;                                                // reset position
    int validlines[24][4] = {{0, 1, 3, 6},     {2, 4, 7, -1},    {5, 8, -1, -1},   {-1, -1, -1, -1},  //
                             {0, 2, 5, 9},     {1, 4, 8, -1},    {3, 7, -1, -1},   {-1, -1, -1, -1},  //
                             {-1, -1, -1, -1}, {1, 2, -1, -1},   {3, 4, 5, -1},    {6, 7, 8, 9},      //
                             {-1, -1, -1, -1}, {11, 14, -1, -1}, {12, 15, 17, -1}, {13, 16, 18, 19},  //
                             {10, 14, 17, 19}, {11, 15, 18, -1}, {12, 16, -1, -1}, {-1, -1, -1, -1},  //
                             {10, 11, 12, 13}, {14, 15, 16, -1}, {17, 18, -1, -1}, {-1, -1, -1, -1}};

    int _blockfrontship = 0;
    int _FB             = 0;
    int _shipsize       = 0;
    int streakvalid     = 0;

    switch (_shipplaced) {
        case no_ship:
            _blockfrontship = 0;
            _FB             = 1;
            break;
        case small_ship:  // possible blocks for the front of the 2 long ship
            _blockfrontship = 0;
            _FB             = 1;
            _shipsize       = 2;
            break;
        case medium_front:  // possible blocks for the back of the 2 long ship
            _blockfrontship = _ships[1];
            _FB             = 0;
            _shipsize       = 2;
            break;
        case medium_whole:  // possible blocks for the front of the 3 long ship
            _blockfrontship = 0;
            _FB             = 1;
            _shipsize       = 3;
            break;
        case long_front:  // possible blocks for the back of the 2 long ship
            _blockfrontship = _ships[3];
            _FB             = 0;
            _shipsize       = 3;
            break;
        case 6: return -1;
        default: break;
    }

    // remove placed ships from possible locations
    for (int k = 0; k < 6; k++) {
        // skips the front of the 2 long ship
        if (_shipplaced == medium_front && k == 1)
            k++;
        // skips the front of the 3 long ship
        if (_shipplaced == long_front && k == 3)
            k++;

        for (int j = 0; j < 24; j++) {
            for (int i = 0; i < 4; i++) {
                if (validlines[j][i] == _ships[k])
                    validlines[j][i] = invalid;
            }
        }
    }


    // if placing the front of the boat (first part)
    if (_FB) {
        for (int j = 0; j < 24; j++) {
            streakvalid = 0;
            for (int i = 0; i < 4; i++) {
                if (validlines[j][i] != invalid) {
                    streakvalid++;
                    // if there is a valid ship position, set the first and last position of the streak as valid
                    if (streakvalid >= _shipsize) {
                        // ESP_LOGE(TAG, "validlines j: %d i: %d", j, i);
                        _position[validlines[j][i]]                 = missedshot;
                        _position[validlines[j][i + 1 - _shipsize]] = missedshot;
                    }
                } else
                    streakvalid = 0;
            }
        }
    }

    // if placing the back of the boat (second part)
    if (!_FB) {
        for (int j = 0; j < 24; j++) {
            for (int i = 0; i < 4; i++) {
                // if this is the block of the front of the ship
                if (validlines[j][i] == _blockfrontship) {
                    // check the block before
                    int blocktocheck = i + 1 - _shipsize;
                    if (blocktocheck >= 0 && blocktocheck < 4)  // check if the block is exist
                        if (validlines[j][blocktocheck] != -1)  // check if the block is valid
                        {
                            _position[validlines[j][blocktocheck]] = missedshot;
                            // ESP_LOGE(TAG, "validlines j: %d i: %d", j, i);
                            // check is the ship is 3 and block inbetween front and back is invalid, invalidate the
                            // result
                            if (_shipsize == 3 && validlines[j][i - 1] == -1) {
                                _position[validlines[j][blocktocheck]] = 0;
                                // ESP_LOGE(TAG, "nullified");
                            }
                        }

                    // check the block after
                    blocktocheck = i - 1 + _shipsize;
                    if (blocktocheck >= 0 && blocktocheck < 4)  // check if the block is exist
                        if (validlines[j][blocktocheck] != -1)  // check if the block is valid
                        {
                            _position[validlines[j][blocktocheck]] = missedshot;
                            // ESP_LOGE(TAG, "validlines j: %d i: %d", j, i);

                            // check is the ship is 3 and block inbetween front and back is invalid, invalidate the
                            // result
                            if (_shipsize == 3 && validlines[j][i + 1] == -1) {
                                _position[validlines[j][blocktocheck]] = 0;
                                // ESP_LOGE(TAG, "nullified");
                            }
                        }
                }
            }
        }
    }
    return -1;
}

void AIplaceShips(int ennemyboard[20], int _position[20], int ennemyship[6]) {
    ennemyship[0] = esp_random() % 20;
    int attemptblock;
    CheckforShipPlacement(ennemyship, small_ship, _position);
    do attemptblock = esp_random() % 20;
    while (_position[attemptblock] == 0);
    ennemyship[1] = attemptblock;
    CheckforShipPlacement(ennemyship, medium_front, _position);
    do attemptblock = esp_random() % 20;
    while (_position[attemptblock] == 0);
    ennemyship[2] = attemptblock;
    CheckforShipPlacement(ennemyship, medium_whole, _position);
    do attemptblock = esp_random() % 20;
    while (_position[attemptblock] == 0);
    ennemyship[3] = attemptblock;
    CheckforShipPlacement(ennemyship, long_front, _position);
    do attemptblock = esp_random() % 20;
    while (_position[attemptblock] == 0);
    ennemyship[5] = attemptblock;
    ennemyship[4] = CalculateMidPositionLongShip(ennemyship);
    for (int i = 0; i < 6; i++) ennemyboard[ennemyship[i]] = boat;
    ESP_LOGE(TAG, "ennemyboard:");

    debugboardstatus(ennemyboard);
    debugshipstatus(ennemyship);
}

void CheckforDestroyedShips(int playerboard[20], int ennemyboard[20], int playership[6], int ennemyship[6]) {
    if (playerboard[playership[0]] == boathit)
        playerboard[playership[0]] = boatdestroyed;
    if (playerboard[playership[1]] == boathit && playerboard[playership[2]] == boathit) {
        playerboard[playership[1]] = boatdestroyed;
        playerboard[playership[2]] = boatdestroyed;
    }
    if (playerboard[playership[3]] == boathit && playerboard[playership[4]] == boathit &&
        playerboard[playership[5]] == boathit) {
        playerboard[playership[3]] = boatdestroyed;
        playerboard[playership[4]] = boatdestroyed;
        playerboard[playership[5]] = boatdestroyed;
    }

    if (ennemyboard[ennemyship[0]] == boathit)
        ennemyboard[ennemyship[0]] = boatdestroyed;
    if (ennemyboard[ennemyship[1]] == boathit && ennemyboard[ennemyship[2]] == boathit) {
        ennemyboard[ennemyship[1]] = boatdestroyed;
        ennemyboard[ennemyship[2]] = boatdestroyed;
    }
    if (ennemyboard[ennemyship[3]] == boathit && ennemyboard[ennemyship[4]] == boathit &&
        ennemyboard[ennemyship[5]] == boathit) {
        ennemyboard[ennemyship[3]] = boatdestroyed;
        ennemyboard[ennemyship[4]] = boatdestroyed;
        ennemyboard[ennemyship[5]] = boatdestroyed;
    }
}

int CheckforVictory(int playerboard[20], int ennemyboard[20]) {
    // check for player victory
    int _victoryflag = 1;
    for (int i = 0; i < 20; i++)
        if (ennemyboard[i] == boat) {
            _victoryflag = 0;
        }

    if (_victoryflag)
        return 1;

    // check for player defeat
    _victoryflag = 1;
    for (int i = 0; i < 20; i++)
        if (playerboard[i] == boat) {
            _victoryflag = 0;
        }
    if (_victoryflag)
        return 2;

    // if no one lost

    return 0;
}

int TelegraphtoBlock(char _inputletter) {
    switch (_inputletter) {
        case 'a': return 0; break;
        case 'b': return 1; break;
        case 'd': return 2; break;
        case 'e': return 3; break;
        case 'f': return 4; break;
        case 'g': return 5; break;
        case 'h': return 6; break;
        case 'i': return 7; break;
        case 'k': return 8; break;
        case 'l': return 9; break;
        case 'm': return 10; break;
        case 'n': return 11; break;
        case 'o': return 12; break;
        case 'p': return 13; break;
        case 'r': return 14; break;
        case 's': return 15; break;
        case 't': return 16; break;
        case 'v': return 17; break;
        case 'w': return 18; break;
        case 'y': return 19; break;
        default: return -1; break;
    }
}

void AddBlockStatustoBuffer(int _x, int _y, int _status) {
    pax_buf_t* gfx = bsp_get_gfx_buffer();
    switch (_status) {
        case water: break;
        case missedshot: pax_outline_circle(gfx, BLACK, _x, _y, 3); break;
        case boat: pax_draw_circle(gfx, BLACK, _x, _y, 3); break;
        case boathit: pax_draw_circle(gfx, RED, _x, _y, 3); break;
        case boatdestroyed: pax_draw_circle(gfx, RED, _x, _y, 5); break;
        default: break;
    }
}

void AddTelegraphBlockStatustoBuffer(int _offset_x, int _block, int _status) {
    AddBlockStatustoBuffer(_offset_x + telegraph_X[_block], telegraph_Y[_block], _status);
}

screen_t screen_battleship_entry(QueueHandle_t application_event_queue, QueueHandle_t keyboard_event_queue) {
    int playerboard[20];
    int ennemyboard[20];

    int playership[6];
    int ennemyship[6];

    int _position[20];
    int victoryflag;

    screen_t current_screen_BS = screen_BS_splash;
    while (1) {
        switch (current_screen_BS) {
            case screen_BS_splash:
                {
                    for (int i = 0; i < 20; i++) {
                        playerboard[i] = 0;
                        ennemyboard[i] = 0;
                    }

                    for (int i = 0; i < 6; i++) {
                        playership[i] = -1;
                        ennemyship[i] = -1;
                    }
                    victoryflag       = playing;
                    current_screen_BS = screen_battleship_splash(application_event_queue, keyboard_event_queue);
                    break;
                }
            case screen_BS_placeships:
                {
                    current_screen_BS = screen_battleship_placeships(
                        application_event_queue,
                        keyboard_event_queue,
                        playerboard,
                        _position,
                        playership
                    );
                    break;
                }
            case screen_BS_battle:
                {
                    ESP_LOGE(TAG, "entry");
                    debugboardstatus(playerboard);
                    debugshipstatus(playership);
                    current_screen_BS = screen_battleship_battle(
                        application_event_queue,
                        keyboard_event_queue,
                        playerboard,
                        ennemyboard,
                        _position,
                        &victoryflag,
                        playership,
                        ennemyship,
                        playership[5]
                    );
                    break;
                }
            case screen_BS_victory:
                {
                    current_screen_BS =
                        screen_battleship_victory(application_event_queue, keyboard_event_queue, &victoryflag);
                    break;
                }
            case screen_home:
            default: return screen_home; break;
        }
    }
}

screen_t screen_battleship_splash(QueueHandle_t application_event_queue, QueueHandle_t keyboard_event_queue) {
    InitKeyboard(keyboard_event_queue);
    configure_keyboard_presses(keyboard_event_queue, true, true, false, true, true);

    pax_font_t const * font = pax_font_sky;
    pax_buf_t*         gfx  = bsp_get_gfx_buffer();

    bsp_apply_lut(lut_4s);
    pax_background(gfx, WHITE);
    pax_insert_png_buf(gfx, caronl_png_start, caronl_png_end - caronl_png_start, 0, 0, 0);
    AddOneTextSWtoBuffer(SWITCH_1, "Exit");
    AddOneTextSWtoBuffer(SWITCH_2, "Offline");
    AddOneTextSWtoBuffer(SWITCH_4, "Replay");
    AddOneTextSWtoBuffer(SWITCH_5, "Online");
    bsp_display_flush();
    while (1) {
        event_t event = {0};
        if (xQueueReceive(application_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            switch (event.type) {
                case event_input_button: break;  // Ignore raw button input
                case event_input_keyboard:
                    switch (event.args_input_keyboard.action) {
                        case SWITCH_1: return screen_home; break;
                        case SWITCH_2: return screen_BS_placeships; break;
                        case SWITCH_3: break;
                        case SWITCH_4: bsp_set_addressable_led(0xFF0000); break;  // replay, to implement
                        case SWITCH_5:
                            return screen_repertoire_entry(application_event_queue, keyboard_event_queue, 1);
                            break;  // online, to implement
                        default: break;
                    }
                    break;
                default: ESP_LOGE(TAG, "Unhandled event type %u", event.type);
            }
        }
    }
}

screen_t screen_battleship_placeships(
    QueueHandle_t application_event_queue,
    QueueHandle_t keyboard_event_queue,
    int           playerboard[20],
    int           _position[20],
    int           playership[6]
) {
    InitKeyboard(keyboard_event_queue);
    configure_keyboard_presses(keyboard_event_queue, true, false, false, false, false);
    configure_keyboard_typing(keyboard_event_queue, true);
    for (int i = 0; i < NUM_CHARACTERS; i++) configure_keyboard_character(keyboard_event_queue, i, false);

    int shipplaced  = 0;
    int flagstart   = 0;
    int displayflag = 1;


    while (1) {
        if (displayflag) {
            if (shipplaced >= long_whole) {
                playership[4] = CalculateMidPositionLongShip(playership);
            }
            // enable the input to press start when all ship placed
            if (shipplaced >= long_whole) {
                flagstart = 1;
                configure_keyboard_press(keyboard_event_queue, SWITCH_5, true);
            } else {
                flagstart = 0;
                configure_keyboard_press(keyboard_event_queue, SWITCH_5, false);
            }
            // set all block to water aside from the placed boats
            for (int i = 0; i < 20; i++) playerboard[i] = water;
            for (int i = 0; i < 6; i++) {
                if (playership[i] != -1)
                    playerboard[playership[i]] = boat;
            }

            Display_battleship_placeships(playerboard, shipplaced, flagstart, _position, playership);
            // disable unvalid positions
            for (int i = 0; i < 20; i++)
                if (_position[i] == water)
                    configure_keyboard_character(keyboard_event_queue, PostoCha(i), false);
                else if (_position[i] == missedshot)
                    configure_keyboard_character(keyboard_event_queue, PostoCha(i), true);
            displayflag = 0;
        }
        event_t event = {0};
        if (xQueueReceive(application_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            switch (event.type) {
                case event_input_button: break;  // Ignore raw button input
                case event_input_keyboard:
                    switch (event.args_input_keyboard.action) {
                        case SWITCH_1:
                            //  undo
                            if (shipplaced > 0) {
                                // remove the middle of the long ship
                                if (shipplaced == long_whole) {
                                    playership[shipplaced] = -1;
                                }
                                shipplaced--;
                                playership[shipplaced] = -1;
                                displayflag            = 1;
                            } else {  // exit
                                if (Screen_Confirmation(forfeitprompt, application_event_queue, keyboard_event_queue))
                                    return screen_home;
                            }
                            break;
                        case SWITCH_2: break;
                        case SWITCH_3: break;
                        case SWITCH_4: break;
                        case SWITCH_5: return screen_BS_battle; break;
                        default: break;
                    }
                    // process ship placement inputs
                    if (event.args_input_keyboard.character != '\0' &&
                        TelegraphtoBlock(event.args_input_keyboard.character) != -1 && shipplaced < long_whole) {
                        if (_position[TelegraphtoBlock(event.args_input_keyboard.character)]) {
                            if (shipplaced != (long_whole - 1))
                                playership[shipplaced] = TelegraphtoBlock(event.args_input_keyboard.character);
                            else
                                playership[shipplaced + 1] = TelegraphtoBlock(event.args_input_keyboard.character);
                            shipplaced++;
                            displayflag = 1;
                        }
                    }
                    break;
                default: ESP_LOGE(TAG, "Unhandled event type %u", event.type);
            }
        }
    }
}

void Display_battleship_placeships(
    int playerboard[20], int _shipplaced, int _flagstart, int _position[20], int playership[6]
) {
    int telegraph_x      = 100;
    int text_x           = 220;
    int text_y           = 20;
    int gapinstruction_y = 50;
    int fontsize         = 9;

    pax_font_t const * font = pax_font_sky;
    pax_buf_t*         gfx  = bsp_get_gfx_buffer();

    // ESP_LOGE(TAG, "ship placed: %d", _shipplaced);
    // debugboardstatus(playerboard);
    // debugshipstatus(playership);
    // ESP_LOGE(TAG, "position");
    // debugboardstatus(_position);

    pax_background(gfx, WHITE);
    AddSwitchesBoxtoBuffer(SWITCH_1);

    // display exit or undo and start
    AddSWtoBuffer("", "", "", "", "");
    if (_shipplaced == no_ship)
        AddSWtoBufferL("Exit");
    else if (_shipplaced > no_ship && !_flagstart)
        AddSWtoBufferL("Undo");
    else {
        AddSWtoBufferLR("Undo", "Start");
    }
    DisplayTelegraph(BLACK, telegraph_x);

    // instructions
    pax_center_text(gfx, BLACK, font, fontsize, text_x, text_y, "Place your ships using");
    pax_center_text(gfx, BLACK, font, fontsize, text_x, text_y + fontsize, "the telegraph to enter");
    pax_center_text(gfx, BLACK, font, fontsize, text_x, text_y + fontsize * 2, "its coordinates");
    pax_outline_circle(gfx, BLACK, text_x - 60, text_y + 8 + gapinstruction_y, 3);
    AddBlocktoBuffer(text_x - 60, text_y + 8 + gapinstruction_y);
    pax_draw_text(gfx, BLACK, font, fontsize, text_x - 50, text_y + gapinstruction_y, "Shows where the ");
    pax_draw_text(gfx, BLACK, font, fontsize, text_x - 50, text_y + gapinstruction_y + fontsize, "ships can be placed");

    // ship placed on the left
    int LS_x     = 18;
    int LS_x_gap = 16;
    int LS_y     = 25;
    int LS_y_gap = 25;

    // Draws the  ships
    AddShiptoBuffer(1, east, LS_x, LS_y);
    AddShiptoBuffer(2, east, LS_x, LS_y + LS_y_gap);
    AddShiptoBuffer(3, east, LS_x, LS_y + LS_y_gap * 2);

    // Show which ship block is being placed on the left
    switch (_shipplaced + 1) {
        case small_ship: AddBlockStatustoBuffer(LS_x, LS_y, missedshot); break;
        case medium_front: AddBlockStatustoBuffer(LS_x, LS_y + LS_y_gap, missedshot); break;
        case medium_whole: AddBlockStatustoBuffer(LS_x + LS_x_gap, LS_y + LS_y_gap, missedshot); break;
        case long_front: AddBlockStatustoBuffer(LS_x, LS_y + LS_y_gap * 2, missedshot); break;
        case long_whole: AddBlockStatustoBuffer(LS_x + LS_x_gap * 2, LS_y + LS_y_gap * 2, missedshot); break;
        // case no_ship: break;
        default: break;
    }

    // Show which ship block was placed on the left
    if (_shipplaced >= small_ship)
        AddBlockStatustoBuffer(LS_x, LS_y, boat);
    if (_shipplaced >= medium_front)
        AddBlockStatustoBuffer(LS_x, LS_y + LS_y_gap, boat);
    if (_shipplaced >= medium_whole)
        AddBlockStatustoBuffer(LS_x + LS_x_gap, LS_y + LS_y_gap, boat);
    if (_shipplaced >= long_front)
        AddBlockStatustoBuffer(LS_x, LS_y + LS_y_gap * 2, boat);
    if (_shipplaced >= long_whole) {
        AddBlockStatustoBuffer(LS_x + LS_x_gap, LS_y + LS_y_gap * 2, boat);
        AddBlockStatustoBuffer(LS_x + LS_x_gap * 2, LS_y + LS_y_gap * 2, boat);
    }

    // Draws ships on the board
    if (_shipplaced >= small_ship)
        AddShiptoBuffer(1, 2, telegraph_x + telegraph_X[playership[0]], telegraph_Y[playership[0]]);
    if (_shipplaced >= medium_whole)
        AddShiptoBuffer(
            2,
            CalculateShipOrientation(playership, 2),
            telegraph_x + telegraph_X[playership[1]],
            telegraph_Y[playership[1]]
        );
    if (_shipplaced >= long_whole)
        AddShiptoBuffer(
            3,
            CalculateShipOrientation(playership, 3),
            telegraph_x + telegraph_X[playership[3]],
            telegraph_Y[playership[3]]
        );

    if (_shipplaced < long_whole)
        CheckforShipPlacement(playership, _shipplaced, _position);  // adds free positions for next run

    for (int i = 0; i < 20; i++) {
        // draw the placed boats
        AddTelegraphBlockStatustoBuffer(telegraph_x, i, playerboard[i]);
        // draw the possible placements
        if (_shipplaced < long_whole)
            AddTelegraphBlockStatustoBuffer(telegraph_x, i, _position[i]);
    }
    if (log) {
        ESP_LOGE(TAG, "before display");
        ESP_LOGE(TAG, "_shipplaced: %d", _shipplaced);
        debugboardstatus(playerboard);
        debugshipstatus(playership);
    }
    bsp_display_flush();
}

screen_t screen_battleship_battle(
    QueueHandle_t application_event_queue,
    QueueHandle_t keyboard_event_queue,
    int           playerboard[20],
    int           ennemyboard[20],
    int           _position[20],
    int*          victoryflag,
    int           playership[6],
    int           ennemyship[6],
    int           bodge
) {
    InitKeyboard(keyboard_event_queue);
    configure_keyboard_presses(keyboard_event_queue, true, false, false, false, false);
    configure_keyboard_typing(keyboard_event_queue, true);

    debugboardstatus(playerboard);
    debugshipstatus(playership);

    int  turn                       = ennemy;
    char nickname[nicknamelenght]   = "";
    char ennemyname[nicknamelenght] = "AI";
    int  displayflag                = 1;

    for (int i = 0; i < 6; i++) {
        ennemyboard[ennemyship[i]] = boat;
    }

    nvs_get_str_wrapped("owner", "nickname", nickname, sizeof(nickname));
    AIplaceShips(ennemyboard, _position, ennemyship);
    ESP_LOGE(TAG, "just before display test");
    debugboardstatus(playerboard);
    debugshipstatus(playership);
    playership[5] = bodge;

    while (1) {
        if (displayflag) {
            Display_battleship_battle(
                playerboard,
                ennemyboard,
                nickname,
                ennemyname,
                turn,
                _position,
                playership,
                ennemyship
            );
            displayflag = 0;
        }
        event_t event = {0};
        if (xQueueReceive(application_event_queue, &event, portMAX_DELAY) == pdTRUE) {
            switch (event.type) {
                case event_input_button: break;  // Ignore raw button input
                case event_input_keyboard:
                    switch (event.args_input_keyboard.action) {
                        case SWITCH_1:
                            if (Screen_Confirmation(forfeitprompt, application_event_queue, keyboard_event_queue))
                                return screen_home;
                            break;
                        case SWITCH_2: break;
                        case SWITCH_3: break;
                        case SWITCH_4: break;
                        case SWITCH_5: break;
                        default: break;
                    }
                    if (event.args_input_keyboard.character != '\0' &&
                        TelegraphtoBlock(event.args_input_keyboard.character) != -1) {
                        turn            = player;
                        int hitlocation = TelegraphtoBlock(event.args_input_keyboard.character);
                        if (ennemyboard[hitlocation] == water)
                            ennemyboard[hitlocation] = missedshot;
                        if (ennemyboard[hitlocation] == boat)
                            ennemyboard[hitlocation] = boathit;
                        ESP_LOGE(TAG, "player shot");
                        CheckforDestroyedShips(playerboard, ennemyboard, playership, ennemyship);
                        displayflag = 1;
                        debugboardstatus(ennemyboard);
                        debugboardstatus(playerboard);

                        if (CheckforVictory(playerboard, ennemyboard)) {
                            ESP_LOGE(TAG, "enter victroy");
                            vTaskDelay(pdMS_TO_TICKS(1000));
                            *victoryflag = CheckforVictory(playerboard, ennemyboard);
                            ESP_LOGE(TAG, "Victory flag: %d", *victoryflag);
                            return screen_BS_victory;
                        }

                        vTaskDelay(pdMS_TO_TICKS(1000));
                        turn = ennemy;

                        do hitlocation = esp_random() % 20;
                        while ((playerboard[hitlocation] == missedshot) || (playerboard[hitlocation] == boathit) ||
                               (playerboard[hitlocation] == boatdestroyed));
                        if (playerboard[hitlocation] == water)
                            playerboard[hitlocation] = missedshot;
                        if (playerboard[hitlocation] == boat)
                            playerboard[hitlocation] = boathit;
                        ESP_LOGE(TAG, "ennemy shot");
                        debugboardstatus(ennemyboard);
                        debugboardstatus(playerboard);

                        CheckforDestroyedShips(playerboard, ennemyboard, playership, ennemyship);

                        displayflag = 1;
                        if (CheckforVictory(playerboard, ennemyboard)) {
                            ESP_LOGE(TAG, "enter victroy");
                            vTaskDelay(pdMS_TO_TICKS(1000));
                            *victoryflag = CheckforVictory(playerboard, ennemyboard);
                            ESP_LOGE(TAG, "Victory flag: %d", *victoryflag);
                            return screen_BS_victory;
                        }
                        vTaskDelay(pdMS_TO_TICKS(1000));
                    }
                    break;
                default: ESP_LOGE(TAG, "Unhandled event type %u", event.type);
            }
        }
    }
}

void Display_battleship_battle(
    int  playerboard[20],
    int  ennemyboard[20],
    char _nickname[nicknamelenght],
    char _ennemyname[nicknamelenght],
    int  _turn,
    int  _position[20],
    int  playership[6],
    int  ennemyship[6]
) {
    pax_font_t const * font = pax_font_sky;
    pax_buf_t*         gfx  = bsp_get_gfx_buffer();

    int telegraphplayer_x = 100;
    int telegraphennemy_x = gfx->height - telegraphplayer_x;
    int text_x            = 23;
    int text_y            = 35;
    int text_fontsize     = 9;

    ESP_LOGE(TAG, "battle test");
    debugboardstatus(playerboard);
    debugshipstatus(playership);

    pax_background(gfx, WHITE);
    AddSWtoBufferL("Exit");

    DisplayTelegraph(BLACK, telegraphplayer_x);
    DisplayTelegraph(BLACK, telegraphennemy_x);

    int pl_x = 8;
    int pl_y = 8;
    int tn_y = 20;

    pax_draw_text(gfx, BLACK, font, 14, pl_x, pl_y, _nickname);
    Justify_right_text(gfx, BLACK, font, 14, pl_x, pl_y, _ennemyname);

    if (_turn == ennemy)  // counter intuitive, but it is waiting on next player so it's inverted
        pax_draw_text(gfx, BLACK, font, text_fontsize, pl_x, tn_y, "your turn");
    if (_turn == player)
        Justify_right_text(gfx, BLACK, font, text_fontsize, pl_x, tn_y, "your turn");

    // instructions
    pax_draw_text(gfx, BLACK, font, text_fontsize, text_x, text_y, "Miss");
    pax_outline_circle(gfx, BLACK, text_x - 10, text_y + 5, 3);
    AddBlocktoBuffer(text_x - 10, text_y + 5);
    pax_draw_text(gfx, BLACK, font, text_fontsize, text_x, text_y + text_fontsize * 2, "Hit");
    pax_draw_circle(gfx, RED, text_x - 10, text_y + text_fontsize * 2 + 5, 3);
    AddBlocktoBuffer(text_x - 10, text_y + text_fontsize * 2 + 5);
    pax_draw_text(gfx, BLACK, font, text_fontsize, text_x, text_y + text_fontsize * 4, "Sunken");
    pax_draw_circle(gfx, RED, text_x - 10, text_y + text_fontsize * 4 + 5, 5);
    AddBlocktoBuffer(text_x - 10, text_y + text_fontsize * 4 + 5);

    //  add the player ships to buffer
    AddShiptoBuffer(1, 2, telegraphplayer_x + telegraph_X[playership[0]], telegraph_Y[playership[0]]);
    AddShiptoBuffer(
        2,
        CalculateShipOrientation(playership, 2),
        telegraphplayer_x + telegraph_X[playership[1]],
        telegraph_Y[playership[1]]
    );
    AddShiptoBuffer(
        3,
        CalculateShipOrientation(playership, 3),
        telegraphplayer_x + telegraph_X[playership[3]],
        telegraph_Y[playership[3]]
    );

    //  add the ennemy ships to buffer
    if (ennemyboard[ennemyship[0]] == boatdestroyed)
        AddShiptoBuffer(1, 2, telegraphennemy_x + telegraph_X[ennemyship[0]], telegraph_Y[ennemyship[0]]);
    if (ennemyboard[ennemyship[1]] == boatdestroyed)
        AddShiptoBuffer(
            2,
            CalculateShipOrientation(ennemyship, 2),
            telegraphennemy_x + telegraph_X[ennemyship[1]],
            telegraph_Y[ennemyship[1]]
        );
    if (ennemyboard[ennemyship[3]] == boatdestroyed)
        AddShiptoBuffer(
            3,
            CalculateShipOrientation(ennemyship, 3),
            telegraphennemy_x + telegraph_X[ennemyship[3]],
            telegraph_Y[ennemyship[3]]
        );

    for (int i = 0; i < 20; i++) {
        AddTelegraphBlockStatustoBuffer(telegraphplayer_x, i, playerboard[i]);
        // if (ennemyboard[i] != boat)
        AddTelegraphBlockStatustoBuffer(telegraphennemy_x, i, ennemyboard[i]);
    }

    bsp_display_flush();
}

screen_t screen_battleship_victory(
    QueueHandle_t application_event_queue, QueueHandle_t keyboard_event_queue, int* victoryflag
) {
    InitKeyboard(keyboard_event_queue);
    configure_keyboard_presses(keyboard_event_queue, true, true, true, true, true);

    pax_buf_t* gfx = bsp_get_gfx_buffer();
    ESP_LOGE(TAG, "Victory flag: %d", *victoryflag);
    pax_background(gfx, WHITE);
    if (*victoryflag == victory)
        pax_insert_png_buf(gfx, caronv_png_start, caronv_png_end - caronv_png_start, 0, 0, 0);
    if (*victoryflag == defeat)
        pax_insert_png_buf(gfx, carond_png_start, carond_png_end - carond_png_start, 0, 0, 0);
    bsp_display_flush();

    while (1) {
        event_t event = {0};
        if (xQueueReceive(application_event_queue, &event, 5000) == pdTRUE) {
            switch (event.type) {
                case event_input_button: break;  // Ignore raw button input
                case event_input_keyboard:
                    switch (event.args_input_keyboard.action) {
                        case SWITCH_1: return screen_BS_splash; break;
                        case SWITCH_2: return screen_BS_splash; break;
                        case SWITCH_3: return screen_BS_splash; break;
                        case SWITCH_4: return screen_BS_splash; break;
                        case SWITCH_5: return screen_BS_splash; break;
                        default: break;
                    }
                    break;
                default: ESP_LOGE(TAG, "Unhandled event type %u", event.type);
            }
        }
    }
}
