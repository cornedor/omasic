#version 440

// Draws the color field as a terminal would: a grid of characters, each
// picked by how bright the field is under it. With glyphMix at 0 the field
// shows through as plain light instead.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    vec2 cells;
    float glyphCount;
    float glyphMix;
    float high;
    float time;
    vec4 backdrop;
};

layout(binding = 1) uniform sampler2D field;
layout(binding = 2) uniform sampler2D atlas;

float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

void main() {
    vec2 uv = qt_TexCoord0;
    vec4 flowing = texture(field, uv);
    vec3 light = flowing.rgb * flowing.a * 1.15;

    vec2 cell = floor(uv * cells);
    vec2 inCell = fract(uv * cells);
    vec4 under = texture(field, (cell + 0.5) / cells);
    float seed = hash(cell);

    // High notes set cells off "decrypting": for a moment a cell races
    // through random characters, lit up, before it settles again.
    float spell = floor(time * 0.8 + seed * 9.0);
    // Only where there is light to play with: the dark stays dark.
    float chance = (0.01 + 0.12 * high) * smoothstep(0.04, 0.3, under.a);
    float scrambling = step(1.0 - chance, hash(cell + spell * 1.37));
    float scrambled = floor(hash(cell + floor(time * 14.0) * 0.61) * (glyphCount - 1.0)) + 1.0;
    float settled = floor(clamp(pow(under.a, 1.25) + (seed - 0.5) * 0.08, 0.0, 0.999) * glyphCount);
    float index = mix(settled, scrambled, scrambling);

    // The atlas is one row of characters, dimmest first.
    float ink = texture(atlas, vec2((index + inCell.x) / glyphCount, inCell.y)).a;
    vec3 character = under.rgb * (0.3 + 1.1 * under.a);
    character = mix(character, mix(under.rgb, vec3(1.0), 0.45) * (0.6 + 0.6 * under.a), scrambling);
    vec3 glyphs = character * ink + under.rgb * under.a * 0.2;

    vec3 color = backdrop.rgb + mix(light, glyphs, glyphMix);
    fragColor = vec4(color, 1.0) * qt_Opacity;
}
