

typedef struct {
        int b;
} struct_foo;

module serial_adder #() ();

    generate
    endgenerate

    struct_foo foo = '{5,6};
    assign foo.c = 0;

endmodule
