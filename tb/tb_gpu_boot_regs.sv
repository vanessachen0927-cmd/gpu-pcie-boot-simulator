module tb_gpu_boot_regs;

    logic clk;
    initial clk = 0;
    always #5 clk = ~clk;

    logic rst_n = 0;
    logic bus_valid = 0;
    logic bus_write = 0;
    logic [7:0] bus_addr = 0;
    logic [31:0] bus_wdata = 0;
    logic [31:0] bus_rdata;
    logic fw_valid = 0;
    logic fw_load_done = 0;
    logic boot_ready;
    logic boot_error;

    gpu_boot_regs #(
        .TIMEOUT_CYCLES(8)
    ) dut (
        .clk(clk),
        .rst_n(rst_n),
        .bus_valid(bus_valid),
        .bus_write(bus_write),
        .bus_addr(bus_addr),
        .bus_wdata(bus_wdata),
        .bus_rdata(bus_rdata),
        .fw_valid(fw_valid),
        .fw_load_done(fw_load_done),
        .boot_ready(boot_ready),
        .boot_error(boot_error)
    );

    task automatic tick;
        @(posedge clk);
        #1;
    endtask

    task automatic reset_dut;
        @(negedge clk);
        rst_n = 0;
        bus_valid = 0;
        bus_write = 0;
        fw_valid = 0;
        fw_load_done = 0;
        tick();
        @(negedge clk);
        rst_n = 1;
    endtask

    task automatic start_boot;
        @(negedge clk);
        bus_valid = 1;
        bus_write = 1;
        bus_addr = 8'h00;
        bus_wdata = 32'h1;
        tick();
        @(negedge clk);
        bus_valid = 0;
        bus_write = 0;
    endtask

    task automatic check_read(
        input logic [7:0] address,
        input logic [31:0] expected,
        input logic [31:0] mask
    );
        @(negedge clk);
        bus_addr = address;
        bus_valid = 1;
        bus_write = 0;
        #1;
        if ((bus_rdata & mask) !== expected)
            $fatal(1, "Register 0x%02h: got %08h expected %08h",
                   address, bus_rdata & mask, expected);
        bus_valid = 0;
    endtask

    initial begin
        // Successful boot initiated via register write.
        reset_dut();
        start_boot();

        fw_load_done = 1;
        fw_valid = 1;
        tick();
        tick();

        if (!boot_ready || boot_error)
            $fatal(1, "Register-driven boot failed");

        check_read(8'h04, 32'h1, 32'h1);
        check_read(8'h08, 32'h0, 32'hFFFFFFFF);
        check_read(8'h0C, 32'h3, 32'h3);

        $display("PASS: register-driven normal boot");

        // Invalid firmware.
        reset_dut();
        start_boot();

        fw_load_done = 1;
        fw_valid = 0;
        tick();
        tick();

        if (!boot_error || boot_ready)
            $fatal(1, "Invalid firmware was accepted");

        check_read(8'h08, 32'h1, 32'hFFFFFFFF);
        $display("PASS: register-reported firmware error");

        // Timeout.
        reset_dut();
        start_boot();

        repeat (9) tick();

        if (!boot_error)
            $fatal(1, "Timeout not detected");

        check_read(8'h08, 32'h2, 32'hFFFFFFFF);
        $display("PASS: register-reported timeout");

        // Reset clears error.
        reset_dut();
        check_read(8'h08, 32'h0, 32'hFFFFFFFF);
        $display("PASS: reset clears hardware error");

        $display("ALL GPU REGISTER INTERFACE TESTS PASSED");
        $finish;
    end

endmodule
