#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <conio.h>  // For _kbhit() and _getch()
#include <windows.h> // For Sleep(), COORD, SetConsoleCursorPosition, etc.

// --- Game Constants ---
#define SCREEN_WIDTH 60
#define SCREEN_HEIGHT 20
#define MAX_BULLETS 10
#define MAX_ENEMIES 5
#define PLAYER_CHAR '^'
#define BULLET_CHAR '|'
#define ENEMY_CHAR 'V'
#define FRAME_DELAY_MS 50 // 50 milliseconds delay

// --- Structures ---
typedef struct {
    int x, y;
    int prev_x, prev_y; // NEW: To store the position from the last frame
    int active;
} Entity;

// --- Game State Variables ---
Entity player;
Entity bullets[MAX_BULLETS];
Entity enemies[MAX_ENEMIES];
int score = 0;
int game_over = 0;

// --- Windows Console Functions ---

// Function to move the console cursor to (x, y)
// Note: We use x+1, y+1 when calling this to account for the border.
void gotoXY(int x, int y) {
    COORD coord = {x, y};
    SetConsoleCursorPosition(GetStdHandle(STD_OUTPUT_HANDLE), coord);
}

// Function to hide the console cursor
void hideCursor() {
    CONSOLE_CURSOR_INFO cursorInfo;
    GetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo);
    cursorInfo.bVisible = FALSE;
    SetConsoleCursorInfo(GetStdHandle(STD_OUTPUT_HANDLE), &cursorInfo);
}

// Function to clear the console screen
void clearScreen() {
    system("cls");
}

// --- Game Logic Functions ---

void initializeGame() {
    hideCursor();
    srand(time(NULL));

    // Initialize player
    player.x = SCREEN_WIDTH / 2;
    player.y = SCREEN_HEIGHT - 1;
    player.prev_x = player.x;
    player.prev_y = player.y;

    // Initialize entities
    for (int i = 0; i < MAX_BULLETS; i++) {
        bullets[i].active = 0;
        bullets[i].prev_y = -1; // Set outside screen
    }
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].active = 0;
        enemies[i].prev_y = -1; // Set outside screen
    }

    // Draw the initial borders once
    gotoXY(0, 0);
    for (int i = 0; i < SCREEN_WIDTH + 2; i++) printf("#");
    printf("\n");
    for (int y = 1; y <= SCREEN_HEIGHT; y++) {
        gotoXY(0, y); printf("#");
        gotoXY(SCREEN_WIDTH + 1, y); printf("#");
    }
    gotoXY(0, SCREEN_HEIGHT + 1);
    for (int i = 0; i < SCREEN_WIDTH + 2; i++) printf("#");
    gotoXY(0, SCREEN_HEIGHT + 2);
    printf("SCORE: %d | Controls: A/D/Space | Q to Quit", score);
}

void drawScreen() {

    // 1. Update Score Display (only when needed)
    gotoXY(7, SCREEN_HEIGHT + 2);
    printf("%d", score);


    // --- STEP 1: CLEAR PREVIOUS POSITIONS ---

    // Clear Player's old spot
    // The player only moves left/right, so we only need to clear the spot it just left.
    if (player.x != player.prev_x) {
        gotoXY(player.prev_x + 1, player.prev_y + 1);
        printf(" ");
    }

    // Clear Bullets' old spots
    for (int i = 0; i < MAX_BULLETS; i++) {
        // Clear if the bullet was active in the previous frame AND was on screen
        if (bullets[i].active || bullets[i].prev_y >= 0) {
            gotoXY(bullets[i].prev_x + 1, bullets[i].prev_y + 1);
            printf(" ");
        }
    }

    // Clear Enemies' old spots
    for (int i = 0; i < MAX_ENEMIES; i++) {
        // Clear if the enemy was active in the previous frame AND was on screen
        if (enemies[i].active || enemies[i].prev_y >= 0) {
             gotoXY(enemies[i].prev_x + 1, enemies[i].prev_y + 1);
             printf(" ");
        }
    }


    // --- STEP 2: DRAW NEW POSITIONS ---

    // Draw player
    gotoXY(player.x + 1, player.y + 1);
    printf("%c", PLAYER_CHAR);
    player.prev_x = player.x;
    player.prev_y = player.y;

    // Draw bullets
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active && bullets[i].y >= 0 && bullets[i].y < SCREEN_HEIGHT) {
            gotoXY(bullets[i].x + 1, bullets[i].y + 1);
            printf("%c", BULLET_CHAR);
        }
    }

    // Draw enemies
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active && enemies[i].y >= 0 && enemies[i].y < SCREEN_HEIGHT) {
            gotoXY(enemies[i].x + 1, enemies[i].y + 1);
            printf("%c", ENEMY_CHAR);
        }
    }
}

void handleInput() {
    if (_kbhit()) {
        char key = _getch();

        player.prev_x = player.x; // Store player's position before input

        switch (key) {
            case 'a':
            case 'A':
                if (player.x > 0) player.x--;
                break;
            case 'd':
            case 'D':
                if (player.x < SCREEN_WIDTH - 1) player.x++;
                break;
            case ' ': // Space to shoot
                for (int i = 0; i < MAX_BULLETS; i++) {
                    if (!bullets[i].active) {
                        bullets[i].active = 1;
                        bullets[i].x = player.x;
                        bullets[i].y = player.y - 1;
                        bullets[i].prev_x = bullets[i].x; // Initial prev must match x
                        bullets[i].prev_y = bullets[i].y;
                        break;
                    }
                }
                break;
            case 'q':
            case 'Q':
                game_over = 1;
                break;
        }
    }
}

void updateBullets() {
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (bullets[i].active) {
            bullets[i].prev_x = bullets[i].x;
            bullets[i].prev_y = bullets[i].y;

            bullets[i].y--; // Move up

            // Deactivate if off-screen
            if (bullets[i].y < 0) {
                bullets[i].active = 0;
            }
        }
    }
}

void updateEnemies() {
    // Enemy Spawning
    if (rand() % 25 == 0) {
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (!enemies[i].active) {
                enemies[i].active = 1;
                enemies[i].x = rand() % SCREEN_WIDTH;
                enemies[i].y = 0;
                enemies[i].prev_x = enemies[i].x;
                enemies[i].prev_y = enemies[i].y;
                break;
            }
        }
    }

    // Enemy Movement
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            enemies[i].prev_x = enemies[i].x;
            enemies[i].prev_y = enemies[i].y;

            if (rand() % 5 == 0) {
                enemies[i].y++; // Move down
            }

            // Check for collision with player (Game Over)
            if (enemies[i].y == player.y && enemies[i].x == player.x) {
                game_over = 1;
                return;
            }

            // Deactivate if off-screen (missed)
            if (enemies[i].y >= SCREEN_HEIGHT) {
                // Clear the enemy's final position before deactivating
                gotoXY(enemies[i].x + 1, enemies[i].y + 1);
                printf(" ");
                enemies[i].active = 0;
            }
        }
    }
}

void checkCollisions() {
    for (int b = 0; b < MAX_BULLETS; b++) {
        if (bullets[b].active) {
            for (int e = 0; e < MAX_ENEMIES; e++) {
                if (enemies[e].active) {
                    // Collision check
                    if (bullets[b].x == enemies[e].x && bullets[b].y == enemies[e].y) {
                        // Clear the spot where they collided
                        gotoXY(bullets[b].x + 1, bullets[b].y + 1);
                        printf(" ");

                        bullets[b].active = 0;
                        enemies[e].active = 0;
                        score += 10;
                        break;
                    }
                }
            }
        }
    }
}

void gameOverScreen() {
    clearScreen();
    gotoXY(SCREEN_WIDTH / 2 - 5, SCREEN_HEIGHT / 2);
    printf("GAME OVER!");
    gotoXY(SCREEN_WIDTH / 2 - 8, SCREEN_HEIGHT / 2 + 1);
    printf("Final Score: %d", score);
    gotoXY(SCREEN_WIDTH / 2 - 12, SCREEN_HEIGHT / 2 + 3);
    printf("Press any key to exit...");
    _getch();
}


// --- Main Function ---

int main() {
    clearScreen();
    initializeGame();

    while (!game_over) {
        // 1. Input
        handleInput();

        if (game_over) break;

        // 2. Update Game State
        updateBullets();
        updateEnemies();
        checkCollisions();

        // 3. Draw (Clear old positions, draw new ones)
        drawScreen();

        // 4. Control Speed
        Sleep(FRAME_DELAY_MS);
    }

    gameOverScreen();

    return 0;
}
