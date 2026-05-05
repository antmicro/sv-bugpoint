module if_body_replacer;
  initial begin
    if ($test$plusargs("prefer_then"))
      $display("then body only");
    else
      $display("fallback else body survived");

    if ($test$plusargs("no_else"))
      // body-specific comment
      $display("standalone if body survived");

    if ($test$plusargs("chain_first"))
      $display("else-if chain first body");
    else if ($test$plusargs("chain_second"))
      $display("else-if chain second body");
    else if ($test$plusargs("chain_third"))
      $display("else-if chain third body");
    else
      $display("else-if chain final body survived");
  end
endmodule
