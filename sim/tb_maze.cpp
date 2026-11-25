#include <verilated.h>
#include <verilated_vcd_c.h>

#include "Vmaze_32x32_top.h"
#include "Vmaze_32x32_top___024root.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <queue>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

// SIM_SHIFT=12 -> DB_MAX = 240000 >> 12 = 58
constexpr int DB_WAIT_CYCLES = 64;

constexpr uint8_t SEG_LUT[10] = {
    0b0111111, // 0
    0b0000110, // 1
    0b1011011, // 2
    0b1001111, // 3
    0b1100110, // 4
    0b1101101, // 5
    0b1111101, // 6
    0b0000111, // 7
    0b1111111, // 8
    0b1101111  // 9
};

struct SimHarness {
    Vmaze_32x32_top* top = nullptr;
    VerilatedVcdC* tfp = nullptr;
    uint64_t sim_time = 0;

    explicit SimHarness(const char* vcd_path) {
        Verilated::traceEverOn(true);
        top = new Vmaze_32x32_top;
        tfp = new VerilatedVcdC;
        top->trace(tfp, 99);
        tfp->open(vcd_path);

        top->clk = 0;
        top->rst_n = 1;
        top->sw_row_inv = 0;
        top->btn_up = 0;
        top->btn_down = 0;
        top->btn_left = 0;
        top->btn_right = 0;
        top->eval();
    }

    ~SimHarness() {
        if (tfp) {
            tfp->close();
            delete tfp;
        }
        if (top) {
            top->final();
            delete top;
        }
    }

    void tick() {
        top->clk = 0;
        top->eval();
        tfp->dump(sim_time++);
        top->clk = 1;
        top->eval();
        tfp->dump(sim_time++);
    }

    void step_cycles(int n) {
        for (int i = 0; i < n; ++i) tick();
    }

    void reset() {
        top->btn_up = 0;
        top->btn_down = 0;
        top->btn_left = 0;
        top->btn_right = 0;
        top->sw_row_inv = 0;
        top->rst_n = 0;
        step_cycles(4);
        top->rst_n = 1;
        step_cycles(2);
    }

    void clear_buttons() {
        top->btn_up = 0;
        top->btn_down = 0;
        top->btn_left = 0;
        top->btn_right = 0;
    }

    void press_dir(int dx, int dy) {
        clear_buttons();
        if (dy < 0) top->btn_up = 1;
        else if (dy > 0) top->btn_down = 1;
        else if (dx < 0) top->btn_left = 1;
        else if (dx > 0) top->btn_right = 1;
        step_cycles(DB_WAIT_CYCLES);
        clear_buttons();
        step_cycles(DB_WAIT_CYCLES);
    }

    const Vmaze_32x32_top___024root* r() const { return top->rootp; }
};

void check(bool cond, const std::string& msg) {
    if (!cond) {
        throw std::runtime_error(msg);
    }
}

std::vector<std::pair<int, int>> solve_maze_bfs(const Vmaze_32x32_top___024root* r) {
    auto is_wall = [&](int x, int y) -> bool {
        return ((r->maze_32x32_top__DOT__maze[y] >> (31 - x)) & 1) != 0;
    };

    std::array<std::array<bool, 32>, 32> vis{};
    std::array<std::array<std::pair<int, int>, 32>, 32> parent{};
    std::queue<std::pair<int, int>> q;

    vis[1][1] = true;
    parent[1][1] = {-1, -1};
    q.push({1, 1});

    const int dirs[4][2] = {{0, -1}, {0, 1}, {-1, 0}, {1, 0}};
    bool found = false;
    while (!q.empty()) {
        auto [x, y] = q.front();
        q.pop();
        if (x == 30 && y == 30) {
            found = true;
            break;
        }
        for (const auto& d : dirs) {
            int nx = x + d[0];
            int ny = y + d[1];
            if (nx >= 0 && nx < 32 && ny >= 0 && ny < 32 && !vis[ny][nx] && !is_wall(nx, ny)) {
                vis[ny][nx] = true;
                parent[ny][nx] = {x, y};
                q.push({nx, ny});
            }
        }
    }

    check(found, "BFS failed to find path from (1,1) to (30,30)");

    std::vector<std::pair<int, int>> path;
    std::pair<int, int> cur{30, 30};
    while (cur.first != -1) {
        path.push_back(cur);
        cur = parent[cur.second][cur.first];
    }
    std::reverse(path.begin(), path.end());
    return path;
}

void test_reset_and_heartbeat(SimHarness& sim) {
    sim.reset();
    const auto* r = sim.r();
    check(r->maze_32x32_top__DOT__px == 1 && r->maze_32x32_top__DOT__py == 1,
          "Reset should place player at (1,1)");
    check(sim.top->seg_tens == SEG_LUT[0] && sim.top->seg_ones == SEG_LUT[0],
          "Reset should clear 7-seg step counter to 00");
    check(sim.top->led_win == 1, "Reset should keep D1 led_win OFF (Active-Low = 1)");

    bool saw_low = false, saw_high = false;
    for (int i = 0; i < 2048; ++i) {
        sim.tick();
        if (sim.top->led_alive == 0) saw_low = true;
        if (sim.top->led_alive == 1) saw_high = true;
    }
    check(saw_low && saw_high, "D0 led_alive heartbeat must toggle");
    std::cout << "[PASS] Test 1: Reset & D0 Heartbeat\n";
}

void test_wall_collision(SimHarness& sim) {
    sim.reset();
    sim.press_dir(0, -1); // UP into wall (1,0)
    sim.press_dir(-1, 0); // LEFT into wall (0,1)
    const auto* r = sim.r();
    check(r->maze_32x32_top__DOT__px == 1 && r->maze_32x32_top__DOT__py == 1,
          "Player must not walk into walls");
    check(r->maze_32x32_top__DOT__step_tens == 0 && r->maze_32x32_top__DOT__step_ones == 0,
          "Wall collision must not increment steps");
    std::cout << "[PASS] Test 2: Wall Collision Detection\n";
}

void test_single_step_and_bcd_carry(SimHarness& sim) {
    sim.reset();
    const auto* r = sim.r();

    // Hold RIGHT continuously across 4 debounce periods -> must move only 1 cell
    sim.top->btn_right = 1;
    sim.step_cycles(4 * DB_WAIT_CYCLES);
    sim.clear_buttons();
    sim.step_cycles(DB_WAIT_CYCLES);

    check(r->maze_32x32_top__DOT__px == 2 && r->maze_32x32_top__DOT__py == 1,
          "Holding button must only advance 1 cell");
    check(r->maze_32x32_top__DOT__step_tens == 0 && r->maze_32x32_top__DOT__step_ones == 1,
          "Holding button must count only 1 step");

    // Take 9 more valid steps (4 right to (5,1), 2 down to (5,3), 2 left to (3,3), 1 down to (3,4))
    const int moves[9][2] = {
        {1, 0}, {1, 0}, {1, 0},   // -> (5,1) steps 2,3,4
        {0, 1}, {0, 1},           // -> (5,3) steps 5,6
        {-1, 0}, {-1, 0},         // -> (3,3) steps 7,8
        {0, 1},                   // -> (3,4) step 9
        {0, 1}                    // -> (3,5) step 10
    };
    for (int i = 0; i < 8; ++i) {
        sim.press_dir(moves[i][0], moves[i][1]);
    }
    check(sim.top->seg_tens == SEG_LUT[0] && sim.top->seg_ones == SEG_LUT[9],
          "Step 09 must decode to 09 on 7-seg");

    sim.press_dir(moves[8][0], moves[8][1]);
    check(sim.top->seg_tens == SEG_LUT[1] && sim.top->seg_ones == SEG_LUT[0],
          "Step 10 must carry BCD to 10 on 7-seg");
    std::cout << "[PASS] Test 3: Single-Step Edge Detection & BCD Carry (09 -> 10)\n";
}

void test_camera_clamping_and_row_inv(SimHarness& sim) {
    sim.reset();
    const auto* r = sim.r();
    auto path = solve_maze_bfs(r);

    bool saw_left_clamp = false;
    bool saw_mid_x = false;
    bool saw_right_clamp = false;
    bool saw_top_clamp = false;
    bool saw_mid_y = false;
    bool saw_bottom_clamp = false;

    for (size_t i = 1; i < path.size(); ++i) {
        int dx = path[i].first - path[i - 1].first;
        int dy = path[i].second - path[i - 1].second;
        sim.press_dir(dx, dy);

        int px = r->maze_32x32_top__DOT__px;
        int py = r->maze_32x32_top__DOT__py;
        int cx = r->maze_32x32_top__DOT__cam_x;
        int cy = r->maze_32x32_top__DOT__cam_y;

        int exp_cx = (px < 8) ? 0 : (px > 23 ? 16 : px - 8);
        int exp_cy = (py < 8) ? 0 : (py > 23 ? 16 : py - 8);
        check(cx == exp_cx && cy == exp_cy, "Viewport camera clamping mismatch");

        if (px < 8) saw_left_clamp = true;
        else if (px > 23) saw_right_clamp = true;
        else saw_mid_x = true;

        if (py < 8) saw_top_clamp = true;
        else if (py > 23) saw_bottom_clamp = true;
        else saw_mid_y = true;
    }

    check(saw_left_clamp && saw_mid_x && saw_right_clamp, "Must exercise all X clamping regions");
    check(saw_top_clamp && saw_mid_y && saw_bottom_clamp, "Must exercise all Y clamping regions");

    // Check SW0 row polarity inversion across all 16 scan rows
    for (int i = 0; i < 16; ++i) {
        sim.top->sw_row_inv = 0;
        sim.top->eval();
        uint16_t normal_row = sim.top->mat_row;
        sim.top->sw_row_inv = 1;
        sim.top->eval();
        uint16_t inv_row = sim.top->mat_row;
        check(inv_row == static_cast<uint16_t>(~normal_row),
              "sw_row_inv must invert all 16 bits of mat_row");
        sim.tick();
    }
    sim.top->sw_row_inv = 0;
    std::cout << "[PASS] Test 4: Camera Clamping & SW0 Polarity Inversion\n";
}

void test_goal_and_win_lock(SimHarness& sim) {
    sim.reset();
    const auto* r = sim.r();
    auto path = solve_maze_bfs(r);
    int shortest_steps = static_cast<int>(path.size()) - 1;
    check(shortest_steps == 90, "Expected 90-step shortest path in maze");

    for (size_t i = 1; i < path.size(); ++i) {
        int dx = path[i].first - path[i - 1].first;
        int dy = path[i].second - path[i - 1].second;
        sim.press_dir(dx, dy);
    }

    check(r->maze_32x32_top__DOT__px == 30 && r->maze_32x32_top__DOT__py == 30,
          "Player must reach goal (30,30)");
    check(r->maze_32x32_top__DOT__game_won == 1, "game_won must assert at (30,30)");
    check(sim.top->led_win == 0, "D1 led_win must light up (Active-Low = 0) on win");
    check(sim.top->seg_tens == SEG_LUT[9] && sim.top->seg_ones == SEG_LUT[0],
          "7-seg must show 90 steps on shortest path completion");

    // Try moving after winning -> position and steps must stay locked
    sim.press_dir(-1, 0);
    sim.press_dir(0, -1);
    check(r->maze_32x32_top__DOT__px == 30 && r->maze_32x32_top__DOT__py == 30,
          "Player position must lock after winning");
    check(r->maze_32x32_top__DOT__step_tens == 9 && r->maze_32x32_top__DOT__step_ones == 0,
          "Step counter must freeze after winning");

    // Verify win border animation toggles on row 0
    bool saw_border_on = false, saw_border_off = false;
    for (int i = 0; i < 1024; ++i) {
        sim.tick();
        if (sim.top->mat_row == 0x0001) { // scan_row == 0
            if (sim.top->mat_col == 0xFFFF) saw_border_on = true;
            if (sim.top->mat_col == 0x0000) saw_border_off = true;
        }
    }
    check(saw_border_on && saw_border_off, "Win animation border must blink on row 0");
    std::cout << "[PASS] Test 5: Goal Reach (90 steps), Win Lock & Border Animation\n";
}

} // namespace

int main(int argc, char** argv) {
    Verilated::commandArgs(argc, argv);

    try {
        SimHarness sim("build/maze_test.vcd");
        test_reset_and_heartbeat(sim);
        test_wall_collision(sim);
        test_single_step_and_bcd_carry(sim);
        test_camera_clamping_and_row_inv(sim);
        test_goal_and_win_lock(sim);
        std::cout << "ALL 5 VERIFICATION SUITES PASSED (VCD: build/maze_test.vcd)\n";
    } catch (const std::exception& ex) {
        std::cerr << "[FAIL] " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
