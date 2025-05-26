// SPDX-License-Identifier: Apache-2.0
// nonsense code that should error out on access to undefined struct member

module full_adder (
        input a,
        input b,
        input cin,
        output s,
        output cout);
    assign s = a^b^cin;
    assign cout = (a&b) | (b&cin) | (a&cin);
endmodule

module full_adder2 (
        input a,
        input b,
        input cin,
        output s,
        output cout);
    assign s = a^b^cin;
    assign cout = (a&b) | (b&cin) | (a&cin);
endmodule

module full_adder3 (
        input a,
        input b,
        input cin,
        output s,
        output cout);
    assign s = a^b^cin;
    assign cout = (a&b) | (b&cin) | (a&cin);
endmodule

typedef struct {
        byte a;
        int b;
} struct_foo;

`verilator_config
lint_off -rule UNUSED -file "*/rtl/prim_onehot_check.sv" -match "Signal is not used: 'clk_i'"
`begin_keywords "1800-2012"
module serial_adder #(WIDTH=32) (
        input  [WIDTH-1:0] a,
        input  [WIDTH-1:0] b,
        input  cin,
        output [WIDTH-1:0] s,
        output cout);

    wire [WIDTH:0] k;
    wire [WIDTH:0] l;
    wire [WIDTH:0] c;
    wire [WIDTH:0] m;
    wire [WIDTH:0] n = 4;

    generate for (genvar i = 0; i < WIDTH; i++)
        full_adder fa(a[i], b[i], c[i], s[i], c[i+1]);
    endgenerate

    struct_foo foo = '{5,6};
    assign foo.a = 25;
    assign foo.c = 0;
    
    assign c[0] = cin;
    assign cout = c[WIDTH];

    bind full_adder full_adder2 bound_adder (.a(a), .b(b), .cin(cin), .s(s), .cout(cout));

endmodule

`verilator_config
lint_off -rule UNUSED -file "*/rtl/prim_onehot_check.sv" -match "Signal is not used: 'clk_i'"
