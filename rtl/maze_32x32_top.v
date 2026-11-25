module maze_32x32_top #(
    parameter integer SIM_SHIFT = 0
)(
    input  wire        clk,        // PIN_28 (12MHz from VeriInstrument)
    input  wire        rst_n,      // PIN_132 (Onboard BT0, Active-Low)
    input  wire        sw_row_inv, // PIN_134 (Onboard SW0: invert Matrix Row polarity)
    input  wire        btn_up,     // Virtual Push Button (Active-High)
    input  wire        btn_down,   // Virtual Push Button (Active-High)
    input  wire        btn_left,   // Virtual Push Button (Active-High)
    input  wire        btn_right,  // Virtual Push Button (Active-High)
    output wire        led_alive,  // PIN_122 (Onboard D0 heartbeat, Active-Low)
    output wire        led_win,    // PIN_123 (Onboard D1: lights up when reaching goal, Active-Low)
    output wire [15:0] mat_row,    // Matrix LED 16x16 Row[15:0]
    output reg  [15:0] mat_col,    // Matrix LED 16x16 Col[15:0]
    output reg  [6:0]  seg_tens,   // 7-Seg Tens digit {g,f,e,d,c,b,a}
    output reg  [6:0]  seg_ones    // 7-Seg Ones digit {g,f,e,d,c,b,a}
);

    // =========================================================================
    // 0. Simulation / Hardware Timing Parameters
    // =========================================================================
    // When SIM_SHIFT = 0 (default for Quartus II), all counters match 12 MHz.
    // In Verilator simulation, SIM_SHIFT > 0 scales counters by 2^SIM_SHIFT.
    localparam [17:0] DB_MAX         = (240000 >> SIM_SHIFT);
    localparam integer SCAN_LSB      = 12 - SIM_SHIFT;
    localparam integer BLINK_FAST_BIT = 20 - SIM_SHIFT;
    localparam integer BLINK_SLOW_BIT = 22 - SIM_SHIFT;

    // =========================================================================
    // 1. Clock Dividers & Blink Generators (12MHz USB Clock at SIM_SHIFT=0)
    // =========================================================================
    reg [23:0] cnt;
    always @(posedge clk or negedge rst_n) begin
        if (!rst_n)
            cnt <= 24'd0;
        else
            cnt <= cnt + 24'd1;
    end

    assign led_alive = cnt[BLINK_SLOW_BIT]; // ~1.4 Hz heartbeat on D0
    wire blink_fast  = cnt[BLINK_FAST_BIT]; // ~5.7 Hz blink for Player
    wire blink_slow  = cnt[BLINK_SLOW_BIT]; // ~1.4 Hz blink for Goal

    // =========================================================================
    // 2. 32x32 Maze Map ROM (0: Path, 1: Wall, x=0..31 from left to right)
    // =========================================================================
    /* verilator lint_off ASCRANGE */
    reg [0:31] maze [0:31] /* verilator public_flat_rd */;
    /* verilator lint_on ASCRANGE */
    initial begin
        //             01234567890123456789012345678901
        maze[ 0] = 32'b11111111111111111111111111111111;
        maze[ 1] = 32'b10000010000000001000000000000001; // Start at (1,1)
        maze[ 2] = 32'b10111010111111101011111111111101;
        maze[ 3] = 32'b10100010100000100010000000000101;
        maze[ 4] = 32'b10101110101110111110111111110101;
        maze[ 5] = 32'b10000000000000000000100000010101;
        maze[ 6] = 32'b11111011101011111111101111010101;
        maze[ 7] = 32'b10000010001010000000001001010001;
        maze[ 8] = 32'b10111110111010111111111011011111;
        maze[ 9] = 32'b10000000100010000000001000000001;
        maze[10] = 32'b11111111101111101111101111111101;
        maze[11] = 32'b10000000001000000000100000000101;
        maze[12] = 32'b10111111111011101110111111110101;
        maze[13] = 32'b10100000000010000010000000010101;
        maze[14] = 32'b10101111111110111011111111010101;
        maze[15] = 32'b10101000000000100000000001000101;
        maze[16] = 32'b10101011111111101111011101111101;
        maze[17] = 32'b10001010000000001000000100000001;
        maze[18] = 32'b11101010111111111011110111111101;
        maze[19] = 32'b10001000100000000010010000000101;
        maze[20] = 32'b10111111101111111110110111110101;
        maze[21] = 32'b10000000101000000000100000010101;
        maze[22] = 32'b11111110101011111111101111010101;
        maze[23] = 32'b10000010101010000000001001000101;
        maze[24] = 32'b10111010101010111111111001111101;
        maze[25] = 32'b10100010001000100000000010000001;
        maze[26] = 32'b10101111111111101111111110101101;
        maze[27] = 32'b10100000000000001000000000100101;
        maze[28] = 32'b10111111111111111011111111101101;
        maze[29] = 32'b10000000000000000010000000000001;
        maze[30] = 32'b11111111111111111010111111111101; // Goal at (30,30)
        maze[31] = 32'b11111111111111111111111111111111;
    end

    // =========================================================================
    // 3. Button Debounce & Single-Step Edge Detection (~20ms sample interval)
    // =========================================================================
    reg [17:0] db_cnt;
    wire db_tick /* verilator public_flat_rd */;
    assign db_tick = (db_cnt == DB_MAX);

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n)
            db_cnt <= 18'd0;
        else if (db_tick)
            db_cnt <= 18'd0;
        else
            db_cnt <= db_cnt + 18'd1;
    end

    reg [3:0] btn_sync, btn_prev;
    wire [3:0] btn_raw   = {btn_up, btn_down, btn_left, btn_right};
    wire [3:0] btn_pulse = btn_sync & ~btn_prev; // Rising edge pulse

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            btn_sync <= 4'd0;
            btn_prev <= 4'd0;
        end else begin
            btn_prev <= btn_sync;
            if (db_tick)
                btn_sync <= btn_raw;
        end
    end

    // =========================================================================
    // 4. Player Movement, Collision Detection & BCD Step Counter
    // =========================================================================
    localparam [4:0] GOAL_X = 5'd30;
    localparam [4:0] GOAL_Y = 5'd30;

    reg [4:0] px /* verilator public_flat_rd */;
    reg [4:0] py /* verilator public_flat_rd */;
    reg [3:0] step_tens /* verilator public_flat_rd */;
    reg [3:0] step_ones /* verilator public_flat_rd */;

    wire game_won /* verilator public_flat_rd */;
    assign game_won = (px == GOAL_X) && (py == GOAL_Y);
    assign led_win  = ~game_won; // Active-Low onboard LED D1

    // Calculate candidate next position
    reg [4:0] next_x, next_y;
    reg       move_req;

    always @(*) begin
        next_x   = px;
        next_y   = py;
        move_req = 1'b0;
        if (!game_won) begin
            if (btn_pulse[3]) begin          // UP
                next_y   = py - 5'd1;
                move_req = 1'b1;
            end else if (btn_pulse[2]) begin // DOWN
                next_y   = py + 5'd1;
                move_req = 1'b1;
            end else if (btn_pulse[1]) begin // LEFT
                next_x   = px - 5'd1;
                move_req = 1'b1;
            end else if (btn_pulse[0]) begin // RIGHT
                next_x   = px + 5'd1;
                move_req = 1'b1;
            end
        end
    end

    always @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            px        <= 5'd1;
            py        <= 5'd1;
            step_tens <= 4'd0;
            step_ones <= 4'd0;
        end else if (move_req) begin
            // Check wall collision (0 = walkable path, 1 = wall)
            if (maze[next_y][next_x] == 1'b0) begin
                px <= next_x;
                py <= next_y;
                // Increment BCD step counter up to 99
                if (step_ones == 4'd9) begin
                    if (step_tens < 4'd9) begin
                        step_ones <= 4'd0;
                        step_tens <= step_tens + 4'd1;
                    end
                end else begin
                    step_ones <= step_ones + 4'd1;
                end
            end
        end
    end

    // =========================================================================
    // 5. 16x16 Viewport Camera Clamping (Prevents Underflow/Overflow)
    // =========================================================================
    wire [4:0] cam_x /* verilator public_flat_rd */;
    wire [4:0] cam_y /* verilator public_flat_rd */;

    assign cam_x = (px < 5'd8)  ? 5'd0  :
                   (px > 5'd23) ? 5'd16 : (px - 5'd8);

    assign cam_y = (py < 5'd8)  ? 5'd0  :
                   (py > 5'd23) ? 5'd16 : (py - 5'd8);

    // =========================================================================
    // 6. 16x16 Matrix LED Dynamic Scanner (~2.9kHz row rate at 12MHz)
    // =========================================================================
    wire [3:0] scan_row = cnt[SCAN_LSB + 3 : SCAN_LSB];
    reg  [15:0] row_one_hot;
    assign mat_row = sw_row_inv ? ~row_one_hot : row_one_hot;

    wire [4:0] abs_y = cam_y + {1'b0, scan_row};

    integer c;
    reg [4:0] abs_x;
    reg is_wall, is_player, is_goal, is_border;

    always @(*) begin
        row_one_hot = (16'd1 << scan_row);
        for (c = 0; c < 16; c = c + 1) begin
            abs_x     = cam_x + c[4:0];
            is_wall   = maze[abs_y][abs_x];
            is_player = (abs_x == px) && (abs_y == py) && blink_fast;
            is_goal   = (abs_x == GOAL_X) && (abs_y == GOAL_Y) && blink_slow;
            is_border = (scan_row == 4'd0) || (scan_row == 4'd15) || (c == 0) || (c == 15);

            if (game_won)
                mat_col[c] = is_border ? blink_fast : (is_player | is_goal);
            else
                mat_col[c] = is_wall | is_player | is_goal;
        end
    end

    // =========================================================================
    // 7. Seven-Segment Decoders (Active-High {g,f,e,d,c,b,a})
    // =========================================================================
    function [6:0] bcd_to_7seg;
        input [3:0] bcd;
        begin
            case (bcd)
                4'd0: bcd_to_7seg = 7'b0111111;
                4'd1: bcd_to_7seg = 7'b0000110;
                4'd2: bcd_to_7seg = 7'b1011011;
                4'd3: bcd_to_7seg = 7'b1001111;
                4'd4: bcd_to_7seg = 7'b1100110;
                4'd5: bcd_to_7seg = 7'b1101101;
                4'd6: bcd_to_7seg = 7'b1111101;
                4'd7: bcd_to_7seg = 7'b0000111;
                4'd8: bcd_to_7seg = 7'b1111111;
                4'd9: bcd_to_7seg = 7'b1101111;
                default: bcd_to_7seg = 7'b0000000;
            endcase
        end
    endfunction

    always @(*) begin
        seg_tens = bcd_to_7seg(step_tens);
        seg_ones = bcd_to_7seg(step_ones);
    end

endmodule
