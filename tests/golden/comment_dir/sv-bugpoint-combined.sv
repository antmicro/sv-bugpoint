`define KEEP
  module t;
  initial begin
    `ifndef KEEP
      $stop
    `endif
    $finish;
  end
  endmodule
