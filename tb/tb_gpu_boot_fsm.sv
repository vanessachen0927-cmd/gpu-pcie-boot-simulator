module tb_gpu_boot_fsm;

    logic clk;
    initial clk = 0;
    logic rst_n = 0;
    logic start = 0;
    logic fw_valid = 0;
    logic fw_load_done = 0;

    logic ready;
    logic error;
    logic [2:0] state;

    localparam logic [2:0] IDLE = 3'd0;
    localparam logic [2:0] FW_LOAD = 3'd1;
    localparam logic [2:0] READY = 3'd3;
    localparam logic [2:0] ERROR = 3'd4;

    always #5 clk = ~clk;

    gpu_boot_fsm #(
        .TIMEOUT_CYCLES(8)
    ) dut (
        .clk(clk),
        .rst_n(rst_n),
        .start(start),
        .fw_valid(fw_valid),
        .fw_load_done(fw_load_done),
        .ready(ready),
        .error(error),
        .state(state)
    );

    task automatic tick;
        @(posedge clk);
        #1;
    endtask

    task automatic reset_dut;
        @(negedge clk);
        rst_n = 0;
        start = 0;
        fw_valid = 0;
        fw_load_done = 0;
        tick();
        if (state !== IDLE || ready || error)
            $fatal(1, "Reset did not return to IDLE");
        @(negedge clk);
        rst_n = 1;
    endtask

    task automatic begin_boot;
        @(negedge clk);
        start = 1;
        tick();
        if (state !== FW_LOAD)
            $fatal(1, "Start did not enter FW_LOAD");
        @(negedge clk);
        start = 0;
    endtask

    initial begin
        // Test 1: Successful firmware boot.
        reset_dut();
        begin_boot();

        fw_load_done = 1;
        fw_valid = 1;
        tick();
        tick();

        if (!ready || error || state !== READY)
            $fatal(1, "Normal boot failed");

        $display("PASS: normal GPU firmware boot");

        // Test 2: Firmware authentication failure.
        reset_dut();
        begin_boot();

        fw_load_done = 1;
        fw_valid = 0;
        tick();
        tick();

        if (!error || ready || state !== ERROR)
            $fatal(1, "Invalid firmware was accepted");

        $display("PASS: invalid firmware rejected");

        // Test 3: Firmware load timeout.
        reset_dut();
        begin_boot();

        repeat (9) tick();

        if (!error || state !== ERROR)
            $fatal(1, "Timeout was not detected");

        $display("PASS: firmware load timeout detected");

        // Test 4: Reset and recovery.
        reset_dut();

        if (state !== IDLE || ready || error)
            $fatal(1, "Controller failed to recover");

        $display("PASS: reset recovery");
        $display("ALL GPU BOOT RTL TESTS PASSED");

        $finish;
    end

endmodule
