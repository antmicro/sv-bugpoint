module if_body_replacer;
  initial begin
    $display("fallback else body survived");
    // body-specific comment
    $display("standalone if body survived");
     $display("else-if chain final body survived");
  end
endmodule
