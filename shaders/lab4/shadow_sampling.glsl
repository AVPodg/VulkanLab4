layout(set=2,binding=0,std140) uniform Shadows {
    mat4 sun;
    mat4 spot;
    ivec4 options;
} shadows;
layout(set=2,binding=1) uniform sampler2DShadow sunShadow;
layout(set=2,binding=2) uniform sampler2DShadow spotShadow;

float visibility(sampler2DShadow map, mat4 lightVP) {
    vec4 clip = lightVP * vec4(worldPosition,1.0);
    if (clip.w <= 0.0) return 1.0;
    vec3 projected = clip.xyz / clip.w;
    vec2 uv = projected.xy * 0.5 + 0.5;
    if (projected.z <= 0.0 || projected.z >= 1.0 || any(lessThan(uv,vec2(0))) || any(greaterThan(uv,vec2(1)))) return 1.0;
    if (shadows.options.z == 0) return texture(map,vec3(uv,projected.z));
    vec2 texel = 1.0 / vec2(textureSize(map,0));
    float sum = 0.0;
    for (int y=-1; y<=1; ++y)
        for (int x=-1; x<=1; ++x)
            sum += texture(map,vec3(uv+vec2(x,y)*texel,projected.z));
    return sum / 9.0;
}

float sunVisibility() {
    return shadows.options.x != 0 ? visibility(sunShadow,shadows.sun) : 1.0;
}
float spotVisibility(int index) {
    return index == shadows.options.y ? visibility(spotShadow,shadows.spot) : 1.0;
}
