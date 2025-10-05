#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include "raylib.h"

#if defined(PLATFORM_DESKTOP)
    #define GLSL_VERSION 330
#else   // PLATFORM_ANDROID, PLATFORM_WEB
    #define GLSL_VERSION 100
#endif

#if defined(PLATFORM_WEB)
    #include <emscripten/emscripten.h>
#endif

#define MAX_POSTPRO_SHADERS 12
const double toDegrees = 180.0 / M_PI;

// ============================
// Tipos
// ============================
typedef struct graph {
    int size;
    int xpos;
    int ypos;
} graph;

typedef struct star {
    double temp;
    double radius;
} star;

typedef struct planet {
    double period;
    double theta;
    double inclination;
    double temp;
    double x;
    double y;
    double z;
    double radius;
    double orbitRadius;
    double transitDuration;
    double transitSignalToNoise;
} planet;

// ============================
// Funciones de lógica
// ============================
double periodic_dip(double t, double period_factor, double center, double width, double depth, double baseline, double sharpness) {
    double P = period_factor * M_PI;
    double delta = fmod(t - center + 0.5 * P, P) - 0.5 * P;
    if (delta < -0.5 * P) delta += P;
    else if (delta > 0.5 * P) delta -= P;
    return baseline - depth * exp(-pow(fabs(delta / width), sharpness));
}

static inline unsigned char clamp255(float v) {
    if (v < 0.0f) return 0;
    if (v > 255.0f) return 255;
    return (unsigned char)v;
}

Color blackBodyColor(double temp){
    float x = (float)(temp / 1000.0);
    float x2 = x * x;
    float x3 = x2 * x;
    float x4 = x3 * x;
    float x5 = x4 * x;

    float Rf, Gf, Bf;

    // red
    if (temp <= 6600) Rf = 1.0f;
    else Rf = 0.0002889f * x5 - 0.01258f * x4 + 0.2148f * x3 - 1.776f * x2 + 6.907f * x - 8.723f;

    // green
    if (temp <= 6600) Gf = -4.593e-05f * x5 + 0.001424f * x4 - 0.01489f * x3 + 0.0498f * x2 + 0.1669f * x - 0.1653f;
    else Gf = -1.308e-07f * x5 + 1.745e-05f * x4 - 0.0009116f * x3 + 0.02348f * x2 - 0.3048f * x + 2.159f;

    // blue
    if (temp <= 2000.0) Bf = 0.0f;
    else if (temp < 6600.0) Bf = 0.00001764f * x5 + 0.0003575f * x4 - 0.01554f * x3 + 0.1549f * x2 - 0.3682f * x + 0.2386f;
    else Bf = 1.0f;

    Color c = { clamp255(Rf*255.0f), clamp255(Gf*255.0f), clamp255(Bf*255.0f), 255 };
    return c;
}

void drawStar (star* s){
    DrawSphere((Vector3){0.0f, 0.0f, 0.0f}, (float)s->radius, blackBodyColor(s->temp));
}

void drawPlanet (planet* p){
    DrawSphere((Vector3){(float)p->x, (float)p->y, (float)p->z}, (float)p->radius, blackBodyColor(p->temp));
}

void drawOrbit (planet* p){
    DrawCircle3D((Vector3){0.0f,0.0f,0.0f}, (float)p->orbitRadius, (Vector3){1,0,0}, (float)(p->inclination * toDegrees + 90.0), WHITE);
}

void graphCurve(planet* p, graph* g){
    (void)p; // no usado por ahora
    DrawRectangle(g->xpos, g->ypos, g->size, 200, BLACK);
    DrawRectangleLines(g->xpos, g->ypos, g->size, 200, GRAY);
    for (int i = 0; i < g->size; i++) {
        double t = i * 0.01 - 2.0;
        double a = periodic_dip(t, 1.0, 0.0, 0.3, -1.0, 1.0, 4.0);
        DrawPixel(i + g->xpos, (int)(94*a) + g->ypos, RED);
    }
}

void updateOrbitPoint(planet* p){
    double r = p->orbitRadius;
    double theta = p->theta;
    double inc = p->inclination + M_PI/2.0;
    p->x = r * cos(theta);
    p->y = r * sin(theta) * cos(inc);
    p->z = r * sin(theta) * sin(inc);
}

// ============================
// Estado global para WEB
// ============================
static double g_timeT = 0.0;
static Camera g_camera = {0};
static star g_star = {0};
static planet g_planet = {0};
static graph g_graph = {0};
static RenderTexture2D g_target; // por si quisieras usarlo en el futuro

// ============================
// Dibujo por frame (compatible WEB)
// ============================
void UpdateDrawFrame(void)
{
    g_timeT += GetFrameTime();

    updateOrbitPoint(&g_planet);
    g_planet.theta = g_timeT;
    g_planet.inclination = 0.3;

    g_camera.position = (Vector3){
        14.0f * (float)sin(g_timeT / 16.0),
         4.0f + 0.1f * (float)cos(g_timeT / 20.0),
        12.0f * (float)cos(g_timeT / 16.0)
    };

    BeginDrawing();
        ClearBackground(BLACK);
        BeginMode3D(g_camera);
            drawStar(&g_star);
            drawOrbit(&g_planet);
            drawPlanet(&g_planet);
            DrawGrid(8, 2.0f);
        EndMode3D();

        graphCurve(&g_planet, &g_graph);
        DrawFPS(10, 10);
    EndDrawing();
}

int main(void)
{
    // Antialias puede no estar disponible en todos los navegadores, pero no estorba.
//    SetConfigFlags(FLAG_MSAA_4X_HINT);

    // En web, el canvas manda; 0x0 deja que Emscripten ajuste al canvas CSS.
    const int screenWidth  = 0x0;
    const int screenHeight = 0x0;

    #if defined(PLATFORM_WEB)
        InitWindow(0, 0, "raylib web - exoplanet orbit");
    #else
        InitWindow(screenWidth, screenHeight, "raylib [models] example - geometric shapes");
    #endif

    // Si quieres bloquear 60 FPS en desktop
    #if !defined(PLATFORM_WEB)
        SetTargetFPS(60);
    #endif

    g_target = LoadRenderTexture(GetScreenWidth(), GetScreenHeight());

    g_camera.position = (Vector3){ 0.0f, 10.0f, 10.0f };
    g_camera.target   = (Vector3){ 0.0f, -1.0f, 0.0f };
    g_camera.up       = (Vector3){ 0.0f, 1.0f, 0.0f };
    g_camera.fovy     = 45.0f;
    g_camera.projection = CAMERA_PERSPECTIVE;

    g_star.temp   = 4000.0;
    g_star.radius = 0.75;

    g_planet.period       = 10.0;
    g_planet.theta        = 0.0;
    g_planet.inclination  = 0.0;
    g_planet.temp         = 1000.0;
    g_planet.x            = 3.0;
    g_planet.y            = 3.0;
    g_planet.z            = 0.0;
    g_planet.radius       = 0.25;
    g_planet.orbitRadius  = 5.0;

    g_graph.size = (int)(200 * M_PI);
    g_graph.xpos = 0;
    g_graph.ypos = GetScreenHeight() - 200;

    #if defined(PLATFORM_WEB)
        emscripten_set_main_loop(UpdateDrawFrame, 0, 1);
    #else
        while (!WindowShouldClose()) {
            UpdateDrawFrame();
        }
        UnloadRenderTexture(g_target);
        CloseWindow();
    #endif

    return 0;
}
