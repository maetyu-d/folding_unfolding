#include "CityOpenGLRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>

namespace
{
constexpr auto gridStep = 96.0f;

float snappedFootprint (float rawFootprint) noexcept
{
    return static_cast<float> (juce::jmax (1, juce::roundToInt (rawFootprint / gridStep))) * gridStep;
}

juce::Colour rainbowAt (float value, float alpha = 1.0f) noexcept
{
    const auto hue = std::fmod (value + 1.0f, 1.0f);
    return juce::Colour::fromHSV (hue, 0.92f, 0.98f, alpha);
}

juce::String vertexShader()
{
    return R"(
        attribute vec4 position;
        attribute vec4 normal;
        attribute vec4 sourceColour;
        attribute float selectedAmount;
        attribute float flashAmount;

        uniform vec2 viewportSize;
        uniform vec2 pan;
        uniform float zoom;
        uniform float viewMode;

        varying vec4 destinationColour;

        void main()
        {
            vec3 n = normalize (normal.xyz);
            vec3 keyLight = normalize (vec3 (-0.42, -0.62, 0.82));
            vec3 fillLight = normalize (vec3 (0.56, 0.18, 0.72));

            float diffuse = max (dot (n, keyLight), 0.0);
            float fill = max (dot (n, fillLight), 0.0);
            float light = 0.42 + diffuse * 0.52 + fill * 0.18;

            vec3 litColour = sourceColour.rgb * light;
            litColour = mix (litColour, vec3 (1.0, 0.82, 0.24), selectedAmount * 0.22);
            litColour = mix (litColour, vec3 (1.0, 0.93, 0.46), flashAmount * 0.72);

            destinationColour = vec4 (litColour, sourceColour.a);

            float isoX = (position.x - position.y) * 0.8660254038;
            float isoY = (position.x + position.y) * 0.5 - position.z;
            float topX = position.x;
            float topY = position.y;
            float projectionMix = step (0.5, viewMode);
            float screenX = viewportSize.x * 0.5 + pan.x + mix (isoX, topX, projectionMix) * zoom;
            float screenY = viewportSize.y * 0.5 + pan.y + mix (isoY, topY, projectionMix) * zoom;

            vec2 clip = vec2 ((screenX / viewportSize.x) * 2.0 - 1.0,
                              1.0 - (screenY / viewportSize.y) * 2.0);

            float depth = clamp ((position.x + position.y + position.z * 0.7) / 4000.0, -0.95, 0.95);
            gl_Position = vec4 (clip, depth, 1.0);
        }
    )";
}

juce::String fragmentShader()
{
    return R"(
        varying vec4 destinationColour;

        void main()
        {
            gl_FragColor = destinationColour;
        }
    )";
}

juce::OpenGLShaderProgram::Attribute* createAttribute (juce::OpenGLShaderProgram& shader,
                                                       const char* name)
{
    using namespace ::juce::gl;

    if (glGetAttribLocation (shader.getProgramID(), name) < 0)
        return nullptr;

    return new juce::OpenGLShaderProgram::Attribute (shader, name);
}

juce::OpenGLShaderProgram::Uniform* createUniform (juce::OpenGLShaderProgram& shader,
                                                   const char* name)
{
    using namespace ::juce::gl;

    if (glGetUniformLocation (shader.getProgramID(), name) < 0)
        return nullptr;

    return new juce::OpenGLShaderProgram::Uniform (shader, name);
}
}

struct CityOpenGLRenderer::Attributes
{
    explicit Attributes (juce::OpenGLShaderProgram& program)
    {
        position.reset (createAttribute (program, "position"));
        normal.reset (createAttribute (program, "normal"));
        sourceColour.reset (createAttribute (program, "sourceColour"));
        selectedAmount.reset (createAttribute (program, "selectedAmount"));
        flashAmount.reset (createAttribute (program, "flashAmount"));
    }

    void enable()
    {
        using namespace ::juce::gl;

        if (position != nullptr)
        {
            glVertexAttribPointer (position->attributeID,
                                   3,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   sizeof (Vertex),
                                   reinterpret_cast<GLvoid*> (offsetof (Vertex, position)));
            glEnableVertexAttribArray (position->attributeID);
        }

        if (normal != nullptr)
        {
            glVertexAttribPointer (normal->attributeID,
                                   3,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   sizeof (Vertex),
                                   reinterpret_cast<GLvoid*> (offsetof (Vertex, normal)));
            glEnableVertexAttribArray (normal->attributeID);
        }

        if (sourceColour != nullptr)
        {
            glVertexAttribPointer (sourceColour->attributeID,
                                   4,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   sizeof (Vertex),
                                   reinterpret_cast<GLvoid*> (offsetof (Vertex, colour)));
            glEnableVertexAttribArray (sourceColour->attributeID);
        }

        if (selectedAmount != nullptr)
        {
            glVertexAttribPointer (selectedAmount->attributeID,
                                   1,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   sizeof (Vertex),
                                   reinterpret_cast<GLvoid*> (offsetof (Vertex, selected)));
            glEnableVertexAttribArray (selectedAmount->attributeID);
        }

        if (flashAmount != nullptr)
        {
            glVertexAttribPointer (flashAmount->attributeID,
                                   1,
                                   GL_FLOAT,
                                   GL_FALSE,
                                   sizeof (Vertex),
                                   reinterpret_cast<GLvoid*> (offsetof (Vertex, flash)));
            glEnableVertexAttribArray (flashAmount->attributeID);
        }
    }

    void disable()
    {
        using namespace ::juce::gl;

        if (position != nullptr)       glDisableVertexAttribArray (position->attributeID);
        if (normal != nullptr)         glDisableVertexAttribArray (normal->attributeID);
        if (sourceColour != nullptr)   glDisableVertexAttribArray (sourceColour->attributeID);
        if (selectedAmount != nullptr) glDisableVertexAttribArray (selectedAmount->attributeID);
        if (flashAmount != nullptr)    glDisableVertexAttribArray (flashAmount->attributeID);
    }

    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> position;
    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> normal;
    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> sourceColour;
    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> selectedAmount;
    std::unique_ptr<juce::OpenGLShaderProgram::Attribute> flashAmount;
};

struct CityOpenGLRenderer::Uniforms
{
    explicit Uniforms (juce::OpenGLShaderProgram& program)
    {
        viewportSize.reset (createUniform (program, "viewportSize"));
        pan.reset (createUniform (program, "pan"));
        zoom.reset (createUniform (program, "zoom"));
        viewMode.reset (createUniform (program, "viewMode"));
    }

    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> viewportSize;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> pan;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> zoom;
    std::unique_ptr<juce::OpenGLShaderProgram::Uniform> viewMode;
};

CityOpenGLRenderer::CityOpenGLRenderer (juce::OpenGLContext& context)
    : openGLContext (context)
{
}

CityOpenGLRenderer::~CityOpenGLRenderer() = default;

void CityOpenGLRenderer::newOpenGLContextCreated()
{
    using namespace ::juce::gl;

    releaseOpenGLResources();
    glGenBuffers (1, &vertexBuffer);
    initialiseShader();
}

void CityOpenGLRenderer::renderOpenGL()
{
    using namespace ::juce::gl;

    const auto state = createRenderState ? createRenderState() : CityRenderState {};
    const auto width = juce::jmax (1, state.width);
    const auto height = juce::jmax (1, state.height);
    const auto desktopScale = static_cast<float> (openGLContext.getRenderingScale());

    juce::OpenGLHelpers::clear (state.colourWireframeMode ? juce::Colours::black
                                                           : juce::Colour (0xfff8fbf5));

    if (shader == nullptr)
        initialiseShader();

    if (shader == nullptr || attributes == nullptr || uniforms == nullptr || vertexBuffer == 0)
        return;

    buildMeshes (state);

    glViewport (0,
                0,
                juce::roundToInt (desktopScale * static_cast<float> (width)),
                juce::roundToInt (desktopScale * static_cast<float> (height)));

    glDisable (GL_DEPTH_TEST);
    glEnable (GL_BLEND);
    glBlendFunc (GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    shader->use();

    if (uniforms->viewportSize != nullptr)
        uniforms->viewportSize->set (static_cast<float> (width), static_cast<float> (height));

    if (uniforms->pan != nullptr)
        uniforms->pan->set (state.pan.x, state.pan.y);

    if (uniforms->zoom != nullptr)
        uniforms->zoom->set (state.zoom);

    if (uniforms->viewMode != nullptr)
        uniforms->viewMode->set (state.viewMode == CityViewMode::topDown ? 1.0f : 0.0f);

    glBindBuffer (GL_ARRAY_BUFFER, vertexBuffer);
    attributes->enable();

    if (! state.colourWireframeMode)
        drawVertices (groundVertices, GL_TRIANGLES);

    drawVertices (powerZoneVertices, GL_TRIANGLES);
    drawVertices (gridVertices, GL_LINES, 1.25f);
    if (! state.colourWireframeMode)
    {
        drawVertices (blockTriangleVertices, GL_TRIANGLES);
        drawVertices (triangleVertices, GL_TRIANGLES);
        drawVertices (neonVertices, GL_TRIANGLES);
    }

    drawVertices (blockOutlineVertices, GL_LINES, state.colourWireframeMode ? 1.45f : 1.1f);
    drawVertices (outlineVertices, GL_LINES, state.colourWireframeMode ? 1.65f : 2.0f);

    attributes->disable();
    glBindBuffer (GL_ARRAY_BUFFER, 0);
}

void CityOpenGLRenderer::openGLContextClosing()
{
    releaseOpenGLResources();
}

void CityOpenGLRenderer::initialiseShader()
{
    if (shader != nullptr)
        return;

    auto newShader = std::make_unique<juce::OpenGLShaderProgram> (openGLContext);

    if (newShader->addVertexShader (juce::OpenGLHelpers::translateVertexShaderToV3 (vertexShader()))
        && newShader->addFragmentShader (juce::OpenGLHelpers::translateFragmentShaderToV3 (fragmentShader()))
        && newShader->link())
    {
        shader = std::move (newShader);
        shader->use();
        attributes = std::make_unique<Attributes> (*shader);
        uniforms = std::make_unique<Uniforms> (*shader);
    }
    else
    {
        jassertfalse;
        shader.reset();
        attributes.reset();
        uniforms.reset();
    }
}

void CityOpenGLRenderer::releaseOpenGLResources()
{
    using namespace ::juce::gl;

    attributes.reset();
    uniforms.reset();
    shader.reset();

    if (vertexBuffer != 0)
    {
        glDeleteBuffers (1, &vertexBuffer);
        vertexBuffer = 0;
    }
}

void CityOpenGLRenderer::buildMeshes (const CityRenderState& state)
{
    groundVertices.clear();
    powerZoneVertices.clear();
    gridVertices.clear();
    blockFaces.clear();
    faces.clear();
    neonFaces.clear();
    blockTriangleVertices.clear();
    triangleVertices.clear();
    neonVertices.clear();
    blockOutlineVertices.clear();
    outlineVertices.clear();
    colourWireframeMode = state.colourWireframeMode;

    const auto moduleLikeCount = state.modules.size();
    const auto cueCount = state.tipTriggerCues.size();
    if (! colourWireframeMode)
    {
        blockFaces.reserve (state.blocks.size() * 10);
        faces.reserve (moduleLikeCount * 104 + state.powerSources.size() * 24 + state.powerSwitches.size() * 16 + state.blocks.size() * 10);
        neonFaces.reserve (moduleLikeCount * 168 + state.platters.size() * 156 + state.planks.size() * 48 + cueCount * 132 + state.powerCables.size() * 16 + state.powerFeedCables.size() * 12);
    }
    blockOutlineVertices.reserve (state.blocks.size() * 28);
    outlineVertices.reserve (moduleLikeCount * 40 + state.platters.size() * 24 + state.planks.size() * 12 + state.powerCables.size() * 6 + state.powerFeedCables.size() * 6);

    IsoProjector visibilityView;
    visibilityView.zoom = juce::jmax (0.01f, state.zoom);
    visibilityView.pan = state.pan;
    visibilityView.viewMode = state.viewMode;
    visibilityView.centre = { static_cast<float> (state.width) * 0.5f,
                              static_cast<float> (state.height) * 0.5f };

    const auto visible = [&visibilityView, &state] (Vec2 position, float radius, float elevation) noexcept
    {
        const auto screen = visibilityView.project ({ position.x, position.y, elevation });
        const auto screenRadius = (radius * 1.9f + std::abs (elevation) * 0.42f) * visibilityView.zoom + 180.0f;

        return screen.x >= -screenRadius
            && screen.y >= -screenRadius
            && screen.x <= static_cast<float> (state.width) + screenRadius
            && screen.y <= static_cast<float> (state.height) + screenRadius;
    };

    addGround (state);

    for (const auto& powerSwitch : state.powerSwitches)
        if (! colourWireframeMode && visible (powerSwitch.position, powerSwitch.areaRadius, powerSwitch.elevation))
            addPowerZone (powerSwitch);

    addGrid (state);

    for (const auto& block : state.blocks)
        if (visible (block.position, block.hitRadius(), block.topElevation()))
            addBlock (block, state);

    addPowerFeeds (state);
    addPowerCables (state);

    for (const auto& source : state.powerSources)
        if (visible (source.position, source.radius * 1.8f, source.elevation + 60.0f))
            addPowerSource (source, state);

    for (const auto& powerSwitch : state.powerSwitches)
        if (visible (powerSwitch.position, powerSwitch.triggerRadius * 2.0f, powerSwitch.elevation + 40.0f))
            addPowerSwitch (powerSwitch, state);

    for (const auto& platter : state.platters)
        if (visible (platter.position, platter.hitRadius(), platter.elevation + 28.0f))
            addPlatter (platter, state);

    for (const auto& plank : state.planks)
        if (visible (plank.position, plank.hitRadius(), plank.elevation + 24.0f))
            addPlank (plank, state);

    for (const auto& module : state.modules)
        if (visible (module.position, module.radius + module.flapDepth, module.elevation + module.flapDepth))
            addModule (module, state);

    addTipTriggerCues (state);
    addSelectionHandles (state);

    auto sortFaces = [] (std::vector<Face>& faceList)
    {
        std::stable_sort (faceList.begin(), faceList.end(), [] (const Face& a, const Face& b)
        {
            return a.depth < b.depth;
        });
    };

    auto flattenFaces = [] (const std::vector<Face>& faceList, std::vector<Vertex>& vertices)
    {
        vertices.resize (faceList.size() * 3);
        auto* out = vertices.data();

        for (const auto& face : faceList)
        {
            *out++ = face.vertices[0];
            *out++ = face.vertices[1];
            *out++ = face.vertices[2];
        }
    };

    if (! colourWireframeMode)
    {
        sortFaces (blockFaces);
        sortFaces (faces);
        sortFaces (neonFaces);
        flattenFaces (blockFaces, blockTriangleVertices);
        flattenFaces (faces, triangleVertices);
        flattenFaces (neonFaces, neonVertices);
    }
}

void CityOpenGLRenderer::drawVertices (const std::vector<Vertex>& vertices,
                                       unsigned int primitive,
                                       float lineWidth)
{
    using namespace ::juce::gl;

    if (vertices.empty())
        return;

    glLineWidth (lineWidth);
    glBufferData (GL_ARRAY_BUFFER,
                  static_cast<GLsizeiptr> (vertices.size() * sizeof (Vertex)),
                  vertices.data(),
                  GL_DYNAMIC_DRAW);
    glDrawArrays (primitive, 0, static_cast<GLsizei> (vertices.size()));
}

void CityOpenGLRenderer::addGround (const CityRenderState& state)
{
    if (state.colourWireframeMode)
        return;

    IsoProjector view;
    view.zoom = juce::jmax (0.01f, state.zoom);
    view.pan = state.pan;
    view.viewMode = state.viewMode;
    view.centre = { static_cast<float> (state.width) * 0.5f, static_cast<float> (state.height) * 0.5f };

    const auto centreWorld = view.unprojectToGround (view.centre);
    const auto halfSpan = juce::jmax (850.0f, 1250.0f / view.zoom);
    const auto minX = centreWorld.x - halfSpan;
    const auto maxX = centreWorld.x + halfSpan;
    const auto minY = centreWorld.y - halfSpan;
    const auto maxY = centreWorld.y + halfSpan;

    const auto colourA = juce::Colour (0xfff8fbf5);
    const auto colourB = juce::Colour (0xffeef7f0);
    const auto normal = Vec3 { 0.0f, 0.0f, 1.0f };
    const auto selected = 0.0f;
    const auto flash = 0.0f;

    const auto a = Vec3 { minX, minY, -18.0f };
    const auto b = Vec3 { maxX, minY, -18.0f };
    const auto c = Vec3 { maxX, maxY, -18.0f };
    const auto d = Vec3 { minX, maxY, -18.0f };

    groundVertices.push_back (makeVertex (a, normal, colourA, selected, flash));
    groundVertices.push_back (makeVertex (b, normal, colourA, selected, flash));
    groundVertices.push_back (makeVertex (c, normal, colourB, selected, flash));
    groundVertices.push_back (makeVertex (a, normal, colourA, selected, flash));
    groundVertices.push_back (makeVertex (c, normal, colourB, selected, flash));
    groundVertices.push_back (makeVertex (d, normal, colourB, selected, flash));
}

void CityOpenGLRenderer::addPowerZone (const PowerSwitch& powerSwitch)
{
    constexpr auto segments = 36;
    const auto normal = Vec3 { 0.0f, 0.0f, 1.0f };
    const auto zoneZ = powerSwitch.elevation - 12.0f;
    const auto centre = Vec3 { powerSwitch.position.x, powerSwitch.position.y, zoneZ };
    const auto colour = rainbowAt (powerSwitch.powered ? 0.42f : 0.94f,
                                   0.035f + powerSwitch.flash * 0.055f);

    for (int i = 0; i < segments; ++i)
    {
        const auto a = juce::MathConstants<float>::twoPi * static_cast<float> (i) / static_cast<float> (segments);
        const auto b = juce::MathConstants<float>::twoPi * static_cast<float> (i + 1) / static_cast<float> (segments);
        const auto rimA = Vec3 { powerSwitch.position.x + std::cos (a) * powerSwitch.areaRadius,
                                 powerSwitch.position.y + std::sin (a) * powerSwitch.areaRadius,
                                 zoneZ };
        const auto rimB = Vec3 { powerSwitch.position.x + std::cos (b) * powerSwitch.areaRadius,
                                 powerSwitch.position.y + std::sin (b) * powerSwitch.areaRadius,
                                 zoneZ };

        powerZoneVertices.push_back (makeVertex (centre, normal, colour, 0.0f, powerSwitch.flash * 0.35f));
        powerZoneVertices.push_back (makeVertex (rimA, normal, colour, 0.0f, powerSwitch.flash * 0.35f));
        powerZoneVertices.push_back (makeVertex (rimB, normal, colour, 0.0f, powerSwitch.flash * 0.35f));
    }
}

void CityOpenGLRenderer::addGrid (const CityRenderState& state)
{
    IsoProjector view;
    view.zoom = juce::jmax (0.01f, state.zoom);
    view.pan = state.pan;
    view.viewMode = state.viewMode;
    view.centre = { static_cast<float> (state.width) * 0.5f, static_cast<float> (state.height) * 0.5f };

    const auto centreWorld = view.unprojectToGround (view.centre);
    const auto halfSpan = juce::jmax (900.0f, 1300.0f / view.zoom);
    const auto minX = std::floor ((centreWorld.x - halfSpan) / gridStep) * gridStep;
    const auto maxX = std::ceil ((centreWorld.x + halfSpan) / gridStep) * gridStep;
    const auto minY = std::floor ((centreWorld.y - halfSpan) / gridStep) * gridStep;
    const auto maxY = std::ceil ((centreWorld.y + halfSpan) / gridStep) * gridStep;
    const auto minorColour = state.colourWireframeMode ? juce::Colour (0xff1dd7ff).withAlpha (0.12f)
                                                       : juce::Colour (0xff6f837a).withAlpha (0.22f);
    const auto majorColour = state.colourWireframeMode ? juce::Colour (0xff32f0b2).withAlpha (0.22f)
                                                       : juce::Colour (0xff35a996).withAlpha (0.34f);
    const auto normal = Vec3 { 0.0f, 0.0f, 1.0f };

    for (auto x = minX; x <= maxX; x += gridStep)
    {
        const auto cell = juce::roundToInt (x / gridStep);
        const auto colour = (cell % 4) == 0 ? majorColour : minorColour;
        gridVertices.push_back (makeVertex ({ x, minY, -13.5f }, normal, colour, 0.0f, 0.0f));
        gridVertices.push_back (makeVertex ({ x, maxY, -13.5f }, normal, colour, 0.0f, 0.0f));
    }

    for (auto y = minY; y <= maxY; y += gridStep)
    {
        const auto cell = juce::roundToInt (y / gridStep);
        const auto colour = (cell % 4) == 0 ? majorColour : minorColour;
        gridVertices.push_back (makeVertex ({ minX, y, -13.5f }, normal, colour, 0.0f, 0.0f));
        gridVertices.push_back (makeVertex ({ maxX, y, -13.5f }, normal, colour, 0.0f, 0.0f));
    }
}

void CityOpenGLRenderer::addModule (const FoldingModule& module, const CityRenderState& state)
{
    const auto selected = module.id == state.selectedModuleId;
    const auto timeSeconds = module.powered ? state.timeSeconds : 0.0;
    const auto footprint = snappedFootprint ((module.radius + module.flapDepth) * 2.0f);

    addGridLot (module.position, footprint, module.elevation, colourForSides (module.sides), selected, module.powered);

    if (! colourWireframeMode)
        addShadow (module, timeSeconds, state.globalTempoBpm);

    addModuleOutlines (module, timeSeconds, state.globalTempoBpm, selected);
    addModuleTriggerLight (module, selected);
}

void CityOpenGLRenderer::addBlock (const StackBlock& block, const CityRenderState& state)
{
    const juce::ScopedValueSetter<bool> scopedSetter (collectingBlockGeometry, true);
    const auto half = block.halfSize();
    const auto top = block.topElevation();
    const auto selected = block.id == state.selectedBlockId;
    const auto selectedAmount = selected ? 1.0f : 0.0f;
    const auto districtHue = std::fmod (1.0f
                                      + 0.52f
                                      + static_cast<float> (block.levels) * 0.035f
                                      + (block.position.x - block.position.y) * 0.00035f,
                                      1.0f);
    auto topColour = rainbowAt (districtHue, selected ? 0.22f : 0.10f);
    auto sideA = rainbowAt (districtHue + 0.06f, 0.075f);
    auto sideB = rainbowAt (districtHue + 0.13f, 0.065f);

    if (! block.powered)
    {
        topColour = topColour.withAlpha (0.045f);
        sideA = sideA.withAlpha (0.035f);
        sideB = sideB.withAlpha (0.03f);
    }

    const auto outline = selected ? rainbowAt (districtHue + 0.18f, 1.0f)
                                  : rainbowAt (districtHue, block.powered ? 0.86f : 0.32f);

    const auto x0 = block.position.x - half;
    const auto x1 = block.position.x + half;
    const auto y0 = block.position.y - half;
    const auto y1 = block.position.y + half;

    const auto b0 = Vec3 { x0, y0, 0.0f };
    const auto b1 = Vec3 { x1, y0, 0.0f };
    const auto b2 = Vec3 { x1, y1, 0.0f };
    const auto b3 = Vec3 { x0, y1, 0.0f };
    const auto t0 = Vec3 { x0, y0, top };
    const auto t1 = Vec3 { x1, y0, top };
    const auto t2 = Vec3 { x1, y1, top };
    const auto t3 = Vec3 { x0, y1, top };

    if (colourWireframeMode)
    {
        for (const auto& edge : { std::pair { t0, t1 }, std::pair { t1, t2 }, std::pair { t2, t3 }, std::pair { t3, t0 },
                                  std::pair { b0, b1 }, std::pair { b1, b2 }, std::pair { b2, b3 }, std::pair { b3, b0 },
                                  std::pair { b0, t0 }, std::pair { b1, t1 }, std::pair { b2, t2 }, std::pair { b3, t3 } })
            addLine (edge.first, edge.second, outline, selectedAmount, block.flash * 0.32f);

        for (int level = 1; level < block.levels; ++level)
        {
            const auto z = static_cast<float> (level) * block.levelHeight;
            const auto levelColour = outline.withAlpha (block.powered ? 0.32f : 0.16f);
            addLine ({ x0, y0, z }, { x1, y0, z }, levelColour, selectedAmount, block.flash * 0.2f);
            addLine ({ x1, y0, z }, { x1, y1, z }, levelColour, selectedAmount, block.flash * 0.2f);
            addLine ({ x1, y1, z }, { x0, y1, z }, levelColour, selectedAmount, block.flash * 0.2f);
            addLine ({ x0, y1, z }, { x0, y0, z }, levelColour, selectedAmount, block.flash * 0.2f);
        }

        return;
    }

    addFace (t0, t1, t2, topColour, selectedAmount, block.flash, { 0.0f, 0.0f, 1.0f });
    addFace (t0, t2, t3, topColour.darker (0.04f), selectedAmount, block.flash, { 0.0f, 0.0f, 1.0f });

    addFace (b0, b1, t1, sideA, selectedAmount, block.flash);
    addFace (b0, t1, t0, sideA.darker (0.05f), selectedAmount, block.flash);
    addFace (b1, b2, t2, sideB, selectedAmount, block.flash);
    addFace (b1, t2, t1, sideB.darker (0.06f), selectedAmount, block.flash);
    addFace (b2, b3, t3, sideA.darker (0.18f), selectedAmount, block.flash);
    addFace (b2, t3, t2, sideA.darker (0.24f), selectedAmount, block.flash);
    addFace (b3, b0, t0, sideB.brighter (0.08f), selectedAmount, block.flash);
    addFace (b3, t0, t3, sideB, selectedAmount, block.flash);

    for (const auto& edge : { std::pair { t0, t1 }, std::pair { t1, t2 }, std::pair { t2, t3 }, std::pair { t3, t0 },
                              std::pair { b0, t0 }, std::pair { b1, t1 }, std::pair { b2, t2 }, std::pair { b3, t3 } })
        addLine (edge.first, edge.second, outline, selectedAmount, block.flash * 0.3f);

    for (int level = 1; level < block.levels; ++level)
    {
        const auto z = static_cast<float> (level) * block.levelHeight;
        addLine ({ x0, y0, z }, { x1, y0, z }, outline.withAlpha (0.28f), selectedAmount, block.flash * 0.2f);
        addLine ({ x1, y0, z }, { x1, y1, z }, outline.withAlpha (0.28f), selectedAmount, block.flash * 0.2f);
        addLine ({ x1, y1, z }, { x0, y1, z }, outline.withAlpha (0.22f), selectedAmount, block.flash * 0.2f);
        addLine ({ x0, y1, z }, { x0, y0, z }, outline.withAlpha (0.22f), selectedAmount, block.flash * 0.2f);
    }
}

void CityOpenGLRenderer::addPowerSwitch (const PowerSwitch& powerSwitch, const CityRenderState& state)
{
    constexpr auto segments = 12;
    const auto selectedAmount = powerSwitch.id == state.selectedPowerSwitchId ? 1.0f : 0.0f;
    const auto baseZ = powerSwitch.elevation + 3.0f;
    const auto topZ = baseZ + 9.0f + selectedAmount * 4.0f + powerSwitch.flash * 10.0f;
    const auto radius = powerSwitch.triggerRadius * (0.34f + selectedAmount * 0.08f);
    const auto colour = rainbowAt (powerSwitch.powered ? 0.42f : 0.94f,
                                   powerSwitch.powered ? 0.92f : 0.62f)
                            .interpolatedWith (juce::Colours::white, powerSwitch.flash * 0.18f);
    const auto rimColour = rainbowAt (powerSwitch.powered ? 0.42f : 0.94f,
                                      powerSwitch.powered ? 0.82f : 0.48f);
    const auto centre = Vec3 { powerSwitch.position.x, powerSwitch.position.y, topZ };
    const auto normal = Vec3 { 0.0f, 0.0f, 1.0f };

    if (colourWireframeMode)
    {
        addWireCircle (powerSwitch.position,
                       powerSwitch.triggerRadius,
                       baseZ + 2.0f,
                       rimColour.withAlpha (powerSwitch.powered ? 0.74f : 0.34f),
                       selectedAmount,
                       powerSwitch.flash,
                       28);
        addWireCircle (powerSwitch.position,
                       radius,
                       topZ,
                       colour.withAlpha (0.92f),
                       selectedAmount,
                       powerSwitch.flash,
                       segments);
        addLine ({ powerSwitch.position.x - radius, powerSwitch.position.y, topZ },
                 { powerSwitch.position.x + radius, powerSwitch.position.y, topZ },
                 colour.withAlpha (0.84f),
                 selectedAmount,
                 powerSwitch.flash);
        addLine ({ powerSwitch.position.x, powerSwitch.position.y - radius, topZ },
                 { powerSwitch.position.x, powerSwitch.position.y + radius, topZ },
                 colour.withAlpha (0.84f),
                 selectedAmount,
                 powerSwitch.flash);
        return;
    }

    for (int i = 0; i < segments; ++i)
    {
        const auto a = juce::MathConstants<float>::twoPi * static_cast<float> (i) / static_cast<float> (segments);
        const auto b = juce::MathConstants<float>::twoPi * static_cast<float> (i + 1) / static_cast<float> (segments);
        const auto rimA = Vec3 { powerSwitch.position.x + std::cos (a) * radius,
                                 powerSwitch.position.y + std::sin (a) * radius,
                                 baseZ };
        const auto rimB = Vec3 { powerSwitch.position.x + std::cos (b) * radius,
                                 powerSwitch.position.y + std::sin (b) * radius,
                                 baseZ };

        addFace (centre, rimA, rimB, colour, selectedAmount, powerSwitch.flash, normal);
        addLine (rimA, rimB, rimColour.withAlpha (0.72f), selectedAmount, powerSwitch.flash * 0.2f);
    }

    for (int i = 0; i < segments; i += 2)
    {
        const auto a = juce::MathConstants<float>::twoPi * static_cast<float> (i) / static_cast<float> (segments);
        const auto b = a + 0.06f;
        addLine ({ powerSwitch.position.x + std::cos (a) * powerSwitch.triggerRadius,
                   powerSwitch.position.y + std::sin (a) * powerSwitch.triggerRadius,
                   baseZ + 2.0f },
                 { powerSwitch.position.x + std::cos (b) * powerSwitch.triggerRadius,
                   powerSwitch.position.y + std::sin (b) * powerSwitch.triggerRadius,
                   baseZ + 2.0f },
                 colour.withAlpha (0.62f),
                 selectedAmount,
                 powerSwitch.flash);
    }

    if (powerSwitch.flash > 0.02f)
    {
        constexpr auto contactSegments = 32;
        const auto contactAmount = juce::jlimit (0.0f, 1.0f, powerSwitch.flash);
        const auto contactColour = juce::Colour (0xffffffff).withAlpha (0.92f * contactAmount);
        const auto contactHot = juce::Colour (0xffff2fb1).withAlpha (0.96f * contactAmount);
        const auto ringZ = powerSwitch.elevation + 34.0f + contactAmount * 32.0f;
        const auto outerRadius = powerSwitch.triggerRadius * (1.0f + contactAmount * 0.55f);
        const auto innerRadius = powerSwitch.triggerRadius * (0.72f + contactAmount * 0.18f);
        const auto tubeRadius = 7.0f + contactAmount * 7.0f;

        auto addRing = [this] (Vec3 ringCentre, float ringRadius, float tubeRadiusValue, juce::Colour ringColour, float flash)
        {
            for (int i = 0; i < contactSegments; ++i)
            {
                const auto a = juce::MathConstants<float>::twoPi * static_cast<float> (i) / static_cast<float> (contactSegments);
                const auto b = juce::MathConstants<float>::twoPi * static_cast<float> (i + 1) / static_cast<float> (contactSegments);
                addTube ({ ringCentre.x + std::cos (a) * ringRadius, ringCentre.y + std::sin (a) * ringRadius, ringCentre.z },
                         { ringCentre.x + std::cos (b) * ringRadius, ringCentre.y + std::sin (b) * ringRadius, ringCentre.z },
                         ringColour,
                         1.0f,
                         flash,
                         tubeRadiusValue,
                         neonFaces);
            }
        };

        addRing ({ powerSwitch.position.x, powerSwitch.position.y, ringZ }, outerRadius, tubeRadius, contactHot, contactAmount);
        addRing ({ powerSwitch.position.x, powerSwitch.position.y, ringZ + 18.0f }, innerRadius, tubeRadius * 0.72f, contactColour, contactAmount);

        const auto beaconHeight = 70.0f + contactAmount * 90.0f;
        addTube ({ powerSwitch.position.x, powerSwitch.position.y, ringZ },
                 { powerSwitch.position.x, powerSwitch.position.y, ringZ + beaconHeight },
                 contactColour,
                 1.0f,
                 contactAmount,
                 tubeRadius * 0.9f,
                 neonFaces);

        const auto cross = outerRadius * 0.72f;
        addTube ({ powerSwitch.position.x - cross, powerSwitch.position.y, ringZ + 24.0f },
                 { powerSwitch.position.x + cross, powerSwitch.position.y, ringZ + 24.0f },
                 contactHot,
                 1.0f,
                 contactAmount,
                 tubeRadius * 0.68f,
                 neonFaces);
        addTube ({ powerSwitch.position.x, powerSwitch.position.y - cross, ringZ + 24.0f },
                 { powerSwitch.position.x, powerSwitch.position.y + cross, ringZ + 24.0f },
                 contactHot,
                 1.0f,
                 contactAmount,
                 tubeRadius * 0.68f,
                 neonFaces);
    }
}

void CityOpenGLRenderer::addPowerSource (const PowerSource& source, const CityRenderState& state)
{
    constexpr auto segments = 6;
    const auto selectedAmount = source.id == state.selectedPowerSourceId ? 1.0f : 0.0f;
    const auto baseZ = source.elevation + 4.0f;
    const auto topZ = baseZ + 34.0f + source.flash * 8.0f;
    const auto radius = source.radius * (0.58f + selectedAmount * 0.08f);
    const auto topColour = source.powered ? rainbowAt (0.14f, 0.90f) : rainbowAt (0.72f, 0.22f);
    const auto sideA = source.powered ? rainbowAt (0.94f, 0.08f) : rainbowAt (0.72f, 0.035f);
    const auto sideB = source.powered ? rainbowAt (0.54f, 0.08f) : rainbowAt (0.72f, 0.035f);
    const auto normal = Vec3 { 0.0f, 0.0f, 1.0f };
    const auto top = Vec3 { source.position.x, source.position.y, topZ };
    const auto bottom = Vec3 { source.position.x, source.position.y, baseZ };

    if (colourWireframeMode)
    {
        addWireCircle (source.position,
                       radius,
                       baseZ,
                       topColour.withAlpha (source.powered ? 0.82f : 0.32f),
                       selectedAmount,
                       source.flash,
                       segments);
        addWireCircle (source.position,
                       radius * 0.72f,
                       topZ - 8.0f,
                       topColour.withAlpha (source.powered ? 0.92f : 0.38f),
                       selectedAmount,
                       source.flash,
                       segments);
        addLine ({ source.position.x, source.position.y, baseZ },
                 top,
                 topColour.withAlpha (0.90f),
                 selectedAmount,
                 source.flash);
        addLine ({ source.position.x - radius * 0.55f, source.position.y, topZ + 8.0f },
                 { source.position.x + radius * 0.55f, source.position.y, topZ + 8.0f },
                 topColour,
                 selectedAmount,
                 source.flash);
        addLine ({ source.position.x, source.position.y - radius * 0.55f, topZ + 8.0f },
                 { source.position.x, source.position.y + radius * 0.55f, topZ + 8.0f },
                 topColour,
                 selectedAmount,
                 source.flash);
        juce::ignoreUnused (bottom, normal, sideA, sideB);
        return;
    }

    for (int i = 0; i < segments; ++i)
    {
        const auto a = juce::MathConstants<float>::twoPi * static_cast<float> (i) / static_cast<float> (segments)
                     + juce::MathConstants<float>::pi / 6.0f;
        const auto b = juce::MathConstants<float>::twoPi * static_cast<float> (i + 1) / static_cast<float> (segments)
                     + juce::MathConstants<float>::pi / 6.0f;
        const auto lowerA = Vec3 { source.position.x + std::cos (a) * radius,
                                   source.position.y + std::sin (a) * radius,
                                   baseZ };
        const auto lowerB = Vec3 { source.position.x + std::cos (b) * radius,
                                   source.position.y + std::sin (b) * radius,
                                   baseZ };
        const auto upperA = Vec3 { source.position.x + std::cos (a) * radius * 0.72f,
                                   source.position.y + std::sin (a) * radius * 0.72f,
                                   topZ - 8.0f };
        const auto upperB = Vec3 { source.position.x + std::cos (b) * radius * 0.72f,
                                   source.position.y + std::sin (b) * radius * 0.72f,
                                   topZ - 8.0f };

        addFace (top, upperA, upperB, topColour, selectedAmount, source.flash, normal);
        addFace (lowerA, lowerB, upperB, (i % 2) == 0 ? sideA : sideB, selectedAmount, source.flash);
        addFace (lowerA, upperB, upperA, (i % 2) == 0 ? sideA.darker (0.08f) : sideB.darker (0.08f), selectedAmount, source.flash);
        addLine (lowerA, lowerB, topColour.withAlpha (0.86f), selectedAmount, source.flash);
        addLine (upperA, top, topColour.withAlpha (0.78f), selectedAmount, source.flash);
    }

    addLine ({ source.position.x - radius * 0.55f, source.position.y, topZ + 8.0f },
             { source.position.x + radius * 0.55f, source.position.y, topZ + 8.0f },
             topColour, selectedAmount, source.flash);
    addLine ({ source.position.x, source.position.y - radius * 0.55f, topZ + 8.0f },
             { source.position.x, source.position.y + radius * 0.55f, topZ + 8.0f },
             topColour, selectedAmount, source.flash);
    juce::ignoreUnused (bottom);
}

void CityOpenGLRenderer::addPowerFeeds (const CityRenderState& state)
{
    auto sourceForId = [&state] (int id) -> const PowerSource*
    {
        const auto found = std::find_if (state.powerSources.begin(),
                                         state.powerSources.end(),
                                         [id] (const PowerSource& source) { return source.id == id; });
        return found != state.powerSources.end() ? &*found : nullptr;
    };

    auto switchForId = [&state] (int id) -> const PowerSwitch*
    {
        const auto found = std::find_if (state.powerSwitches.begin(),
                                         state.powerSwitches.end(),
                                         [id] (const PowerSwitch& powerSwitch) { return powerSwitch.id == id; });
        return found != state.powerSwitches.end() ? &*found : nullptr;
    };

    for (const auto& cable : state.powerFeedCables)
    {
        const auto* source = sourceForId (cable.sourceId);
        const auto* powerSwitch = switchForId (cable.switchId);

        if (source == nullptr || powerSwitch == nullptr)
            continue;

        const auto sourcePoint = Vec3 { source->position.x, source->position.y, source->elevation + 46.0f };
        const auto switchPoint = Vec3 { powerSwitch->position.x, powerSwitch->position.y, powerSwitch->elevation + 18.0f };
        const auto mid = Vec3 { (sourcePoint.x + switchPoint.x) * 0.5f,
                                (sourcePoint.y + switchPoint.y) * 0.5f,
                                juce::jmax (sourcePoint.z, switchPoint.z) + 28.0f };
        const auto selectedAmount = source->id == state.selectedPowerSourceId
                                 || powerSwitch->id == state.selectedPowerSwitchId ? 1.0f : 0.0f;
        const auto colour = source->powered ? rainbowAt (0.12f, 0.92f)
                                            : rainbowAt (0.94f, 0.42f);

        addNeonLine (sourcePoint, mid, colour, selectedAmount, source->flash * 0.35f);
        addNeonLine (mid, switchPoint, colour, selectedAmount, powerSwitch->flash * 0.35f);
    }
}

void CityOpenGLRenderer::addPowerCables (const CityRenderState& state)
{
    auto switchForId = [&state] (int id) -> const PowerSwitch*
    {
        const auto found = std::find_if (state.powerSwitches.begin(),
                                         state.powerSwitches.end(),
                                         [id] (const PowerSwitch& powerSwitch) { return powerSwitch.id == id; });
        return found != state.powerSwitches.end() ? &*found : nullptr;
    };

    auto targetPoint = [&state] (CityObjectKind kind, int id) -> std::optional<Vec3>
    {
        if (kind == CityObjectKind::module)
        {
            const auto found = std::find_if (state.modules.begin(),
                                             state.modules.end(),
                                             [id] (const FoldingModule& module) { return module.id == id; });
            if (found != state.modules.end())
                return Vec3 { found->position.x, found->position.y, found->elevation + 8.0f };
        }
        else if (kind == CityObjectKind::platter)
        {
            const auto found = std::find_if (state.platters.begin(),
                                             state.platters.end(),
                                             [id] (const FairgroundPlatter& platter) { return platter.id == id; });
            if (found != state.platters.end())
                return Vec3 { found->position.x, found->position.y, found->elevation + 10.0f };
        }
        else if (kind == CityObjectKind::block)
        {
            const auto found = std::find_if (state.blocks.begin(),
                                             state.blocks.end(),
                                             [id] (const StackBlock& block) { return block.id == id; });
            if (found != state.blocks.end())
                return Vec3 { found->position.x, found->position.y, found->topElevation() + 8.0f };
        }
        else if (kind == CityObjectKind::plank)
        {
            const auto found = std::find_if (state.planks.begin(),
                                             state.planks.end(),
                                             [id] (const Plank& plank) { return plank.id == id; });
            if (found != state.planks.end())
                return Vec3 { found->position.x, found->position.y, found->elevation + 4.0f };
        }

        return std::nullopt;
    };

    for (const auto& cable : state.powerCables)
    {
        const auto* powerSwitch = switchForId (cable.switchId);
        const auto target = targetPoint (cable.targetKind, cable.targetId);

        if (powerSwitch == nullptr || ! target.has_value())
            continue;

        const auto source = Vec3 { powerSwitch->position.x, powerSwitch->position.y, powerSwitch->elevation + 18.0f };
        const auto mid = Vec3 { (source.x + target->x) * 0.5f,
                                (source.y + target->y) * 0.5f,
                                juce::jmax (source.z, target->z) + 18.0f };
        const auto selectedAmount = powerSwitch->id == state.selectedPowerSwitchId ? 1.0f : 0.0f;
        const auto colour = powerSwitch->powered ? rainbowAt (0.44f, 0.78f)
                                                 : rainbowAt (0.94f, 0.42f);
        const auto flash = powerSwitch->flash * 0.28f;

        addLine (source, mid, colour, selectedAmount, flash);
        addLine (mid, *target, colour, selectedAmount, flash);

        if (powerSwitch->pulse > 0.0f)
        {
            const auto progress = smoothStep01 (1.0f - powerSwitch->pulse);
            const auto pulseColour = powerSwitch->powered ? rainbowAt (0.16f, 0.95f)
                                                          : rainbowAt (0.94f, 0.75f);
            addCablePulse (source, mid, *target, progress, pulseColour, powerSwitch->pulse);
        }
    }
}

void CityOpenGLRenderer::addCablePulse (Vec3 a, Vec3 b, Vec3 c, float progress, juce::Colour colour, float intensity)
{
    auto pointOn = [] (Vec3 start, Vec3 end, float t)
    {
        return Vec3 { start.x + (end.x - start.x) * t,
                      start.y + (end.y - start.y) * t,
                      start.z + (end.z - start.z) * t };
    };

    const auto clamped = juce::jlimit (0.0f, 1.0f, progress);
    const auto tail = juce::jmax (0.0f, clamped - 0.10f);
    const auto head = juce::jmin (1.0f, clamped + 0.08f);

    auto pointOnPath = [&] (float t)
    {
        return t < 0.5f ? pointOn (a, b, t * 2.0f)
                        : pointOn (b, c, (t - 0.5f) * 2.0f);
    };

    addNeonLine (pointOnPath (tail),
                 pointOnPath (head),
                 colour.withAlpha (0.92f),
                 1.0f,
                 juce::jlimit (0.0f, 1.0f, intensity));
}

void CityOpenGLRenderer::addGridLot (Vec2 centre, float footprint, float elevation, juce::Colour colour, bool selected, bool powered)
{
    const auto half = footprint * 0.5f;
    const auto z = elevation - 14.0f;
    const auto selectedAmount = selected ? 1.0f : 0.0f;
    const auto fill = (powered ? colour : juce::Colour (0xff8b948f)).withAlpha (selected ? 0.075f : 0.025f);
    const auto outline = (selected ? rainbowAt (0.15f, 0.94f) : colour)
                             .withAlpha (selected ? 0.72f : 0.24f);

    const auto a = Vec3 { centre.x - half, centre.y - half, z };
    const auto b = Vec3 { centre.x + half, centre.y - half, z };
    const auto c = Vec3 { centre.x + half, centre.y + half, z };
    const auto d = Vec3 { centre.x - half, centre.y + half, z };
    const auto normal = Vec3 { 0.0f, 0.0f, 1.0f };

    if (! colourWireframeMode)
    {
        addFace (a, b, c, fill, selectedAmount, 0.0f, normal);
        addFace (a, c, d, fill, selectedAmount, 0.0f, normal);
    }

    addLine (a, b, outline, selectedAmount, 0.0f);
    addLine (b, c, outline, selectedAmount, 0.0f);
    addLine (c, d, outline, selectedAmount, 0.0f);
    addLine (d, a, outline, selectedAmount, 0.0f);
}

void CityOpenGLRenderer::addWireCircle (Vec2 centre,
                                        float radius,
                                        float elevation,
                                        juce::Colour colour,
                                        float selected,
                                        float flash,
                                        int segments)
{
    const auto clampedSegments = juce::jlimit (6, 64, segments);

    for (int i = 0; i < clampedSegments; ++i)
    {
        const auto a = juce::MathConstants<float>::twoPi * static_cast<float> (i) / static_cast<float> (clampedSegments);
        const auto b = juce::MathConstants<float>::twoPi * static_cast<float> (i + 1) / static_cast<float> (clampedSegments);
        addLine ({ centre.x + std::cos (a) * radius, centre.y + std::sin (a) * radius, elevation },
                 { centre.x + std::cos (b) * radius, centre.y + std::sin (b) * radius, elevation },
                 colour,
                 selected,
                 flash);
    }
}

void CityOpenGLRenderer::addTipTriggerCues (const CityRenderState& state)
{
    for (const auto& cue : state.tipTriggerCues)
    {
        const auto lifeSeconds = cue.contactOnly ? (cue.tipTip ? 0.20f : 0.16f)
                                                 : (cue.tipTip ? 0.26f : 0.18f);
        const auto t = juce::jlimit (0.0f, 1.0f, cue.age / lifeSeconds);
        const auto alpha = std::pow (1.0f - smoothStep01 (t), 2.05f);
        const auto baseColour = cue.tipTip ? rainbowAt (cue.muted ? 0.74f : 0.54f, cue.muted ? 0.48f : 0.92f)
                                           : colourForSides (cue.sides);
        const auto colour = (cue.muted ? juce::Colour (0xffd8dde7) : baseColour).withAlpha ((cue.muted ? 0.72f : 0.96f) * alpha);
        const auto hotBase = cue.tipTip ? juce::Colour (0xffffffff)
                                        : (cue.contactOnly ? juce::Colour (0xffffffff)
                                                           : juce::Colour (0xffff4fd2));
        const auto hot = (cue.muted ? juce::Colour (0xff9aa3b5) : hotBase).withAlpha ((cue.muted ? 0.68f : 0.98f) * alpha);
        const auto lift = (cue.contactOnly ? (cue.tipTip ? 5.0f : 2.0f) : 3.0f)
                        + (cue.contactOnly ? (cue.tipTip ? 6.0f : 2.0f) : (cue.tipTip ? 8.0f : 5.0f)) * (1.0f - t);
        const auto tip = Vec3 { cue.tip.x, cue.tip.y, cue.tip.z + lift };
        const auto targetLift = cue.tipTip ? lift : lift * 0.35f;
        const auto target = Vec3 { cue.target.x, cue.target.y, cue.target.z + targetLift };
        const auto mid = Vec3 { (tip.x + target.x) * 0.5f,
                                (tip.y + target.y) * 0.5f,
                                juce::jmax (tip.z, target.z) + 18.0f * alpha };

        auto addThickSegment = [this] (Vec3 a, Vec3 b, juce::Colour segmentColour, float thickness, float flash)
        {
            if (colourWireframeMode)
            {
                addLine (a, b, segmentColour.withAlpha (juce::jlimit (0.18f, 0.98f, segmentColour.getFloatAlpha())), 1.0f, flash);
                juce::ignoreUnused (thickness);
                return;
            }

            addTube (a, b, segmentColour, 1.0f, flash, thickness, neonFaces);
        };

        auto addDiamond = [&addThickSegment] (Vec3 centre, float size, juce::Colour diamondColour, float thickness, float flash)
        {
            const auto north = Vec3 { centre.x, centre.y - size, centre.z };
            const auto east  = Vec3 { centre.x + size, centre.y, centre.z };
            const auto south = Vec3 { centre.x, centre.y + size, centre.z };
            const auto west  = Vec3 { centre.x - size, centre.y, centre.z };

            addThickSegment (north, east, diamondColour, thickness, flash);
            addThickSegment (east, south, diamondColour, thickness, flash);
            addThickSegment (south, west, diamondColour, thickness, flash);
            addThickSegment (west, north, diamondColour, thickness, flash);
        };

        auto addBracket = [&addThickSegment] (Vec3 centre,
                                              float nx,
                                              float ny,
                                              float px,
                                              float py,
                                              float size,
                                              juce::Colour bracketColour,
                                              float thickness,
                                              float flash)
        {
            const auto half = size * 0.5f;
            const auto arm = size * 0.34f;
            addThickSegment ({ centre.x + px * half - nx * arm, centre.y + py * half - ny * arm, centre.z },
                             { centre.x + px * half + nx * arm, centre.y + py * half + ny * arm, centre.z },
                             bracketColour,
                             thickness,
                             flash);
            addThickSegment ({ centre.x - px * half - nx * arm, centre.y - py * half - ny * arm, centre.z },
                             { centre.x - px * half + nx * arm, centre.y - py * half + ny * arm, centre.z },
                             bracketColour,
                             thickness,
                             flash);
        };

        if (cue.contactOnly)
        {
            const auto tube = cue.tipTip ? 4.5f : 3.6f;

            if (cue.tipTip)
            {
                const auto mainColour = (cue.muted ? juce::Colour (0xffb9c0d0) : juce::Colour (0xff39f4ff)).withAlpha (0.92f * alpha);
                const auto accentColour = (cue.muted ? juce::Colour (0xff7f8798) : juce::Colour (0xffff4fd2)).withAlpha (0.86f * alpha);
                const auto white = juce::Colour (0xffffffff).withAlpha (0.82f * alpha);
                const auto beat = 0.5f + 0.5f * std::sin (static_cast<float> (state.timeSeconds) * 18.0f
                                                        + static_cast<float> (cue.sides) * 0.7f);
                const auto reticleSize = 14.0f + beat * 6.0f;
                const auto dx = target.x - tip.x;
                const auto dy = target.y - tip.y;
                const auto len = std::sqrt (dx * dx + dy * dy);
                const auto nx = len > 0.001f ? dx / len : 1.0f;
                const auto ny = len > 0.001f ? dy / len : 0.0f;
                const auto px = -ny;
                const auto py = nx;
                const auto pulse = std::fmod (static_cast<float> (state.timeSeconds) * 3.2f
                                            + static_cast<float> (cue.sides) * 0.11f,
                                            1.0f);
                const auto signal = Vec3 { tip.x + dx * pulse, tip.y + dy * pulse, tip.z + (target.z - tip.z) * pulse };

                addThickSegment (tip, target, mainColour.withAlpha (0.42f * alpha), tube * 0.86f, alpha * 0.6f);
                addThickSegment (tip, target, white.withAlpha (0.38f * alpha), 2.2f, alpha * 0.5f);

                for (int i = 0; i < 3; ++i)
                {
                    const auto startT = std::fmod (pulse + static_cast<float> (i) * 0.33f, 1.0f);
                    const auto endT = juce::jmin (1.0f, startT + 0.11f);
                    const auto a = Vec3 { tip.x + dx * startT, tip.y + dy * startT, tip.z + (target.z - tip.z) * startT };
                    const auto b = Vec3 { tip.x + dx * endT, tip.y + dy * endT, tip.z + (target.z - tip.z) * endT };
                    addThickSegment (a, b, accentColour.withAlpha ((0.52f - static_cast<float> (i) * 0.12f) * alpha), 3.6f, alpha);
                }

                addDiamond (tip, reticleSize, mainColour, 3.2f, alpha);
                addDiamond (target, reticleSize, accentColour, 3.2f, alpha);

                const auto bracket = reticleSize + 10.0f;
                const auto bracketLength = 14.0f;
                addThickSegment ({ tip.x + px * bracket - nx * bracketLength, tip.y + py * bracket - ny * bracketLength, tip.z },
                                 { tip.x + px * bracket + nx * bracketLength, tip.y + py * bracket + ny * bracketLength, tip.z },
                                 white, 2.6f, alpha);
                addThickSegment ({ target.x - px * bracket - nx * bracketLength, target.y - py * bracket - ny * bracketLength, target.z },
                                 { target.x - px * bracket + nx * bracketLength, target.y - py * bracket + ny * bracketLength, target.z },
                                 white, 2.6f, alpha);

                addDiamond (signal, 7.0f + beat * 3.0f, white, 3.0f, alpha);

                if (cue.muted)
                {
                    const auto slash = 20.0f;
                    addThickSegment ({ mid.x - slash, mid.y - slash, mid.z },
                                     { mid.x + slash, mid.y + slash, mid.z },
                                     juce::Colour (0xffd8dde7).withAlpha (0.8f * alpha),
                                     4.5f,
                                     alpha);
                }

                continue;
            }

            const auto dx = target.x - tip.x;
            const auto dy = target.y - tip.y;
            const auto len = std::sqrt (dx * dx + dy * dy);
            const auto nx = len > 0.001f ? dx / len : 1.0f;
            const auto ny = len > 0.001f ? dy / len : 0.0f;
            const auto px = -ny;
            const auto py = nx;
            const auto start = Vec3 { tip.x + dx * 0.12f, tip.y + dy * 0.12f, tip.z + (target.z - tip.z) * 0.12f };
            const auto endT = 0.72f + 0.24f * (1.0f - t);
            const auto end = Vec3 { tip.x + dx * endT, tip.y + dy * endT, tip.z + (target.z - tip.z) * endT };

            addThickSegment (start, end, colour.withAlpha (0.76f * alpha), tube, alpha);
            addThickSegment ({ start.x + px * 5.0f, start.y + py * 5.0f, start.z },
                             { end.x + px * 5.0f, end.y + py * 5.0f, end.z },
                             hot.withAlpha (0.7f * alpha),
                             1.8f,
                             alpha);
            addDiamond (tip, 9.0f, hot.withAlpha (0.9f * alpha), 2.4f, alpha);
            addBracket (target, nx, ny, px, py, 22.0f, colour.withAlpha (0.88f * alpha), 2.6f, alpha);

            continue;
        }

        if (cue.tipTip)
        {
            addThickSegment (tip, target, colour.withAlpha (0.84f * alpha), 6.0f, alpha);
            continue;
        }

        if (! cue.muted)
        {
            const auto dx = target.x - tip.x;
            const auto dy = target.y - tip.y;
            const auto dz = target.z - tip.z;
            const auto len = std::sqrt (dx * dx + dy * dy);
            const auto nx = len > 0.001f ? dx / len : 1.0f;
            const auto ny = len > 0.001f ? dy / len : 0.0f;
            const auto px = -ny;
            const auto py = nx;
            const auto start = Vec3 { tip.x + dx * 0.08f, tip.y + dy * 0.08f, tip.z + dz * 0.08f };
            const auto endT = juce::jmin (1.0f, 0.52f + t * 0.86f);
            const auto traceEnd = Vec3 { tip.x + dx * endT, tip.y + dy * endT, tip.z + dz * endT };
            addThickSegment ({ start.x - px * 4.5f, start.y - py * 4.5f, start.z },
                             { traceEnd.x - px * 4.5f, traceEnd.y - py * 4.5f, traceEnd.z },
                             colour.withAlpha (0.92f * alpha),
                             4.2f,
                             alpha);
            addThickSegment ({ start.x + px * 4.5f, start.y + py * 4.5f, start.z },
                             { traceEnd.x + px * 4.5f, traceEnd.y + py * 4.5f, traceEnd.z },
                             hot.withAlpha (0.84f * alpha),
                             1.8f,
                             alpha);
            addBracket (target, nx, ny, px, py, 24.0f, hot.withAlpha (0.94f * alpha), 3.0f, alpha);
            addDiamond (target, 7.0f, colour.withAlpha (0.9f * alpha), 2.0f, alpha);
        }

        const auto markSize = cue.muted ? 10.0f : 12.0f;
        addDiamond (tip, markSize, colour.withAlpha (0.92f * alpha), cue.muted ? 2.8f : 3.6f, alpha);

        for (int i = 0; i < 4; ++i)
        {
            const auto a = juce::MathConstants<float>::twoPi * static_cast<float> (i) / 4.0f;
            const auto inner = markSize * 0.55f;
            const auto outer = markSize * (1.15f + 0.25f * alpha);
            addThickSegment ({ tip.x + std::cos (a) * inner, tip.y + std::sin (a) * inner, tip.z },
                             { tip.x + std::cos (a) * outer, tip.y + std::sin (a) * outer, tip.z },
                             colour.withAlpha (0.72f * alpha),
                             cue.muted ? 2.2f : 2.8f,
                             alpha);
        }

        if (cue.muted)
        {
            const auto xRadius = 12.0f + 4.0f * alpha;
            addThickSegment ({ tip.x - xRadius, tip.y - xRadius, tip.z },
                             { tip.x + xRadius, tip.y + xRadius, tip.z },
                             hot.withAlpha (0.96f * alpha),
                             3.2f,
                             alpha);
            addThickSegment ({ tip.x + xRadius, tip.y - xRadius, tip.z },
                             { tip.x - xRadius, tip.y + xRadius, tip.z },
                             hot.withAlpha (0.96f * alpha),
                             3.2f,
                             alpha);
            continue;
        }
    }
}

void CityOpenGLRenderer::addSelectionHandles (const CityRenderState& state)
{
    if (state.selectedModuleId >= 0)
    {
        const auto found = std::find_if (state.modules.begin(),
                                         state.modules.end(),
                                         [&state] (const FoldingModule& module) { return module.id == state.selectedModuleId; });
        if (found != state.modules.end())
            addHandleRing (found->position,
                           found->radius + found->flapDepth,
                           found->elevation + 16.0f,
                           found->phase,
                           juce::Colour (0xfffff1a8));
    }

    if (state.selectedPlatterId >= 0)
    {
        const auto found = std::find_if (state.platters.begin(),
                                         state.platters.end(),
                                         [&state] (const FairgroundPlatter& platter) { return platter.id == state.selectedPlatterId; });
        if (found != state.platters.end())
            addHandleRing (found->position,
                           found->diameter * 0.5f,
                           found->elevation + 18.0f,
                           found->phase,
                           juce::Colour (0xff75d7ff));
    }
}

void CityOpenGLRenderer::addHandleRing (Vec2 centre, float radius, float elevation, float phase, juce::Colour colour)
{
    constexpr auto segments = 40;
    const auto ringRadius = juce::jmax (18.0f, radius);
    const auto z = elevation;
    const auto ringColour = colour.withAlpha (0.72f);

    for (int i = 0; i < segments; ++i)
    {
        const auto a = juce::MathConstants<float>::twoPi * static_cast<float> (i) / static_cast<float> (segments);
        const auto b = juce::MathConstants<float>::twoPi * static_cast<float> (i + 1) / static_cast<float> (segments);
        addLine ({ centre.x + std::cos (a) * ringRadius,
                   centre.y + std::sin (a) * ringRadius,
                   z },
                 { centre.x + std::cos (b) * ringRadius,
                   centre.y + std::sin (b) * ringRadius,
                   z },
                 ringColour,
                 1.0f,
                 0.08f);
    }

    const auto phasePoint = Vec3 { centre.x + std::cos (phase) * ringRadius,
                                   centre.y + std::sin (phase) * ringRadius,
                                   z + 12.0f };
    const auto dotRadius = juce::jlimit (10.0f, 34.0f, ringRadius * 0.045f);

    for (int i = 0; i < 10; ++i)
    {
        const auto a = juce::MathConstants<float>::twoPi * static_cast<float> (i) / 10.0f;
        const auto b = juce::MathConstants<float>::twoPi * static_cast<float> (i + 1) / 10.0f;
        addNeonLine ({ phasePoint.x + std::cos (a) * dotRadius,
                       phasePoint.y + std::sin (a) * dotRadius,
                       phasePoint.z },
                     { phasePoint.x + std::cos (b) * dotRadius,
                       phasePoint.y + std::sin (b) * dotRadius,
                       phasePoint.z },
                     juce::Colour (0xffff6f91),
                     1.0f,
                     0.55f);
    }

    addLine ({ centre.x, centre.y, z },
             phasePoint,
             juce::Colour (0xffff6f91).withAlpha (0.55f),
             1.0f,
             0.1f);
}

void CityOpenGLRenderer::addPlatter (const FairgroundPlatter& platter, const CityRenderState& state)
{
    const auto selected = platter.id == state.selectedPlatterId;
    auto platterState = state;

    if (! platter.powered)
        platterState.timeSeconds = 0.0;

    addGridLot (platter.position,
                snappedFootprint (platter.diameter),
                platter.elevation,
                juce::Colour (0xff3d8fa7),
                selected,
                platter.powered);
    addPlatterDisc (platter, platterState, selected);
    addPlatterStands (platter, platterState, selected);
}

void CityOpenGLRenderer::addPlatterDisc (const FairgroundPlatter& platter,
                                         const CityRenderState& state,
                                         bool selected)
{
    constexpr auto segments = 28;
    const auto platterZ = platter.elevation + 2.0f;
    const auto centre = Vec3 { platter.position.x, platter.position.y, platterZ };
    const auto rotation = platter.rotation + platter.rotationAt (state.timeSeconds, state.globalTempoBpm);
    const auto radius = platter.diameter * 0.5f;
    const auto selectedAmount = selected ? 1.0f : 0.0f;
    for (int i = 0; i < segments; ++i)
    {
        const auto a = rotation + juce::MathConstants<float>::twoPi * static_cast<float> (i) / static_cast<float> (segments);
        const auto b = rotation + juce::MathConstants<float>::twoPi * static_cast<float> (i + 1) / static_cast<float> (segments);
        const auto rimA = Vec3 { platter.position.x + std::cos (a) * radius,
                                 platter.position.y + std::sin (a) * radius,
                                 platterZ };
        const auto rimB = Vec3 { platter.position.x + std::cos (b) * radius,
                                 platter.position.y + std::sin (b) * radius,
                                 platterZ };
        const auto innerA = Vec3 { platter.position.x + std::cos (a) * radius * 0.74f,
                                   platter.position.y + std::sin (a) * radius * 0.74f,
                                   platterZ + 4.0f };
        const auto innerB = Vec3 { platter.position.x + std::cos (b) * radius * 0.74f,
                                   platter.position.y + std::sin (b) * radius * 0.74f,
                                   platterZ + 4.0f };
        const auto colour = rainbowAt (static_cast<float> (i) / static_cast<float> (segments),
                                       platter.powered ? 0.90f : 0.24f);

        addNeonLine (rimA, rimB, colour, selectedAmount, platter.flash * 0.7f);
        addNeonLine (innerA, innerB, rainbowAt (static_cast<float> (i + 6) / static_cast<float> (segments),
                                                platter.powered ? 0.72f : 0.18f), selectedAmount, platter.flash * 0.55f);
    }

    for (int i = 0; i < platter.stands; ++i)
    {
        const auto stand = platter.standPosition (i, state.timeSeconds, state.globalTempoBpm);
        addNeonLine (centre,
                     { stand.x, stand.y, platterZ + 7.0f },
                     rainbowAt (static_cast<float> (i) / static_cast<float> (juce::jmax (1, platter.stands)),
                                platter.powered ? 0.82f : 0.20f),
                     selectedAmount,
                     platter.flash);
    }
}

void CityOpenGLRenderer::addPlatterStands (const FairgroundPlatter& platter,
                                           const CityRenderState& state,
                                           bool selected)
{
    const auto selectedAmount = selected ? 1.0f : 0.0f;
    const auto standColour = selected ? rainbowAt (0.15f) : rainbowAt (0.55f, platter.powered ? 0.88f : 0.24f);
    const auto standTopColour = rainbowAt (0.94f, platter.powered ? 0.82f : 0.22f);
    const auto hub = Vec3 { platter.position.x, platter.position.y, platter.elevation + 8.0f };
    const auto hubRadius = platter.mountRadius();

    for (int i = 0; i < platter.stands; ++i)
    {
        const auto stand = platter.standPosition (i, state.timeSeconds, state.globalTempoBpm);
        const auto standTop = Vec3 { stand.x, stand.y, platter.elevation + 10.0f };
        addNeonLine (hub, standTop, standColour, selectedAmount, platter.standFlashes[static_cast<size_t> (i)]);

        for (int j = 0; j < 5; ++j)
        {
            const auto a = juce::MathConstants<float>::twoPi * static_cast<float> (j) / 5.0f;
            const auto b = juce::MathConstants<float>::twoPi * static_cast<float> (j + 1) / 5.0f;
            addNeonLine ({ stand.x + std::cos (a) * hubRadius, stand.y + std::sin (a) * hubRadius, platter.elevation + 7.0f },
                         { stand.x + std::cos (b) * hubRadius, stand.y + std::sin (b) * hubRadius, platter.elevation + 7.0f },
                         standTopColour,
                         selectedAmount,
                         platter.standFlashes[static_cast<size_t> (i)]);
        }
    }
}

void CityOpenGLRenderer::addPlank (const Plank& plank, const CityRenderState& state)
{
    const auto selectedAmount = plank.id == state.selectedPlankId ? 1.0f : 0.0f;
    const auto socket = plank.socketPosition();
    const auto start = Vec3 { plank.position.x, plank.position.y, plank.elevation };
    const auto end = Vec3 { socket.x, socket.y, plank.elevation };
    const auto colour = (plank.powered ? juce::Colour (0xffff9f6e) : juce::Colour (0xff8b948f))
                            .withAlpha (plank.powered ? 0.92f : 0.34f);
    const auto accent = (selectedAmount > 0.0f ? juce::Colour (0xfffff1a8) : juce::Colour (0xff75d7ff))
                            .withAlpha (plank.powered ? 0.86f : 0.28f);
    const auto flash = plank.powered ? plank.flash : 0.0f;

    if (colourWireframeMode)
    {
        addLine (start, end, colour, selectedAmount, flash);
        addWireCircle (socket, plank.socketRadius(), plank.elevation + 4.0f, accent, selectedAmount, flash, 14);
        return;
    }

    addTube (start, end, colour, selectedAmount, flash, 5.5f + selectedAmount * 1.5f, neonFaces);

    const auto socketRadius = plank.socketRadius();
    constexpr auto segments = 16;
    for (int i = 0; i < segments; ++i)
    {
        const auto a = juce::MathConstants<float>::twoPi * static_cast<float> (i) / static_cast<float> (segments);
        const auto b = juce::MathConstants<float>::twoPi * static_cast<float> (i + 1) / static_cast<float> (segments);
        addTube ({ socket.x + std::cos (a) * socketRadius, socket.y + std::sin (a) * socketRadius, plank.elevation + 4.0f },
                 { socket.x + std::cos (b) * socketRadius, socket.y + std::sin (b) * socketRadius, plank.elevation + 4.0f },
                 accent,
                 selectedAmount,
                 flash,
                 3.8f,
                 neonFaces);
    }
}

void CityOpenGLRenderer::addShadow (const FoldingModule& module, double timeSeconds, float globalTempoBpm)
{
    auto vertices = module.floorVertices();
    const auto fold = module.foldAt (timeSeconds, globalTempoBpm);
    const auto shadowInflate = module.flapDepth * (0.28f + (1.0f - fold) * 0.48f);

    for (auto& vertex : vertices)
    {
        const auto fromCentre = ::normalised (Vec2 { vertex.x - module.position.x, vertex.y - module.position.y });
        vertex.x += fromCentre.x * shadowInflate;
        vertex.y += fromCentre.y * shadowInflate;
        vertex.z = module.elevation - 9.0f;
    }

    const auto centre = Vec3 { module.position.x, module.position.y, module.elevation - 9.0f };
    const auto colour = colourForSides (module.sides).withAlpha (module.powered ? 0.045f : 0.018f);

    for (size_t i = 0; i < vertices.size(); ++i)
        addFace (centre,
                 vertices[i],
                 vertices[(i + 1) % vertices.size()],
                 colour,
                 0.0f,
                 0.0f,
                 { 0.0f, 0.0f, 1.0f });
}

void CityOpenGLRenderer::addFloor (const FoldingModule& module, bool selected)
{
    const auto vertices = module.floorVertices();
    const auto centre = Vec3 { module.position.x, module.position.y, module.elevation };
    auto colour = colourForSides (module.sides).withAlpha (0.12f);
    const auto selectedAmount = selected ? 1.0f : 0.0f;
    const auto flash = module.powered ? module.flash : 0.0f;

    if (! module.powered)
        colour = colour.withAlpha (0.035f);

    for (size_t i = 0; i < vertices.size(); ++i)
        addFace (centre,
                 vertices[i],
                 vertices[(i + 1) % vertices.size()],
                 colour,
                 selectedAmount,
                 flash,
                 { 0.0f, 0.0f, 1.0f });
}

void CityOpenGLRenderer::addFlaps (const FoldingModule& module,
                                   double timeSeconds,
                                   float globalTempoBpm,
                                   bool selected)
{
    const auto triangles = module.flapTriangles (timeSeconds, globalTempoBpm);
    const auto fold = module.foldAt (timeSeconds, globalTempoBpm);
    auto base = colourForSides (module.sides);
    const auto selectedAmount = selected ? 1.0f : 0.0f;
    const auto flash = module.powered ? module.flash : 0.0f;

    if (! module.powered)
        base = base.withAlpha (0.22f);

    for (size_t i = 0; i < triangles.size(); ++i)
    {
        const auto shade = 0.08f + 0.12f * static_cast<float> (i % 3);
        const auto colour = base.interpolatedWith (juce::Colours::white, 0.18f + fold * 0.22f)
                                .darker (shade);

        addFace (triangles[i][0], triangles[i][1], triangles[i][2], colour, selectedAmount, flash);
    }
}

void CityOpenGLRenderer::addModuleOutlines (const FoldingModule& module,
                                            double timeSeconds,
                                            float globalTempoBpm,
                                            bool selected)
{
    const auto floor = module.floorVertices();
    const auto flaps = module.flapTriangles (timeSeconds, globalTempoBpm);
    const auto base = colourForSides (module.sides);
    const auto outline = selected ? rainbowAt (0.15f, 1.0f)
                                  : base.withAlpha (module.powered ? 0.96f : 0.26f);
    const auto brace = base.withAlpha (module.powered ? 0.62f : 0.16f);
    const auto selectedAmount = selected ? 1.0f : 0.0f;
    const auto flash = module.powered ? juce::jmax (module.flash * 0.32f, module.foldAt (timeSeconds, globalTempoBpm) * 0.035f) : 0.0f;
    const auto centre = Vec3 { module.position.x, module.position.y, module.elevation + 1.5f };

    for (size_t i = 0; i < floor.size(); ++i)
    {
        addNeonLine (floor[i], floor[(i + 1) % floor.size()], outline, selectedAmount, flash);

        if ((i % 2) == 0 || selected)
            addNeonLine (centre, floor[i], brace, selectedAmount, flash * 0.65f);
    }

    for (const auto& flap : flaps)
    {
        addNeonLine (flap[0], flap[2], outline, selectedAmount, flash);
        addNeonLine (flap[1], flap[2], outline, selectedAmount, flash);
    }
}

void CityOpenGLRenderer::addModuleTriggerLight (const FoldingModule& module, bool selected)
{
    constexpr auto segments = 18;
    const auto selectedAmount = selected ? 1.0f : 0.0f;
    const auto noteFlash = module.powered ? module.soundingPitchFlash : 0.0f;
    const auto flash = module.powered ? juce::jmax (module.flash, noteFlash) : 0.0f;
    const auto z = module.elevation + 7.0f + flash * 9.0f;
    const auto radius = juce::jlimit (7.0f, 24.0f, module.radius * 0.12f);
    const auto litRadius = radius * (1.0f + flash * 0.55f + selectedAmount * 0.18f);
    const auto centre = Vec3 { module.position.x, module.position.y, z };
    const auto normal = Vec3 { 0.0f, 0.0f, 1.0f };
    const auto pitchHue = module.soundingPitch > 0.0f
        ? std::fmod ((module.soundingPitch - 36.0f) / 48.0f + 1.0f, 1.0f)
        : 0.0f;
    const auto base = noteFlash > 0.02f && module.soundingPitch > 0.0f
        ? rainbowAt (pitchHue, 0.98f)
        : colourForSides (module.sides);
    const auto fill = (module.powered ? base.interpolatedWith (juce::Colours::white, 0.38f + flash * 0.36f)
                                      : juce::Colour (0xff8b948f))
                          .withAlpha (module.powered ? 0.34f + flash * 0.58f : 0.14f);
    const auto rim = (flash > 0.02f ? juce::Colour (0xffffffff) : base)
                         .withAlpha (module.powered ? 0.62f + flash * 0.34f : 0.24f);

    if (colourWireframeMode)
    {
        addWireCircle (module.position, litRadius, z, rim, selectedAmount, flash, segments);

        if (flash > 0.02f)
            addWireCircle (module.position,
                           litRadius * (1.45f + flash * 0.45f),
                           z + 2.0f,
                           base.interpolatedWith (juce::Colours::white, 0.42f).withAlpha (0.42f + flash * 0.28f),
                           selectedAmount,
                           flash,
                           segments);

        juce::ignoreUnused (centre, normal, fill);
        return;
    }

    for (int i = 0; i < segments; ++i)
    {
        const auto a = juce::MathConstants<float>::twoPi * static_cast<float> (i) / static_cast<float> (segments);
        const auto b = juce::MathConstants<float>::twoPi * static_cast<float> (i + 1) / static_cast<float> (segments);
        const auto rimA = Vec3 { module.position.x + std::cos (a) * litRadius,
                                 module.position.y + std::sin (a) * litRadius,
                                 z };
        const auto rimB = Vec3 { module.position.x + std::cos (b) * litRadius,
                                 module.position.y + std::sin (b) * litRadius,
                                 z };

        addFace (centre, rimA, rimB, fill, selectedAmount, flash, normal);
        addTube (rimA, rimB, rim, selectedAmount, flash, 1.9f + flash * 2.6f, neonFaces);
    }

    if (flash > 0.02f)
    {
        const auto haloRadius = litRadius * (1.55f + flash * 0.6f);
        const auto haloColour = base.interpolatedWith (juce::Colours::white, 0.42f)
                                    .withAlpha (0.18f + flash * 0.38f);

        for (int i = 0; i < segments; ++i)
        {
            const auto a = juce::MathConstants<float>::twoPi * static_cast<float> (i) / static_cast<float> (segments);
            const auto b = juce::MathConstants<float>::twoPi * static_cast<float> (i + 1) / static_cast<float> (segments);
            addTube ({ module.position.x + std::cos (a) * haloRadius,
                       module.position.y + std::sin (a) * haloRadius,
                       z + 2.0f },
                     { module.position.x + std::cos (b) * haloRadius,
                       module.position.y + std::sin (b) * haloRadius,
                       z + 2.0f },
                     haloColour,
                     selectedAmount,
                     flash,
                     2.4f + flash * 2.0f,
                     neonFaces);
        }
    }
}

void CityOpenGLRenderer::addFace (Vec3 a,
                                  Vec3 b,
                                  Vec3 c,
                                  juce::Colour colour,
                                  float selected,
                                  float flash,
                                  Vec3 normal)
{
    if (normal.x == 0.0f && normal.y == 0.0f && normal.z == 0.0f)
        normal = normalForTriangle (a, b, c);

    auto& destination = collectingBlockGeometry ? blockFaces : faces;
    destination.push_back ({ { makeVertex (a, normal, colour, selected, flash),
                               makeVertex (b, normal, colour, selected, flash),
                               makeVertex (c, normal, colour, selected, flash) },
                             faceDepth (a, b, c) });
}

void CityOpenGLRenderer::addLine (Vec3 a, Vec3 b, juce::Colour colour, float selected, float flash)
{
    const auto normal = Vec3 { 0.0f, 0.0f, 1.0f };
    auto& destination = collectingBlockGeometry ? blockOutlineVertices : outlineVertices;
    destination.push_back (makeVertex (a, normal, colour, selected, flash));
    destination.push_back (makeVertex (b, normal, colour, selected, flash));
}

void CityOpenGLRenderer::addNeonLine (Vec3 a, Vec3 b, juce::Colour colour, float selected, float flash)
{
    if (colourWireframeMode)
    {
        addLine (a,
                 b,
                 colour.withSaturation (juce::jmax (0.82f, colour.getSaturation()))
                       .withBrightness (1.0f)
                       .withAlpha (juce::jlimit (0.52f, 1.0f, colour.getFloatAlpha() * 1.4f)),
                 selected,
                 juce::jmax (flash * 0.5f, selected * 0.18f));
        return;
    }

    if (collectingBlockGeometry)
    {
        addLine (a, b, colour, selected, flash);
        return;
    }

    const auto normal = Vec3 { 0.0f, 0.0f, 1.0f };
    const auto material = colour.withSaturation (juce::jmax (0.86f, colour.getSaturation()))
                                .withBrightness (0.98f)
                                .interpolatedWith (juce::Colours::white, selected * 0.08f + flash * 0.025f)
                                .withAlpha (juce::jlimit (0.18f, 0.98f, colour.getFloatAlpha()));
    const auto accent = material.withAlpha (juce::jlimit (0.18f, 0.42f, material.getFloatAlpha() * 0.36f));

    addTube (a, b, material, selected, flash * 0.14f, 1.35f + selected * 0.3f, neonFaces);

    if (selected > 0.0f || flash > 0.0f)
    {
        outlineVertices.push_back (makeVertex (a, normal, accent, selected, flash * 0.35f));
        outlineVertices.push_back (makeVertex (b, normal, accent, selected, flash * 0.35f));
    }
}

void CityOpenGLRenderer::addTube (Vec3 a,
                                  Vec3 b,
                                  juce::Colour colour,
                                  float selected,
                                  float flash,
                                  float radius,
                                  std::vector<Face>& destination)
{
    constexpr auto segments = 5;

    const auto dir = normalised (subtract (b, a));
    auto sideA = normalised ({ -dir.y, dir.x, 0.0f });

    if (std::abs (sideA.x) + std::abs (sideA.y) + std::abs (sideA.z) < 0.001f)
        sideA = { 1.0f, 0.0f, 0.0f };

    const auto sideB = normalised ({
        dir.y * sideA.z - dir.z * sideA.y,
        dir.z * sideA.x - dir.x * sideA.z,
        dir.x * sideA.y - dir.y * sideA.x
    });

    auto ringPoint = [sideA, sideB, radius] (Vec3 origin, int index)
    {
        const auto angle = juce::MathConstants<float>::twoPi * static_cast<float> (index) / static_cast<float> (segments);
        const auto ca = std::cos (angle) * radius;
        const auto sa = std::sin (angle) * radius;

        return Vec3 {
            origin.x + sideA.x * ca + sideB.x * sa,
            origin.y + sideA.y * ca + sideB.y * sa,
            origin.z + sideA.z * ca + sideB.z * sa
        };
    };

    for (int i = 0; i < segments; ++i)
    {
        const auto p0 = ringPoint (a, i);
        const auto p1 = ringPoint (a, i + 1);
        const auto q0 = ringPoint (b, i);
        const auto q1 = ringPoint (b, i + 1);
        const auto displayNormal = Vec3 { 0.0f, 0.0f, 1.0f };

        destination.push_back ({ { makeVertex (p0, displayNormal, colour, selected, flash),
                                   makeVertex (p1, displayNormal, colour, selected, flash),
                                   makeVertex (q1, displayNormal, colour, selected, flash) },
                                 faceDepth (p0, p1, q1) });
        destination.push_back ({ { makeVertex (p0, displayNormal, colour, selected, flash),
                                   makeVertex (q1, displayNormal, colour, selected, flash),
                                   makeVertex (q0, displayNormal, colour, selected, flash) },
                                 faceDepth (p0, q1, q0) });
    }
}

CityOpenGLRenderer::Vertex CityOpenGLRenderer::makeVertex (Vec3 position,
                                                           Vec3 normal,
                                                           juce::Colour colour,
                                                           float selected,
                                                           float flash)
{
    normal = normalised (normal);

    return {
        { position.x, position.y, position.z },
        { normal.x, normal.y, normal.z },
        { colour.getFloatRed(), colour.getFloatGreen(), colour.getFloatBlue(), colour.getFloatAlpha() },
        selected,
        juce::jlimit (0.0f, 1.0f, flash)
    };
}

Vec3 CityOpenGLRenderer::normalForTriangle (Vec3 a, Vec3 b, Vec3 c) noexcept
{
    const auto ab = subtract (b, a);
    const auto ac = subtract (c, a);

    return normalised ({
        ab.y * ac.z - ab.z * ac.y,
        ab.z * ac.x - ab.x * ac.z,
        ab.x * ac.y - ab.y * ac.x
    });
}

Vec3 CityOpenGLRenderer::normalised (Vec3 value) noexcept
{
    const auto len = std::sqrt (value.x * value.x + value.y * value.y + value.z * value.z);

    if (len <= 0.0001f)
        return { 0.0f, 0.0f, 1.0f };

    return { value.x / len, value.y / len, value.z / len };
}

Vec3 CityOpenGLRenderer::subtract (Vec3 a, Vec3 b) noexcept
{
    return { a.x - b.x, a.y - b.y, a.z - b.z };
}

float CityOpenGLRenderer::faceDepth (Vec3 a, Vec3 b, Vec3 c) noexcept
{
    return (a.x + a.y + a.z * 0.7f
          + b.x + b.y + b.z * 0.7f
          + c.x + c.y + c.z * 0.7f) / 3.0f;
}

juce::Colour CityOpenGLRenderer::colourForSides (int sides)
{
    switch (juce::jlimit (3, 8, sides))
    {
        case 3: return rainbowAt (0.00f);
        case 4: return rainbowAt (0.10f);
        case 5: return rainbowAt (0.18f);
        case 6: return rainbowAt (0.36f);
        case 7: return rainbowAt (0.62f);
        case 8: return rainbowAt (0.78f);
        default: break;
    }

    return rainbowAt (0.52f);
}

bool CityOpenGLRenderer::visibleInView (Vec2 position, float radius, float elevation, const CityRenderState& state) noexcept
{
    IsoProjector view;
    view.zoom = juce::jmax (0.01f, state.zoom);
    view.pan = state.pan;
    view.viewMode = state.viewMode;
    view.centre = { static_cast<float> (state.width) * 0.5f,
                    static_cast<float> (state.height) * 0.5f };

    const auto screen = view.project ({ position.x, position.y, elevation });
    const auto screenRadius = (radius * 1.9f + std::abs (elevation) * 0.42f) * view.zoom + 180.0f;

    return screen.x >= -screenRadius
        && screen.y >= -screenRadius
        && screen.x <= static_cast<float> (state.width) + screenRadius
        && screen.y <= static_cast<float> (state.height) + screenRadius;
}
