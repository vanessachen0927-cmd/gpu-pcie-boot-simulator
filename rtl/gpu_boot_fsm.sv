module gpu_boot_fsm #(
    parameter int unsigned TIMEOUT_CYCLES = 8
) (
    input  logic       clk,
    input  logic       rst_n,
    input  logic       start,
    input  logic       fw_valid,
    input  logic       fw_load_done,
    output logic       ready,
    output logic       error,
    output logic [2:0] state
);

    typedef enum logic [2:0] {
        IDLE      = 3'd0,
        FW_LOAD   = 3'd1,
        FW_VERIFY = 3'd2,
        READY     = 3'd3,
        ERROR     = 3'd4
    } boot_state_t;

    boot_state_t current_state, next_state;
    logic [31:0] load_cycles;

    // Sequential state and timeout counter.
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            current_state <= IDLE;
            load_cycles   <= 32'd0;
        end else begin
            current_state <= next_state;

            if (current_state != FW_LOAD)
                load_cycles <= 32'd0;
            else if (!fw_load_done)
                load_cycles <= load_cycles + 32'd1;
        end
    end

    // Combinational next-state logic.
    always_comb begin
        next_state = current_state;

        case (current_state)
            IDLE: begin
                if (start)
                    next_state = FW_LOAD;
            end

            FW_LOAD: begin
                if (fw_load_done)
                    next_state = FW_VERIFY;
                else if (load_cycles >= TIMEOUT_CYCLES - 1)
                    next_state = ERROR;
            end

            FW_VERIFY: begin
                if (fw_valid)
                    next_state = READY;
                else
                    next_state = ERROR;
            end

            READY: next_state = READY;
            ERROR: next_state = ERROR;

            default: next_state = ERROR;
        endcase
    end

    assign state = current_state;
    assign ready = (current_state == READY);
    assign error = (current_state == ERROR);

endmodule
