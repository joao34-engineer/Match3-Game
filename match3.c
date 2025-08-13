#include "raylib.h"
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <math.h>

#define BOARD_SIZE 8
#define TILE_SIZE 42
#define TILE_TYPES 5
#define SCORE_FONT_SIZE 32
#define MAX_SCORE_POPUPS 32

const char tile_chars[TILE_TYPES] = { '#', '@', '$', '%', '&' };

char board[BOARD_SIZE][BOARD_SIZE];
bool matched[BOARD_SIZE][BOARD_SIZE] = { 0 };
float fall_offset[BOARD_SIZE][BOARD_SIZE] = { 0 };

int score = 0;
Vector2 grid_origin;
Texture2D background;
Font score_font;
Vector2 selected_tile = { -1, -1 };
float fall_speed = 8.0f;
float match_delay_timer = 0.0f;
const float MATCH_DELAY_DURATION = 0.2f;

float score_scale = 1.0f;
float score_scale_velocity = 0.0f;
bool score_animating = false;

Music background_music;
Sound match_sound;

Texture2D tile_textures[TILE_TYPES];

typedef enum {
	STATE_IDLE,
	STATE_ANIMATING,
	STATE_MATCH_DELAY
} TileState;

TileState tile_state;

typedef struct {
	Vector2 position;
	int amount;
	float lifetime;
	float alpha;
	bool active;
} ScorePopup;

ScorePopup score_popups[MAX_SCORE_POPUPS] = { 0 };

char random_tile() {
	return tile_chars[rand() % TILE_TYPES];
}

void swap_tiles(int x1, int y1, int x2, int y2) {
	char temp = board[y1][x1];
	board[y1][x1] = board[y2][x2];
	board[y2][x2] = temp;
}

bool are_tiles_adjacent(Vector2 a, Vector2 b) {
	return (abs((int)a.x - (int)b.x) + abs((int)a.y - (int)b.y)) == 1;
}

void add_score_popup(int x, int y, int amount, Vector2 grid_origin) {
	for (int i = 0; i < MAX_SCORE_POPUPS; i++) {
		if (!score_popups[i].active) {
			score_popups[i].position = (Vector2){
				grid_origin.x + x * TILE_SIZE + TILE_SIZE / 2,
				grid_origin.y + y * TILE_SIZE + TILE_SIZE / 2
			};
			score_popups[i].amount = amount;
			score_popups[i].lifetime = 1.0f;
			score_popups[i].alpha = 1.0f;
			score_popups[i].active = true;
			break;
		}
	}
}

bool find_matches() {
	bool found = false;
	for (int y = 0; y < BOARD_SIZE; y++) {
		for (int x = 0; x < BOARD_SIZE; x++) {
			matched[y][x] = false;
		}
	}

	for (int y = 0; y < BOARD_SIZE; y++) {
		for (int x = 0; x < BOARD_SIZE - 2; x++) {
			char t = board[y][x];
			if (t == board[y][x + 1] &&
				t == board[y][x + 2]) {
				matched[y][x] = matched[y][x + 1] = matched[y][x + 2] = true;
				// update score
				score += 10;
				found = true;
				PlaySound(match_sound);

				score_animating = true;
				score_scale = 2.0f;
				score_scale_velocity = -2.5f;

				add_score_popup(x, y, 10, grid_origin);
			}
		}
	}

	for (int x = 0; x < BOARD_SIZE; x++) {
		for (int y = 0; y < BOARD_SIZE - 2; y++) {
			char t = board[y][x];
			if (t == board[y + 1][x] &&
				t == board[y + 2][x]) {
				matched[y][x] = matched[y + 1][x] = matched[y + 2][x] = true;
				score += 10;
				found = true;
				PlaySound(match_sound);

				score_animating = true;
				score_scale = 2.0f;
				score_scale_velocity = -2.5f;

				add_score_popup(x, y, 10, grid_origin);
			}
		}
	}

	return found;
}

void resolve_matches() {
	for (int x = 0; x < BOARD_SIZE; x++) {
		int write_y = BOARD_SIZE - 1;
		for (int y = BOARD_SIZE - 1; y >= 0; y--) {
			if (!matched[y][x]) {
				if (y != write_y) {
					board[write_y][x] = board[y][x];
					fall_offset[write_y][x] = (write_y - y) * TILE_SIZE;
					board[y][x] = ' ';
				}
				write_y--;
			}
		}

		// fill empty spots with new random tiles
		while (write_y >= 0) {
			board[write_y][x] = random_tile();
			fall_offset[write_y][x] = (write_y + 1) * TILE_SIZE;
			write_y--;
		}
	}

	tile_state = STATE_ANIMATING;
}

void init_board() {
	for (int y = 0; y < BOARD_SIZE; y++) {
		for (int x = 0; x < BOARD_SIZE; x++) {
			board[y][x] = random_tile();
		}
	}

	int grid_width = BOARD_SIZE * TILE_SIZE;
	int grid_height = BOARD_SIZE * TILE_SIZE;

	grid_origin = (Vector2){
		(GetScreenWidth() - grid_width) / 2,
		(GetScreenHeight() - grid_height) / 2
	};

	if (find_matches()) {
		resolve_matches();
	}
	else {
		tile_state = STATE_IDLE;
	}
}

void draw_generated_tile(int x, int y, char tile_type, bool is_matched) {
    float centerX = grid_origin.x + (x * TILE_SIZE) + TILE_SIZE/2;
    float centerY = grid_origin.y + (y * TILE_SIZE) + TILE_SIZE/2 - fall_offset[y][x];
    float size = TILE_SIZE * 0.7f;
    
    // Map tile character to texture index
    int textureIndex;
    switch(tile_type) {
        case '#': textureIndex = 0; break;  // Red diamond
        case '@': textureIndex = 1; break;  // Blue circle
        case '$': textureIndex = 2; break;  // Green square
        case '%': textureIndex = 3; break;  // Gold triangle
        case '&': textureIndex = 4; break;  // Purple hexagon
        default:  textureIndex = 0;
    }
    
    // Calculate position to draw the texture centered
    Rectangle destRect = {
        centerX - size/2,      // x position
        centerY - size/2,      // y position
        size,                  // width
        size                   // height
    };
    
    // Source rectangle (the entire texture)
    Rectangle srcRect = {
        0, 0, 
        tile_textures[textureIndex].width,
        tile_textures[textureIndex].height
    };
    
    // Draw the texture
    Color tintColor = matched[y][x] ? YELLOW : WHITE;
    DrawTexturePro(
        tile_textures[textureIndex],
        srcRect,
        destRect,
        (Vector2){ 0, 0 },
        0.0f,
        tintColor
    );
    
    // Add a glowing outline for matched tiles
    if (matched[y][x]) {
        DrawRectangleLinesEx(destRect, 2, YELLOW);
    }
    
    // Add a shine effect
    if (!matched[y][x]) {
        DrawCircle(centerX - size/5, centerY - size/5, size/10, Fade(WHITE, 0.7f));
    }
}

void generate_tile_textures() {
    Color colors[TILE_TYPES] = {
        RED,    // # - Red - Diamond
        BLUE,   // @ - Blue - Circle
        GREEN,  // $ - Green - Square
        GOLD,   // % - Gold - Triangle
        PURPLE  // & - Purple - Hexagon
    };
    
    int texture_size = 64;  // larger than TILE_SIZE for better quality
    
    for (int i = 0; i < TILE_TYPES; i++) {
        Image img = GenImageColor(texture_size, texture_size, BLANK);
        
        int center = texture_size/2;
        int radius = (int)(texture_size * 0.4f);
        
        // Draw shape on the image based on tile type
        switch(i) {
            case 0: { // Diamond (#) - Red
                // Draw a diamond shape (rotated square)
                Vector2 points[4] = {
                    { center, center - radius },           // top
                    { center + radius, center },           // right
                    { center, center + radius },           // bottom
                    { center - radius, center }            // left
                };
                
                // Fill the diamond
                for (int y = 0; y < texture_size; y++) {
                    for (int x = 0; x < texture_size; x++) {
                        // Simple point-in-polygon test for diamond
                        if (abs(x - center) + abs(y - center) <= radius) {
                            ImageDrawPixel(&img, x, y, colors[i]);
                        }
                    }
                }
                break;
            }
            
            case 1: // Circle (@) - Blue
                ImageDrawCircle(&img, center, center, radius, colors[i]);
                break;
                
            case 2: { // Square ($) - Green
                int square_size = radius * 2;
                int start_x = center - radius;
                int start_y = center - radius;
                ImageDrawRectangle(&img, start_x, start_y, square_size, square_size, colors[i]);
                break;
            }
                
            case 3: { // Triangle (%) - Gold
                // Draw a basic triangle
                Vector2 points[3] = {
                    { center, center - radius },                  // top
                    { center - radius, center + radius * 0.7f },  // bottom left
                    { center + radius, center + radius * 0.7f }   // bottom right
                };
                
                // Fill the triangle (simple approach)
                for (int y = 0; y < texture_size; y++) {
                    for (int x = 0; x < texture_size; x++) {
                        float d1 = (float)(y - points[0].y) / (points[1].y - points[0].y) - 
                                  (float)(x - points[0].x) / (points[1].x - points[0].x);
                        float d2 = (float)(y - points[1].y) / (points[2].y - points[1].y) - 
                                  (float)(x - points[1].x) / (points[2].x - points[1].x);
                        float d3 = (float)(y - points[2].y) / (points[0].y - points[2].y) - 
                                  (float)(x - points[2].x) / (points[0].x - points[2].x);
                        
                        // This is a simplified approach that works for our basic triangle
                        if (y >= points[0].y && y <= points[1].y && 
                            x >= center - (y - points[0].y) && x <= center + (y - points[0].y)) {
                            ImageDrawPixel(&img, x, y, colors[i]);
                        }
                    }
                }
                break;
            }
                
            case 4: { // Hexagon (&) - Purple
                // Draw a hexagon
                int hex_radius = radius;
                for (int y = 0; y < texture_size; y++) {
                    for (int x = 0; x < texture_size; x++) {
                        int dx = abs(x - center);
                        int dy = abs(y - center);
                        // Simple algorithm for hexagon hit testing
                        if (dx <= hex_radius/2 || (2*dy + dx <= hex_radius*1.5f)) {
                            ImageDrawPixel(&img, x, y, colors[i]);
                        }
                    }
                }
                break;
            }
        }
        
        // Add a shine effect in the top-left
        ImageDrawCircle(&img, center - radius/2, center - radius/2, radius/4, Fade(WHITE, 0.5f));
        
        // Add a border
        int border_thickness = 2;
        Color border_color = ColorBrightness(colors[i], -0.3f);
        
        // Top edge
        for (int x = center - radius; x <= center + radius; x++) {
            for (int b = 0; b < border_thickness; b++) {
                ImageDrawPixel(&img, x, center - radius + b, border_color);
            }
        }
        
        // Bottom edge
        for (int x = center - radius; x <= center + radius; x++) {
            for (int b = 0; b < border_thickness; b++) {
                ImageDrawPixel(&img, x, center + radius - b, border_color);
            }
        }
        
        // Left edge
        for (int y = center - radius; y <= center + radius; y++) {
            for (int b = 0; b < border_thickness; b++) {
                ImageDrawPixel(&img, center - radius + b, y, border_color);
            }
        }
        
        // Right edge
        for (int y = center - radius; y <= center + radius; y++) {
            for (int b = 0; b < border_thickness; b++) {
                ImageDrawPixel(&img, center + radius - b, y, border_color);
            }
        }
        
        tile_textures[i] = LoadTextureFromImage(img);
        UnloadImage(img);
    }
}

int main(void) {
	const int screen_width = 800;
	const int screen_height = 450;

	InitWindow(screen_width, screen_height, "Raylib 2D ASCII MATCH");
	SetTargetFPS(60);
	srand(time(NULL));

	InitAudioDevice();

	background = LoadTexture("assets/background.jpg");
	score_font = LoadFontEx("assets/04b03.ttf", SCORE_FONT_SIZE, NULL, 0);
	background_music = LoadMusicStream("assets/bgm.mp3");
	match_sound = LoadSound("assets/match.mp3");

	PlayMusicStream(background_music);

	generate_tile_textures();
	init_board();
	Vector2 mouse = { 0, 0 };

	while (!WindowShouldClose()) {

		UpdateMusicStream(background_music);

		// update game logic
		mouse = GetMousePosition();
		if (tile_state == STATE_IDLE && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
			int x = (mouse.x - grid_origin.x) / TILE_SIZE;
			int y = (mouse.y - grid_origin.y) / TILE_SIZE;
			if (x >= 0 && x < BOARD_SIZE && y >= 0 && y < BOARD_SIZE) {
				Vector2 current_tile = (Vector2){ x, y };
				if (selected_tile.x < 0) {
					selected_tile = current_tile;
				}
				else {
					if (are_tiles_adjacent(selected_tile, current_tile)) {
						swap_tiles(selected_tile.x, selected_tile.y, current_tile.x, current_tile.y);
						if (find_matches()) {
							resolve_matches();
						}
						else {
							swap_tiles(selected_tile.x, selected_tile.y, current_tile.x, current_tile.y);
						}
					}
					selected_tile = (Vector2){ -1, -1 };
				}
			}
		}

		if (tile_state == STATE_ANIMATING) {
			bool still_animating = false;

			for (int y = 0; y < BOARD_SIZE; y++) {
				for (int x = 0; x < BOARD_SIZE; x++) {
					if (fall_offset[y][x] > 0) {
						fall_offset[y][x] -= fall_speed;
						if (fall_offset[y][x] < 0) {
							fall_offset[y][x] = 0;
						}
						else {
							still_animating = true;
						}

					}
				}
			}

			if (!still_animating) {
				tile_state = STATE_MATCH_DELAY;
				match_delay_timer = MATCH_DELAY_DURATION;
			}
		}

		if (tile_state == STATE_MATCH_DELAY) {
			match_delay_timer -= GetFrameTime();
			if (match_delay_timer <= 0.0f) {
				if (find_matches()) {
					resolve_matches();
				}
				else {
					tile_state = STATE_IDLE;
				}
			}
		}

		// update our score popups array
		for (int i = 0; i < MAX_SCORE_POPUPS; i++) {
			if (score_popups[i].active) {
				score_popups[i].lifetime -= GetFrameTime();
				score_popups[i].position.y -= 30 * GetFrameTime();
				score_popups[i].alpha = score_popups[i].lifetime;

				if (score_popups[i].lifetime <= 0.0f) {
					score_popups[i].active = false;
				}
			}
		}

		// update the score animation
		if (score_animating) {
			score_scale += score_scale_velocity * GetFrameTime();
			if (score_scale <= 1.0f) {
				score_scale = 1.0f;
				score_animating = false;
			}
		}


		BeginDrawing();
		ClearBackground(BLACK);

		DrawTexturePro(
			background,
			(Rectangle) {
			0, 0, background.width, background.height
		},
			(Rectangle) {
			0, 0, GetScreenWidth(), GetScreenHeight()
		},
			(Vector2) {
			0, 0
		},
			0.0f,
			WHITE
		);

		DrawRectangle(
			grid_origin.x,
			grid_origin.y,
			BOARD_SIZE * TILE_SIZE,
			BOARD_SIZE * TILE_SIZE,
			Fade(DARKGRAY, 0.60f)
		);

		for (int y = 0; y < BOARD_SIZE; y++) {
			for (int x = 0; x < BOARD_SIZE; x++) {
				Rectangle rect = {
					grid_origin.x + (x * TILE_SIZE),
					grid_origin.y + (y * TILE_SIZE),
					TILE_SIZE,
					TILE_SIZE
				};

				DrawRectangleLinesEx(rect, 1, DARKGRAY);

				if (board[y][x] != ' ') {
					draw_generated_tile(x, y, board[y][x], matched[y][x]);
				}
			}
		}

		// draw selected tile
		if (selected_tile.x >= 0) {
			DrawRectangleLinesEx((Rectangle) {
				grid_origin.x + (selected_tile.x * TILE_SIZE),
					grid_origin.y + (selected_tile.y * TILE_SIZE),
					TILE_SIZE, TILE_SIZE
			}, 2, YELLOW);
		}

		DrawTextEx(
			score_font,
			TextFormat("SCORE: %d", score),
			(Vector2) {
			20, 20
		},
			SCORE_FONT_SIZE* score_scale,
			1.0f,
			YELLOW
		);

		// draw score popups
		for (int i = 0; i < MAX_SCORE_POPUPS; i++) {
			if (score_popups[i].active) {
				Color c = Fade(YELLOW, score_popups[i].alpha);
				DrawText(
					TextFormat("+%d", score_popups[i].amount),
					score_popups[i].position.x,
					score_popups[i].position.y,
					20, c);
			}
		}

		EndDrawing();
	}

	StopMusicStream(background_music);
	UnloadMusicStream(background_music);
	UnloadSound(match_sound);
	UnloadTexture(background);
	UnloadFont(score_font);

	for (int i = 0; i < TILE_TYPES; i++) {
		UnloadTexture(tile_textures[i]);
	}

	CloseAudioDevice();

	CloseWindow();
	return 0;
}

