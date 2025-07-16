// SPDX-License-Identifier: Apache-2.0
// nonsense code that should error out on access to undefined struct member

module full_adder2 (
        input a,
        input b,
        input cin,
        output s,
        output cout);
    assign s = a^b^cin;
    assign cout = (a&b) | (b&cin) | (a&cin);
endmodule
