// SPDX-License-Identifier: Apache-2.0
// nonsense code that should error out on access to undefined struct member

module id0 (
        input id1,
        input id2,
        input id3,
        output id4,
        output id5);
    assign id4 = id1^id2^id3;
    assign id5 = (id1&id2) | (id2&id3) | (id1&id3);
endmodule

module id6 (
        input id1,
        input id2,
        input id3,
        output id4,
        output id5);
    assign id4 = id1^id2^id3;
    assign id5 = (id1&id2) | (id2&id3) | (id1&id3);
endmodule

module id7 (
        input id1,
        input id2,
        input id3,
        output id4,
        output id5);
    assign id4 = id1^id2^id3;
    assign id5 = (id1&id2) | (id2&id3) | (id1&id3);
endmodule

typedef struct {
        byte id1;
        int id2;
} id8;

`verilator_config
id9 -id10 id11 -id12 "*/rtl/prim_onehot_check.sv" -id13 "Signal is not used: 'clk_i'"
`begin_keywords "1800-2012"
module id14 #(id15=32) (
        input  [id15-1:0] id1,
        input  [id15-1:0] id2,
        input  id3,
        output [id15-1:0] id4,
        output id5);

    wire [id15:0] id16;
    wire [id15:0] id17;
    wire [id15:0] id18;
    wire [32:0] id19 = id16 + id17 + id18 + id1 + id2;
    wire [id15:0] id20 = 4;

    generate for (genvar id21 = 0; id21 < id15; id21++)
        id0 id22(id1[id21], id2[id21], id18[id21], id4[id21], id18[id21+1]);
    endgenerate

    id8 id23 = '{5,id19};
    assign id23.id1 = 25;
    assign id23.id18 = 0;
    
    assign id18[0] = id3;
    assign id5 = id18[id15];

    bind id0 id6 id24 (.id1(id1), .id2(id2), .id3(id3), .id4(id4), .id5(id5));

endmodule

`verilator_config
id9 -id10 id11 -id12 "*/rtl/prim_onehot_check.sv" -id13 "Signal is not used: 'clk_i'"
