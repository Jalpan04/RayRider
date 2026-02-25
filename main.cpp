#include "raylib.h"
#include "raymath.h"
#include "rlgl.h"
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <vector>

#define GRAVITY 32.0f
#define MOUSE_SENSITIVITY 0.003f

// Media-Based Attribute System structures
enum class MediaType { LITERATURE, MUSIC, CINEMA, NONE };

struct CharacterStats {
  float empathy;    // Literature: unlocking dialogues
  float kinetic;    // Music: movement/combat speed
  float perception; // Cinema: revealing traps/paths
};

struct GameState {
  CharacterStats stats;
  MediaType activeRadioType; // For motorcycle radio buffs
};

enum class TextureType { BASE, PILLAR, HIGHWAY };

// Architecture Collision System
struct BrutalistBlock {
  BoundingBox bounds;
  Color color;
  TextureType texType;
};

std::vector<BrutalistBlock> levelBlocks;

// Colors for the Daylight environment
const Color COLOR_CONCRETE_LIGHT = {220, 220, 225, 255};
const Color COLOR_CONCRETE_MID = {190, 190, 195, 255};
const Color COLOR_CONCRETE_DARK = {160, 160, 165, 255};
const Color COLOR_SHADOW_VOID = {135, 206, 235, 255}; // Sky blue

void AddBlock(Vector3 pos, Vector3 size, Color color,
              TextureType texType = TextureType::BASE) {
  BoundingBox box;
  box.min = {pos.x - size.x / 2, pos.y, pos.z - size.z / 2};
  box.max = {pos.x + size.x / 2, pos.y + size.y, pos.z + size.z / 2};
  levelBlocks.push_back({box, color, texType});
}
// -----------------------------------------------------------------------------
// Architectural Generators (Brutalist Archetypes)
// -----------------------------------------------------------------------------

void GenerateBuilding_Simple(Vector3 pos, Vector3 size) {
  // A simple rectangular building
  AddBlock(pos, size, COLOR_CONCRETE_MID, TextureType::PILLAR);
}

void GenerateCity() {
  levelBlocks.clear();

  // The Simple Highway
  AddBlock({0, 0, -500}, {20.0f, 1.0f, 1000.0f}, COLOR_CONCRETE_LIGHT,
           TextureType::HIGHWAY);

  // Place simple buildings alongside the highway
  GenerateBuilding_Simple({-40, 0, -50}, {20.0f, 40.0f, 20.0f});
  GenerateBuilding_Simple({40, 0, -120}, {25.0f, 60.0f, 20.0f});
  GenerateBuilding_Simple({-45, 0, -200}, {30.0f, 50.0f, 30.0f});
  GenerateBuilding_Simple({40, 0, -320}, {20.0f, 80.0f, 20.0f});
  GenerateBuilding_Simple({-35, 0, -420}, {15.0f, 30.0f, 15.0f});
  GenerateBuilding_Simple({50, 0, -550}, {40.0f, 100.0f, 30.0f});

  // End Block
  AddBlock({0, 0, -950}, {80.0f, 20.0f, 80.0f}, COLOR_CONCRETE_DARK,
           TextureType::BASE);
}

float GetGroundHeight(Vector3 position, float currentY,
                      float stepHeight = 2.0f) {
  float maxGround = -9999.0f;
  for (const auto &block : levelBlocks) {
    // Broad x/z collision
    if (position.x >= block.bounds.min.x && position.x <= block.bounds.max.x &&
        position.z >= block.bounds.min.z && position.z <= block.bounds.max.z) {

      // Check if we are above or can step up onto it
      if (currentY + stepHeight >= block.bounds.max.y) {
        if (block.bounds.max.y > maxGround) {
          maxGround = block.bounds.max.y;
        }
      }
    }
  }
  // Fallback to absolute floor at 0 if no blocks found
  return maxGround == -9999.0f ? 0.0f : maxGround;
}

Vector3 ResolveWallCollisions(Vector3 position, float radius, float height,
                              float stepHeight = 2.0f) {
  Vector3 newPos = position;
  for (const auto &block : levelBlocks) {
    // Check if we are physically within the Y-bounds of the block, considering
    // step height
    if (newPos.y + height > block.bounds.min.y &&
        newPos.y + stepHeight < block.bounds.max.y) {

      BoundingBox inflated = block.bounds;
      inflated.min.x -= radius;
      inflated.max.x += radius;
      inflated.min.z -= radius;
      inflated.max.z += radius;

      if (newPos.x > inflated.min.x && newPos.x < inflated.max.x &&
          newPos.z > inflated.min.z && newPos.z < inflated.max.z) {

        // Push out based on the shortest penetration distance
        float distLeft = newPos.x - inflated.min.x;
        float distRight = inflated.max.x - newPos.x;
        float distFront = newPos.z - inflated.min.z;
        float distBack = inflated.max.z - newPos.z;

        float minDist =
            fminf(fminf(distLeft, distRight), fminf(distFront, distBack));

        if (minDist == distLeft)
          newPos.x = inflated.min.x;
        else if (minDist == distRight)
          newPos.x = inflated.max.x;
        else if (minDist == distFront)
          newPos.z = inflated.min.z;
        else
          newPos.z = inflated.max.z;
      }
    }
  }
  return newPos;
}

// Custom Motorcycle State
struct Motorcycle {
  Vector3 position;
  Vector3 velocity;
  float yaw;
  float pitch; // For leaning
  float speed;
  bool isGrounded;
  float groundOffset;
  float fuel;      // Remaining fuel 0-100
  int parts;       // Scavenged parts
  bool isBoosting; // From music stats maybe later
};

// Player states mapping to animations
enum PlayerState {
  IDLE = 9,
  WALKING = 44,
  SPRINTING = 37,
  JUMPING = 15,
  CROUCHING = 2,
  CROUCH_WALKING = 1
};

struct Player {
  Vector3 position;
  Vector3 velocity;
  float yaw;
  PlayerState state;
  bool isGrounded;
  float speed;
  int animFrameCounter;
  bool isRidingBike;
};

// Simplified collision check against a flat ground plane at y = 0
bool CheckGroundCollision(Vector3 position) { return (position.y <= 0.0f); }

void UpdatePlayer(Player *player, Camera3D *camera, float *cameraYaw,
                  float *cameraPitch, GameState *state, float dt) {
  // Determine movement direction from camera yaw
  Vector3 forward = {sinf(*cameraYaw), 0.0f, cosf(*cameraYaw)};
  Vector3 right = {cosf(*cameraYaw), 0.0f, -sinf(*cameraYaw)};

  Vector3 moveInput = {0.0f, 0.0f, 0.0f};
  if (IsKeyDown(KEY_W))
    moveInput.z += 1.0f;
  if (IsKeyDown(KEY_S))
    moveInput.z -= 1.0f;
  if (IsKeyDown(KEY_A))
    moveInput.x += 1.0f;
  if (IsKeyDown(KEY_D))
    moveInput.x -= 1.0f;

  // Normalize input
  if (Vector3Length(moveInput) > 0.0f) {
    moveInput = Vector3Normalize(moveInput);
  }

  // Determine target speed and state
  float targetSpeed = 0.0f;
  PlayerState targetState = IDLE;

  // Music kinetic stat affects base walk/sprint speeds
  float speedMod = 1.0f + (state->stats.kinetic / 50.0f);

  if (Vector3Length(moveInput) > 0.1f) {
    if (IsKeyDown(KEY_LEFT_SHIFT)) {
      targetSpeed = 12.0f * speedMod;
      targetState = SPRINTING;
    } else if (IsKeyDown(KEY_LEFT_CONTROL)) {
      targetSpeed = 2.0f * speedMod;
      targetState = CROUCH_WALKING;
    } else {
      targetSpeed = 8.0f * speedMod;
      targetState = WALKING;
    }
  } else if (IsKeyDown(KEY_LEFT_CONTROL)) {
    targetState = CROUCHING;
  }

  // Handle jumping
  if (player->isGrounded && IsKeyPressed(KEY_SPACE)) {
    player->velocity.y = 10.0f;
    player->isGrounded = false;
    targetState = JUMPING;
  }

  if (!player->isGrounded) {
    targetState = JUMPING;
    player->velocity.y -= GRAVITY * dt;
  }

  // Update state and anim frame
  if (player->state != targetState) {
    player->state = targetState;
    player->animFrameCounter = 0; // Reset animation on state change
  }

  // Apply movement
  player->speed = Lerp(player->speed, targetSpeed, dt * 10.0f);

  Vector3 desiredVelocity =
      Vector3Add(Vector3Scale(forward, moveInput.z * player->speed),
                 Vector3Scale(right, moveInput.x * player->speed));

  player->velocity.x = desiredVelocity.x;
  player->velocity.z = desiredVelocity.z;

  player->position.x += player->velocity.x * dt;
  player->position.z += player->velocity.z * dt;
  player->position.y += player->velocity.y * dt;

  // Resolve Wall Collisions
  player->position = ResolveWallCollisions(player->position, 0.5f, 1.8f);

  // Update yaw to face movement direction if moving
  if (Vector3Length(moveInput) > 0.1f) {
    player->yaw = atan2f(player->velocity.x, player->velocity.z);
  }

  // Ground Check
  float groundY = GetGroundHeight(player->position, player->position.y, 2.0f);
  if (player->position.y <= groundY) {
    player->position.y = groundY;
    player->velocity.y = 0.0f;
    player->isGrounded = true;
    if (player->state == JUMPING)
      player->state = IDLE;
  }
}

void UpdateMotorcycle(Motorcycle *bike, GameState *state, float dt) {
  // Input for driving
  float targetSpeed = 0.0f;

  // Radio passive buffs
  float speedMod = 1.0f;
  float turnSpeed = 2.0f;
  if (state->activeRadioType == MediaType::MUSIC) {
    speedMod = 1.3f;  // Faster top speed
    turnSpeed = 2.5f; // Sharper turns
    bike->isBoosting = true;
  } else {
    bike->isBoosting = false;
  }

  if (IsKeyDown(KEY_W)) {
    targetSpeed = 40.0f * speedMod;
    // Basic fuel drain
    if (bike->fuel > 0.0f) {
      bike->fuel -= dt * 0.5f;
    } else {
      targetSpeed = 0.0f; // Out of fuel
    }
  }
  if (IsKeyDown(KEY_S))
    targetSpeed = -10.0f;

  if (IsKeyDown(KEY_A))
    bike->yaw += turnSpeed * dt * (fmaxf(0.5f, bike->speed / 40.0f));
  if (IsKeyDown(KEY_D))
    bike->yaw -= turnSpeed * dt * (fmaxf(0.5f, bike->speed / 40.0f));

  // Apply lean based on turning
  float targetPitch = 0.0f;
  if (IsKeyDown(KEY_A))
    targetPitch = 0.3f * (bike->speed / 40.0f);
  if (IsKeyDown(KEY_D))
    targetPitch = -0.3f * (bike->speed / 40.0f);
  bike->pitch = Lerp(bike->pitch, targetPitch, dt * 5.0f);

  // Accelerate / Brake
  if (IsKeyDown(KEY_S) && bike->speed > 1.0f) {
    bike->speed = Lerp(bike->speed, 0.0f, dt * 4.0f);
  } else {
    bike->speed =
        Lerp(bike->speed, targetSpeed, dt * (targetSpeed == 0 ? 2.0f : 1.0f));
  }

  // Jump Logic
  if (bike->isGrounded && IsKeyPressed(KEY_SPACE)) {
    bike->velocity.y = 12.0f; // Vertical jump force
    bike->isGrounded = false;
  }

  // Gravity
  if (!bike->isGrounded) {
    bike->velocity.y -= GRAVITY * dt;
  }

  // Update Velocity vector based on yaw (inverted to match model front)
  Vector3 forward = {-sinf(bike->yaw), 0, -cosf(bike->yaw)};
  bike->velocity.x = forward.x * bike->speed;
  bike->velocity.z = forward.z * bike->speed;

  // Apply movement
  bike->position.x += bike->velocity.x * dt;
  bike->position.z += bike->velocity.z * dt;
  bike->position.y += bike->velocity.y * dt;

  // Resolve Wall Collisions
  bike->position = ResolveWallCollisions(bike->position, 1.0f, 1.5f);

  // Ground check
  float groundY = GetGroundHeight(bike->position, bike->position.y, 1.5f);
  if (bike->position.y <= groundY) {
    bike->position.y = groundY;
    bike->isGrounded = true;
    bike->velocity.y = 0.0f;
  } else {
    bike->isGrounded = false;
  }
}

int main() {
  InitWindow(1280, 720, "Curious - Bike Riding Demo");
  SetTargetFPS(60);
  DisableCursor();

  // Bike Setup
  Motorcycle bike = {0};
  bike.position = (Vector3){0.0f, 0.0f, 0.0f};
  bike.yaw = 90.0f * DEG2RAD;
  bike.isGrounded = true;
  bike.fuel = 100.0f;
  bike.parts = 0;

  // Global Game State
  GameState gameState = {0};
  gameState.stats = {10.0f, 10.0f, 10.0f}; // Start base 10
  gameState.activeRadioType = MediaType::NONE;

  // Player Setup
  Player player = {0};
  player.position = {0.0f, 0.0f, 3.0f}; // Start near the bike
  player.velocity = {0.0f, 0.0f, 0.0f};
  player.yaw = PI; // Facing the bike initially
  player.state = IDLE;
  player.isGrounded = false;
  player.speed = 0.0f;
  player.animFrameCounter = 0;
  player.isRidingBike = false;
  Camera3D camera = {0};
  camera.position = (Vector3){0.0f, 2.0f, 0.0f}; // Set in loop
  camera.target = (Vector3){0.0f, 1.8f, 1.0f};
  camera.up = (Vector3){0.0f, 1.0f, 0.0f};
  camera.fovy = 70.0f;
  camera.projection = CAMERA_PERSPECTIVE;

  float cameraYaw = 0.0f;
  float cameraPitch = 0.0f;

  bool isFirstPerson = false;

  GenerateCity();

  // Load Assets
  Model bikeModel = LoadModel("model/model_fixed.glb");

  // Calculate Ground Offset based on Bounding Box and Scale
  float bikeScale = 0.0075f; // User's requested final scale
  BoundingBox box = GetModelBoundingBox(bikeModel);
  bike.groundOffset = -box.min.y * bikeScale;

  // Render Target for Low-Res PS2 Aesthetic
  // Increased slightly from 320x240 to 480x360 to handle detailed concrete
  // textures better
  int renderWidth = 480;
  int renderHeight = 360;
  RenderTexture2D lowResTarget = LoadRenderTexture(renderWidth, renderHeight);
  SetTextureFilter(lowResTarget.texture,
                   TEXTURE_FILTER_POINT); // Nearest-neighbor scaling

  // --- Shadow Mapping Setup ---
  int shadowMapResolution = 2048; // High res shadow map
  RenderTexture2D shadowMapTarget =
      LoadRenderTexture(shadowMapResolution, shadowMapResolution);

  Shader ps2Shader = LoadShader("shaders/ps2_lit.vs", "shaders/ps2_lit.fs");
  Shader shadowShader =
      LoadShader("shaders/shadow_map.vs", "shaders/shadow_map.fs");

  // Shader Setup
  int lightDirLoc = GetShaderLocation(ps2Shader, "lightDir");
  int lightColorLoc = GetShaderLocation(ps2Shader, "lightColor");
  int ambientLoc = GetShaderLocation(ps2Shader, "ambientColor");
  int fogDensityLoc = GetShaderLocation(ps2Shader, "fogDensity");
  int fogColorLoc = GetShaderLocation(ps2Shader, "fogColor");
  int resolutionLoc = GetShaderLocation(ps2Shader, "resolution");
  int jitterIntLoc = GetShaderLocation(ps2Shader, "vertexJitterIntensity");
  int colorDepthLoc = GetShaderLocation(ps2Shader, "colorDepth");
  int shadowMapLoc = GetShaderLocation(ps2Shader, "shadowMap");
  int matLightLoc = GetShaderLocation(ps2Shader, "matLight");

  Mesh cubeMesh = GenMeshCube(1.0f, 1.0f, 1.0f);
  Model mapBlockModel = LoadModelFromMesh(cubeMesh);
  mapBlockModel.materials[0].shader = ps2Shader;

  Vector3 lightDir = Vector3Normalize((Vector3){0.5f, -1.0f, 0.6f});
  Vector3 lightColor = {1.5f, 1.45f,
                        1.4f}; // Warmer, brighter directional sunlight
  Vector3 ambientColor = {0.6f, 0.6f, 0.65f}; // Brighter ambient light

  // Daylight sky fog
  float fogDensity = 0.005f;
  Vector3 fogColor = {0.53f, 0.81f,
                      0.92f}; // Sky blue (135, 206, 235) scaled to 0-1

  Vector2 resolution = {(float)renderWidth, (float)renderHeight};
  float jitterIntensity = 0.5f; // Controls wobble size
  float colorDepthValue =
      64.0f; // Increased from 16 to 64 to smooth out grainy texture banding

  SetShaderValue(ps2Shader, lightDirLoc, &lightDir, SHADER_UNIFORM_VEC3);
  SetShaderValue(ps2Shader, lightColorLoc, &lightColor, SHADER_UNIFORM_VEC3);
  SetShaderValue(ps2Shader, ambientLoc, &ambientColor, SHADER_UNIFORM_VEC3);
  SetShaderValue(ps2Shader, fogDensityLoc, &fogDensity, SHADER_UNIFORM_FLOAT);
  SetShaderValue(ps2Shader, fogColorLoc, &fogColor, SHADER_UNIFORM_VEC3);
  SetShaderValue(ps2Shader, resolutionLoc, &resolution, SHADER_UNIFORM_VEC2);
  SetShaderValue(ps2Shader, jitterIntLoc, &jitterIntensity,
                 SHADER_UNIFORM_FLOAT);
  SetShaderValue(ps2Shader, colorDepthLoc, &colorDepthValue,
                 SHADER_UNIFORM_FLOAT);

  // Color Setup - Monochromatic Brutalist with minimal accents
  Color matColors[10] = {(Color){40, 40, 40, 255},   (Color){30, 30, 32, 255},
                         (Color){25, 25, 27, 255},   (Color){20, 20, 22, 255},
                         (Color){22, 22, 24, 255},   (Color){45, 45, 48, 255},
                         (Color){80, 80, 85, 255},   (Color){40, 40, 40, 255},
                         (Color){240, 200, 40, 255}, (Color){40, 40, 42, 255}};

  Image whiteImg = GenImageColor(1, 1, WHITE);
  Texture2D whiteTex = LoadTextureFromImage(whiteImg);
  UnloadImage(whiteImg);

  for (int i = 0; i < bikeModel.materialCount && i < 10; i++) {
    bikeModel.materials[i].shader = ps2Shader;
    bikeModel.materials[i].maps[MATERIAL_MAP_ALBEDO].texture = whiteTex;
    bikeModel.materials[i].maps[MATERIAL_MAP_ALBEDO].color = matColors[i];
  }

  Model bikeOutlineModel = bikeModel;
  bikeOutlineModel.materials =
      (Material *)RL_CALLOC(bikeModel.materialCount, sizeof(Material));
  for (int i = 0; i < bikeModel.materialCount; i++) {
    bikeOutlineModel.materials[i] = LoadMaterialDefault();
    bikeOutlineModel.materials[i].maps[MATERIAL_MAP_ALBEDO].texture = whiteTex;
    bikeOutlineModel.materials[i].maps[MATERIAL_MAP_ALBEDO].color =
        (Color){10, 10, 10, 255};
  }

  // Generate extremely large concrete plane for the "Void" map base
  Mesh groundMesh = GenMeshPlane(10000.0f, 10000.0f, 100, 100);
  Model groundModel = LoadModelFromMesh(groundMesh);
  groundModel.materials[0].shader = ps2Shader;
  groundModel.materials[0].maps[MATERIAL_MAP_ALBEDO].color =
      (Color){150, 150, 150, 255}; // Brighter ground

  // Load Player Character and Animations
  Model playerModel = LoadModel("UAL1_Standard.glb");
  int animsCount = 0;
  ModelAnimation *anims = LoadModelAnimations("UAL1_Standard.glb", &animsCount);

  // Set up player material
  for (int i = 0; i < playerModel.materialCount; i++) {
    playerModel.materials[i].shader = ps2Shader;
    // Brighter Player
    playerModel.materials[i].maps[MATERIAL_MAP_ALBEDO].color =
        (Color){180, 180, 185, 255};
  }

  while (!WindowShouldClose()) {
    float dt = GetFrameTime();
    float time = (float)GetTime();

    // --- Camera Input ---
    Vector2 mouseDelta = GetMouseDelta();
    cameraYaw -= mouseDelta.x * MOUSE_SENSITIVITY;
    cameraPitch -= mouseDelta.y * MOUSE_SENSITIVITY;
    cameraPitch = Clamp(cameraPitch, -1.2f, 1.2f);

    if (IsKeyPressed(KEY_V)) {
      isFirstPerson = !isFirstPerson;
    }

    // --- Mount/Dismount Logic ---
    if (IsKeyPressed(KEY_F)) {
      if (!player.isRidingBike) {
        // Try to mount
        float distToBike = Vector3Distance(player.position, bike.position);
        if (distToBike < 3.0f) {
          player.isRidingBike = true;
          player.animFrameCounter = 0; // Reset anim for sitting
        }
      } else {
        // Dismount
        player.isRidingBike = false;
        // Place player to the left of the bike
        Vector3 left = {cosf(bike.yaw), 0.0f, -sinf(bike.yaw)};
        player.position = Vector3Add(bike.position, Vector3Scale(left, 1.5f));
        player.position.y = 0.0f; // Ground level
        player.velocity = {0.0f, 0.0f, 0.0f};
        player.yaw = bike.yaw; // Face same way as bike
        player.animFrameCounter = 0;
      }
    }

    // --- Input: Media Consumption & Radio ---
    if (IsKeyPressed(KEY_ONE))
      gameState.stats.empathy += 5.0f;
    if (IsKeyPressed(KEY_TWO))
      gameState.stats.kinetic += 5.0f;
    if (IsKeyPressed(KEY_THREE))
      gameState.stats.perception += 5.0f;

    if (player.isRidingBike) {
      if (IsKeyPressed(KEY_SEVEN))
        gameState.activeRadioType = MediaType::LITERATURE;
      if (IsKeyPressed(KEY_EIGHT))
        gameState.activeRadioType = MediaType::MUSIC;
      if (IsKeyPressed(KEY_NINE))
        gameState.activeRadioType = MediaType::CINEMA;
      if (IsKeyPressed(KEY_ZERO))
        gameState.activeRadioType = MediaType::NONE;
    }

    // --- State Logic ---
    Vector3 activeTargetPos;
    float activeYaw;

    if (player.isRidingBike) {
      // Bike logic
      UpdateMotorcycle(&bike, &gameState, dt);

      // Sync player to bike seat
      player.position = bike.position;
      player.position.y += 0.5f; // Seat height offset
      player.yaw =
          bike.yaw +
          PI; // Model faces opposite Z originally, so flip to match bike front

      // Determine riding animation state
      if (bike.speed > 1.0f) {
        player.state = (PlayerState)5; // Driving_Loop (ID 5)
      } else {
        player.state = (PlayerState)31; // Sitting_Idle_Loop (ID 31)
      }

      activeTargetPos = bike.position;
      // The bike's visual front is opposite to its yaw
      activeYaw = bike.yaw + PI;

    } else {
      // Player logic
      UpdatePlayer(&player, &camera, &cameraYaw, &cameraPitch, &gameState, dt);
      activeTargetPos = player.position;
      activeYaw = player.yaw;
    }

    // --- Camera Logic ---
    Vector3 viewDir = {sinf(cameraYaw) * cosf(cameraPitch), sinf(cameraPitch),
                       cosf(cameraYaw) * cosf(cameraPitch)};

    // Resetting FOV to the default 70 as requested
    camera.fovy = 70.0f;

    if (isFirstPerson) {
      // 1st Person Camera
      if (player.isRidingBike) {
        camera.position = player.position;
        camera.position.y += 1.4f; // Eye level while sitting
        // Push backward slightly when riding to see down to the bike/handlebars
        Vector3 forward = {sinf(activeYaw), 0.0f, cosf(activeYaw)};
        camera.position.x -= forward.x * 0.1f;
        camera.position.z -= forward.z * 0.1f;
      } else {
        camera.position = player.position;
        camera.position.y += 1.6f; // Eye level while standing
        Vector3 forward = {sinf(activeYaw), 0.0f, cosf(activeYaw)};
        camera.position.x +=
            forward.x * 0.35f; // Push forward to avoid clipping while walking
        camera.position.z += forward.z * 0.35f;
      }

      camera.target = Vector3Add(camera.position, viewDir);
    } else {
      // 3rd Person Camera
      float cameraDist = player.isRidingBike ? 2.5f : 1.5f; // Zoomed in a bit
      camera.position =
          Vector3Subtract(activeTargetPos, Vector3Scale(viewDir, cameraDist));
      camera.position.y += 1.5f;

      camera.target = activeTargetPos;
      camera.target.y += 1.5f;
    }

    // --- Animation Logic ---
    int currentAnimId = (int)player.state;

    // Safety check just in case the requested animation index is out of bounds
    if (anims != NULL && currentAnimId >= 0 && currentAnimId < animsCount) {
      player.animFrameCounter++;
      UpdateModelAnimation(playerModel, anims[currentAnimId],
                           player.animFrameCounter %
                               anims[currentAnimId].frameCount);
    }

    // --- 1. Shadow Map Pass ---
    // define our directional light mathematically as an orthographic projection
    Vector3 lightPos = Vector3Add(activeTargetPos,
                                  Vector3Scale(Vector3Negate(lightDir), 50.0f));
    Camera3D lightCam = {0};
    lightCam.position = lightPos;
    lightCam.target = activeTargetPos;
    lightCam.up = (Vector3){0.0f, 1.0f, 0.0f};
    lightCam.fovy = 50.0f; // Works as orthographic size for shadow map
    lightCam.projection = CAMERA_ORTHOGRAPHIC;

    Matrix matLightView =
        MatrixLookAt(lightCam.position, lightCam.target, lightCam.up);
    Matrix matLightProj = MatrixOrtho(-50.0, 50.0, -50.0, 50.0, 1.0, 100.0);
    Matrix matLight = MatrixMultiply(matLightView, matLightProj);

    // Update light matrix in main shader
    SetShaderValueMatrix(ps2Shader, matLightLoc, matLight);

    BeginTextureMode(shadowMapTarget);
    ClearBackground(WHITE); // White = max depth
    BeginMode3D(lightCam);

    // Swap materials to use the simple depth shader
    groundModel.materials[0].shader = shadowShader;
    mapBlockModel.materials[0].shader = shadowShader;
    bikeModel.materials[0].shader = shadowShader;
    playerModel.materials[0].shader = shadowShader;

    // Draw specifically for the shadow depth buffer
    DrawModel(groundModel, (Vector3){0, 0, 0}, 1.0f, WHITE);
    for (const auto &block : levelBlocks) {
      if (block.bounds.max.y > 0.0f) {
        Vector3 size = {block.bounds.max.x - block.bounds.min.x,
                        block.bounds.max.y - block.bounds.min.y,
                        block.bounds.max.z - block.bounds.min.z};
        Vector3 center = {block.bounds.min.x + size.x / 2.0f,
                          block.bounds.min.y + size.y / 2.0f,
                          block.bounds.min.z + size.z / 2.0f};
        rlPushMatrix();
        rlTranslatef(center.x, center.y, center.z);
        rlScalef(size.x, size.y, size.z);
        DrawModel(mapBlockModel, (Vector3){0, 0, 0}, 1.0f, WHITE);
        rlPopMatrix();
      }
    }

    // Draw Bike Depth
    rlPushMatrix();
    rlTranslatef(bike.position.x, bike.position.y + bike.groundOffset,
                 bike.position.z);
    rlRotatef((bike.yaw * RAD2DEG) - 90.0f, 0, 1, 0);
    rlScalef(bikeScale, bikeScale, bikeScale);
    DrawModel(bikeModel, (Vector3){0, 0, 0}, 1.0f, WHITE);
    rlPopMatrix();

    // Draw Player Depth
    rlPushMatrix();
    rlTranslatef(player.position.x, player.position.y, player.position.z);
    rlRotatef(player.yaw * RAD2DEG, 0.0f, 1.0f, 0.0f);
    DrawModel(playerModel, (Vector3){0, 0, 0}, 1.0f, WHITE);
    rlPopMatrix();

    EndMode3D();
    EndTextureMode();

    // --- 2. Main Scene Pass ---
    // Restore shaders
    groundModel.materials[0].shader = ps2Shader;
    mapBlockModel.materials[0].shader = ps2Shader;
    for (int i = 0; i < bikeModel.materialCount; i++)
      bikeModel.materials[i].shader = ps2Shader;
    for (int i = 0; i < playerModel.materialCount; i++)
      playerModel.materials[i].shader = ps2Shader;

    BeginTextureMode(lowResTarget);
    ClearBackground((Color){135, 206, 235, 255}); // Daylight sky blue

    // Bind shadow map texture to slot 1 (since material texture defaults to
    // slot 0)
    SetShaderValueTexture(ps2Shader, shadowMapLoc, shadowMapTarget.texture);

    BeginMode3D(camera);

    DrawModel(groundModel, (Vector3){0, 0, 0}, 1.0f, WHITE);

    // Draw Brutalist Blocks
    for (const auto &block : levelBlocks) {
      // Only draw above-ground blocks to save triangles (since the ground is
      // already drawn by groundModel)
      if (block.bounds.max.y > 0.0f) {
        Vector3 size = {block.bounds.max.x - block.bounds.min.x,
                        block.bounds.max.y - block.bounds.min.y,
                        block.bounds.max.z - block.bounds.min.z};
        Vector3 center = {block.bounds.min.x + size.x / 2.0f,
                          block.bounds.min.y + size.y / 2.0f,
                          block.bounds.min.z + size.z / 2.0f};
        // Apply the block material color directly
        mapBlockModel.materials[0].maps[MATERIAL_MAP_ALBEDO].color =
            block.color;

        rlPushMatrix();
        rlTranslatef(center.x, center.y, center.z);
        rlScalef(size.x, size.y, size.z);
        DrawModel(mapBlockModel, (Vector3){0, 0, 0}, 1.0f, WHITE);
        rlPopMatrix();
      }
    }

    // Draw Bike
    rlPushMatrix();
    rlTranslatef(bike.position.x, bike.position.y + bike.groundOffset,
                 bike.position.z);
    // Rotate an additional -90 degrees (right) to align model
    rlRotatef((bike.yaw * RAD2DEG) - 90.0f, 0, 1, 0);
    // Scale bike down to match the calculated bikeScale
    rlScalef(bikeScale, bikeScale, bikeScale);
    DrawModel(bikeModel, (Vector3){0, 0, 0}, 1.0f, WHITE);
    rlPopMatrix();

    // Draw Player
    rlPushMatrix();
    rlTranslatef(player.position.x, player.position.y, player.position.z);
    rlRotatef(player.yaw * RAD2DEG, 0.0f, 1.0f, 0.0f);
    // rlScalef(1.0f, 1.0f, 1.0f); // Default scale
    DrawModel(playerModel, (Vector3){0, 0, 0}, 1.0f, WHITE);
    rlPopMatrix();

    EndMode3D();
    EndTextureMode();

    // --- Render Final Screen ---
    BeginDrawing();
    ClearBackground(BLACK);

    // Nearest-neighbor upscale the low res texture to fit the screen size
    float scaledW = (float)GetScreenWidth();
    float scaledH = (float)GetScreenHeight();
    DrawTexturePro(lowResTarget.texture,
                   (Rectangle){0.0f, 0.0f, (float)lowResTarget.texture.width,
                               (float)-lowResTarget.texture.height},
                   (Rectangle){0.0f, 0.0f, scaledW, scaledH}, (Vector2){0, 0},
                   0.0f, WHITE);

    // Re-add basic PS2 style system text
    if (!player.isRidingBike) {
      float distToBike = Vector3Distance(player.position, bike.position);
      if (distToBike < 3.0f) {
        DrawText("PRESS 'F' TO MOUNT", GetScreenWidth() / 2 - 100,
                 GetScreenHeight() - 100, 20, GREEN);
      }
    } else {
      DrawText(
          "W: Accelerate | S: Brake | A/D: Steer | SPACE: Jump | F: Dismount",
          20, GetScreenHeight() - 40, 20, GREEN);
      DrawText(TextFormat("SPEED: %02.0f", bike.speed), 20, 20, 30, GREEN);
      DrawText(TextFormat("FUEL: %02.0f", fmaxf(0.0f, bike.fuel)), 20, 60, 30,
               GREEN);
    }

    DrawFPS(GetScreenWidth() - 100, 10);
    EndDrawing();
  }

  UnloadModel(bikeModel);
  if (anims != NULL)
    UnloadModelAnimations(anims, animsCount);
  UnloadModel(playerModel);
  UnloadModel(groundModel);
  UnloadModel(mapBlockModel);
  UnloadRenderTexture(lowResTarget);
  UnloadRenderTexture(shadowMapTarget); // Add shadowMap cleanup
  UnloadShader(ps2Shader);
  UnloadShader(shadowShader); // Add shadowShader cleanup
  CloseWindow();
  return 0;
}
