class c;
  static function int f();
endfunction
  static virtual function int g();
endfunction
endclass
function bit finish;
  $finish;
endfunction
module t;
  initial if(c::f()==0 && c::g()==0 && finish());
endmodule