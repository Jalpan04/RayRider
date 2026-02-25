#version 330

// Input vertex attributes (from vertex shader)
in vec3 fragPosition;
in vec2 fragTexCoord;
in vec4 fragColor;
in vec3 fragNormal;

// Output fragment color
out vec4 finalColor;

// Texture attributes
uniform sampler2D texture0;
uniform sampler2D shadowMap; // New: Shadow map texture
uniform vec4 colDiffuse;

// Lighting attributes
uniform vec3 lightDir;
uniform vec3 lightColor;
uniform vec3 ambientColor;
uniform mat4 matLight; // New: Light space matrix

// Fog Attributes
uniform float fogDensity = 0.03;
uniform vec3 fogColor = vec3(0.5, 0.5, 0.52);

// Post-processing attributes
uniform float colorDepth = 16.0; // Simulate 16-bit color banding

void main()
{
    // Affine texture mapping trick: Divide by W to undo perspective correction
    vec2 affineTexCoord = fragTexCoord / gl_FragCoord.w;

    // Fetch Texture
    vec4 texelColor = texture(texture0, affineTexCoord);
    vec3 baseColor = texelColor.rgb * colDiffuse.rgb * fragColor.rgb;

    // Very basic diffuse lighting appropriate for the era
    vec3 normal = normalize(fragNormal);
    float NdotL = max(dot(normal, lightDir), 0.0);
    vec3 diffuse = lightColor * NdotL;

    // --- Shadow Calculation ---
    vec4 fragPosLightSpace = matLight * vec4(fragPosition, 1.0);
    // Perform perspective divide
    vec3 projCoords = fragPosLightSpace.xyz / fragPosLightSpace.w;
    // Transform to [0,1] range
    projCoords = projCoords * 0.5 + 0.5;
    
    float shadow = 0.0;
    // Only calculate shadows if inside the light frustum
    if (projCoords.z <= 1.0) {
        float currentDepth = projCoords.z;
        // Basic PCF (Percentage-Closer Filtering) for slightly softer shadow edges
        float bias = max(0.005 * (1.0 - dot(normal, lightDir)), 0.0005);
        vec2 texelSize = 1.0 / textureSize(shadowMap, 0);
        
        for(int x = -1; x <= 1; ++x) {
            for(int y = -1; y <= 1; ++y) {
                float pcfDepth = texture(shadowMap, projCoords.xy + vec2(x, y) * texelSize).r; 
                shadow += currentDepth - bias > pcfDepth ? 1.0 : 0.0;        
            }    
        }
        shadow /= 9.0;
    }

    // Atmospheric Rim Lighting (Fake Global Illumination bounce)
    // Makes the concrete pop slightly against the void
    vec3 viewDir = normalize(-fragPosition); // Approximation, assumes camera near origin for this effect
    float rim = 1.0 - max(dot(viewDir, normal), 0.0);
    rim = smoothstep(0.6, 1.0, rim);
    vec3 rimLight = lightColor * rim * 0.3; // Tint rim with light color

    // Combine light, shadow, rim, and base color
    // If in shadow, only ambient applies.
    vec3 lighting = ambientColor + ((1.0 - shadow) * diffuse) + (rimLight * (1.0 - shadow*0.8));
    vec3 resultColor = baseColor * lighting;

    // --- Color Quantization (Banding) for PS2 aesthetic ---
    resultColor = floor(resultColor * colorDepth + 0.5) / colorDepth;

    // --- Distance Fog ---
    // Calculate depth based distance
    float dist = gl_FragCoord.z / gl_FragCoord.w;
    float fogFactor = exp(-pow(dist * fogDensity, 2.0));
    fogFactor = clamp(fogFactor, 0.0, 1.0);

    // Mix final color with fog
    resultColor = mix(fogColor, resultColor, fogFactor);

    finalColor = vec4(resultColor, texelColor.a * colDiffuse.a);
}
