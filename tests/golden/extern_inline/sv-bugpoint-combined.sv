class c;
  static function int f();
endfunction
  static virtual function int g(int a);
  return a + 1;
endfunction
endclass
module t;
  initial if(c::f()==0 && c::g(1)==2) $finish;
endmodule