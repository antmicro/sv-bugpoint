

typedef struct {
        int b;
} struct_foo;

`begin_keywords "1800-2012"
module serial_adder #() ();

    generate
    endgenerate

    struct_foo foo = '{5,6};
    assign foo.c = 0;

endmodule

