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

