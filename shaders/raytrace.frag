#version 460 core

#define M_PI 3.141592

layout(location=0) out vec4 FRAG_COLOR;

layout(location=0) in struct VERT_OUT {
    vec3 pos;
} vert;

flat in mat4 invP;
flat in mat4 invV;

uniform vec3 camera_pos;

struct SceneModel {
    vec4 pos;
    uint type; // 0 - Y-oriented plane, 1 - sphere 
};

layout(std140, binding=0) uniform SpherePositions {
    SceneModel models[16];
};
uniform uint model_count;

struct Material {
    vec4 albedo_shininess;
    float metalness;
};

layout(std140, binding=1) uniform Materials {
    Material materials[16];
};

layout(std140, binding=2) uniform Settings {
    uint max_iterations;
    uint diffuse_samples;
    float prec;
    float max_dist;
};

struct HitInfo {
    uint idx;
    vec3 normal;
    float t;
    vec3 hitpos;
    bool hit;
};

float scene_sdf(vec3 p, inout int idx) {
    float dist = max_dist;
    int best_idx = 0;
    for(idx=0; idx<model_count; ++idx) {
        float d; 
        if(models[idx].type == 0) {
            d = abs(p.y);
        } else if(models[idx].type == 1) {
            d = distance(models[idx].pos.xyz, p) - models[idx].pos.w;
        }
        if(d < dist) {
            dist = d;
            best_idx = idx;
        }
    } 
    idx = best_idx;
    return dist;
}

HitInfo trace_scene(vec3 o, vec3 d) {
    HitInfo info;
    info.idx = 0;
    info.normal = vec3(0.0, 0.0, 0.0);
    info.t = 0.0;
    info.hitpos = vec3(0.0);
    info.hit = false;

    float t = 0.0;
    int idx = 0;
    for(int i=0; i<max_iterations; ++i) {
        vec3 p = o + d*t;

        float sdf = scene_sdf(p, idx);
        if(sdf <= prec) { info.hit = true; break; }

        t += sdf;
        if(t >= max_dist) { break; }
    }

    info.idx = idx;
    info.t = t;
    info.hitpos = o + d*t;
    if(info.hit) {
        SceneModel model = models[idx];
        if(model.type == 0) { info.normal = vec3(0, 1, 0); }
        else if(model.type == 1) {
            info.normal = normalize(info.hitpos - model.pos.xyz);
        }
    }

    return info;
}

vec2 hash2(inout float seed) {
    return fract(sin(vec2(seed+=0.1,seed+=0.1))*vec2(43758.5453123,22578.1459123));
}   

vec3 importanceSamplePhong(vec3 normal, vec3 viewDir, float shininess, float rand1, float rand2) {
    // Generate a random direction within the cosine lobe
    float phi = 2.0 * M_PI * rand1;
    float cosTheta = pow(rand2, 1.0 / (shininess + 1.0));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);
    vec3 sampleDir = vec3(sinTheta * cos(phi), sinTheta * sin(phi), cosTheta);

    // Transform the sample direction to the world space
    vec3 tangent = normalize(cross(normal, viewDir));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);
    return TBN * sampleDir;
}

vec3 importanceSamplePhong2(vec3 normal, vec3 viewDir, float shininess, float rand1, float rand2) {
    const float eps = 1e-10;
    const float cos_max = pow(eps, 1.0 / (2.0 + shininess));
    const float cosTheta = (1.0 - cos_max) * rand2 + cos_max;
    const float sinTheta = pow(1.0 - cosTheta*cosTheta, 0.5);
    const float phi = 2.0 * M_PI * rand1;

    const vec3 dir = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

    // Transform the sample direction to the world space
    const vec3 tangent = normalize(cross(normal, viewDir));
    const vec3 bitangent = cross(normal, tangent);
    const mat3 TBN = mat3(tangent, bitangent, normal);
    return TBN * dir;
}

const vec3 light = vec3(0.0, 5.0, 0.0);
const float light_intensity = 30.0;

vec3 phong_brdf(vec3 d, HitInfo info) {
    if(!info.hit) { return vec3(0.0); }

    HitInfo shadow_result = trace_scene(info.hitpos + info.normal*prec, normalize(light - info.hitpos));
    float shadow = 1.0;
    if(shadow_result.t < distance(light, info.hitpos)) {
        shadow = 0.01;
    }

    vec3 light_dir = -normalize(info.hitpos - light);
    float angle = max(dot(light_dir, info.normal), 0.0);

    vec3 color = materials[info.idx].albedo_shininess.rgb * angle;
    vec3 halfway = normalize(light_dir - d);
    color += pow(max(dot(info.normal, halfway), 0.0), materials[info.idx].albedo_shininess.a);
    color *= 1.0 / (pow(distance(info.hitpos, light), 2.0)) * light_intensity;
    return color * shadow;
}

void main() {
    vec4 p = vec4(vert.pos.xy, -1.0, 1.0);
    p = invP * p;
    float seed = p.x + p.y * 3.43121412313;
    p /= p.w;
    p.w = 0.0;
    p = invV * normalize(p);
    p = normalize(p);

    vec3 d1 = p.xyz;
    HitInfo result = trace_scene(camera_pos, d1);

    vec3 color = phong_brdf(d1, result);

    vec3 acc_indirect = vec3(0.0);
    for(int i=0; i<0; ++i) {
        vec2 hash = hash2(seed);

        float shininess = materials[result.idx].albedo_shininess.a;
        vec3 n = importanceSamplePhong2(result.normal, d1, shininess, hash.x, hash.y);
        const float eps = 1e-10;
        const float cos_max = pow(eps, 1.0 / (2.0 + shininess));
        HitInfo info = trace_scene(result.hitpos + result.normal * 0.05, n);

        vec3 light_dir = -normalize(info.hitpos - light);
        float angle = max(dot(light_dir, info.normal), 0.0);

        vec3 color = materials[info.idx].albedo_shininess.rgb;

        HitInfo shadow_result = trace_scene(info.hitpos + info.normal*prec, normalize(light - info.hitpos));
        float shadow = 1.0;
        float hit_light_dist = distance(light, info.hitpos);
        if(shadow_result.t < hit_light_dist) {
            shadow = 0.01;
        }

        const float cosTheta = (1.0 - cos_max) * hash.y + cos_max;
        vec3 halfway = normalize(light_dir - n);
        color *= 1.0 / (pow(hit_light_dist, 2.0)) * light_intensity * shadow;

        acc_indirect += color * (shininess + 2.0) * (1.0 - cos_max) * pow(cosTheta, shininess);
    }
    // color += acc_indirect / float(diffuse_samples);

    vec3 reflect_dir = normalize(reflect(d1, result.normal));
    HitInfo reflect_info = trace_scene(result.hitpos + result.normal*0.1, reflect_dir);

    if(reflect_info.hit) {
        color += phong_brdf(reflect_dir, reflect_info) * materials[result.idx].metalness;
    }

    FRAG_COLOR = vec4(color, 1.0);
}