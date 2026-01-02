#include <SDL3/SDL.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_oldnames.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_video.h>

#include <assert.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

/* =========================
   CONFIG
   ========================= */

#define cerr(...) fprintf(stderr, __VA_ARGS__)

#define WIDTH 1280
#define HEIGHT 720

#define BUFFER_SCALE 4
#define BUFFER_WIDTH (WIDTH / BUFFER_SCALE)
#define BUFFER_HEIGHT (HEIGHT / BUFFER_SCALE)

#define ASSERT(expr, ...)                                                      \
  if (!(expr)) {                                                               \
    cerr(__VA_ARGS__);                                                         \
    exit(EXIT_FAILURE);                                                        \
  }

/* =========================
   TYPES
   ========================= */

typedef float f32;
typedef double f64;

typedef int8_t i8;
typedef int16_t i16;
typedef int32_t i32;
typedef int64_t i64;

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;

typedef size_t usize;

typedef struct {
  f32 x, y;
} v2;
typedef struct {
  u32 x, y;
} u2;

/* =========================
   WINDOW / RENDERER
   ========================= */

typedef struct {
  u2 size;
  SDL_Window *handle;
  const char *title;
  bool running;
} window_t;

typedef struct {
  SDL_Renderer *handle;
  SDL_Texture *tex;
  u32 buffer[BUFFER_WIDTH * BUFFER_HEIGHT];
} renderer_t;

/* =========================
   PLAYER
   ========================= */

typedef struct {
  v2 pos;
  v2 dir;
  v2 plane;
} player_t;

/* =========================
   STATE
   ========================= */

typedef struct {
  renderer_t ren;
  window_t win;
  player_t player;
} state_t;

/* =========================
   MAP
   ========================= */

static int map_data[16][16] = {
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 1},
    {1, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 3, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 4, 4, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 4, 4, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1},
    {1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
};

/* =========================
   COLORS
   ========================= */

static u32 wall_color(i32 id) {
  switch (id) {
  case 1:
    return 0xAAAAAAFF;
  case 2:
    return 0xFF5555FF;
  case 3:
    return 0x55FF55FF;
  case 4:
    return 0x5555FFFF;
  default:
    return 0x000000FF;
  }
}

/* =========================
   INIT / SHUTDOWN
   ========================= */

void init_sdl(void) {
  ASSERT(SDL_Init(SDL_INIT_VIDEO), "SDL_Init failed: %s", SDL_GetError());
}

void init_win(state_t *s, u2 size, const char *title) {
  s->win.size = size;
  s->win.title = title;
  s->win.handle = SDL_CreateWindow(title, size.x, size.y, 0);
  assert(s->win.handle);
}

void init_renderer(state_t *s) {
  s->ren.handle = SDL_CreateRenderer(s->win.handle, NULL);
  assert(s->ren.handle);

  s->ren.tex = SDL_CreateTexture(s->ren.handle, SDL_PIXELFORMAT_RGBA8888,
                                 SDL_TEXTUREACCESS_STREAMING, BUFFER_WIDTH,
                                 BUFFER_HEIGHT);

  assert(s->ren.tex);
  SDL_SetTextureScaleMode(s->ren.tex, SDL_SCALEMODE_NEAREST);
}

void kill_state(state_t *s) {
  SDL_DestroyTexture(s->ren.tex);
  SDL_DestroyRenderer(s->ren.handle);
  SDL_DestroyWindow(s->win.handle);
}

void kill_sdl(void) { SDL_Quit(); }

/* =========================
   RENDER
   ========================= */

void clear_buffer(renderer_t *r, u32 color) {
  for (u32 i = 0; i < BUFFER_WIDTH * BUFFER_HEIGHT; ++i)
    r->buffer[i] = color;
}

void render(renderer_t *r) {
  SDL_UpdateTexture(r->tex, NULL, r->buffer, BUFFER_WIDTH * sizeof(u32));
  SDL_RenderClear(r->handle);
  SDL_RenderTexture(r->handle, r->tex, NULL, NULL);
  SDL_RenderPresent(r->handle);
}

/* =========================
   PLAYER UPDATE
   ========================= */

void update_player(state_t *s, f32 dt, const bool *keys) {
  player_t *p = &s->player;

  f32 move_speed = 3.0f * dt;
  f32 rot_speed = 2.0f * dt;

  if (keys[SDL_SCANCODE_W]) {
    f32 nx = p->pos.x + p->dir.x * move_speed;
    f32 ny = p->pos.y + p->dir.y * move_speed;
    if (map_data[(i32)p->pos.y][(i32)nx] == 0)
      p->pos.x = nx;
    if (map_data[(i32)ny][(i32)p->pos.x] == 0)
      p->pos.y = ny;
  }

  if (keys[SDL_SCANCODE_S]) {
    f32 nx = p->pos.x - p->dir.x * move_speed;
    f32 ny = p->pos.y - p->dir.y * move_speed;
    if (map_data[(i32)p->pos.y][(i32)nx] == 0)
      p->pos.x = nx;
    if (map_data[(i32)ny][(i32)p->pos.x] == 0)
      p->pos.y = ny;
  }

  if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_D]) {
    f32 angle = rot_speed * (keys[SDL_SCANCODE_A] ? 1.0f : -1.0f);
    f32 cos_a = cosf(angle);
    f32 sin_a = sinf(angle);

    v2 old_dir = p->dir;
    p->dir.x = old_dir.x * cos_a - old_dir.y * sin_a;
    p->dir.y = old_dir.x * sin_a + old_dir.y * cos_a;

    v2 old_plane = p->plane;
    p->plane.x = old_plane.x * cos_a - old_plane.y * sin_a;
    p->plane.y = old_plane.x * sin_a + old_plane.y * cos_a;
  }
}

/* =========================
   RAYCAST
   ========================= */

void draw_raycast(state_t *s) {
  renderer_t *r = &s->ren;
  player_t *p = &s->player;

  for (i32 x = 0; x < BUFFER_WIDTH; ++x) {
    f32 camera_x = 2.0f * x / (f32)BUFFER_WIDTH - 1.0f;

    f32 ray_dir_x = p->dir.x + p->plane.x * camera_x;
    f32 ray_dir_y = p->dir.y + p->plane.y * camera_x;

    i32 map_x = (i32)p->pos.x;
    i32 map_y = (i32)p->pos.y;

    f32 delta_x = ray_dir_x == 0 ? 1e30f : fabsf(1.0f / ray_dir_x);
    f32 delta_y = ray_dir_y == 0 ? 1e30f : fabsf(1.0f / ray_dir_y);

    f32 side_x, side_y;
    i32 step_x, step_y;
    i32 side = 0;

    if (ray_dir_x < 0) {
      step_x = -1;
      side_x = (p->pos.x - map_x) * delta_x;
    } else {
      step_x = 1;
      side_x = (map_x + 1.0f - p->pos.x) * delta_x;
    }

    if (ray_dir_y < 0) {
      step_y = -1;
      side_y = (p->pos.y - map_y) * delta_y;
    } else {
      step_y = 1;
      side_y = (map_y + 1.0f - p->pos.y) * delta_y;
    }

    while (map_data[map_y][map_x] == 0) {
      if (side_x < side_y) {
        side_x += delta_x;
        map_x += step_x;
        side = 0;
      } else {
        side_y += delta_y;
        map_y += step_y;
        side = 1;
      }
    }

    f32 perp_dist = side == 0
                        ? (map_x - p->pos.x + (1 - step_x) * 0.5f) / ray_dir_x
                        : (map_y - p->pos.y + (1 - step_y) * 0.5f) / ray_dir_y;

    i32 line_h = (i32)(BUFFER_HEIGHT / perp_dist);
    i32 draw_start = -line_h / 2 + BUFFER_HEIGHT / 2;
    i32 draw_end = line_h / 2 + BUFFER_HEIGHT / 2;

    if (draw_start < 0)
      draw_start = 0;
    if (draw_end >= BUFFER_HEIGHT)
      draw_end = BUFFER_HEIGHT - 1;

    u32 color = wall_color(map_data[map_y][map_x]);
    if (side)
      color = (color >> 1) & 0x7F7F7FFF;

    for (i32 y = draw_start; y <= draw_end; ++y)
      r->buffer[y * BUFFER_WIDTH + x] = color;
  }
}

/* =========================
   MAIN
   ========================= */

int main(void) {
  init_sdl();

  state_t state = {0};

  init_win(&state, (u2){WIDTH, HEIGHT}, "CPU RAYCASTER");
  init_renderer(&state);

  state.player.pos = (v2){7.5f, 7.5f};
  state.player.dir = (v2){1.0f, 0.0f};
  state.player.plane = (v2){0.0f, 0.66f};

  state.win.running = true;

  u64 last = SDL_GetTicks();

  while (state.win.running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      if (e.type == SDL_EVENT_QUIT)
        state.win.running = false;
    }

    u64 now = SDL_GetTicks();
    f32 dt = (now - last) / 1000.0f;
    last = now;

    const bool *keys = SDL_GetKeyboardState(NULL);

    update_player(&state, dt, keys);

    clear_buffer(&state.ren, 0x202020FF);
    draw_raycast(&state);
    render(&state.ren);
  }

  kill_state(&state);
  kill_sdl();
  return 0;
}
