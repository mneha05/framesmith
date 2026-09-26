#version 450
layout(location=0) out vec3 color;
vec2 p[3]=vec2[](vec2(0,-.65),vec2(.65,.65),vec2(-.65,.65));
vec3 c[3]=vec3[](vec3(.15,.27,.85),vec3(.23,.85,.65),vec3(.91,.44,.64));
void main(){gl_Position=vec4(p[gl_VertexIndex],0,1);color=c[gl_VertexIndex];}
