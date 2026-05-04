#version 450

layout(location = 0) out vec4 fragColor;

struct PointLight {
  vec4 position;
  vec4 color;
};

layout(set = 0, binding = 0) uniform GlobalUbo {
  mat4 projection;
  mat4 view;
  mat4 invView;
  vec4 ambientLightColor;
  PointLight pointLights[10];
  int numLights;
} ubo;

layout(push_constant) uniform Push {
  vec4 config; // radius, spacing, major step, y
  vec4 minorColor;
  vec4 majorColor;
  vec4 axisXColor;
  vec4 axisZColor;
} push;

void main() {
  int radius = int(push.config.x);
  float spacing = push.config.y;
  int majorStep = max(1, int(push.config.z));
  float y = push.config.w;
  int linesPerAxis = radius * 2 + 1;
  int lineIndex = gl_VertexIndex / 2;
  int endpoint = gl_VertexIndex & 1;
  bool zLine = lineIndex < linesPerAxis;
  int localIndex = zLine ? lineIndex : lineIndex - linesPerAxis;
  int coordIndex = localIndex - radius;
  float coord = float(coordIndex) * spacing;
  float extent = float(radius) * spacing;

  vec3 position;
  if (zLine) {
    position = vec3(endpoint == 0 ? -extent : extent, y, coord);
  } else {
    position = vec3(coord, y, endpoint == 0 ? -extent : extent);
  }

  if (coordIndex == 0) {
    fragColor = zLine ? push.axisXColor : push.axisZColor;
  } else if ((abs(coordIndex) % majorStep) == 0) {
    fragColor = push.majorColor;
  } else {
    fragColor = push.minorColor;
  }

  gl_Position = ubo.projection * ubo.view * vec4(position, 1.0);
}
