// SPDX-License-Identifier: Apache-2.0
// nonsense code that should error out on access to undefined struct member

typedef struct {
        byte id1;
        int id2;
} id8;

module id9 #(id10=32) (
        input  [id10-1:0] id1,
        input  [id10-1:0] id2,
        input  id3,
        output [id10-1:0] id4,
        output id5);

    wire [id10:0] id11;
    wire [id10:0] id12;
    wire [id10:0] id13;
    wire [32:0] id14 = id11 + id12 + id13 + id1 + id2;
    wire [id10:0] id15 = 4;

    generate for (genvar id16 = 0; id16 < id10; id16++)
        id0 id17(id1[id16], id2[id16], id13[id16], id4[id16], id13[id16+1]);
    endgenerate

    id8 id18 = '{5,id14};
    assign id18.id1 = 25;
    assign id18.id13 = 0;
    
    assign id13[0] = id3;
    assign id5 = id13[id10];

    bind id0 id6 id19 (.id1(id1), .id2(id2), .id3(id3), .id4(id4), .id5(id5));

endmodule

