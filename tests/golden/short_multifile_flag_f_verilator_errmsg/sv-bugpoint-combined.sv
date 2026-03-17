typedef struct {
        int b;
} struct_foo;
module serial_adder #() ();
    wire [32:0] m;
    struct_foo foo = '{5,m};
    assign foo.c = 0;
endmodule
