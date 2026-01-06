#include <SDL3/SDL.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_oldnames.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>

#include <SDL3_image/SDL_image.h>

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

#define FORMAT SDL_PIXELFORMAT_ABGR8888

#define BUFFER_SCALE 1
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
  i32 x, y;
} i2;

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

typedef struct {
  SDL_Texture *handle;
  SDL_Rect rect;
  u32 *pixels;
  i32 w, h;
} texture_t;

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

  s->ren.tex =
      SDL_CreateTexture(s->ren.handle, FORMAT, SDL_TEXTUREACCESS_STREAMING,
                        BUFFER_WIDTH, BUFFER_HEIGHT);

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

texture_t load_tex(state_t *ptr, const char *path) {
  texture_t t = {0};

  SDL_Surface *surf = IMG_Load(path);
  ASSERT(surf, "Failed to load %s: %s\n", path, SDL_GetError());

  SDL_Surface *temp = SDL_ConvertSurface(surf, FORMAT);
  SDL_DestroySurface(surf);
  ASSERT(temp, "Surface convert failed: %s\n", SDL_GetError());

  t.w = temp->w;
  t.h = temp->h;

  t.pixels = malloc(t.w * t.h * sizeof(u32));
  ASSERT(t.pixels, "Out of memory\n");

  memcpy(t.pixels, temp->pixels, t.w * t.h * sizeof(u32));

  for (i32 i = 0; i < t.w * t.h; ++i) {
    u32 p = t.pixels[i];

    u8 r = p & 0xFF;
    u8 g = (p >> 8) & 0xFF;
    u8 b = (p >> 16) & 0xFF;
    u8 a = (p >> 24) & 0xFF;

    r = (r * a) / 255;
    g = (g * a) / 255;
    b = (b * a) / 255;

    if (a == 0) {
      t.pixels[i] = 0;
    } else
      t.pixels[i] = (a << 24) | (b << 16) | (g << 8) | r;
  }

  t.handle = SDL_CreateTextureFromSurface(ptr->ren.handle, temp);
  SDL_DestroySurface(temp);
  ASSERT(t.handle, "Texture create failed: %s\n", SDL_GetError());

  /* scale + placement */
  t.rect.w = t.w / 2;
  t.rect.h = t.h / 2;
  t.rect.x = BUFFER_WIDTH - t.rect.w;
  t.rect.y = BUFFER_HEIGHT - t.rect.h;

  return t;
}

void draw_tex(renderer_t *r, texture_t tex) {
  for (i32 y = 0; y < tex.rect.h; ++y) {
    i32 dst_y = tex.rect.y + y;
    if (dst_y < 0 || dst_y >= BUFFER_HEIGHT)
      continue;

    i32 src_y = y * tex.h / tex.rect.h;

    for (i32 x = 0; x < tex.rect.w; ++x) {
      i32 dst_x = tex.rect.x + x;
      if (dst_x < 0 || dst_x >= BUFFER_WIDTH)
        continue;

      i32 src_x = x * tex.w / tex.rect.w;

      u32 pixel = tex.pixels[src_y * tex.w + src_x];
      u8 a = pixel >> 24;

      if (a == 0)
        continue;

      u32 *dst = &r->buffer[dst_y * BUFFER_WIDTH + dst_x];

      if (a == 255) {
        *dst = pixel;
      } else {
        u32 bg = *dst;

        u8 r1 = (bg >> 16) & 0xFF;
        u8 g1 = (bg >> 8) & 0xFF;
        u8 b1 = bg & 0xFF;

        u8 r2 = (pixel >> 16) & 0xFF;
        u8 g2 = (pixel >> 8) & 0xFF;
        u8 b2 = pixel & 0xFF;

        /*  */
        // premultiplied alpha blend
        *dst = (0xFF << 24) | (((r2 + r1 * (255 - a) / 255)) << 0) |
               (((g2 + g1 * (255 - a) / 255)) << 8) |
               (((b2 + b1 * (255 - a) / 255)) << 16);

        *dst = ((r2 * a + r1 * (255 - a)) / 255 << 16) |
               ((g2 * a + g1 * (255 - a)) / 255 << 8) |
               ((b2 * a + b1 * (255 - a)) / 255) | 0xFF000000;
      }
    }
  }
}

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

    // --- camera space ---
    f32 camera_x = 2.0f * x / (f32)BUFFER_WIDTH - 1.0f;

    // --- ray direction ---
    v2 ray_dir = {.x = p->dir.x + p->plane.x * camera_x,
                  .y = p->dir.y + p->plane.y * camera_x};

    // --- current map square ---
    i2 map = {.x = (i32)p->pos.x, .y = (i32)p->pos.y};

    // --- delta distances ---
    v2 delta = {.x = (ray_dir.x == 0.0f) ? 1e30f : fabsf(1.0f / ray_dir.x),
                .y = (ray_dir.y == 0.0f) ? 1e30f : fabsf(1.0f / ray_dir.y)};

    // --- step & initial side distances ---
    i2 step;
    v2 side_dist;

    if (ray_dir.x < 0.0f) {
      step.x = -1;
      side_dist.x = (p->pos.x - map.x) * delta.x;
    } else {
      step.x = 1;
      side_dist.x = (map.x + 1.0f - p->pos.x) * delta.x;
    }

    if (ray_dir.y < 0.0f) {
      step.y = -1;
      side_dist.y = (p->pos.y - map.y) * delta.y;
    } else {
      step.y = 1;
      side_dist.y = (map.y + 1.0f - p->pos.y) * delta.y;
    }

    // --- DDA ---
    i32 side = 0; // 0 = x side, 1 = y side

    while (map_data[map.y][map.x] == 0) {
      if (side_dist.x < side_dist.y) {
        side_dist.x += delta.x;
        map.x += step.x;
        side = 0;
      } else {
        side_dist.y += delta.y;
        map.y += step.y;
        side = 1;
      }
    }

    // --- raw wall distance ---
    f32 raw_dist;
    if (side == 0) {
      raw_dist = (map.x - p->pos.x + (1 - step.x) * 0.5f) / ray_dir.x;
    } else {
      raw_dist = (map.y - p->pos.y + (1 - step.y) * 0.5f) / ray_dir.y;
    }

    // --- fisheye correction ---
    f32 ray_len = sqrtf(ray_dir.x * ray_dir.x + ray_dir.y * ray_dir.y);
    v2 ray_norm = {.x = ray_dir.x / ray_len, .y = ray_dir.y / ray_len};

    f32 perp_dist = raw_dist * (ray_norm.x * p->dir.x + ray_norm.y * p->dir.y);

    // --- projection ---
    i32 line_h = (i32)(BUFFER_HEIGHT / perp_dist);

    i32 draw_start = -line_h / 2 + BUFFER_HEIGHT / 2;
    i32 draw_end = line_h / 2 + BUFFER_HEIGHT / 2;

    if (draw_start < 0)
      draw_start = 0;
    if (draw_end >= BUFFER_HEIGHT)
      draw_end = BUFFER_HEIGHT - 1;

    // --- color & shading ---
    u32 color = wall_color(map_data[map.y][map.x]);
    if (side == 1)
      color = (color >> 1) & 0x7F7F7FFF;

    // --- draw column ---
    for (i32 y = draw_start; y <= draw_end; ++y) {
      r->buffer[y * BUFFER_WIDTH + x] = color;
    }
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

  texture_t hand = load_tex(&state, "./res/gfx/hand.png");

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

    clear_buffer(&state.ren, 0x20202000);
    draw_raycast(&state);

    draw_tex(&state.ren, hand);
    render(&state.ren);
  }

  kill_state(&state);
  kill_sdl();
  return 0;
}
