module t;
  logic clk, a;
  sequence s;
    @(posedge clk) a;
  endsequence
  initial begin
    $finish;
  end;
endmodule
