#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#define DA_IMPLEMENTATION
#include "da.h"

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

#define PART_COUNT (1 << 14)
#define WIDTH  800
#define HEIGHT 800
#define DEPTH  800

#define SHM_CELL_SIZE 20
#define SHM_ROWS   (int32_t) ((HEIGHT + SHM_CELL_SIZE - 1) / SHM_CELL_SIZE)
#define SHM_COLS   (int32_t) ((WIDTH  + SHM_CELL_SIZE - 1) / SHM_CELL_SIZE)
#define SHM_SLICES (int32_t) ((DEPTH  + SHM_CELL_SIZE - 1) / SHM_CELL_SIZE)
#define TOTAL_CELLS (SHM_SLICES * SHM_ROWS * SHM_COLS)

#define INTERACTION_RADIUS     6.0f
#define INTERACTION_RADIUS_SQ  (INTERACTION_RADIUS * INTERACTION_RADIUS)

typedef struct {
    float pos_x[PART_COUNT];
    float pos_y[PART_COUNT];
    float pos_z[PART_COUNT];
    float vel_x[PART_COUNT];
    float vel_y[PART_COUNT];
    float vel_z[PART_COUNT];
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

static inline int32_t get_cell_index(float x, float y, float z)
{
    int32_t col   = (int32_t)(x / SHM_CELL_SIZE);
    int32_t row   = (int32_t)(y / SHM_CELL_SIZE);
    int32_t slice = (int32_t)(z / SHM_CELL_SIZE);
    col   = (int32_t)Clamp(col, 0, SHM_COLS - 1);
    row   = (int32_t)Clamp(row, 0, SHM_ROWS - 1);
    slice = (int32_t)Clamp(slice, 0, SHM_SLICES - 1);
    return slice * (SHM_ROWS * SHM_COLS) + row * SHM_COLS + col;
}

void init_particles(void) {
    for (size_t i = 0; i < PART_COUNT; ++i) {
        curr_particles->pos_x[i] = (float)(rand() % WIDTH);
        curr_particles->pos_y[i] = (float)(rand() % HEIGHT);
        curr_particles->pos_z[i] = (float)(rand() % DEPTH);
        curr_particles->vel_x[i] = (float)((rand() % 200) - 100) * 0.5f;
        curr_particles->vel_y[i] = (float)((rand() % 200) - 100) * 0.5f;
        curr_particles->vel_z[i] = (float)((rand() % 200) - 100) * 0.5f;
    }
}

void update_particles(float dt)
{
    for (size_t i = 0; i < TOTAL_CELLS; ++i) grid.count[i] = 0;

    for(size_t i = 0; i < PART_COUNT; ++i)
    {
        float px = curr_particles->pos_x[i] + curr_particles->vel_x[i] * dt;
        float py = curr_particles->pos_y[i] + curr_particles->vel_y[i] * dt;
        float pz = curr_particles->pos_z[i] + curr_particles->vel_z[i] * dt;
        float vx = curr_particles->vel_x[i];
        float vy = curr_particles->vel_y[i];
        float vz = curr_particles->vel_z[i];

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

        if(pz < 0.0f) 
        { 
            pz = 0.0f; vz *= -1.0f; 
        } 
        else if(pz >= (float) DEPTH)
        { 
            pz = (float) (DEPTH - 1); vz *= -1.0f; 
        }

        curr_particles->pos_x[i] = px;
        curr_particles->pos_y[i] = py;
        curr_particles->pos_z[i] = pz;
        curr_particles->vel_x[i] = vx;
        curr_particles->vel_y[i] = vy;
        curr_particles->vel_z[i] = vz;

        grid.count[get_cell_index(px, py, pz)]++;
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
        float pz = curr_particles->pos_z[i];
        int32_t c_idx = get_cell_index(px, py, pz);
        int32_t dest_idx = grid.offset[c_idx] + grid.count[c_idx];
        grid.count[c_idx]++;

        next_particles->pos_x[dest_idx] = px;
        next_particles->pos_y[dest_idx] = py;
        next_particles->pos_z[dest_idx] = pz;
        next_particles->vel_x[dest_idx] = curr_particles->vel_x[i];
        next_particles->vel_y[dest_idx] = curr_particles->vel_y[i];
        next_particles->vel_z[dest_idx] = curr_particles->vel_z[i];
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
        float pz = curr_particles->pos_z[i];

        int32_t col   = (int32_t) (px / SHM_CELL_SIZE);
        int32_t row   = (int32_t) (py / SHM_CELL_SIZE);
        int32_t slice = (int32_t) (pz / SHM_CELL_SIZE);
        col   = Clamp(col, 0, SHM_COLS - 1);
        row   = Clamp(row, 0, SHM_ROWS - 1);
        slice = Clamp(slice, 0, SHM_SLICES - 1);

        int32_t start_col   = max(0, col - 1);
        int32_t end_col     = min(col + 1, SHM_COLS - 1);
        int32_t start_row   = max(0, row - 1);
        int32_t end_row     = min(row + 1, SHM_ROWS - 1);
        int32_t start_slice = max(0, slice - 1);
        int32_t end_slice   = min(slice + 1, SHM_SLICES - 1);

        for (int32_t s = start_slice; s <= end_slice; ++s)
        {
            for (int32_t r = start_row; r <= end_row; ++r)
            {
                for (int32_t c = start_col; c <= end_col; ++c)
                {
                    int32_t n_cell_idx = s * (SHM_ROWS * SHM_COLS) + r * SHM_COLS + c;
                    int32_t start_idx = grid.offset[n_cell_idx];
                    int32_t end_idx = (n_cell_idx < TOTAL_CELLS - 1) ? grid.offset[n_cell_idx + 1] : PART_COUNT;

                    for(int32_t j = start_idx; j < end_idx; ++j)
                    {
                        if(i == j) continue;

                        float dx = curr_particles->pos_x[j] - px;
                        float dy = curr_particles->pos_y[j] - py;
                        float dz = curr_particles->pos_z[j] - pz;
                        float dist_sq = (dx * dx) + (dy * dy) + (dz * dz);

                        if (dist_sq < INTERACTION_RADIUS_SQ && dist_sq > 0.0001f)
                        {
                            float dist = sqrtf(dist_sq);
                            float overlap = INTERACTION_RADIUS - dist;
                            float nx = dx / dist;
                            float ny = dy / dist;
                            float nz = dz / dist;

                            curr_particles->pos_x[j] += nx * overlap * 0.5f;
                            curr_particles->pos_y[j] += ny * overlap * 0.5f;
                            curr_particles->pos_z[j] += nz * overlap * 0.5f;
                            curr_particles->pos_x[i] -= nx * overlap * 0.5f;
                            curr_particles->pos_y[i] -= ny * overlap * 0.5f;
                            curr_particles->pos_z[i] -= nz * overlap * 0.5f;
                        }
                    }
                }
            }
        }
    }
}

void render_particles(void) 
{
    for (int i = 0; i < PART_COUNT; ++i)
    {
        float x = curr_particles->pos_x[i] - WIDTH * 0.5f;
        float y = curr_particles->pos_y[i] - HEIGHT * 0.5f;
        float z = curr_particles->pos_z[i] - DEPTH * 0.5f;

        DrawCube((Vector3){x, y, z}, 4.0f, 4.0f, 4.0f, GRAY);
        //DrawSphere((Vector3){x, y, z}, 4.0f, GRAY);
    }
}

int main(void)
{
    srand(time(0));
    init_particles();
    InitWindow(WIDTH, HEIGHT, "Particle System");
    SetTargetFPS(60);

    Camera3D camera = { 0 };
    camera.position = (Vector3){ 0.0f, 0.0f, 1200.0f };
    camera.target = (Vector3){ 0.0f, 0.0f, 0.0f };
    camera.up = (Vector3){ 0.0f, 1.0f, 0.0f };
    camera.fovy = 45.0f;
    camera.projection = CAMERA_PERSPECTIVE;
    bool camera_free = false;

    static float cam_angle_x = 0.0f;
    static float cam_angle_y = 1.0f;
    static float cam_distance = 2000.0f;

    while(!WindowShouldClose())
    {
        UpdateCamera(&camera, CAMERA_ORBITAL);
        if (IsKeyPressed(KEY_F)) 
        {
            camera_free = !camera_free;
            if (camera_free) DisableCursor();
            else EnableCursor();
        }

        if (camera_free) {
            UpdateCamera(&camera, CAMERA_FREE);
        }

        float wheel = GetMouseWheelMove();
        if (wheel != 0.0f) 
        {
            cam_distance -= wheel * 80.0f;
            cam_distance = Clamp(cam_distance, 100.0f, 3000.0f);
        }

        if (IsMouseButtonDown(MOUSE_BUTTON_RIGHT)) 
        {
            Vector2 delta = GetMouseDelta();
            cam_angle_x += delta.x * 0.005f;
            cam_angle_y -= delta.y * 0.005f;
            cam_angle_y = Clamp(cam_angle_y, 0.01f, (float)PI - 0.01f);
        }

        camera.position.x = camera.target.x + cam_distance * sinf(cam_angle_y) * cosf(cam_angle_x);
        camera.position.y = camera.target.y + cam_distance * cosf(cam_angle_y);
        camera.position.z = camera.target.z + cam_distance * sinf(cam_angle_y) * sinf(cam_angle_x);

        float dt = GetFrameTime();
        if (dt > 0.1f) dt = 0.1f;

        update_particles(dt);
        resolve_collisions();

        BeginDrawing();
            ClearBackground(BLACK);

            BeginMode3D(camera);
                render_particles();
                DrawCubeWires((Vector3){0, 0, 0}, WIDTH, HEIGHT, DEPTH, DARKGRAY);
            EndMode3D();

            DrawText(TextFormat("FPS: %d", GetFPS()), 10, 10, 20, GREEN);
            DrawText(TextFormat("Particles: %d", PART_COUNT), 10, 35, 20, GREEN);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
