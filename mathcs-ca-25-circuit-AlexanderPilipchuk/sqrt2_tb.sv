`include "sqrt2.sv"

module sqrt2_tb_csv;
    reg CLK;
    integer fd;

    reg  [15:0] IO_DATA;
    wire [15:0] ans;
    assign ans = IO_DATA;

    wire IS_NAN;
    wire IS_PINF;
    wire IS_NINF;
    wire RESULT;

    reg ENABLE;
    reg [8*20-1:0] TEST_NAME;

    sqrt2 dut(
        .IO_DATA(ans),
        .IS_NAN(IS_NAN),
        .IS_PINF(IS_PINF),
        .IS_NINF(IS_NINF),
        .RESULT(RESULT),
        .CLK(CLK),
        .ENABLE(ENABLE)
    );

    initial begin
        CLK = 0;
        fd = $fopen("sqrt2_log.csv", "w");
        $fwrite(fd, "TestName,Time,CLK,IS_NAN,IS_PINF,IS_NINF,RESULT,DATA_OUT_hex\n");

        TEST_NAME = "POS_INF_TEST";
        IO_DATA   = 16'h7C00;
        ENABLE    = 1;
        #2;
        IO_DATA   = 16'bz;
        #23;
        ENABLE    = 0;
        #2;

        
        TEST_NAME = "NEG_INF_TEST";
        IO_DATA   = 16'hFC00;
        ENABLE    = 1;
        #2;
        IO_DATA   = 16'bz;
        #23;
        ENABLE    = 0;
        #2;

        TEST_NAME = "NAN_VARIANT_TEST";
        IO_DATA   = 16'h7E01;
        ENABLE    = 1;
        #2;
        IO_DATA   = 16'bz;
        #23;
        ENABLE    = 0;
        #2;

        TEST_NAME = "SNaN_TEST";
        IO_DATA   = 16'h7D01;
        ENABLE    = 1;
        #2;
        IO_DATA   = 16'bz;
        #23;
        ENABLE    = 0;
        #2;

        TEST_NAME = "P_ZERO_TEST";
        IO_DATA   = 16'h0000;
        ENABLE    = 1;
        #2;
        IO_DATA   = 16'bz;
        #23;
        ENABLE    = 0;
        #2;

        
        TEST_NAME = "N_ZERO_TEST";
        IO_DATA   = 16'h8000;
        ENABLE    = 1;
        #2;
        IO_DATA   = 16'bz;
        #23;
        ENABLE    = 0;
        #2;

        TEST_NAME = "MIN_DENORMAL_TEST";
        IO_DATA   = 16'h0002;
        ENABLE    = 1;
        #2;
        IO_DATA   = 16'bz;
        #23;
        ENABLE    = 0;
        #2;

        
        TEST_NAME = "DENORMAL_TEST";
        IO_DATA   = 16'h00F2;
        ENABLE    = 1;
        #2;
        IO_DATA   = 16'bz;
        #23;
        ENABLE    = 0;
        #2;

        
        TEST_NAME = "MAX_NUM_TEST";
        IO_DATA   = 16'h7BFE;
        ENABLE    = 1;
        #2;
        IO_DATA   = 16'bz;
        #23;
        ENABLE    = 0;
        #2;

        TEST_NAME = "NEGATIVE_TEST";
        IO_DATA   = 16'hC2D1;
        ENABLE    = 1;
        #2;
        IO_DATA   = 16'bz;
        #23;
        ENABLE    = 0;
        #2;

        TEST_NAME = "Base_SQRT";
        IO_DATA   = 16'h3E50;
        ENABLE    = 1;
        #2;
        IO_DATA   = 16'bz;
        #23;
        ENABLE    = 0;
        #2;


        TEST_NAME = "Hard_SQRT";
        IO_DATA   = 16'h4E42;
        ENABLE    = 1;
        #2;
        IO_DATA   = 16'bz;
        #23;
        ENABLE    = 0;
        #2;


        TEST_NAME = "MIN_DENORMAL_1_TEST";
        IO_DATA   = 16'h0001;
        ENABLE    = 1;
        #2;
        IO_DATA   = 16'bz;
        #23;
        ENABLE    = 0;
        #2;

        TEST_NAME = "MIN_DENORMAL_3_TEST";
        IO_DATA   = 16'h0003;
        ENABLE    = 1;
        #2;
        IO_DATA   = 16'bz;
        #23;
        ENABLE    = 0;
        #2;

        TEST_NAME = "MAX_DENORMAL_TEST";
        IO_DATA   = 16'h03FF;
        ENABLE    = 1;
        #2;
        IO_DATA   = 16'bz;
        #23;
        ENABLE    = 0;
        #2;

        TEST_NAME = "MIN_NORMAL_TEST";
        IO_DATA   = 16'h0400;
        ENABLE    = 1;
        #2;
        IO_DATA   = 16'bz;
        #23;
        ENABLE    = 0;
        #2;

        TEST_NAME = "JUST_ABOVE_MIN_NORMAL_TEST";
        IO_DATA   = 16'h0401;
        ENABLE    = 1;
        #2;
        IO_DATA   = 16'bz;
        #23;
        ENABLE    = 0;
        #2;

        TEST_NAME = "ONE_TEST";
        IO_DATA   = 16'h3C00;
        ENABLE    = 1;
        #2;
        IO_DATA   = 16'bz;
        #23;
        ENABLE    = 0;
        #2;

        TEST_NAME = "FOUR_TEST";
        IO_DATA   = 16'h4400;
        ENABLE    = 1;
        #2;
        IO_DATA   = 16'bz;
        #23;
        ENABLE    = 0;
        #2;

        TEST_NAME = "SMALL_NORMAL_TEST";
        IO_DATA   = 16'h0800;
        ENABLE    = 1;
        #2;
        IO_DATA   = 16'bz;
        #23;
        ENABLE    = 0;
        #2;

        $fclose(fd);
        $finish;
    end

    always #1 CLK = ~CLK;

    always @(posedge CLK) begin
        if (ENABLE) begin
            $fwrite(fd, "%s,%0d,%b,%b,%b,%b,%b,%04h\n",
                TEST_NAME, $time, CLK, IS_NAN, IS_PINF, IS_NINF, RESULT, ans);
        end
    end

endmodule
