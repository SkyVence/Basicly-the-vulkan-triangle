static const float3 positions[3] = { float3(0.0, -0.5, 0.0), float3(0.5, 0.5, 0.0), float3(-0.5, 0.5, 0.0) };

static const float3 colors[3] = { float3(1.0, 0.0, 0.0), float3(0.0, 1.0, 0.0), float3(0.0, 0.0, 1.0) };

struct Data {
	float rot;
	int	  backTextureIndex;
};

[[vk::push_constant]]
Data pc;

[[vk::binding(0)]]
Texture2D<float4> textures[1];

[[vk::binding(1)]]
SamplerState texSampler;

struct VertexOutput {
	float3 color;
	float2 uv;

	float4 sv_position : SV_Position;
};

float3 rotateY(float3 pos, float angle) {
	float c = cos(angle);
	float s = sin(angle);
	return float3(pos.x * c + pos.z * s, pos.y, -pos.x * s + pos.z * c);
}

[shader("vertex")]
VertexOutput vertMain(uint vid: SV_VertexID) {
	VertexOutput output;
	float3		 pos3D = positions[vid];

	if (vid == 0)
		output.uv = float2(0.5, 0.0);
	else if (vid == 1)
		output.uv = float2(1.0, 1.0);
	else
		output.uv = float2(0.0, 1.0);

	output.color	   = colors[vid];
	pos3D			   = rotateY(pos3D, pc.rot);
	output.sv_position = float4(pos3D.x, pos3D.y, pos3D.z + 0.5, 1.0);

	return output;
}

[shader("fragment")]
float4 fragMain(VertexOutput input, bool isFrontFace: SV_IsFrontFace) : SV_Target {
	if (isFrontFace) {
		return float4(input.color, 1.0);
	} else {
		int	   mode = abs(pc.backTextureIndex) % 4;
		float2 finalUV;

		float u = input.uv.x;
		float v = input.uv.y;

		float wave_freq = 10.0;
		float wave_amp	= 0.2;

		float bulge_force = 5.0;

		float f_disto_1 = -1.5;

		float f_disto_3 = -3.2;

		switch (mode) {
		case 0: {
			float offset = sin(v * wave_freq) * wave_amp;
			finalUV.x	 = u + offset;
			finalUV.y	 = v;
		} break;

		case 1: {
			float compression = pow(v, f_disto_1);
			finalUV.x		  = (u - 0.5) * compression + 0.5;
			finalUV.y		  = v;
		} break;

		case 2: {
			float bulge = sin(v * 3.14159) * bulge_force;
			finalUV.x	= (u - 0.5) * bulge + 0.5;
			finalUV.y	= v;
		} break;

		case 3: {
			float compression = pow(v, f_disto_3);
			finalUV.x		  = (u - 0.5) * compression + 0.5;
			finalUV.y		  = v;
		} break;
		}

		float4 texColor = textures[0].Sample(texSampler, finalUV);
		return float4(texColor.rgb, 1.0);
	}
}
