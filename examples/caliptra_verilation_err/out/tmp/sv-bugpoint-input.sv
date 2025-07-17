
`begin_keywords "1800-2023"
module spiflash #(
) (
  input logic sck
);
  task automatic return_byte( int data, int io_mode);
    automatic int unsigned rails;
    repeat(8/rails) begin
      fork
        begin
          @(negedge sck);
        end
      join_any
    end
  endtask : return_byte
endmodule: spiflash
`begin_keywords "1800-2023"
`begin_keywords "1800-2023"
`begin_keywords "1800-2023"
`begin_keywords "1800-2023"
`begin_keywords "1800-2023"
`begin_keywords "1800-2023"
