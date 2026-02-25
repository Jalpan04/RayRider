#version 330

// Input vertex attributes (from vertex shader)
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec3 fragNormal;
in vec4 fragColor;

// Input uniform values
uniform sampler2D texture0;
uniform vec4 colDiffuse;

// Lighting uniforms
uniform vec3 lightDir;       // direction TO the light (normalized)
uniform vec3 lightColor;     // light color
uniform vec3 ambientColor;   // ambient light
uniform vec3 viewPos;        // camera position
uniform float specPower;     // specular exponent

// Output fragment color
out vec4 finalColor;

void main()
{
    // Base color from texture * material tint
    vec4 texelColor = texture(texture0, fragTexCoord) * colDiffuse;

    // Normal (already interpolated + normalized from VS)
    vec3 N = normalize(fragNormal);

    // Diffuse (Lambert)
    float diff = max(dot(N, lightDir), 0.0);

    // Specular (Blinn-Phong)
    vec3 V = normalize(viewPos - fragPosition);
    vec3 H = normalize(lightDir + V);
    float spec = pow(max(dot(N, H), 0.0), specPower);

    // Combine
    vec3 ambient  = ambientColor * texelColor.rgb;
    vec3 diffuse  = lightColor * diff * texelColor.rgb;
    vec3 specular = lightColor * spec * 0.3; // subtle specular

    vec3 result = ambient + diffuse + specular;

    finalColor = vec4(result, texelColor.a);
}
