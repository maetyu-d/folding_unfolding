#pragma once

#include "CityModel.h"
#include "FairgroundPlatter.h"
#include "FoldingModule.h"
#include "JuceIncludes.h"
#include "PowerSwitch.h"
#include "Plank.h"
#include "StackBlock.h"

#include <functional>
#include <memory>
#include <vector>

struct CityRenderState
{
    struct TipTriggerCue
    {
        Vec3 tip;
        Vec3 target;
        int sides = 6;
        float age = 0.0f;
        bool muted = false;
        bool contactOnly = false;
        bool tipTip = false;
    };

    std::vector<FoldingModule> modules;
    std::vector<FairgroundPlatter> platters;
    std::vector<StackBlock> blocks;
    std::vector<Plank> planks;
    std::vector<PowerSwitch> powerSwitches;
    std::vector<PowerSource> powerSources;
    std::vector<PowerFeedCable> powerFeedCables;
    std::vector<PowerCable> powerCables;
    std::vector<TipTriggerCue> tipTriggerCues;
    juce::Point<float> pan;
    float zoom = 1.0f;
    CityViewMode viewMode = CityViewMode::isometric;
    bool colourWireframeMode = false;
    int width = 1;
    int height = 1;
    int selectedModuleId = -1;
    int selectedPlatterId = -1;
    int selectedBlockId = -1;
    int selectedPlankId = -1;
    int selectedPowerSwitchId = -1;
    int selectedPowerSourceId = -1;
    double timeSeconds = 0.0;
    float globalTempoBpm = FoldingModule::defaultGlobalTempoBpm;
};

class CityOpenGLRenderer final : public juce::OpenGLRenderer
{
public:
    explicit CityOpenGLRenderer (juce::OpenGLContext& context);
    ~CityOpenGLRenderer() override;

    void newOpenGLContextCreated() override;
    void renderOpenGL() override;
    void openGLContextClosing() override;

    std::function<CityRenderState()> createRenderState;

private:
    struct Vertex
    {
        float position[3] = {};
        float normal[3] = {};
        float colour[4] = {};
        float selected = 0.0f;
        float flash = 0.0f;
    };

    struct Face
    {
        std::array<Vertex, 3> vertices;
        float depth = 0.0f;
    };

    struct Attributes;
    struct Uniforms;

    void initialiseShader();
    void releaseOpenGLResources();
    void buildMeshes (const CityRenderState& state);
    void drawVertices (const std::vector<Vertex>& vertices, unsigned int primitive, float lineWidth = 1.0f);

    void addGround (const CityRenderState& state);
    void addPowerZone (const PowerSwitch& powerSwitch);
    void addGrid (const CityRenderState& state);
    void addBlock (const StackBlock& block, const CityRenderState& state);
    void addPlank (const Plank& plank, const CityRenderState& state);
    void addPowerFeeds (const CityRenderState& state);
    void addPowerCables (const CityRenderState& state);
    void addPowerSwitch (const PowerSwitch& powerSwitch, const CityRenderState& state);
    void addPowerSource (const PowerSource& source, const CityRenderState& state);
    void addTipTriggerCues (const CityRenderState& state);
    void addSelectionHandles (const CityRenderState& state);
    void addGridLot (Vec2 centre, float footprint, float elevation, juce::Colour colour, bool selected, bool powered);
    void addWireCircle (Vec2 centre, float radius, float elevation, juce::Colour colour, float selected = 0.0f, float flash = 0.0f, int segments = 32);
    void addModule (const FoldingModule& module, const CityRenderState& state);
    void addPlatter (const FairgroundPlatter& platter, const CityRenderState& state);
    void addPlatterDisc (const FairgroundPlatter& platter, const CityRenderState& state, bool selected);
    void addPlatterStands (const FairgroundPlatter& platter, const CityRenderState& state, bool selected);
    void addShadow (const FoldingModule& module, double timeSeconds, float globalTempoBpm);
    void addFloor (const FoldingModule& module, bool selected);
    void addFlaps (const FoldingModule& module, double timeSeconds, float globalTempoBpm, bool selected);
    void addModuleOutlines (const FoldingModule& module, double timeSeconds, float globalTempoBpm, bool selected);
    void addModuleTriggerLight (const FoldingModule& module, bool selected);
    void addHandleRing (Vec2 centre, float radius, float elevation, float phase, juce::Colour colour);
    void addCablePulse (Vec3 a, Vec3 b, Vec3 c, float progress, juce::Colour colour, float intensity);

    void addFace (Vec3 a,
                  Vec3 b,
                  Vec3 c,
                  juce::Colour colour,
                  float selected,
                  float flash,
                  Vec3 normal = {});

    void addLine (Vec3 a, Vec3 b, juce::Colour colour, float selected = 0.0f, float flash = 0.0f);
    void addNeonLine (Vec3 a, Vec3 b, juce::Colour colour, float selected = 0.0f, float flash = 0.0f);
    void addTube (Vec3 a,
                  Vec3 b,
                  juce::Colour colour,
                  float selected,
                  float flash,
                  float radius,
                  std::vector<Face>& destination);

    static Vertex makeVertex (Vec3 position,
                              Vec3 normal,
                              juce::Colour colour,
                              float selected,
                              float flash);

    static Vec3 normalForTriangle (Vec3 a, Vec3 b, Vec3 c) noexcept;
    static Vec3 normalised (Vec3 value) noexcept;
    static Vec3 subtract (Vec3 a, Vec3 b) noexcept;
    static float faceDepth (Vec3 a, Vec3 b, Vec3 c) noexcept;
    static juce::Colour colourForSides (int sides);
    static bool visibleInView (Vec2 position, float radius, float elevation, const CityRenderState& state) noexcept;

    juce::OpenGLContext& openGLContext;
    std::unique_ptr<juce::OpenGLShaderProgram> shader;
    std::unique_ptr<Attributes> attributes;
    std::unique_ptr<Uniforms> uniforms;

    unsigned int vertexBuffer = 0;
    std::vector<Vertex> groundVertices;
    std::vector<Vertex> powerZoneVertices;
    std::vector<Vertex> gridVertices;
    std::vector<Face> blockFaces;
    std::vector<Face> faces;
    std::vector<Face> neonFaces;
    std::vector<Vertex> blockTriangleVertices;
    std::vector<Vertex> triangleVertices;
    std::vector<Vertex> neonVertices;
    std::vector<Vertex> blockOutlineVertices;
    std::vector<Vertex> outlineVertices;
    bool collectingBlockGeometry = false;
    bool colourWireframeMode = false;
};
