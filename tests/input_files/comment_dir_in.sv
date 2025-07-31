// non-functional comment, should be removed
`define KEEP
`define REMOVE
// non-functional comment, should be removed
`line 1 "this/line/directive/can/be/removed" 0
 // verilator lint_off ASSIGNDLY
  module t;
  `define myfinish $finish
  initial begin
    `ifndef KEEP
      $stop
    `endif
    $finish;
  end
  endmodule

`line 2 "this/line/directive/can/be/removed/2" 1
// non-functional comment, should be removed