//non-functional comment, should be removed
`define KEEP //comment-at-line-end (currently not removed)
`define REMOVE //comment-at-line-end (currently removed as side-effect)
//`define REMOVETOO
  module t;
  `define myfinish $finish
  initial begin
     //non-functional comment, should be removed
    `ifndef KEEP
      $stop
    `endif
    $finish;
  end
  endmodule
`line 2 "this/line/directive/can/be/removed/2" 1
