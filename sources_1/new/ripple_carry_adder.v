// 4-bit ripple -carry adder
module ripple_carry_adder(
input wire [3:0] x,    // 4-bit input x
input wire [3:0] y,    // 4-bit input y
input wire       sub,  // (sub == 1) ? subtraction : addition

output wire [3:0] sum, // result
output wire       cout // carry out
);

wire    y_0, y_1, y_2, y_3;
wire    c1, c2, c3;

//mux for avarible subtraction
mux MUX0(   .a(y[0]),   .b(!y[0]),      .sel(sub),      .out(y_0) ); // MUX0
mux MUX1(   .a(y[1]),   .b(!y[1]),      .sel(sub),      .out(y_1) ); // MUX1
mux MUX2(   .a(y[2]),   .b(!y[2]),      .sel(sub),      .out(y_2) ); // MUX2
mux MUX3(   .a(y[3]),   .b(!y[3]),      .sel(sub),      .out(y_3) ); // MUX3

//full adder for addition
full_adder FA0( .x(x[0]),    .y(y_0),    .cin(sub),  .s(sum[0]), .cout(c1)); //FA0
full_adder FA1( .x(x[1]),    .y(y_1),    .cin(c1),  .s(sum[1]), .cout(c2)); //FA1
full_adder FA2( .x(x[2]),    .y(y_2),    .cin(c2),  .s(sum[2]), .cout(c3)); //FA2
full_adder FA3( .x(x[3]),    .y(y_3),    .cin(c3),  .s(sum[3]), .cout(cout)); //FA3

endmodule