#version 330

in vec3 vViewSpacePosition;
in vec3 vViewSpaceNormal;
in vec2 vTexCoords;

uniform vec3 uLightDirection;
uniform vec3 uLightIntensity;


out vec3 fColor;


const float pi = 3.14;


void main()
{
   // Need another normalization because interpolation of vertex attributes does not maintain unit length

    vec3 simple_brdf = vec3(1/pi,1/pi,1/pi);

   
   //vec3 viewSpaceNormal = normalize(vViewSpaceNormal);
   //fColor = viewSpaceNormal;
   fColor = simple_brdf* uLightIntensity * dot(vViewSpaceNormal, uLightDirection);
}