#include <raylib.h>
#include <raymath.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#define DA_IMPLEMENTATION
#include "da.h"

#define PART_COUNT (1 << 20)
#define PART_RADIUS 1
#define WIDTH  800
#define HEIGHT 600

#define SHM_CELL_SIZE 20
#define SHM_ROWS (int32_t) ((WIDTH  + SHM_CELL_SIZE - 1) / SHM_CELL_SIZE)
#define SHM_COLS (int32_t) ((HEIGHT + SHM_CELL_SIZE - 1) / SHM_CELL_SIZE)

typedef struct {
    Vector2 pos;
    Vector2 vel;
} Particle;

typedef struct {
    size_t count;
    size_t capacity;
    Particle *items;
} Particles;

static size_t particle_count = 0;
static Particle particles[PART_COUNT];
static Particles cells[SHM_ROWS * SHM_COLS];    

void shm_install(Particle p)
{
    const int32_t col = (int32_t) p.pos.x / SHM_CELL_SIZE;
    const int32_t row = (int32_t) p.pos.y / SHM_CELL_SIZE;
    if (col >= 0 && col < SHM_COLS && row >= 0 && row < SHM_ROWS)
    {
      da_append(&cells[col * SHM_ROWS + row], p);
    }
}

void shm_nbors(Particle p, Particles *nbors)
{
    nbors->count = 0;
    const int32_t col = p.pos.x / SHM_CELL_SIZE;
    const int32_t row = p.pos.y / SHM_CELL_SIZE;
    const int search_radius = fmax(1.0f, PART_RADIUS / SHM_CELL_SIZE);
    
    const int32_t start_col = fmax(0, col - search_radius);
    const int32_t end_col   = fmin(SHM_COLS - 1, col + search_radius);
    const int32_t start_row = fmax(0, row - search_radius);
    const int32_t end_row   = fmin(SHM_ROWS - 1, row + search_radius);
    
    for(int32_t c = start_col; c < end_col; ++c)
    {
        for(int32_t r = start_row; r < end_row; ++r)
        {
            Particles parts = cells[c * SHM_ROWS + r];
            da_append_many(nbors, parts.items, parts.count * sizeof(*parts.items));
        }
    }
}

void shm_clear(void)
{
    for(size_t i = 0; i < SHM_ROWS * SHM_COLS; ++i)
    {
        cells[i].count = 0; 
    }
}

void install_particle(Particle p)
{
    shm_install(p);
    particles[particle_count++] = p;
}

void init_particles(void)
{
    for(size_t i = 0; i < PART_COUNT; ++i)
    {
        particles[i].pos.x = rand() % WIDTH;
        particles[i].pos.y = rand() % HEIGHT;
        particles[i].vel.x = rand() % WIDTH;
        particles[i].vel.y = rand() % HEIGHT;
        install_particle(particles[i]);
    }
}

int main(void)
{
    srand(time(0));
    init_particles();
    InitWindow(WIDTH, HEIGHT, "Particle System");

    while(!WindowShouldClose())
    {
        ClearBackground(BLACK);
        BeginDrawing();

        EndDrawing();
    }

    CloseWindow();

    return 0;
}
