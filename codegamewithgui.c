
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <gtk/gtk.h>
#include <cairo.h>
#include <math.h> 

// --- Game Constants (UNCHANGED) ---
#define SCREEN_WIDTH 36      
#define SCREEN_HEIGHT 20
#define MAX_PLAYER_BULLETS 10      
#define MAX_ENEMY_BULLETS 10       
#define MAX_ENEMIES 5
#define FRAME_DELAY_MS 60 
#define SHOOT_SCORE_THRESHOLD 250  
#define ENEMY_SHOOT_CHANCE 20      

// --- GUI Constants (UNCHANGED) ---
#define PIXEL_SCALE 25      
#define DEFAULT_WINDOW_WIDTH (SCREEN_WIDTH * PIXEL_SCALE)
#define DEFAULT_WINDOW_HEIGHT (SCREEN_HEIGHT * PIXEL_SCALE)
#define HUD_AREA_HEIGHT 40  
#define MAX_LIVES 3 

// --- Player/Enemy Constants (UNCHANGED) ---
#define PLAYER_SIZE_SCALE 1.5 
#define RED_ENEMY_SCALE 0.5 
#define BLUE_ENEMY_SCALE 0.75 

// --- Game States (UNCHANGED) ---
#define GAME_STATE_HOME 0
#define GAME_STATE_RUNNING 1
#define GAME_STATE_OVER 2
#define GAME_STATE_INSTRUCTIONS 3
#define GAME_STATE_PAUSED 4

// --- Structures (UNCHANGED) ---
typedef struct {
    int x, y;
    int active;
    int type;      
    int frame_delay;
} Entity;

typedef struct {
    int x, y;
    int active;
    int is_enemy; 
} Bullet;

// --- Game State Variables (UNCHANGED) ---
Entity player;
Bullet player_bullets[MAX_PLAYER_BULLETS]; 
Bullet enemy_bullets[MAX_ENEMY_BULLETS];   
Entity enemies[MAX_ENEMIES];
int score = 0;
int game_over = 0;
int game_difficulty_level = 1;
int game_state = GAME_STATE_HOME; 
int player_lives = MAX_LIVES; 
int countdown_value = 0; 

// --- Explosion Variables / Timers / GTK Globals (UNCHANGED) ---
int small_blast_x = -1, small_blast_y = -1; 
int big_blast_x = -1, big_blast_y = -1;   
guint invulnerability_timer_id = 0; 
int is_player_invulnerable = 0;    
guint game_over_delay_timer_id = 0; 
guint countdown_timer_id = 0;

// --- HOME SCREEN ANIMATION VARIABLE (UNCHANGED) ---
int star_scroll_offset = 0;
int star_tick_counter = 0; 

GtkWidget *drawing_area;
GtkWidget *vbox_game_content; 
GtkWidget *vbox_home;         
GtkWidget *overlay;           
guint timer_id = 0;    

// --- HUD WIDGETS (UNCHANGED) ---
GtkWidget *score_text_label;  
GtkWidget *level_text_label;  
GtkWidget *heart_box;         
GtkWidget *hearts[MAX_LIVES]; 

// --- Forward Declarations (UNCHANGED) ---
static void start_game_button_clicked(GtkWidget *widget, gpointer data);
static void restart_game(void);
static void go_to_home_screen(void);
void initializeGame(void);
static gboolean game_tick(gpointer data);
static void activate(GtkApplication *app, gpointer user_data); 
static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data); 
static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data); 
static void update_hearts_display();
void draw_centered_text(cairo_t *cr, double current_width, const char *text, double y, double font_size);
static gboolean end_invulnerability_period(gpointer data); 
static gboolean finalize_game_over(gpointer data); 
static gboolean countdown_tick(gpointer data);
static gboolean end_instructions_delay(gpointer data);


// --- GTK/Cairo Helper Functions (UNCHANGED) ---

static void update_hearts_display() {
    for (int i = 0; i < MAX_LIVES; i++) {
        if (i < player_lives) {
            gtk_widget_show(hearts[i]);
        } else {
            gtk_widget_hide(hearts[i]);
        }
    }
}

void draw_centered_text(cairo_t *cr, double current_width, const char *text, double y, double font_size) {
    cairo_text_extents_t extents;
    cairo_set_font_size(cr, font_size);
    cairo_text_extents(cr, text, &extents);
    
    double x = (current_width / 2) - (extents.width / 2) - extents.x_bearing;
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, text);
}


// --- Game Flow Timer Functions (UNCHANGED) ---

static gboolean finalize_game_over(gpointer data) {
    game_state = GAME_STATE_OVER;
    game_over_delay_timer_id = 0;
    big_blast_x = -1; 
    big_blast_y = -1;
    if (timer_id == 0) {
        timer_id = g_timeout_add(FRAME_DELAY_MS, game_tick, NULL);
    }
    gtk_widget_queue_draw(drawing_area);
    return FALSE;
}

static gboolean end_invulnerability_period(gpointer data) {
    is_player_invulnerable = 0;
    invulnerability_timer_id = 0;
    
    if (game_over_delay_timer_id == 0) { 
        big_blast_x = -1; 
        big_blast_y = -1;
    }
    gtk_widget_queue_draw(drawing_area);
    return FALSE; 
}


// --- Core Game Logic Functions (UNCHANGED) ---

void initializeGame() {
    srand(time(NULL));
    player.x = SCREEN_WIDTH / 2; 
    player.y = SCREEN_HEIGHT - 1;
    player.active = 1;
    score = 0;
    game_over = 0;
    game_difficulty_level = 1; 
    player_lives = MAX_LIVES;
    is_player_invulnerable = 0;
    
    if (invulnerability_timer_id != 0) { g_source_remove(invulnerability_timer_id); invulnerability_timer_id = 0; }
    if (game_over_delay_timer_id != 0) { g_source_remove(game_over_delay_timer_id); game_over_delay_timer_id = 0; }
    if (countdown_timer_id != 0) { g_source_remove(countdown_timer_id); countdown_timer_id = 0; }
    
    for (int i = 0; i < MAX_PLAYER_BULLETS; i++) player_bullets[i].active = 0;
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) enemy_bullets[i].active = 0; 
    for (int i = 0; i < MAX_ENEMIES; i++) {
        enemies[i].active = 0;
        enemies[i].type = 0; 
        enemies[i].frame_delay = 0;
    }
    
    small_blast_x = -1; small_blast_y = -1;
    big_blast_x = -1; big_blast_y = -1;
    
    if (score_text_label) { 
        char score_str[50]; 
        sprintf(score_str, "SCORE: %d", score); 
        gtk_label_set_text(GTK_LABEL(score_text_label), score_str); 
    }
    if (level_text_label) { 
        char level_str[50]; 
        sprintf(level_str, "LEVEL: %d", game_difficulty_level); 
        gtk_label_set_text(GTK_LABEL(level_text_label), level_str); 
    }
    update_hearts_display();
}

void updatePlayerBullets() {
    for (int i = 0; i < MAX_PLAYER_BULLETS; i++) {
        if (player_bullets[i].active) {
            player_bullets[i].y -= 2; 
            if (player_bullets[i].y < 0) {
                player_bullets[i].active = 0;
            }
        }
    }
}

void updateEnemyBullets() {
    for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
        if (enemy_bullets[i].active) {
            enemy_bullets[i].y += 1; 
            
            if (!is_player_invulnerable && 
                enemy_bullets[i].x == player.x && 
                enemy_bullets[i].y >= player.y) {
                
                player_lives--;
                update_hearts_display();
                
                enemy_bullets[i].active = 0;

                if (player_lives <= 0) {
                    big_blast_x = player.x; 
                    big_blast_y = player.y; 
                    if (timer_id != 0) { g_source_remove(timer_id); timer_id = 0; }
                    game_over_delay_timer_id = g_timeout_add(3000, finalize_game_over, NULL);
                    game_state = GAME_STATE_PAUSED;
                } else {
                    big_blast_x = -1; 
                    big_blast_y = -1; 
                    is_player_invulnerable = 1;
                    player.x = SCREEN_WIDTH / 2;
                    player.y = SCREEN_HEIGHT - 1;
                    invulnerability_timer_id = g_timeout_add(2000, end_invulnerability_period, NULL);
                }
                return; 
            }
            
            if (enemy_bullets[i].y >= SCREEN_HEIGHT) {
                enemy_bullets[i].active = 0;
            }
        }
    }
}

void updateEnemies() {
    int spawn_chance_threshold = 25 - (game_difficulty_level > 20 ? 20 : game_difficulty_level); 
    if (spawn_chance_threshold < 5) spawn_chance_threshold = 5; 

    if (rand() % spawn_chance_threshold == 0) {
        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (!enemies[i].active) {
                enemies[i].active = 1;
                enemies[i].x = rand() % SCREEN_WIDTH; 
                enemies[i].y = 0;
                
                int is_blue_allowed = (score >= 150);
                
                if (is_blue_allowed && (rand() % 100 < 20)) {
                    enemies[i].type = 1; 
                } else {
                    enemies[i].type = 0; 
                }

                enemies[i].frame_delay = (enemies[i].type == 0) ? 4 : 2; 
                break;
            }
        }
    }

    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (enemies[i].active) {
            
            if (enemies[i].frame_delay <= 0) {
                enemies[i].y++; 
                enemies[i].frame_delay = (enemies[i].type == 0) ? 4 : 2; 
            } else {
                enemies[i].frame_delay--;
            }

            int should_enemy_shoot = 0;

            if (enemies[i].type == 0) {
                if (score >= SHOOT_SCORE_THRESHOLD && (rand() % ENEMY_SHOOT_CHANCE) == 0) {
                    should_enemy_shoot = 1;
                }
            } else if (enemies[i].type == 1) {
                if (score >= 400 && (rand() % ENEMY_SHOOT_CHANCE) == 0) {
                    should_enemy_shoot = 1;
                }
            }

            if (should_enemy_shoot) {
                for (int j = 0; j < MAX_ENEMY_BULLETS; j++) {
                    if (!enemy_bullets[j].active) {
                        enemy_bullets[j].active = 1;
                        enemy_bullets[j].is_enemy = 1;
                        enemy_bullets[j].x = enemies[i].x;
                        enemy_bullets[j].y = enemies[i].y + 1;
                        break;
                    }
                }
            }
            
            if (!is_player_invulnerable && enemies[i].y == player.y && enemies[i].x == player.x) {
                player_lives--;
                update_hearts_display();
                
                if (player_lives <= 0) {
                    big_blast_x = player.x; 
                    big_blast_y = player.y;
                    if (timer_id != 0) { g_source_remove(timer_id); timer_id = 0; }
                    game_over_delay_timer_id = g_timeout_add(3000, finalize_game_over, NULL);
                    game_state = GAME_STATE_PAUSED; 
                    return; 
                } else {
                    big_blast_x = -1; 
                    big_blast_y = -1; 
                    is_player_invulnerable = 1;
                    player.x = SCREEN_WIDTH / 2; 
                    player.y = SCREEN_HEIGHT - 1;
                    enemies[i].active = 0; 
                    invulnerability_timer_id = g_timeout_add(2000, end_invulnerability_period, NULL);
                }
            }

            if (enemies[i].y >= SCREEN_HEIGHT) {
                enemies[i].active = 0;
            }
        }
    }
}

void checkCollisions() {
    for (int b = 0; b < MAX_PLAYER_BULLETS; b++) {
        if (player_bullets[b].active) {
            for (int e = 0; e < MAX_ENEMIES; e++) {
                if (enemies[e].active) {
                    
                    if (player_bullets[b].x == enemies[e].x && 
                       (player_bullets[b].y == enemies[e].y || player_bullets[b].y + 1 == enemies[e].y || player_bullets[b].y + 2 == enemies[e].y)) {   
                        
                        small_blast_x = player_bullets[b].x; 
                        small_blast_y = enemies[e].y; 
                        
                        player_bullets[b].active = 0;
                        enemies[e].active = 0;
                        
                        if (enemies[e].type == 1) { score += 20; } else { score += 10; }
                        
                        break;
                    }
                }
            }
        }
    }
    
    for (int pb = 0; pb < MAX_PLAYER_BULLETS; pb++) {
        if (player_bullets[pb].active) {
            for (int eb = 0; eb < MAX_ENEMY_BULLETS; eb++) {
                if (enemy_bullets[eb].active) {
                    
                    if (player_bullets[pb].x == enemy_bullets[eb].x && 
                       (player_bullets[pb].y <= enemy_bullets[eb].y && player_bullets[pb].y + 2 >= enemy_bullets[eb].y)) {
                        
                        small_blast_x = player_bullets[pb].x; 
                        small_blast_y = player_bullets[pb].y; 
                        
                        player_bullets[pb].active = 0;
                        enemy_bullets[eb].active = 0;
                        
                        break;
                    }
                }
            }
        }
    }
}


// --- Game Timer Control Functions (UNCHANGED) ---

static gboolean countdown_tick(gpointer data) {
    if (game_state != GAME_STATE_INSTRUCTIONS) {
        countdown_timer_id = 0;
        return FALSE; 
    }
    
    countdown_value--;
    gtk_widget_queue_draw(drawing_area); 

    if (countdown_value < 0) {
        game_state = GAME_STATE_RUNNING;
        GtkWidget *parent_window = gtk_widget_get_ancestor(drawing_area, GTK_TYPE_WINDOW);
        if (parent_window) gtk_widget_grab_focus(parent_window);
        countdown_timer_id = 0;
        return FALSE; 
    }
    
    return TRUE; 
}

static gboolean game_tick(gpointer data) {
    small_blast_x = -1; small_blast_y = -1;
    
    // 1. --- Animation Update (Always Run) ---
    star_tick_counter++;
    if (star_tick_counter >= 3) { 
        star_scroll_offset = (star_scroll_offset + 1) % SCREEN_HEIGHT;
        star_tick_counter = 0;
    }
    
    gtk_widget_queue_draw(drawing_area);
    
    // 2. --- Running Game Logic ---
    if (game_state != GAME_STATE_RUNNING) { 
        return TRUE;
    }

    int new_level = (score / 100) + 1;
    if (new_level > game_difficulty_level) {
        game_difficulty_level = new_level;
        
        char level_str[50];
        sprintf(level_str, "LEVEL: %d", game_difficulty_level);
        gtk_label_set_text(GTK_LABEL(level_text_label), level_str);
    }
    
    updatePlayerBullets();
    updateEnemyBullets(); 
    updateEnemies(); 
    checkCollisions();

    char score_str[50];
    sprintf(score_str, "SCORE: %d", score);
    gtk_label_set_text(GTK_LABEL(score_text_label), score_str);

    return TRUE;
}

static gboolean end_instructions_delay(gpointer data) {
    countdown_value = 5; 
    
    if (countdown_timer_id != 0) {
        g_source_remove(countdown_timer_id);
    }
    
    countdown_timer_id = g_timeout_add(1000, countdown_tick, NULL); 
    
    gtk_widget_queue_draw(drawing_area);
    return FALSE; 
}


// --- GTK Event Handlers (UNCHANGED) ---

static void restart_game() {
    if (game_over_delay_timer_id != 0) {
        g_source_remove(game_over_delay_timer_id);
        game_over_delay_timer_id = 0;
    }
    
    game_state = GAME_STATE_RUNNING; 
    initializeGame(); 

    if (timer_id == 0) {
        timer_id = g_timeout_add(FRAME_DELAY_MS, game_tick, NULL);
    }
    gtk_widget_show(vbox_game_content);
    gtk_widget_queue_draw(drawing_area);
}

static void go_to_home_screen() {
    if (invulnerability_timer_id != 0) { g_source_remove(invulnerability_timer_id); invulnerability_timer_id = 0; }
    if (game_over_delay_timer_id != 0) { g_source_remove(game_over_delay_timer_id); game_over_delay_timer_id = 0; }
    if (countdown_timer_id != 0) { g_source_remove(countdown_timer_id); countdown_timer_id = 0; }

    game_state = GAME_STATE_HOME; 
    initializeGame(); 
    
    if (timer_id == 0) {
        timer_id = g_timeout_add(FRAME_DELAY_MS, game_tick, NULL);
    }

    gtk_widget_hide(vbox_game_content); 
    gtk_widget_show(vbox_home);         
    
    GtkWidget *parent_window = gtk_widget_get_ancestor(drawing_area, GTK_TYPE_WINDOW);
    if (parent_window) gtk_widget_grab_focus(parent_window);
    
    gtk_widget_queue_draw(drawing_area);
}


static void start_game_button_clicked(GtkWidget *widget, gpointer data) {
    gtk_widget_hide(vbox_home);         
    gtk_widget_show(vbox_game_content); 

    game_state = GAME_STATE_INSTRUCTIONS;
    initializeGame(); 

    end_instructions_delay(NULL); 

    gtk_widget_queue_draw(drawing_area);
}

// --- DRAWING LOGIC (MODIFIED HOME SCREEN DRAWING) ---

static void draw_star_field(cairo_t *cr, double cell_w, double cell_h) {
    cairo_set_source_rgb(cr, 1.0, 1.0, 1.0);
        
    srand(1234); 
    for (int i = 0; i < 50; i++) {
        int star_x = rand() % SCREEN_WIDTH;
        int star_y_base = rand() % SCREEN_HEIGHT;
        
        int star_y = (star_y_base + star_scroll_offset) % SCREEN_HEIGHT;
        
        cairo_rectangle(cr, star_x * cell_w, star_y * cell_h, cell_w * 0.1, cell_h * 0.1);
        cairo_fill(cr);
    }
}

// MODIFIED: Simplified home screen drawing to only include the large, centered titles.
void draw_home_ship_and_title(cairo_t *cr, double current_height, double current_width) {
    
    // 1. Draw Title Text
    cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);

    // Calculate vertical center point (35% down for the title block)
    double base_y = current_height * 0.35; 

    // Draw SPACE 
    cairo_set_source_rgb(cr, 1.0, 0.0, 0.0); // Red
    draw_centered_text(cr, current_width, "SPACE", base_y, 80.0); // Larger font size
    
    // Draw SHOOTER 
    base_y += 90.0; // Move down for the second line
    cairo_set_source_rgb(cr, 0.0, 1.0, 0.0); // Green
    draw_centered_text(cr, current_width, "SHOOTER", base_y, 80.0); // Larger font size
}


static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data) {
    
    cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
    cairo_paint(cr);

    double current_width = (double)gtk_widget_get_allocated_width(widget);
    double current_height = (double)gtk_widget_get_allocated_height(widget);
    
    double cell_w = current_width / SCREEN_WIDTH;
    double cell_h = current_height / SCREEN_HEIGHT;
    double current_y;
    
    
    // --- Star Field Background (Always drawn for HOME/MENU states) ---
    if (game_state == GAME_STATE_HOME) { 
        draw_star_field(cr, cell_w, cell_h);
    }
    
    // --- Home Screen Drawing ---
    if (game_state == GAME_STATE_HOME) {
        // MODIFIED: Call new function signature
        draw_home_ship_and_title(cr, current_height, current_width);
        return FALSE; 
    }
    
    // --- Instructions Screen (UNCHANGED LOGIC) ---
    if (game_state == GAME_STATE_INSTRUCTIONS) {
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); 
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        
        current_y = 50.0;
        
        draw_centered_text(cr, current_width, "INSTRUCTIONS:", current_y, 20.0); current_y += 40;
        draw_centered_text(cr, current_width, "Objective: Destroy all incoming enemies before they hit you!", current_y, 18.0); current_y += 50;
        draw_centered_text(cr, current_width, "Controls:", current_y, 20.0); current_y += 25;
        cairo_set_font_size(cr, 18.0);
        draw_centered_text(cr, current_width, "A  : Move Left", current_y, 18.0); current_y += 20;
        draw_centered_text(cr, current_width, "D  : Move Right", current_y, 18.0); current_y += 20;
        draw_centered_text(cr, current_width, "SPACE: Fire Bullet", current_y, 18.0); current_y += 35;
        draw_centered_text(cr, current_width, "Q: Quit Game", current_y, 18.0); current_y += 20;
        draw_centered_text(cr, current_width, "P: Pause Game", current_y, 18.0); current_y += 20;
        draw_centered_text(cr, current_width, "R: Restart (from Game Over)", current_y, 18.0); current_y += 20;
        draw_centered_text(cr, current_width, "H: Home (from Game Over)", current_y, 18.0); current_y += 40;
        draw_centered_text(cr, current_width, "Enemy Types:", current_y, 20.0); current_y += 25;
        cairo_set_source_rgb(cr, 1.0, 0.0, 0.0); 
        draw_centered_text(cr, current_width, "Red Circles (10 Pts): Standard Speed, Shoot at 250 Score", current_y, 18.0); current_y += 20;
        cairo_set_source_rgb(cr, 0.0, 0.0, 1.0); 
        draw_centered_text(cr, current_width, "Blue Circles (20 Pts): Swift Speed & Larger Target", current_y, 18.0); current_y += 40;
        
        char countdown_str[20];
        if (countdown_value > 0) {
            sprintf(countdown_str, "%d...", countdown_value);
            cairo_set_source_rgb(cr, 1.0, 1.0, 0.0); 
            draw_centered_text(cr, current_width, "GET READY!", current_height - 80.0, 25.0);
            draw_centered_text(cr, current_width, countdown_str, current_height - 50.0, 30.0);
        } else if (countdown_value == 0 && countdown_timer_id != 0) {
            cairo_set_source_rgb(cr, 0.0, 1.0, 0.0); 
            draw_centered_text(cr, current_width, "GO!!!", current_height - 50.0, 40.0);
        }
        
        return FALSE;
    }

    // --- Game Over Screen (UNCHANGED LOGIC) ---
    if (game_state == GAME_STATE_OVER) { 
        cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); 
        cairo_select_font_face(cr, "Sans", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
        
        current_y = current_height / 2 - 40;
        
        draw_centered_text(cr, current_width, "GAME OVER!", current_y, 30.0); current_y += 40;
        
        char final_score_str[100];
        sprintf(final_score_str, "Final Score: %d", score);
        draw_centered_text(cr, current_width, final_score_str, current_y, 20.0); current_y += 40;
        
        cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
        draw_centered_text(cr, current_width, "Press 'R' to Restart or 'H' for Home Screen", current_y, 15.0);
        
        return FALSE;
    }
    
    // --- Drawing Game Entities (RUNNING or PAUSED) (UNCHANGED LOGIC) ---

    if (game_state == GAME_STATE_RUNNING || game_state == GAME_STATE_PAUSED) {
        
        double player_w_scaled = cell_w * PLAYER_SIZE_SCALE;
        double player_h_scaled = cell_h * PLAYER_SIZE_SCALE;
        double offset_x = (player_w_scaled - cell_w) / 2.0;
        double offset_y = (player_h_scaled - cell_h) / 2.0;
        
        if (player.active) {
            int should_draw = 1;
            if (is_player_invulnerable && (rand() % 4 == 0)) { should_draw = 0; }
            if (big_blast_x != -1 && player_lives <= 0) { should_draw = 0; }

            if (should_draw) {
                cairo_set_source_rgb(cr, 0.0, 0.8, 0.0); 
                double base_x = player.x * cell_w - offset_x;
                double base_y = player.y * cell_h - offset_y;
                
                cairo_move_to(cr, base_x + player_w_scaled / 2.0, base_y);
                cairo_line_to(cr, base_x, base_y + player_h_scaled);
                cairo_line_to(cr, base_x + player_w_scaled, base_y + player_h_scaled);
                cairo_close_path(cr);
                cairo_fill(cr);
            }
        }

        cairo_set_source_rgb(cr, 1.0, 1.0, 0.0); 
        cairo_set_line_width(cr, cell_w / 8.0); 
        for (int i = 0; i < MAX_PLAYER_BULLETS; i++) {
            if (player_bullets[i].active) {
                double bx = player_bullets[i].x * cell_w + cell_w / 2.0; 
                double by_top = player_bullets[i].y * cell_h;
                double by_bottom = player_bullets[i].y * cell_h + cell_h; 
                cairo_move_to(cr, bx, by_top);
                cairo_line_to(cr, bx, by_bottom);
                cairo_stroke(cr);
            }
        }
        
        cairo_set_source_rgb(cr, 1.0, 0.0, 0.0); 
        cairo_set_line_width(cr, cell_w / 8.0); 
        for (int i = 0; i < MAX_ENEMY_BULLETS; i++) {
            if (enemy_bullets[i].active) {
                double bx = enemy_bullets[i].x * cell_w + cell_w / 2.0; 
                double by_top = enemy_bullets[i].y * cell_h;
                double by_bottom = enemy_bullets[i].y * cell_h + cell_h; 
                cairo_move_to(cr, bx, by_top);
                cairo_line_to(cr, bx, by_bottom);
                cairo_stroke(cr);
            }
        }

        for (int i = 0; i < MAX_ENEMIES; i++) {
            if (enemies[i].active) {
                double radius;
                
                if (enemies[i].type == 0) {
                    cairo_set_source_rgb(cr, 1.0, 0.0, 0.0); 
                    radius = cell_w * RED_ENEMY_SCALE; 
                } else {
                    cairo_set_source_rgb(cr, 0.0, 0.0, 1.0); 
                    radius = cell_w * BLUE_ENEMY_SCALE; 
                }
                
                double center_x = enemies[i].x * cell_w + cell_w / 2.0;
                double center_y = enemies[i].y * cell_h + cell_h / 2.0;
                
                cairo_arc(cr, center_x, center_y, radius, 0, 2 * M_PI);
                cairo_fill(cr);
            }
        }
    
        if (small_blast_x != -1) {
            double center_x = small_blast_x * cell_w + cell_w / 2.0;
            double center_y = small_blast_y * cell_h + cell_h / 2.0;
            double radius = cell_w * 0.7; 
            cairo_set_source_rgb(cr, 1.0, 0.6, 0.0); 
            cairo_arc(cr, center_x, center_y, radius, 0, 2 * M_PI);
            cairo_fill(cr);
        }

        if (big_blast_x != -1) {
            
            #define NUM_BLAST_RAYS 12
            #define RAY_LENGTH_SCALE 2.5
            #define RAY_WIDTH_SCALE 0.15 
            
            double center_x = big_blast_x * cell_w + cell_w / 2.0;
            double center_y = big_blast_y * cell_h + cell_h / 2.0;
            
            double ray_length = cell_w * RAY_LENGTH_SCALE;
            double ray_width = cell_w * RAY_WIDTH_SCALE;
            
            cairo_set_source_rgb(cr, 1.0, 0.8, 0.0); 
            cairo_set_line_cap(cr, CAIRO_LINE_CAP_ROUND);
            cairo_set_line_width(cr, ray_width);

            for (int i = 0; i < NUM_BLAST_RAYS; i++) {
                double angle = i * (2 * M_PI / NUM_BLAST_RAYS);
                
                double start_x = center_x + (cell_w * 1.0 * cos(angle)); 
                double start_y = center_y + (cell_w * 1.0 * sin(angle));
                
                double end_x = center_x + (ray_length * cos(angle));
                double end_y = center_y + (ray_length * sin(angle));

                cairo_move_to(cr, start_x, start_y);
                cairo_line_to(cr, end_x, end_y);
                cairo_stroke(cr);
            }

            double inner_radius = cell_w * 0.5;
            double outer_radius = cell_w * 1.5;

            cairo_pattern_t *pattern = cairo_pattern_create_radial(
                center_x, center_y, inner_radius * 0.5, 
                center_x, center_y, outer_radius         
            );

            cairo_pattern_add_color_stop_rgb(pattern, 0.0, 1.0, 1.0, 0.7); 
            cairo_pattern_add_color_stop_rgb(pattern, 0.3, 1.0, 0.8, 0.0); 
            cairo_pattern_add_color_stop_rgb(pattern, 1.0, 0.8, 0.2, 0.0); 

            cairo_set_source(cr, pattern);
            
            cairo_arc(cr, center_x, center_y, outer_radius, 0, 2 * M_PI);
            cairo_fill(cr);

            cairo_pattern_destroy(pattern);
        }

        if (game_state == GAME_STATE_PAUSED) {
            if (game_over_delay_timer_id == 0) { 
                cairo_set_source_rgb(cr, 1.0, 1.0, 1.0); 
                current_y = current_height / 2 - 20;
                
                draw_centered_text(cr, current_width, "PAUSED", current_y, 40.0); current_y += 60;

                cairo_set_source_rgb(cr, 0.8, 0.8, 0.8);
                draw_centered_text(cr, current_width, "Press 'P' to resume, 'R' to restart, or 'H' for Home", current_y, 15.0);
            }
        }
    }
    
    return FALSE;
}

// --- KEY PRESS HANDLER (UNCHANGED) ---

static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) {
    
    if (event->keyval == GDK_KEY_q || event->keyval == GDK_KEY_Q) {
        gtk_main_quit();
        return TRUE;
    }

    if (game_state == GAME_STATE_HOME) { 
        if (event->keyval == GDK_KEY_s || event->keyval == GDK_KEY_S) {
            start_game_button_clicked(NULL, NULL); 
            return TRUE;
        }
        return FALSE;
    }

    if (game_state == GAME_STATE_OVER) { 
        switch (event->keyval) {
            case GDK_KEY_r: 
            case GDK_KEY_R:
                restart_game();
                return TRUE;
            case GDK_KEY_h:
            case GDK_KEY_H:
                go_to_home_screen();
                return TRUE;
            default:
                return FALSE;
        }
    }

    if (game_state == GAME_STATE_RUNNING || game_state == GAME_STATE_PAUSED) {
        
        if (game_state == GAME_STATE_PAUSED && game_over_delay_timer_id == 0) { 
             switch (event->keyval) {
                case GDK_KEY_r: 
                case GDK_KEY_R:
                    restart_game();
                    return TRUE;
                case GDK_KEY_h:
                case GDK_KEY_H:
                    go_to_home_screen();
                    return TRUE;
             }
        }
        
        if (event->keyval == GDK_KEY_p || event->keyval == GDK_KEY_P) {
            if (game_state == GAME_STATE_RUNNING) {
                game_state = GAME_STATE_PAUSED;
            } else if (game_state == GAME_STATE_PAUSED) {
                if (game_over_delay_timer_id == 0) {
                    game_state = GAME_STATE_RUNNING;
                }
            }
            gtk_widget_queue_draw(drawing_area); 
            return TRUE;
        }
    }
    
    if (game_state == GAME_STATE_RUNNING) {
        switch (event->keyval) {
            case GDK_KEY_a:
            case GDK_KEY_A:
                if (player.x > 0) player.x--;
                return TRUE;
            case GDK_KEY_d:
            case GDK_KEY_D:
                if (player.x < SCREEN_WIDTH - 1) player.x++;
                return TRUE;
            case GDK_KEY_space:
                for (int i = 0; i < MAX_PLAYER_BULLETS; i++) {
                    if (!player_bullets[i].active) {
                        player_bullets[i].active = 1;
                        player_bullets[i].is_enemy = 0;
                        player_bullets[i].x = player.x;
                        player_bullets[i].y = player.y - 1;
                        break;
                    }
                }
                return TRUE;
            default:
                return FALSE;
        }
    }
    
    return FALSE;
}

// --- GTK ACTIVATION (FIXED START BUTTON CONNECTION) ---

static void activate(GtkApplication *app, gpointer user_data) {
    GtkWidget *window;
    GtkWidget *vbox_main; 
    GtkWidget *start_button;
    GtkWidget *hbox_hud;
    GtkWidget *spacer_top;  
    
    const int MIN_WIDTH = DEFAULT_WINDOW_WIDTH;
    const int MIN_HEIGHT = DEFAULT_WINDOW_HEIGHT + HUD_AREA_HEIGHT + (5 * 2);

    game_state = GAME_STATE_HOME; 
    initializeGame();

    window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(window), "GTK Space Shooter");
    
    gtk_window_set_default_size(GTK_WINDOW(window), MIN_WIDTH, MIN_HEIGHT);
    gtk_container_set_border_width(GTK_CONTAINER(window), 5);
    
    GdkGeometry hints = {
        .min_width = MIN_WIDTH,
        .min_height = MIN_HEIGHT
    };
    gtk_window_set_geometry_hints(
        GTK_WINDOW(window), 
        NULL, 
        &hints, 
        GDK_HINT_MIN_SIZE
    );

    vbox_main = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox_main);
    
    // 1. Drawing Area setup - Base content
    drawing_area = gtk_drawing_area_new();
    
    // 2. HUD Content (vbox_game_content)
    vbox_game_content = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    
    hbox_hud = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5); 
    gtk_box_pack_start(GTK_BOX(vbox_game_content), hbox_hud, FALSE, FALSE, 5); 
    
    score_text_label = gtk_label_new("SCORE: 0");
    gtk_box_pack_start(GTK_BOX(hbox_hud), score_text_label, TRUE, TRUE, 5); 
    
    GdkRGBA white_color = { 1.0, 1.0, 1.0, 1.0 }; 
    gtk_widget_override_color(score_text_label, GTK_STATE_FLAG_NORMAL, &white_color);

    heart_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2); 
    gtk_box_pack_start(GTK_BOX(hbox_hud), heart_box, FALSE, FALSE, 0); 

    for (int i = 0; i < MAX_LIVES; i++) {
        hearts[i] = gtk_label_new(NULL); 
        gtk_label_set_markup(GTK_LABEL(hearts[i]), "<span size='large' foreground='red'>♥</span>");
        gtk_box_pack_start(GTK_BOX(heart_box), hearts[i], FALSE, FALSE, 0);
    }

    level_text_label = gtk_label_new("LEVEL: 1");
    gtk_box_pack_start(GTK_BOX(hbox_hud), level_text_label, TRUE, TRUE, 5); 
    
    gtk_widget_override_color(level_text_label, GTK_STATE_FLAG_NORMAL, &white_color);


    // --- 3. Home Menu Controls (vbox_home) ---
    vbox_home = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20); 
    
    gtk_widget_set_valign(vbox_home, GTK_ALIGN_CENTER); 
    gtk_widget_set_halign(vbox_home, GTK_ALIGN_CENTER); 
    
    
    // --- POSITIONING LOGIC FOR CENTERING TITLE/BUTTON BLOCK ---
    spacer_top = gtk_label_new(NULL); 
    gtk_widget_set_size_request(spacer_top, 1, DEFAULT_WINDOW_HEIGHT * 0.40); 
    gtk_box_pack_start(GTK_BOX(vbox_home), spacer_top, FALSE, FALSE, 0);

    // Start Button 
    start_button = gtk_button_new_with_label("PRESS 'S' or CLICK TO START");
    gtk_box_pack_start(GTK_BOX(vbox_home), start_button, FALSE, FALSE, 10);

    // FIX: Connect the button click signal
    // The clicked signal was missing its connection to the callback function.
    g_signal_connect(start_button, "clicked", G_CALLBACK(start_game_button_clicked), NULL);
    
    
    // --- 4. Assemble into GtkOverlay (UNCHANGED) ---
    
    overlay = gtk_overlay_new();
    gtk_container_add(GTK_CONTAINER(overlay), drawing_area); 
    
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), vbox_game_content);
    gtk_widget_set_valign(vbox_game_content, GTK_ALIGN_START); 
    
    gtk_overlay_add_overlay(GTK_OVERLAY(overlay), vbox_home);
    
    gtk_box_pack_start(GTK_BOX(vbox_main), overlay, TRUE, TRUE, 0); 
    
    
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(drawing_area, "draw", G_CALLBACK(on_draw_event), NULL);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), NULL);
    
    
    // --- Initial State Configuration ---
    gtk_widget_show_all(window);
    
    gtk_widget_show(vbox_home); 
    gtk_widget_hide(vbox_game_content); 
    gtk_widget_grab_focus(window);
    
    update_hearts_display();
    
    if (timer_id == 0) {
        timer_id = g_timeout_add(FRAME_DELAY_MS, game_tick, NULL);
    }
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new("org.gtk.spaceshooter", G_APPLICATION_FLAGS_NONE);
    
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    
    g_object_unref(app);
    
    return status;
}
