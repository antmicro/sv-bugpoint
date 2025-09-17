module t;
  function dpi_export();
    dpi_import("a");
  endfunction
  import "DPI-C" context function void dpi_import(string s);
  export "DPI-C" function dpi_export;
endmodule