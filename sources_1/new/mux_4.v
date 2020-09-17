// 4-input multiplexer
module mux_4(
input wire [1:0] a,         // input a
input wire [1:0] b,         // input b
input wire [1:0] c,         // input c
input wire [1:0] d,         // input d
input wire [1:0] sel,       // selector

output reg [1:0] out        // output
);

always@(a or b or c or d or sel)
begin
    case(sel)
        2'b00 : out = a;
        2'b01 : out = b;
        2'b10 : out = c;
        2'b11 : out = d;
        //default : out = 2'b00;
        //full case가 아닐 경우 default 필요
     endcase
end

endmodule