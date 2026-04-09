module t;
  logic a, b;
  covergroup cg @ (posedge a);
    coverpoint b;
  endgroup
  cg cg_inst;
  int c;
  bit d;
  initial begin;
    d = c;
    $finish;
  end;
endmodule
