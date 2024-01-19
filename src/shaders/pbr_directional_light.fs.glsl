#version 330

in vec3 vViewSpacePosition;
in vec3 vViewSpaceNormal;
in vec2 vTexCoords;

uniform vec3 uLightDirection;
uniform vec3 uLightIntensity;

uniform vec4 uBaseColorFactor;
uniform sampler2D uBaseColorTexture;

// Metallic-rougness BRDF
uniform float uMetallicFactor;
uniform float uRougnessFactor;
uniform sampler2D uMetallicRoughnessTexture;

out vec3 fColor;

// Constants
const float GAMMA = 2.2;
const float INV_GAMMA = 1. / GAMMA;
const float M_PI = 3.141592653589793;
const float M_1_PI = 1.0 / M_PI;

// We need some simple tone mapping functions
// Basic gamma = 2.2 implementation
// Stolen here:
// https://github.com/KhronosGroup/glTF-Sample-Viewer/blob/master/src/shaders/tonemapping.glsl

// linear to sRGB approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
vec3 LINEARtoSRGB(vec3 color) { return pow(color, vec3(INV_GAMMA)); }

// sRGB to linear approximation
// see http://chilliant.blogspot.com/2012/08/srgb-approximations-for-hlsl.html
vec4 SRGBtoLINEAR(vec4 srgbIn)
{
  return vec4(pow(srgbIn.xyz, vec3(GAMMA)), srgbIn.w);
}

void main()
{
  vec3 N = normalize(vViewSpaceNormal);
  vec3 L = uLightDirection;
  vec4 baseColorFromTexture =
      SRGBtoLINEAR(texture(uBaseColorTexture, vTexCoords));
  float NdotL = clamp(dot(N, L), 0., 1.);
  vec4 baseColor = uBaseColorFactor * baseColorFromTexture;


  vec3 diffuse = baseColor.rgb * M_1_PI;

  fColor = LINEARtoSRGB(diffuse * uLightIntensity * NdotL);

  // METALLIC addon
  // See here : https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#implementation


  vec3 V = normalize(-vViewSpacePosition);
  vec3 H = normalize(L + V);
  float VdotH = clamp(dot(V,H), 0.,1.);
  float VdotN = clamp(dot(V,N), 0.,1.);
  float LdotN = clamp(dot(L,N), 0.,1.);
  float NdotH = clamp(dot(N,H), 0.,1.);
  float NdotV = clamp(dot(N,V), 0.,1.);
  const float black = 0.;

  // You need to compute baseShlickFactor first
  float baseShlickFactor = (1 - abs(VdotH));
  float shlickFactor = baseShlickFactor * baseShlickFactor; // power 2
  shlickFactor *= shlickFactor; // power 4
  shlickFactor *= baseShlickFactor; // power 5 // == (1 - V*H)^5
  

  vec4 MetallicRoughnessFromTexture =
      (texture(uMetallicRoughnessTexture, vTexCoords));
  vec3 metallic  = vec3(MetallicRoughnessFromTexture.b * uMetallicFactor);
  float roughness = MetallicRoughnessFromTexture.g * uRougnessFactor;

  vec3 c_diff = mix(baseColor.rgb, vec3(black), metallic);   // mix(vec3 , vec3 float)
  vec3 f0 = mix(vec3(0.04), baseColor.rgb, metallic);
  float alpha = roughness * roughness;

  vec3 F = f0 + (1 - f0) * shlickFactor ;


  // D : Trowbridge-Reitz/GGX microfacet distribution
  float DenomD = (NdotH * NdotH * (alpha*alpha - 1.) + 1.);
  float NumD = alpha*alpha;
  float D = NumD/(M_PI*DenomD*DenomD);

  // G : separable form of the Smith joint masking-shadowing function
  float DenomG_1 = NdotL+sqrt(alpha*alpha+(1-alpha*alpha)*NdotL*NdotL);
  float DenomG_2 = NdotV+sqrt(alpha*alpha+(1-alpha*alpha)*NdotV*NdotV);
  float NumG_1 = 2 * NdotL;
  float NumG_2 = 2 * NdotV;

  float G = (NumG_1/DenomG_1) *(NumG_2/DenomG_2) ;

  // Vis
  float Vis = G / (4 * abs(VdotN) * abs(LdotN));

  vec3 f_diffuse = (1 - F) * (1 / M_PI) * c_diff;
  //vec3 f_specular = F * D * G / (4 * abs(VdotN) * abs(LdotN)); // formula from doc
  vec3 f_specular = F * Vis * D; // fourmula from lesson not doc

  vec3 material = f_diffuse + f_specular;

  fColor = LINEARtoSRGB(material * uLightIntensity * NdotL);

}