package external_package_2;
    parameter param = 6;
endpackage
package external_package;
    parameter param = 5;
endpackage
package exporting;
    import external_package::*;
    export external_package::param;
endpackage
module t import exporting::*;
#(
    parameter int foo = 10,
    parameter int bar = external_package_2::param,
    parameter int unused_param = 10,
    parameter [param-1:0] packed_param = param'(foo),
    parameter int clog_param = $clog2(param+2)
);
    int a[param];
    bit [bar:0] b;
    bit [foo-1:0] c;
    logic [foo-1:0] packed_a;
    logic [1+external_package::param:0] packed_b;
    initial begin
        $display("%0d,%0d,%0d,%0d,%0d,%0d,%5b,%5b,%0d", $size(a), $size(b), $size(c), foo, $size(packed_a), $size(packed_b), packed_param, param'('b111111), clog_param);
        $finish;
    end
endmodule : t
