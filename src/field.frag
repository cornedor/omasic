#version 440

// The visualizer's color field: the artwork's colors as slowly marbling
// ink. It is drawn small (a few texels per glyph cell) and scaled up, so
// the noise is paid for once per cell rather than once per pixel.
// Output: the color in rgb, how bright it is in alpha.

layout(location = 0) in vec2 qt_TexCoord0;
layout(location = 0) out vec4 fragColor;

layout(std140, binding = 0) uniform buf {
    mat4 qt_Matrix;
    float qt_Opacity;
    float flow;
    float drift;
    float aspect;
    float bass;
    float mid;
    float beatAge;
    vec4 first;
    vec4 second;
    vec4 third;
};

float hash(vec2 p) {
    p = fract(p * vec2(123.34, 456.21));
    p += dot(p, p + 45.32);
    return fract(p.x * p.y);
}

// Noise that changes where it stands instead of sliding past: every point
// of the lattice swells and fades at its own pace, so shapes grow, merge
// and dissolve the way ink does, with no direction to follow.
float pulse(vec2 corner, float time) {
    float seed = hash(corner);
    return 0.5 + 0.5 * sin(6.2832 * seed + time * (0.35 + 0.65 * hash(corner + 17.3)));
}

float noise(vec2 p, float time) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    vec2 u = f * f * f * (f * (f * 6.0 - 15.0) + 10.0);
    return mix(mix(pulse(i, time), pulse(i + vec2(1.0, 0.0), time), u.x),
               mix(pulse(i + vec2(0.0, 1.0), time), pulse(i + vec2(1.0, 1.0), time), u.x), u.y);
}

const mat2 turn = mat2(0.8, 0.6, -0.6, 0.8);

float fbm(vec2 p, float time) {
    float value = 0.0;
    float amplitude = 0.5;
    for (int i = 0; i < 4; i++) {
        value += amplitude * noise(p, time);
        p = turn * p * 2.03 + 3.1;
        // Fine detail lives a little faster than the big shapes.
        time = time * 1.3 + 1.7;
        amplitude *= 0.5;
    }
    return value;
}

void main() {
    vec2 centered = vec2((qt_TexCoord0.x - 0.5) * aspect, qt_TexCoord0.y - 0.5);
    float reach = length(centered);

    // The whole picture slowly winds and unwinds around the middle, more
    // towards the edges, like something stirred.
    float wind = 0.5 * sin(drift * 0.05) + (0.35 + 0.25 * bass) * sin(drift * 0.083 + reach * 2.2);
    vec2 p = mat2(cos(wind), sin(wind), -sin(wind), cos(wind)) * centered * 2.2;

    // Each layer bends the one after it; the bass bends them further, so
    // loud passages swell rather than speed past.
    float warp = 1.6 + 1.6 * bass;
    vec2 q = vec2(fbm(p, flow * 0.35), fbm(p + vec2(5.2, 1.3), flow * 0.35 + 40.0));
    vec2 r = vec2(fbm(p + warp * (q - 0.5) + vec2(1.7, 9.2), flow * 0.26 + 80.0),
                  fbm(p + warp * (q - 0.5) + vec2(8.3, 2.8), flow * 0.26 + 120.0));
    float f = fbm(p + (2.4 + mid) * (r - 0.5), flow * 0.42 + 160.0);

    vec3 color = mix(first.rgb, second.rgb, smoothstep(0.36, 0.6, q.x + 0.6 * (r.x - 0.5)));
    color = mix(color, third.rgb, 0.9 * smoothstep(0.42, 0.62, r.y));

    // Bright ridges where the flow folds over itself, dark water between.
    float light = smoothstep(0.22, 1.0, f * f * 1.5 + 0.55 * dot(r - 0.5, r - 0.5) * 4.0 * f);
    light *= 0.62 + 0.4 * bass + 0.2 * mid;

    // Every beat sends one ring out from the middle.
    float ring = (reach - beatAge * 0.9) * 9.0;
    light += 0.35 * exp(-ring * ring) * exp(-beatAge * 2.2);

    fragColor = vec4(color, clamp(light, 0.0, 1.0));
}
