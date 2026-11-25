#include <SDL.h>
#include <verilated.h>
#include <verilated_vcd_c.h>

#include "Vmaze_32x32_top.h"
#include "Vmaze_32x32_top___024root.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>

namespace {

// SIM_SHIFT=8 scales 12 MHz down by 256x -> 46,875 Hz virtual clock (~781 cycles per 60 FPS frame)
constexpr int CYCLES_PER_FRAME = 781;
constexpr int PANEL_W = 560;
constexpr int MINIMAP_W = 380;
constexpr int WIN_H = 720;

struct Glyph {
    char ch;
    uint8_t rows[7]; // 5 bits per row (MSB=bit4 .. LSB=bit0)
};

// Compact 5x7 uppercase bitmap font (covers A-Z, 0-9, punctuation)
constexpr Glyph FONT_5X7[] = {
    {' ', {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00}},
    {'-', {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00}},
    {'.', {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C}},
    {',', {0x00, 0x00, 0x00, 0x00, 0x00, 0x06, 0x04}},
    {':', {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00}},
    {'/', {0x01, 0x02, 0x04, 0x04, 0x08, 0x10, 0x00}},
    {'(', {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02}},
    {')', {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08}},
    {'[', {0x0E, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0E}},
    {']', {0x0E, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0E}},
    {'+', {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00}},
    {'0', {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E}},
    {'1', {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'2', {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F}},
    {'3', {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E}},
    {'4', {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02}},
    {'5', {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E}},
    {'6', {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E}},
    {'7', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08}},
    {'8', {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E}},
    {'9', {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C}},
    {'A', {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'B', {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E}},
    {'C', {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E}},
    {'D', {0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C}},
    {'E', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F}},
    {'F', {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10}},
    {'G', {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F}},
    {'H', {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11}},
    {'I', {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E}},
    {'J', {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C}},
    {'K', {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11}},
    {'L', {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F}},
    {'M', {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11}},
    {'N', {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}},
    {'O', {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'P', {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10}},
    {'Q', {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D}},
    {'R', {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11}},
    {'S', {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E}},
    {'T', {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04}},
    {'U', {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E}},
    {'V', {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04}},
    {'W', {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11}},
    {'X', {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11}},
    {'Y', {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04}},
    {'Z', {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}},
};

const uint8_t* find_glyph(char c) {
    char uc = (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
    for (const auto& g : FONT_5X7) {
        if (g.ch == uc) return g.rows;
    }
    return FONT_5X7[0].rows;
}

void draw_text(SDL_Renderer* ren, int x, int y, const std::string& text, int scale, SDL_Color col) {
    SDL_SetRenderDrawColor(ren, col.r, col.g, col.b, col.a);
    int cx = x;
    for (char c : text) {
        const uint8_t* rows = find_glyph(c);
        for (int r = 0; r < 7; ++r) {
            for (int bit = 0; bit < 5; ++bit) {
                if (rows[r] & (1 << (4 - bit))) {
                    SDL_Rect px{cx + bit * scale, y + r * scale, scale, scale};
                    SDL_RenderFillRect(ren, &px);
                }
            }
        }
        cx += 6 * scale;
    }
}

void draw_filled_circle(SDL_Renderer* ren, int cx, int cy, int r) {
    for (int dy = -r; dy <= r; ++dy) {
        int dx = static_cast<int>(std::sqrt(static_cast<double>(r * r - dy * dy)));
        SDL_RenderDrawLine(ren, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

// seg_bits: {g,f,e,d,c,b,a} Active-High
void draw_7seg(SDL_Renderer* ren, int x, int y, int w, int h, uint8_t seg_bits) {
    SDL_Rect bg{x, y, w, h};
    SDL_SetRenderDrawColor(ren, 16, 20, 26, 255);
    SDL_RenderFillRect(ren, &bg);
    SDL_SetRenderDrawColor(ren, 55, 65, 81, 255);
    SDL_RenderDrawRect(ren, &bg);

    int t = std::max(4, w / 7);
    int pad = 6;
    int inner_w = w - 2 * pad;
    int mid_y = y + h / 2;

    SDL_Rect segs[7] = {
        {x + pad + t / 2, y + pad, inner_w - t, t},                               // a (bit 0)
        {x + w - pad - t, y + pad + t / 2, t, h / 2 - pad - t / 2},               // b (bit 1)
        {x + w - pad - t, mid_y + t / 2, t, h / 2 - pad - t / 2},                 // c (bit 2)
        {x + pad + t / 2, y + h - pad - t, inner_w - t, t},                       // d (bit 3)
        {x + pad, mid_y + t / 2, t, h / 2 - pad - t / 2},                         // e (bit 4)
        {x + pad, y + pad + t / 2, t, h / 2 - pad - t / 2},                       // f (bit 5)
        {x + pad + t / 2, mid_y - t / 2, inner_w - t, t},                         // g (bit 6)
    };

    for (int i = 0; i < 7; ++i) {
        if ((seg_bits >> i) & 1) {
            SDL_SetRenderDrawColor(ren, 239, 68, 68, 255);
        } else {
            SDL_SetRenderDrawColor(ren, 42, 24, 28, 255);
        }
        SDL_RenderFillRect(ren, &segs[i]);
    }
}

struct ButtonRect {
    SDL_Rect rect;
    const char* label;
};

bool point_in_rect(int mx, int my, const SDL_Rect& r) {
    return mx >= r.x && mx < (r.x + r.w) && my >= r.y && my < (r.y + r.h);
}

void draw_button(SDL_Renderer* ren, const ButtonRect& btn, bool active, SDL_Color active_bg) {
    SDL_Color bg = active ? active_bg : SDL_Color{38, 46, 58, 255};
    SDL_Color border = active ? SDL_Color{226, 232, 240, 255} : SDL_Color{71, 85, 105, 255};
    SDL_SetRenderDrawColor(ren, bg.r, bg.g, bg.b, 255);
    SDL_RenderFillRect(ren, &btn.rect);
    SDL_SetRenderDrawColor(ren, border.r, border.g, border.b, 255);
    SDL_RenderDrawRect(ren, &btn.rect);

    int scale = 2;
    int tw = static_cast<int>(std::strlen(btn.label)) * 6 * scale - scale;
    int th = 7 * scale;
    int tx = btn.rect.x + (btn.rect.w - tw) / 2;
    int ty = btn.rect.y + (btn.rect.h - th) / 2;
    draw_text(ren, tx, ty, btn.label, scale, SDL_Color{241, 245, 249, 255});
}

} // namespace

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    std::string vcd_path;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--vcd" && i + 1 < argc) {
            vcd_path = argv[++i];
        }
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << "SDL_Init failed: " << SDL_GetError() << "\n";
        return 1;
    }

    bool show_minimap = true;
    int win_w = PANEL_W + (show_minimap ? MINIMAP_W : 0);
    SDL_Window* window = SDL_CreateWindow(
        "MazeVerilog - 32x32 FPGA Viewport Simulator (Verilator + SDL2)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        win_w, WIN_H, SDL_WINDOW_SHOWN
    );
    if (!window) {
        std::cerr << "SDL_CreateWindow failed: " << SDL_GetError() << "\n";
        SDL_Quit();
        return 1;
    }

    SDL_Renderer* ren = SDL_CreateRenderer(
        window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC
    );
    if (!ren) {
        ren = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!ren) {
        std::cerr << "SDL_CreateRenderer failed: " << SDL_GetError() << "\n";
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    auto* top = new Vmaze_32x32_top;
    VerilatedVcdC* tfp = nullptr;
    uint64_t sim_time = 0;

    if (!vcd_path.empty()) {
        Verilated::traceEverOn(true);
        tfp = new VerilatedVcdC;
        top->trace(tfp, 99);
        tfp->open(vcd_path.c_str());
    }

    auto eval_step = [&]() {
        top->clk = 0;
        top->eval();
        if (tfp) tfp->dump(sim_time++);
        top->clk = 1;
        top->eval();
        if (tfp) tfp->dump(sim_time++);
    };

    // Initial hardware reset
    top->rst_n = 0;
    top->sw_row_inv = 0;
    top->btn_up = 0;
    top->btn_down = 0;
    top->btn_left = 0;
    top->btn_right = 0;
    for (int i = 0; i < 8; ++i) eval_step();
    top->rst_n = 1;

    // UI button geometry
    const ButtonRect btn_up_ui   {{252, 558, 56, 36}, "UP"};
    const ButtonRect btn_dn_ui   {{252, 646, 56, 36}, "DN"};
    const ButtonRect btn_lt_ui   {{190, 602, 56, 36}, "LT"};
    const ButtonRect btn_rt_ui   {{314, 602, 56, 36}, "RT"};
    const ButtonRect btn_rst_ui  {{404, 558, 116, 36}, "BT0 RST"};
    const ButtonRect btn_inv_ui  {{404, 602, 116, 36}, "SW0 INV"};
    const ButtonRect btn_map_ui  {{404, 646, 116, 36}, "MAP DBG"};

    bool sw_row_inv = false;
    bool mouse_down = false;
    int mouse_x = 0, mouse_y = 0;

    std::array<std::array<float, 16>, 16> led_bright{};

    bool running = true;
    while (running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                running = false;
            } else if (ev.type == SDL_KEYDOWN && !ev.key.repeat) {
                switch (ev.key.keysym.sym) {
                    case SDLK_ESCAPE:
                        running = false;
                        break;
                    case SDLK_i:
                        sw_row_inv = !sw_row_inv;
                        break;
                    case SDLK_m:
                        show_minimap = !show_minimap;
                        SDL_SetWindowSize(window, PANEL_W + (show_minimap ? MINIMAP_W : 0), WIN_H);
                        break;
                    default:
                        break;
                }
            } else if (ev.type == SDL_MOUSEBUTTONDOWN && ev.button.button == SDL_BUTTON_LEFT) {
                mouse_down = true;
                mouse_x = ev.button.x;
                mouse_y = ev.button.y;
                if (point_in_rect(mouse_x, mouse_y, btn_inv_ui.rect)) {
                    sw_row_inv = !sw_row_inv;
                } else if (point_in_rect(mouse_x, mouse_y, btn_map_ui.rect)) {
                    show_minimap = !show_minimap;
                    SDL_SetWindowSize(window, PANEL_W + (show_minimap ? MINIMAP_W : 0), WIN_H);
                }
            } else if (ev.type == SDL_MOUSEBUTTONUP && ev.button.button == SDL_BUTTON_LEFT) {
                mouse_down = false;
            } else if (ev.type == SDL_MOUSEMOTION) {
                mouse_x = ev.motion.x;
                mouse_y = ev.motion.y;
            }
        }

        const uint8_t* keys = SDL_GetKeyboardState(nullptr);
        bool up_pressed = keys[SDL_SCANCODE_UP] || keys[SDL_SCANCODE_W] ||
                          (mouse_down && point_in_rect(mouse_x, mouse_y, btn_up_ui.rect));
        bool dn_pressed = keys[SDL_SCANCODE_DOWN] || keys[SDL_SCANCODE_S] ||
                          (mouse_down && point_in_rect(mouse_x, mouse_y, btn_dn_ui.rect));
        bool lt_pressed = keys[SDL_SCANCODE_LEFT] || keys[SDL_SCANCODE_A] ||
                          (mouse_down && point_in_rect(mouse_x, mouse_y, btn_lt_ui.rect));
        bool rt_pressed = keys[SDL_SCANCODE_RIGHT] || keys[SDL_SCANCODE_D] ||
                          (mouse_down && point_in_rect(mouse_x, mouse_y, btn_rt_ui.rect));
        bool rst_pressed = keys[SDL_SCANCODE_R] ||
                           (mouse_down && point_in_rect(mouse_x, mouse_y, btn_rst_ui.rect));

        top->btn_up = up_pressed ? 1 : 0;
        top->btn_down = dn_pressed ? 1 : 0;
        top->btn_left = lt_pressed ? 1 : 0;
        top->btn_right = rt_pressed ? 1 : 0;
        top->rst_n = rst_pressed ? 0 : 1;
        top->sw_row_inv = sw_row_inv ? 1 : 0;

        int hits[16][16] = {};
        for (int cyc = 0; cyc < CYCLES_PER_FRAME; ++cyc) {
            eval_step();
            uint16_t row_mask = top->mat_row;
            uint16_t col_mask = top->mat_col;
            for (int r = 0; r < 16; ++r) {
                if ((row_mask >> r) & 1) {
                    for (int c = 0; c < 16; ++c) {
                        if ((col_mask >> c) & 1) {
                            hits[r][c] += 1;
                        }
                    }
                }
            }
        }

        for (int r = 0; r < 16; ++r) {
            for (int c = 0; c < 16; ++c) {
                float duty = static_cast<float>(hits[r][c]) * 16.0f / static_cast<float>(CYCLES_PER_FRAME);
                duty = std::min(1.0f, duty);
                led_bright[r][c] = led_bright[r][c] * 0.25f + duty * 0.75f;
            }
        }

        // Render background
        SDL_SetRenderDrawColor(ren, 15, 23, 42, 255);
        SDL_RenderClear(ren);

        // Top bar
        draw_text(ren, 40, 20, "VERIINSTRUMENT // 32X32 MAZE", 2, SDL_Color{226, 232, 240, 255});
        draw_text(ren, 398, 22, "46.9KHZ SIM", 2, SDL_Color{148, 163, 184, 255});

        // 16x16 Matrix LED bezel
        SDL_Rect mat_bg{34, 50, 492, 492};
        SDL_SetRenderDrawColor(ren, 9, 13, 22, 255);
        SDL_RenderFillRect(ren, &mat_bg);
        SDL_SetRenderDrawColor(ren, 51, 65, 85, 255);
        SDL_RenderDrawRect(ren, &mat_bg);

        for (int r = 0; r < 16; ++r) {
            for (int c = 0; c < 16; ++c) {
                float b = led_bright[r][c];
                uint8_t cr = static_cast<uint8_t>(28 + b * 227);
                uint8_t cg = static_cast<uint8_t>(20 + b * 60);
                uint8_t cb = static_cast<uint8_t>(22 + b * 24);
                int cx = 40 + c * 30 + 15;
                int cy = 56 + r * 30 + 15;
                SDL_SetRenderDrawColor(ren, cr, cg, cb, 255);
                draw_filled_circle(ren, cx, cy, 11);
            }
        }

        // Seven-Segment Displays (Tens & Ones)
        draw_text(ren, 40, 554, "STEPS", 2, SDL_Color{148, 163, 184, 255});
        draw_7seg(ren, 40, 574, 52, 84, static_cast<uint8_t>(top->seg_tens));
        draw_7seg(ren, 100, 574, 52, 84, static_cast<uint8_t>(top->seg_ones));

        // Onboard LEDs D0 (Alive) & D1 (Win), Active-Low
        bool d0_on = (top->led_alive == 0);
        bool d1_on = (top->led_win == 0);

        SDL_SetRenderDrawColor(ren, d0_on ? 34 : 24, d0_on ? 197 : 42, d0_on ? 94 : 32, 255);
        draw_filled_circle(ren, 52, 678, 8);
        draw_text(ren, 66, 672, "D0", 2, SDL_Color{203, 213, 225, 255});

        SDL_SetRenderDrawColor(ren, d1_on ? 250 : 40, d1_on ? 204 : 38, d1_on ? 21 : 24, 255);
        draw_filled_circle(ren, 116, 678, 8);
        draw_text(ren, 130, 672, "D1", 2, SDL_Color{203, 213, 225, 255});

        // Virtual Push Buttons + Onboard Switches
        draw_button(ren, btn_up_ui, up_pressed, SDL_Color{37, 99, 235, 255});
        draw_button(ren, btn_dn_ui, dn_pressed, SDL_Color{37, 99, 235, 255});
        draw_button(ren, btn_lt_ui, lt_pressed, SDL_Color{37, 99, 235, 255});
        draw_button(ren, btn_rt_ui, rt_pressed, SDL_Color{37, 99, 235, 255});
        draw_button(ren, btn_rst_ui, rst_pressed, SDL_Color{220, 38, 38, 255});
        draw_button(ren, btn_inv_ui, sw_row_inv, SDL_Color{217, 119, 6, 255});
        draw_button(ren, btn_map_ui, show_minimap, SDL_Color{13, 148, 136, 255});

        // Debug Minimap Panel
        if (show_minimap) {
            SDL_SetRenderDrawColor(ren, 30, 41, 59, 255);
            SDL_RenderDrawLine(ren, PANEL_W, 16, PANEL_W, WIN_H - 16);

            draw_text(ren, PANEL_W + 30, 20, "32X32 MINIMAP + VIEWPORT", 2, SDL_Color{56, 189, 248, 255});

            const auto* r = top->rootp;
            const int px_pos = r->maze_32x32_top__DOT__px;
            const int py_pos = r->maze_32x32_top__DOT__py;
            const int cam_x = r->maze_32x32_top__DOT__cam_x;
            const int cam_y = r->maze_32x32_top__DOT__cam_y;
            const int tens = r->maze_32x32_top__DOT__step_tens;
            const int ones = r->maze_32x32_top__DOT__step_ones;
            const bool won = (r->maze_32x32_top__DOT__game_won != 0);

            const int map_x0 = PANEL_W + 30;
            const int map_y0 = 54;
            const int cell_sz = 10;

            SDL_Rect map_bg{map_x0 - 2, map_y0 - 2, 32 * cell_sz + 4, 32 * cell_sz + 4};
            SDL_SetRenderDrawColor(ren, 9, 13, 22, 255);
            SDL_RenderFillRect(ren, &map_bg);
            SDL_SetRenderDrawColor(ren, 51, 65, 85, 255);
            SDL_RenderDrawRect(ren, &map_bg);

            for (int y = 0; y < 32; ++y) {
                uint32_t row_bits = r->maze_32x32_top__DOT__maze[y];
                for (int x = 0; x < 32; ++x) {
                    bool is_wall = ((row_bits >> (31 - x)) & 1) != 0;
                    SDL_Rect cell{map_x0 + x * cell_sz, map_y0 + y * cell_sz, cell_sz - 1, cell_sz - 1};
                    if (x == px_pos && y == py_pos) {
                        SDL_SetRenderDrawColor(ren, 34, 197, 94, 255);
                    } else if (x == 30 && y == 30) {
                        SDL_SetRenderDrawColor(ren, 250, 204, 21, 255);
                    } else if (is_wall) {
                        SDL_SetRenderDrawColor(ren, 100, 116, 139, 255);
                    } else {
                        SDL_SetRenderDrawColor(ren, 15, 23, 42, 255);
                    }
                    SDL_RenderFillRect(ren, &cell);
                }
            }

            // Highlight 16x16 camera viewport
            SDL_Rect vp{
                map_x0 + cam_x * cell_sz - 1,
                map_y0 + cam_y * cell_sz - 1,
                16 * cell_sz + 1,
                16 * cell_sz + 1
            };
            SDL_SetRenderDrawColor(ren, 56, 189, 248, 255);
            SDL_RenderDrawRect(ren, &vp);
            SDL_Rect vp2{vp.x + 1, vp.y + 1, vp.w - 2, vp.h - 2};
            SDL_RenderDrawRect(ren, &vp2);

            // Telemetry & Legend
            auto fmt2 = [](int v) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "%02d", v);
                return std::string(buf);
            };
            int ty = 398;
            draw_text(ren, map_x0, ty,      "PLAYER : (" + fmt2(px_pos) + ", " + fmt2(py_pos) + ")", 2, SDL_Color{226, 232, 240, 255});
            draw_text(ren, map_x0, ty + 26, "CAMERA : (" + fmt2(cam_x) + ", " + fmt2(cam_y) + ") 16X16", 2, SDL_Color{56, 189, 248, 255});
            draw_text(ren, map_x0, ty + 52, "STEPS  : " + std::to_string(tens) + std::to_string(ones), 2, SDL_Color{248, 113, 113, 255});
            draw_text(ren, map_x0, ty + 78, std::string("STATE  : ") + (won ? "GOAL REACHED" : "EXPLORING"), 2,
                      won ? SDL_Color{250, 204, 21, 255} : SDL_Color{74, 222, 128, 255});
            draw_text(ren, map_x0, ty + 104, std::string("SW0    : ") + (sw_row_inv ? "INVERTED" : "NORMAL"), 2,
                      sw_row_inv ? SDL_Color{251, 146, 60, 255} : SDL_Color{148, 163, 184, 255});

            draw_text(ren, map_x0, ty + 150, "CONTROLS:", 2, SDL_Color{148, 163, 184, 255});
            draw_text(ren, map_x0, ty + 176, "ARROWS / WASD : MOVE", 2, SDL_Color{203, 213, 225, 255});
            draw_text(ren, map_x0, ty + 200, "R : RESET (BT0)", 2, SDL_Color{203, 213, 225, 255});
            draw_text(ren, map_x0, ty + 224, "I : TOGGLE SW0 POLARITY", 2, SDL_Color{203, 213, 225, 255});
            draw_text(ren, map_x0, ty + 248, "M : TOGGLE MINIMAP", 2, SDL_Color{203, 213, 225, 255});
            draw_text(ren, map_x0, ty + 272, "ESC : EXIT", 2, SDL_Color{203, 213, 225, 255});
        }

        SDL_RenderPresent(ren);
    }

    if (tfp) {
        tfp->close();
        delete tfp;
    }
    top->final();
    delete top;

    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
