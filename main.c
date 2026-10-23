#include <raylib.h>
#include <raymath.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#define DA_IMPLEMENTATION
#include "da.h"

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

#define PART_COUNT (1 << 18)
#define WIDTH  (800 * 2)
#define HEIGHT (600 * 2)

#define SHM_CELL_SIZE 20
#define SHM_ROWS (int32_t) ((HEIGHT + SHM_CELL_SIZE - 1) / SHM_CELL_SIZE)
#define SHM_COLS (int32_t) ((WIDTH  + SHM_CELL_SIZE - 1) / SHM_CELL_SIZE)
#define TOTAL_CELLS (SHM_ROWS * SHM_COLS)

#define INTERACTION_RADIUS     6.0f
#define INTERACTION_RADIUS_SQ  (INTERACTION_RADIUS * INTERACTION_RADIUS)

typedef struct {
    float pos_x[PART_COUNT];
    float pos_y[PART_COUNT];
    float vel_x[PART_COUNT];
    float vel_y[PART_COUNT];
} Particles;

typedef struct {
    int32_t count[TOTAL_CELLS];
    int32_t offset[TOTAL_CELLS];
} SpatialHashGrid;

static Particles buffer_a;
static Particles buffer_b;
static Particles *curr_particles = &buffer_a;
static Particles *next_particles = &buffer_b;

static SpatialHashGrid grid;
static Image screen_image;
static Texture2D screen_texture;

static inline int32_t get_cell_index(float x, float y)
{
    int32_t col   = (int32_t)(x / SHM_CELL_SIZE);
    int32_t row   = (int32_t)(y / SHM_CELL_SIZE);
    col   = (int32_t)Clamp(col, 0, SHM_COLS - 1);
    row   = (int32_t)Clamp(row, 0, SHM_ROWS - 1);
    return col * SHM_ROWS + row;
}

void init_particles(void) {
    for (size_t i = 0; i < PART_COUNT; ++i) {
        curr_particles->pos_x[i] = (float)(rand() % WIDTH);
        curr_particles->pos_y[i] = (float)(rand() % HEIGHT);
        curr_particles->vel_x[i] = (float)((rand() % 200) - 100) * 0.5f;
        curr_particles->vel_y[i] = (float)((rand() % 200) - 100) * 0.5f;
    }
}

void update_particles(float dt)
{
    for (size_t i = 0; i < TOTAL_CELLS; ++i) grid.count[i] = 0;

    for(size_t i = 0; i < PART_COUNT; ++i)
    {
        float px = curr_particles->pos_x[i] + curr_particles->vel_x[i] * dt;
        float py = curr_particles->pos_y[i] + curr_particles->vel_y[i] * dt;
        float vx = curr_particles->vel_x[i];
        float vy = curr_particles->vel_y[i];

        if(px < 0.0f)
        { 
            px = 0.0f; vx *= -1.0f; 
        } 
        else if(px >= (float) WIDTH) 
        { 
            px = (float) (WIDTH - 1); vx *= -1.0f; 
        }

        if(py < 0.0f) 
        { 
            py = 0.0f; vy *= -1.0f; 
        } 
        else if(py >= (float) HEIGHT)
        { 
            py = (float) (HEIGHT - 1); vy *= -1.0f; 
        }

        curr_particles->pos_x[i] = px;
        curr_particles->pos_y[i] = py;
        curr_particles->vel_x[i] = vx;
        curr_particles->vel_y[i] = vy;

        grid.count[get_cell_index(px, py)]++;
    }

    int32_t current_offset = 0;
    for (size_t i = 0; i < TOTAL_CELLS; ++i) 
    {
        grid.offset[i] = current_offset;
        current_offset += grid.count[i];
        grid.count[i] = 0;
    }

    for(size_t i = 0; i < PART_COUNT; ++i)
    {
        float px = curr_particles->pos_x[i];
        float py = curr_particles->pos_y[i];
        int32_t c_idx = get_cell_index(px, py);
        int32_t dest_idx = grid.offset[c_idx] + grid.count[c_idx];
        grid.count[c_idx]++;

        next_particles->pos_x[dest_idx] = px;
        next_particles->pos_y[dest_idx] = py;
        next_particles->vel_x[dest_idx] = curr_particles->vel_x[i];
        next_particles->vel_y[dest_idx] = curr_particles->vel_y[i];
    }

    Particles *temp = curr_particles;
    curr_particles = next_particles;
    next_particles = temp;
}

void resolve_collisions(void)
{
    for(int32_t i = 0; i < PART_COUNT; ++i)
    {
        float px = curr_particles->pos_x[i];
        float py = curr_particles->pos_y[i];

        int32_t col = (int32_t) (px / SHM_CELL_SIZE);
        int32_t row = (int32_t) (py / SHM_CELL_SIZE);
        col = Clamp(col, 0, SHM_COLS - 1);
        row = Clamp(row, 0, SHM_ROWS - 1);

        int32_t start_col = max(0, col - 1);
        int32_t end_col   = min(col + 1, SHM_COLS - 1);
        int32_t start_row = max(0, row - 1);
        int32_t end_row   = min(row + 1, SHM_ROWS - 1);


        for (int32_t c = start_col; c <= end_col; ++c) 
        {
            for (int32_t r = start_row; r <= end_row; ++r) 
            {
                int32_t n_cell_idx = c * SHM_ROWS + r;
                int32_t start_idx = grid.offset[n_cell_idx];
                int32_t end_idx = (n_cell_idx < TOTAL_CELLS - 1) ? grid.offset[n_cell_idx + 1] : PART_COUNT;

                for(int32_t j = start_idx; j < end_idx; ++j)
                {
                    if(i == j) continue;

                    float dx = curr_particles->pos_x[j] - px;
                    float dy = curr_particles->pos_y[j] - py;
                    float dist_sq = (dx * dx) + (dy * dy);

                    if (dist_sq < INTERACTION_RADIUS_SQ && dist_sq > 0.0001f)
                    {
                        float dist = sqrtf(dist_sq);
                        float overlap = INTERACTION_RADIUS - dist;
                        float nx = dx / dist;
                        float ny = dy / dist;

                        curr_particles->pos_x[j] += nx * overlap * 0.5f;
                        curr_particles->pos_y[j] += ny * overlap * 0.5f;
                        curr_particles->pos_x[i] -= nx * overlap * 0.5f;
                        curr_particles->pos_y[i] -= ny * overlap * 0.5f;
                    }
                }
            }
        }
    }
}

void render_particles(void) 
{
    ImageClearBackground(&screen_image, BLACK);
    for (int i = 0; i < PART_COUNT; ++i) 
    {
        int x = (int)curr_particles->pos_x[i];
        int y = (int)curr_particles->pos_y[i];
        if (x >= 0 && x < WIDTH && y >= 0 && y < HEIGHT) 
        {
            ImageDrawPixel(&screen_image, x, y, ORANGE);
        }
    }
    UpdateTexture(screen_texture, screen_image.data);
    DrawTexture(screen_texture, 0, 0, WHITE);
}

int main(void)
{
    srand(time(0));
    init_particles();
    InitWindow(WIDTH, HEIGHT, "Particle System");
    SetTargetFPS(60);

    screen_image = GenImageColor(WIDTH, HEIGHT, BLACK);
    ImageFormat(&screen_image, PIXELFORMAT_UNCOMPRESSED_R8G8B8A8);
    screen_texture = LoadTextureFromImage(screen_image);

    while(!WindowShouldClose())
    {
        float dt = GetFrameTime();
        if (dt > 0.1f) dt = 0.1f;

        update_particles(dt);
        resolve_collisions();

        ClearBackground(BLACK);
        BeginDrawing();
            ClearBackground(BLACK);
            render_particles();
            DrawText(TextFormat("FPS: %d", GetFPS()), 10, 10, 20, GREEN);
            DrawText(TextFormat("Particles: %d", PART_COUNT), 10, 35, 20, GREEN);
        EndDrawing();
    }

    UnloadTexture(screen_texture);
    UnloadImage(screen_image);
    CloseWindow();

    return 0;
}
