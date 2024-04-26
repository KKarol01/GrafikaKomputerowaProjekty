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
    vec4 ambient;
    vec4 diffuse;
    vec4 specular;
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

const vec3 lights[] = { vec3(-3.0, 6.0, -1.0), vec3(3.0, 6.0, 1.0) };
const vec3 light_col = vec3(0xfd/255.0, 0xfb/255.0, 0xd3/255.0);
const float light_intensity = 50.0;

float schlick_fresnel(float n2, vec3 rd, vec3 n) {
    const float n1 = 1.0;
    const float f0 = pow((n2 - 1.0) / (n2 + 1.0), 2.0);
    return f0 + (1.0 - f0) * pow((1.0 - dot(rd, n)), 5.0);
}

vec3 phong_brdf(vec3 rd, HitInfo info) {
    if(!info.hit) { return vec3(0.0); }

    vec3 color = vec3(0.0);
    for(int i=0; i<2; ++i) {
        const vec3 light = lights[i];
        const vec3 ld = -normalize(info.hitpos - light);
        const vec3 h = normalize(ld - rd);
        const Material mat = materials[info.idx];

        const float diff = max(dot(info.normal, h), 0.0);

        const vec3 refld = reflect(ld, info.normal);
        const float spec = pow(max(dot(rd, refld), 0.0), mat.specular.w);

        const float att = 1.0 / pow(distance(info.hitpos, light), 2.0);

        const HitInfo shadow_info = trace_scene(info.hitpos + info.normal*0.1, ld);
        float shadow = 1.0;
        if(shadow_info.t < distance(info.hitpos + info.normal*0.1, light)) {
            shadow = 0.0;
        }

        color += light_col * light_intensity * (diff*mat.diffuse.rgb + spec*mat.specular.rgb) * att * shadow + mat.ambient.rgb;
    }
    
    return color * 0.5;
}

void main() {
    vec4 p = vec4(vert.pos.xy, -1.0, 1.0);
    p = invP * p;
    p /= p.w;
    p.w = 0.0;
    p = invV * normalize(p);
    p = normalize(p);

    vec3 d1 = p.xyz;
    HitInfo result = trace_scene(camera_pos, d1);

    vec3 color = phong_brdf(d1, result);

    const vec3 reflect_dir = normalize(reflect(d1, result.normal));
    HitInfo reflect_info = trace_scene(result.hitpos + result.normal*0.1, reflect_dir);

    if(reflect_info.hit) {
        float ior = materials[result.idx].ambient.w;
        float refl = materials[result.idx].diffuse.w;
        ior = schlick_fresnel(ior, reflect_dir, result.normal);
        ior = refl*ior;
        color += phong_brdf(reflect_dir, reflect_info) * ior;
    }

    FRAG_COLOR = vec4(color, 1.0);
}