module t
#(
);
    int a[5];
    bit [6:0] b;
    bit [10-1:0] c;
    logic [10-1:0] packed_a;
    logic [1+5:0] packed_b;
    initial begin
        $display("%0d,%0d,%0d,%0d,%0d,%0d,%5b,%5b,%0d", $size(a), $size(b), $size(c),10, $size(packed_a), $size(packed_b),5'b1010,5'('b111111),3);
        $finish;
    end
endmodule
