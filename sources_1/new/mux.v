// 2-input multiplexer
module mux(
input wire a,       // input a
input wire b,       // input b
input wire sel,     // input selector

output reg out      // output
);

always @(a or b or sel)
begin
    case(sel)
        1'b0 : out = a;
        1'b1 : out = b;
        //default " out = 1'b0;
        // full case가 아닐 경우 default 필요
     endcase
end

endmodule
