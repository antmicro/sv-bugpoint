module t;
  int i=0;
  function void f(int a1, int a2, int a3, int a4, int a5, int a6, int a7);
    if(i==1) $finish;
    i=i+1;
  endfunction;
  initial begin;
    f(1, 2, 3, 4, 5, 6, 7);
    f(2, 3, 4, 5, 6, 7, 8);
  end;
endmodule
