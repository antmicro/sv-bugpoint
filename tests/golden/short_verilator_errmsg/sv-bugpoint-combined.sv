

typedef struct {
        int b;
} struct_foo;

module serial_adder #() ();
    wire [32:0] m;

    generate
    endgenerate

    struct_foo foo = '{5,m};
    assign foo.c = 0;

endmodule

