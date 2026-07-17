module t;
  logic clk, a;
  property p;
    @(posedge clk) a;
  endproperty
  initial begin
    $finish;
  end;
endmodule
