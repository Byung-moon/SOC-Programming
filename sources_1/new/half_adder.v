//`timescale 1ns / 1ps
//Half adder
module half_adder(
input wire x,       // input x
input wire y,       // input y

output wire s,      // sum
output wire cout    // carry out
);

assign s    = x ^ y;    // sum == x xor y
assign cout = x & y;    // carry = x and y

endmodule
