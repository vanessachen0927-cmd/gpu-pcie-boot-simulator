module gpu_boot_regs #(
    parameter int unsigned TIMEOUT_CYCLES = 8
) (
    input  logic        clk,
    input  logic        rst_n,

    // Simplified memory-mapped register interface.
    input  logic        bus_valid,
    input  logic        bus_write,
    input  logic [7:0]  bus_addr,
    input  logic [31:0] bus_wdata,
    output logic [31:0] bus_rdata,

    // Modeled firmware subsystem signals.
    input  logic        fw_valid,
    input  logic        fw_load_done,

    output logic        boot_ready,
    output logic        boot_error
);

    localparam logic [7:0] BOOT_CONTROL = 8'h00;
    localparam logic [7:0] BOOT_STATUS  = 8'h04;
    localparam logic [7:0] ERROR_CODE   = 8'h08;
    localparam logic [7:0] FW_STATUS    = 8'h0C;

    localparam logic [2:0] FW_VERIFY = 3'd2;
    localparam logic [2:0] ERROR     = 3'd4;

    logic [2:0] boot_state;
    logic [31:0] error_code;

    logic start_pulse;

    assign start_pulse =
        bus_valid &&
        bus_write &&
        bus_addr == BOOT_CONTROL &&
        bus_wdata[0];

    gpu_boot_fsm #(
        .TIMEOUT_CYCLES(TIMEOUT_CYCLES)
    ) controller (
        .clk(clk),
        .rst_n(rst_n),
        .start(start_pulse),
        .fw_valid(fw_valid),
        .fw_load_done(fw_load_done),
        .ready(boot_ready),
        .error(boot_error),
        .state(boot_state)
    );

    // Record firmware failures and loading timeouts.
    always_ff @(posedge clk or negedge rst_n) begin
        if (!rst_n) begin
            error_code <= 32'd0;
        end else if (boot_state == FW_VERIFY && !fw_valid) begin
            error_code <= 32'd1;
        end else if (boot_state == ERROR && error_code == 0) begin
            error_code <= 32'd2;
        end
    end

    // Combinational register reads.
    always_comb begin
        bus_rdata = 32'd0;

        if (bus_valid && !bus_write) begin
            case (bus_addr)
                BOOT_CONTROL: bus_rdata = 32'd0;
                BOOT_STATUS:  bus_rdata =
                    {24'd0, boot_state, 3'd0,
                     boot_error, boot_ready};
                ERROR_CODE:   bus_rdata =
                    boot_error && error_code == 0
                    ? 32'd2 : error_code;
                FW_STATUS:    bus_rdata =
                    {30'd0, fw_valid, fw_load_done};
                default: bus_rdata = 32'd0;
            endcase
        end
    end

endmodule
