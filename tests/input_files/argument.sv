module t;
  int i=0;
  function void f(int arg);
    if(i==1) $finish;
    i=i+1;
  endfunction;
  initial begin;
    f(1);
    f(2);
  end;
endmodule
