#version 330

// Input vertex attributes
in vec3 vertexPosition;
in vec2 vertexTexCoord;
in vec3 vertexNormal;
in vec4 vertexColor;

// Input uniform values
uniform mat4 mvp;
uniform mat4 matModel;
uniform mat4 matNormal;

// Output vertex attributes (to fragment shader)
out vec3 fragPosition;
out vec2 fragTexCoord;
out vec4 fragColor;
out vec3 fragNormal;

// Custom uniforms for PS2 effect
uniform vec2 resolution; // e.g. 320x240
uniform float vertexJitterIntensity = 1.0; 
uniform vec2 uvScale = vec2(1.0, 1.0); // New: for tileable textures

void main()
{
    // Standard projection
    vec4 clipPos = mvp * vec4(vertexPosition, 1.0);
    
    // --- Vertex Snapping (Jitter/Wobble) ---
    // Convert to Normalized Device Coordinates (-1 to 1)
    vec3 ndc = clipPos.xyz / clipPos.w;
    
    // Convert to screen/pixel coordinates based on our desired low resolution
    vec2 screenPos = ndc.xy * resolution;
    
    // Snap to the nearest integer pixel
    screenPos = round(screenPos * vertexJitterIntensity) / vertexJitterIntensity;
    
    // Convert back to NDC
    ndc.xy = screenPos / resolution;
    
    // Convert back to clip space
    clipPos.xyz = ndc * clipPos.w;
    
    gl_Position = clipPos;

    // Send world position and normal for basic lighting
    fragPosition = vec3(matModel * vec4(vertexPosition, 1.0f));
    fragNormal = normalize(vec3(matNormal * vec4(vertexNormal, 1.0f)));
    
    // Affine texture mapping trick: Ignore perspective correction by multiplying by W
    // We will divide by W in the fragment shader. 
    fragTexCoord = vertexTexCoord * uvScale * clipPos.w; 
    
    fragColor = vertexColor;
}
