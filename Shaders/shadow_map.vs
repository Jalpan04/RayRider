#version 330

// Input vertex attributes
in vec3 vertexPosition;

// Input uniform variables
uniform mat4 mvp;

void main()
{
    // Compute the clip-space position of the vertex
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
