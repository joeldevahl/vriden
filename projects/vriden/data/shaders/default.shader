{
	"shader_intermediate_t": {
		"properties" : [
			{
				"name" : "color",
				"frequency" : "PER_INSTANCE",
				"data" : {
					"floats" :  [0.0, 0.0, 0.0, 1.0]
				}
			}
		],
		"common_code" : "
			struct VP_INPUT
			{
				float3 position : POSITION;
			};

			struct VP_OUTPUT
			{
				float4 position : SV_Position;
			};
		",
		"vertex_program" : "
			uint draw_id : register(c0);

			StructuredBuffer<float4x4> instance_matrices : register(t1);

			cbuffer view_data : register(b2)
			{
				float4x4 view_mat;
				float4x4 proj_mat;
			};

			void main(in VP_INPUT input, out VP_OUTPUT output)
			{

				float4 obj_pos = float4(input.position, 1.0);
				float4 world_pos = mul(instance_matrices[draw_id], obj_pos);
				float4 eye_pos = mul(proj_mat, mul(view_mat, world_pos));
				output.position = eye_pos;
			}
		",
		"fragment_program" : "
			struct FP_OUTPUT
			{
				float4 color : SV_Target;
			};

			void main(in VP_OUTPUT input, out FP_OUTPUT output)
			{
				output.color = float4(1.0, 0.0, 1.0, 1.0); //color;
			}
		"
	}
}