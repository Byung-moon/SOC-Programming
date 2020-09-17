// 3 to 8  decoder
module decoder(
input wire enable,  // input enable
input wire a,       // input a
input wire b,       // input b
input wire c,       // input c

output reg [7:0] out //out put
);

always@(a or b or c or enable)
begin
	if(enable) begin
	   case({a, b, c})
	       3'b000 : out = 8'b0000_0001;
	       3'b001 : out = 8'b0000_0010;
	       3'b010 : out = 8'b0000_0100;
	       3'b011 : out = 8'b0000_1000;
	       3'b100 : out = 8'b0001_0000;
	       3'b101 : out = 8'b0010_0000;
	       3'b110 : out = 8'b0100_0000;
	       3'b111 : out = 8'b1000_0000;
	       //default : out = 8'b0000_0000 // full case가 아닐 경우 default필요
	    endcase
	end
	else begin
	   out = 8'b0000_0000;
	end
end

endmodule