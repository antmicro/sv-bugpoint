`define KEEP //comment-at-line-end (currently not removed)
  module t;
  initial begin
    `ifndef KEEP
      $stop
    `endif
    $finish;
  end
  endmodule
