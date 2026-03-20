class c;
  extern static function int f(int a);
  extern static virtual function int g(int a);
endclass

function int c::f(int a);
endfunction

function int c::g(int a);
  return a + 1;
endfunction

module t;
  initial if(c::f(1)==0 && c::g(1)==2) $finish;
endmodule