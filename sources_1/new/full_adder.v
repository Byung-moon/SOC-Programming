//Full adder
module full_adder(
input wire x,       // input x
input wire y,       // input y
input wire cin,     // input carry in

output wire s,      // result of x + y
output wire cout    // generated carry
);

wire Ha1_cout;      //wire of A0 half_adder's carry
wire Ha1_s;         // wire of A0 half_adder's sum
wire Ha2_cout;      // wire of A1 half_adder's carry

half_adder HA1( .x(x),      .y(y),      .s(Ha1_s), .cout(Ha1_cout));
half_adder HA2( .x(cin),    .y(Ha1_s),  .s(s),     .cout(Ha2_cout));

assign cout = Ha1_cout | Ha2_cout;

endmodule 