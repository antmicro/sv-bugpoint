module t;
  int i=0;
  function void f();
    if(i==1) $finish;
    i=i+1;
  endfunction;
  initial begin
    f();
    f();
  end;
endmodule
