// nonsense code that should error out on access to undefined struct member

module full_adder (
        input a,
        input b,
        input cin,
        output s,
        output cout);
    assign s = a^b^cin;
endmodule

typedef struct {
        int b;
} struct_foo;

module serial_adder #() (
        input  [WIDTH-1:0] a,
        input  [WIDTH-1:0] b,
        input  cin,
        output [WIDTH-1:0] s,
        output cout);

    wire [WIDTH:0] k;
    wire [WIDTH:0];
    wire [WIDTH:0];
    wire [WIDTH:0];
    wire [WIDTH:0] n = 4;

    generate
    endgenerate


    struct_foo foo = '{5,6};

    wire [3:0] x = 4'hF;
    wire [3:0];

endmodule

