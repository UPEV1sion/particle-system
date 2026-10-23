#include <raylib.h>
#include <raymath.h>
#include <rlgl.h>

#include <immintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

const char *instance_vs =
"#version 330\n"
"in vec3 vertexPosition;\n"
"in vec2 vertexTexCoord;\n"
"in mat4 instanceTransform;\n"
"uniform mat4 mvp;\n"
"out vec2 fragTexCoord;\n"
"void main() {\n"
"    gl_Position = mvp * instanceTransform * vec4(vertexPosition, 1.0);\n"
"    fragTexCoord = vertexTexCoord;\n"
"}\n";

const char *instance_fs =
"#version 330\n"
"in vec2 fragTexCoord;\n"
"uniform vec4 colDiffuse;\n"
"out vec4 finalColor;\n"
"void main() {\n"
"    finalColor = colDiffuse;\n"
"}\n";

#define DA_IMPLEMENTATION
#include "da.h"

#define max(a, b) ((a) > (b) ? (a) : (b))
#define min(a, b) ((a) < (b) ? (a) : (b))

#define PART_COUNT (1 << 12)
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

    __m256 dt_vec      = _mm256_set1_ps(dt);
    __m256 zero_vec    = _mm256_setzero_ps();
    __m256 width_vec   = _mm256_set1_ps((float)WIDTH);
    __m256 height_vec  = _mm256_set1_ps((float)HEIGHT);
    __m256 depth_vec   = _mm256_set1_ps((float)DEPTH);
    __m256 max_w_vec   = _mm256_set1_ps((float)(WIDTH - 1));
    __m256 max_h_vec   = _mm256_set1_ps((float)(HEIGHT - 1));
    __m256 max_d_vec   = _mm256_set1_ps((float)(DEPTH - 1));
    __m256 neg_one_vec = _mm256_set1_ps(-1.0f);

    for(size_t i = 0; i + 7 < PART_COUNT; i += 8)
    {
        // X Axis
        __m256 px = _mm256_loadu_ps(&curr_particles->pos_x[i]);
        __m256 vx = _mm256_loadu_ps(&curr_particles->vel_x[i]);

        px = _mm256_add_ps(px, _mm256_mul_ps(vx, dt_vec));

        __m256 lt_zero_x  = _mm256_cmp_ps(px, zero_vec, _CMP_LT_OQ);
        __m256 ge_width_x = _mm256_cmp_ps(px, width_vec, _CMP_GE_OQ);

        __m256 neg_vx = _mm256_mul_ps(vx, neg_one_vec);
        vx = _mm256_blendv_ps(vx, neg_vx, lt_zero_x);
        px = _mm256_blendv_ps(px, zero_vec, lt_zero_x);

        neg_vx = _mm256_mul_ps(vx, neg_one_vec);
        vx = _mm256_blendv_ps(vx, neg_vx, ge_width_x);
        px = _mm256_blendv_ps(px, max_w_vec, ge_width_x);

        _mm256_storeu_ps(&curr_particles->pos_x[i], px);
        _mm256_storeu_ps(&curr_particles->vel_x[i], vx);

        // Y Axis
        __m256 py = _mm256_loadu_ps(&curr_particles->pos_y[i]);
        __m256 vy = _mm256_loadu_ps(&curr_particles->vel_y[i]);

        py = _mm256_add_ps(py, _mm256_mul_ps(vy, dt_vec));

        __m256 lt_zero_y   = _mm256_cmp_ps(py, zero_vec, _CMP_LT_OQ);
        __m256 ge_height_y = _mm256_cmp_ps(py, height_vec, _CMP_GE_OQ);

        __m256 neg_vy = _mm256_mul_ps(vy, neg_one_vec);
        vy = _mm256_blendv_ps(vy, neg_vy, lt_zero_y);
        py = _mm256_blendv_ps(py, zero_vec, lt_zero_y);

        neg_vy = _mm256_mul_ps(vy, neg_one_vec);
        vy = _mm256_blendv_ps(vy, neg_vy, ge_height_y);
        py = _mm256_blendv_ps(py, max_h_vec, ge_height_y);

        _mm256_storeu_ps(&curr_particles->pos_y[i], py);
        _mm256_storeu_ps(&curr_particles->vel_y[i], vy);

        // Z Axis
        __m256 pz = _mm256_loadu_ps(&curr_particles->pos_z[i]);
        __m256 vz = _mm256_loadu_ps(&curr_particles->vel_z[i]);

        pz = _mm256_add_ps(pz, _mm256_mul_ps(vz, dt_vec));

        __m256 lt_zero_z  = _mm256_cmp_ps(pz, zero_vec, _CMP_LT_OQ);
        __m256 ge_depth_z = _mm256_cmp_ps(pz, depth_vec, _CMP_GE_OQ);

        __m256 neg_vz = _mm256_mul_ps(vz, neg_one_vec);
        vz = _mm256_blendv_ps(vz, neg_vz, lt_zero_z);
        pz = _mm256_blendv_ps(pz, zero_vec, lt_zero_z);

        neg_vz = _mm256_mul_ps(vz, neg_one_vec);
        vz = _mm256_blendv_ps(vz, neg_vz, ge_depth_z);
        pz = _mm256_blendv_ps(pz, max_d_vec, ge_depth_z);

        _mm256_storeu_ps(&curr_particles->pos_z[i], pz);
        _mm256_storeu_ps(&curr_particles->vel_z[i], vz);
    }
    for(size_t i = 0; i < PART_COUNT; ++i)
    {
        float px = curr_particles->pos_x[i];
        float py = curr_particles->pos_y[i];
        float pz = curr_particles->pos_z[i];
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

void render_particles(Matrix *transforms) 
{
    for (int i = 0; i < PART_COUNT; ++i)
    {
        float x = curr_particles->pos_x[i] - WIDTH * 0.5f;
        float y = curr_particles->pos_y[i] - HEIGHT * 0.5f;
        float z = curr_particles->pos_z[i] - DEPTH * 0.5f;

        transforms[i] = MatrixTranslate(x, y, z);
        //DrawSphere((Vector3){x, y, z}, 4.0f, GRAY);
    }
}

int main(void)
{
    srand(time(0));
    init_particles();
    InitWindow(WIDTH, HEIGHT, "Particle System");
    SetTargetFPS(60);

    Mesh cube_mesh = GenMeshCube(4.0f, 4.0f, 4.0f);
    Matrix *transforms = (Matrix *)malloc(PART_COUNT * sizeof(Matrix));

    Shader instancedShader = LoadShaderFromMemory(instance_vs, instance_fs);
    instancedShader.locs[SHADER_LOC_VERTEX_INSTANCETRANSFORM] = GetShaderLocationAttrib(instancedShader, "instanceTransform");

    Material matInstances = LoadMaterialDefault();
    matInstances.shader = instancedShader;
    matInstances.maps[MATERIAL_MAP_DIFFUSE].color = GRAY;

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
                render_particles(transforms);
                DrawMeshInstanced(cube_mesh, matInstances, transforms, PART_COUNT);
                DrawCubeWires((Vector3){0, 0, 0}, WIDTH, HEIGHT, DEPTH, DARKGRAY);
            EndMode3D();

            DrawText(TextFormat("FPS: %d", GetFPS()), 10, 10, 20, GREEN);
            DrawText(TextFormat("Particles: %d", PART_COUNT), 10, 35, 20, GREEN);
        EndDrawing();
    }

    CloseWindow();

    return 0;
}
