
#extension GL_GOOGLE_include_directive : require

#include "../../shader_shared.h"
#include "shadow.glsl"

struct DirLight {
    vec4 direction;
    vec4 color;
    mat4 transform;
};

struct PointLight {
    vec4 pos;
    vec4 color;

    // where x is constant, y is linear and z is qudratic
    vec4 attenuation;
    mat4 transform;
};

struct SpotLight {  
    vec4 pos;
    vec4 direction;
    vec4 color;

    // where x is cutt_off and y is outer_cutt_off
    vec4 cut_offs;
    // where x is constant, y is linear and z is qudratic
    vec4 attenuation;
    mat4 transform;
};

const float ambient_coeff = 0.03;

vec3 calc_dir_light(DirLight light, vec3 normal, vec3 frag_pos, vec3 view_dir, vec3 surface_color, sampler2D shadow_map) {
    vec4 light_space_pos = light.transform * vec4(frag_pos, 1.0);
    vec3 light_dir = normalize(-light.direction.xyz);

    float diff = max(dot(normal, light_dir), 0.0);

    vec3 reflect_dir = reflect(-light_dir, normal);
    float spec = pow(max(dot(view_dir, reflect_dir), 0.0), 32.0);

    vec3 ambient = ambient_coeff * light.color.xyz * surface_color;
    vec3 diffiuse = light.color.xyz * diff * surface_color;
    vec3 specular = vec3(1.0) * spec * surface_color;

    float shadow = calc_shadow(shadow_map, light_space_pos, normal, light_dir);

    return (ambient + (1.0 - shadow) * (diffiuse + specular));
}

vec3 calc_point_light(PointLight light, vec3 normal, vec3 frag_pos, vec3 view_dir, vec3 surface_color) {
    vec3 light_dir = normalize(light.pos.xyz - frag_pos);
    float diff = max(dot(normal, light_dir), 0.0);

    vec3 halfway_dir = normalize(light_dir + view_dir);
    float spec = pow(max(dot(normal, halfway_dir), 0.0), 32.0);

    float constant = light.attenuation.x;
    float linear = light.attenuation.y;
    float quadratic = light.attenuation.z;
    
    float distance = length(light.pos.xyz - frag_pos);
    float attenuation = 1.0 / (constant + linear * distance + quadratic * (distance * distance));

    vec3 ambient = ambient_coeff * light.color.xyz * surface_color * attenuation;
    vec3 diffiuse = light.color.xyz * diff * surface_color * attenuation;
    vec3 specular = light.color.xyz * spec * surface_color * attenuation;

    return (ambient + diffiuse + specular);
}

vec3 calc_spot_light(SpotLight light, vec3 normal, vec3 frag_pos, vec3 view_dir, vec3 surface_color) {
    vec3 light_dir = normalize(light.pos.xyz - frag_pos);
    float diff = max(dot(normal, light_dir), 0.0);

    vec3 halfway_dir = normalize(light_dir + view_dir);
    float spec = pow(max(dot(normal, halfway_dir), 0.0), 32.0);

    float constant = light.attenuation.x;
    float linear = light.attenuation.y;
    float quadratic = light.attenuation.z;
    
    float distance = length(light.pos.xyz - frag_pos);
    float attenuation = 1.0 / (constant + linear * distance + quadratic * (distance * distance));

    vec3 ambient = ambient_coeff * light.color.xyz * surface_color * attenuation;

    float theta = dot(light_dir, normalize(-light.direction.xyz));
    float epsilon = light.cut_offs.x - light.cut_offs.y;
    float intensity = clamp((theta - light.cut_offs.y) / epsilon, 0.0, 1.0);
        
    vec3 diffiuse = light.color.xyz * diff * surface_color * attenuation * intensity;
    vec3 specular = light.color.xyz * spec * surface_color * attenuation * intensity;

    return (ambient + diffiuse + specular);
}

