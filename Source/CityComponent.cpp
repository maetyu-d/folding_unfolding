#include "CityComponent.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <optional>
#include <vector>

namespace
{
constexpr auto buildGridSize = 96.0f;

float snapLengthToBuildGrid (float length, int minCells = 1) noexcept
{
    const auto cells = juce::jmax (minCells, juce::roundToInt (length / buildGridSize));
    return static_cast<float> (cells) * buildGridSize;
}

float gridSquaresForLength (float length) noexcept
{
    return static_cast<float> (juce::jmax (1, juce::roundToInt (length / buildGridSize)));
}

float squaredDistance (Vec2 a, Vec2 b) noexcept
{
    const auto dx = a.x - b.x;
    const auto dy = a.y - b.y;
    return dx * dx + dy * dy;
}

float polygonTipReach (int sides, float radius, float flapDepth) noexcept
{
    const auto clampedSides = juce::jlimit (3, 8, sides);
    const auto apothem = radius * std::cos (juce::MathConstants<float>::pi / static_cast<float> (clampedSides));
    return apothem + flapDepth;
}

float polygonRadiusForTipReach (int sides, float reach, float flapDepth) noexcept
{
    const auto clampedSides = juce::jlimit (3, 8, sides);
    const auto apothemScale = std::cos (juce::MathConstants<float>::pi / static_cast<float> (clampedSides));
    return juce::jlimit (24.0f, 1300.0f, (reach - flapDepth) / juce::jmax (0.001f, apothemScale));
}

float moduleRadiusForGridSquares (float squares, float flapDepth) noexcept
{
    const auto cells = juce::jlimit (1, 28, juce::roundToInt (squares));
    return juce::jlimit (24.0f, 1300.0f, static_cast<float> (cells) * buildGridSize * 0.5f - flapDepth);
}

float lengthForGridSquares (float squares, int maxSquares) noexcept
{
    const auto cells = juce::jlimit (1, maxSquares, juce::roundToInt (squares));
    return static_cast<float> (cells) * buildGridSize;
}

float wrapRadians (float radians) noexcept
{
    const auto wrapped = std::fmod (radians, juce::MathConstants<float>::twoPi);
    return wrapped < 0.0f ? wrapped + juce::MathConstants<float>::twoPi : wrapped;
}

juce::var objectWithPosition (Vec2 position)
{
    auto object = juce::DynamicObject::Ptr (new juce::DynamicObject());
    object->setProperty ("x", position.x);
    object->setProperty ("y", position.y);
    return juce::var (object.get());
}

Vec2 positionFromObject (const juce::DynamicObject& object)
{
    return {
        static_cast<float> ((double) object.getProperty ("x")),
        static_cast<float> ((double) object.getProperty ("y"))
    };
}

int intProperty (const juce::DynamicObject& object, const char* name, int fallback)
{
    const auto value = object.getProperty (name);
    return value.isVoid() ? fallback : static_cast<int> ((int) value);
}

float floatProperty (const juce::DynamicObject& object, const char* name, float fallback)
{
    const auto value = object.getProperty (name);
    return value.isVoid() ? fallback : static_cast<float> ((double) value);
}

bool boolProperty (const juce::DynamicObject& object, const char* name, bool fallback)
{
    const auto value = object.getProperty (name);
    return value.isVoid() ? fallback : static_cast<bool> ((bool) value);
}

juce::String noteNameForMidi (float midiNote)
{
    static constexpr const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
    const auto note = juce::roundToInt (midiNote);
    const auto pitchClass = ((note % 12) + 12) % 12;
    const auto octave = note / 12 - 1;
    return juce::String (names[pitchClass]) + juce::String (octave);
}

CityObjectKind objectKindFromInt (int value)
{
    if (value < static_cast<int> (CityObjectKind::none)
        || value > static_cast<int> (CityObjectKind::powerSource))
        return CityObjectKind::none;

    return static_cast<CityObjectKind> (value);
}
}

CityComponent::CityComponent()
    : cityRenderer (openGLContext)
{
    setWantsKeyboardFocus (true);
    setOpaque (true);

    startTimeSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
    lastFrameTimeSeconds = startTimeSeconds;

    configureToolbar();
    startNewCity();
    syncToolbar();

    cityRenderer.createRenderState = [this]
    {
        return createRenderState();
    };

    openGLContext.setOpenGLVersionRequired (juce::OpenGLContext::openGL3_2);
    openGLContext.setMultisamplingEnabled (true);
    openGLContext.setRenderer (&cityRenderer);
    openGLContext.setComponentPaintingEnabled (true);
    openGLContext.setContinuousRepainting (false);
    openGLContext.attachTo (*this);

    startTimerHz (60);
}

CityComponent::~CityComponent()
{
    stopTimer();
    openGLContext.detach();
}

void CityComponent::paint (juce::Graphics& g)
{
    const juce::ScopedLock lock (modelLock);

    auto view = projector;
    view.centre = getLocalBounds().toFloat().getCentre();

    auto projectGround = [&view] (Vec2 point, float elevation = 0.0f)
    {
        return view.project ({ point.x, point.y, elevation });
    };
    const auto nowSeconds = currentTimeSeconds();

    auto makeLotPath = [&projectGround] (Vec2 centre, float footprint, float elevation)
    {
        juce::Path path;
        const auto half = footprint * 0.5f;
        const Vec2 corners[] = {
            { centre.x - half, centre.y - half },
            { centre.x + half, centre.y - half },
            { centre.x + half, centre.y + half },
            { centre.x - half, centre.y + half }
        };

        for (int i = 0; i < 4; ++i)
        {
            const auto point = projectGround (corners[i], elevation);
            if (i == 0)
                path.startNewSubPath (point);
            else
                path.lineTo (point);
        }

        path.closeSubPath();
        return path;
    };

    auto isMountOccupied = [this] (int platterId, int standIndex)
    {
        const auto* parent = city.findPlatter (platterId);
        const auto clampedStand = parent != nullptr ? juce::jlimit (0, parent->stands - 1, standIndex)
                                                    : standIndex;

        for (const auto& module : city.modules())
            if (module.attachedPlatterId == platterId
             && parent != nullptr
             && juce::jlimit (0, parent->stands - 1, module.attachedStandIndex) == clampedStand)
                return true;

        for (const auto& platter : city.platters())
            if (platter.attachedPlatterId == platterId
             && parent != nullptr
             && juce::jlimit (0, parent->stands - 1, platter.attachedStandIndex) == clampedStand)
                return true;

        for (const auto& plank : city.planks())
            if (plank.attachedPlatterId == platterId
             && parent != nullptr
             && juce::jlimit (0, parent->stands - 1, plank.attachedStandIndex) == clampedStand)
                return true;

        return false;
    };

    for (const auto& cue : tipTriggerCues)
    {
        const auto life = cue.contactOnly ? (cue.tipTip ? 0.20f : 0.16f)
                                          : (cue.tipTip ? 0.26f : 0.18f);
        const auto t = juce::jlimit (0.0f, 1.0f, cue.age / life);
        const auto alpha = std::pow (1.0f - smoothStep01 (t), 2.05f);
        const auto tip = view.project (cue.tip);
        const auto target = view.project (cue.target);
        const auto colour = cue.muted ? juce::Colour (0xffd8dde7)
                          : cue.tipTip ? juce::Colour (0xff58d7ff)
                          : cue.contactOnly ? juce::Colour (0xff22f6ff)
                                            : juce::Colour (0xff35e7ff);
        const auto hot = cue.muted ? juce::Colour (0xff9aa3b5)
                       : cue.tipTip ? juce::Colour (0xffffffff)
                       : cue.contactOnly ? juce::Colour (0xffffffff)
                                         : juce::Colour (0xffff4fd2);
        const auto delta = target - tip;
        const auto length = juce::jmax (0.001f, tip.getDistanceFrom (target));
        const auto along = delta / length;
        const auto across = juce::Point<float> { -along.y, along.x };

        auto drawDiamond = [&g] (juce::Point<float> centre, float size, juce::Colour diamondColour, float thickness)
        {
            juce::Path diamond;
            diamond.startNewSubPath (centre.x, centre.y - size);
            diamond.lineTo (centre.x + size, centre.y);
            diamond.lineTo (centre.x, centre.y + size);
            diamond.lineTo (centre.x - size, centre.y);
            diamond.closeSubPath();
            g.setColour (diamondColour);
            g.strokePath (diamond, juce::PathStrokeType (thickness, juce::PathStrokeType::JointStyle::mitered));
        };

        auto drawBracket = [&g, along, across] (juce::Point<float> centre, float size, juce::Colour bracketColour, float thickness)
        {
            const auto half = size * 0.5f;
            const auto arm = size * 0.34f;
            const auto a = centre + across * half - along * arm;
            const auto b = centre + across * half + along * arm;
            const auto c = centre - across * half - along * arm;
            const auto d = centre - across * half + along * arm;
            g.setColour (bracketColour);
            g.drawLine ({ a, b }, thickness);
            g.drawLine ({ c, d }, thickness);
        };

        if (cue.contactOnly)
        {
            if (cue.tipTip)
            {
                const auto beat = 0.5f + 0.5f * std::sin (static_cast<float> (nowSeconds) * 18.0f
                                                        + static_cast<float> (cue.sides) * 0.7f);
                const auto pulse = std::fmod (static_cast<float> (nowSeconds) * 3.2f
                                            + static_cast<float> (cue.sides) * 0.11f,
                                            1.0f);
                const auto reticle = 9.0f + beat * 4.0f;
                const auto signal = tip + (target - tip) * pulse;

                g.setColour (colour.withAlpha (0.28f * alpha));
                g.drawLine ({ tip, target }, 5.0f);
                g.setColour (hot.withAlpha (0.74f * alpha));
                g.drawLine ({ tip, target }, 1.8f);

                for (int i = 0; i < 3; ++i)
                {
                    const auto startT = std::fmod (pulse + static_cast<float> (i) * 0.33f, 1.0f);
                    const auto endT = juce::jmin (1.0f, startT + 0.10f);
                    const auto a = tip + (target - tip) * startT;
                    const auto b = tip + (target - tip) * endT;
                    g.setColour (juce::Colour (0xffff4fd2).withAlpha ((0.44f - static_cast<float> (i) * 0.11f) * alpha));
                    g.drawLine ({ a, b }, 2.6f);
                }

                drawDiamond (tip, reticle, colour.withAlpha (0.82f * alpha), 2.0f);
                drawDiamond (target, reticle, juce::Colour (0xffff4fd2).withAlpha (0.74f * alpha), 2.0f);
                drawDiamond (signal, 4.5f + beat * 2.0f, hot.withAlpha (0.8f * alpha), 1.8f);

                if (cue.muted)
                {
                    const auto mid = tip + (target - tip) * 0.5f;
                    const auto slash = 11.0f;
                    g.setColour (juce::Colour (0xffd8dde7).withAlpha (0.7f * alpha));
                    g.drawLine (mid.x - slash, mid.y - slash, mid.x + slash, mid.y + slash, 3.0f);
                }

                continue;
            }

            const auto start = tip + delta * 0.12f;
            const auto end = tip + delta * (0.72f + 0.24f * (1.0f - t));
            g.setColour (colour.withAlpha (0.64f * alpha));
            g.drawLine ({ start, end }, 3.0f);
            g.setColour (hot.withAlpha (0.86f * alpha));
            g.drawLine ({ start + across * 3.5f, end + across * 3.5f }, 1.2f);
            drawDiamond (tip, 7.0f + 2.0f * alpha, hot.withAlpha (0.9f * alpha), 1.7f);
            drawBracket (target, 15.0f + 5.0f * alpha, colour.withAlpha (0.9f * alpha), 1.8f);

            continue;
        }

        const auto start = tip + delta * 0.08f;
        const auto traceEnd = tip + delta * juce::jmin (1.0f, 0.52f + t * 0.86f);
        const auto lane = across * (cue.muted ? 2.0f : 3.0f);
        g.setColour (colour.withAlpha (0.66f * alpha));
        g.drawLine ({ start - lane, traceEnd - lane }, cue.muted ? 2.2f : 3.4f);
        g.setColour (hot.withAlpha (0.96f * alpha));
        g.drawLine ({ start + lane, traceEnd + lane }, cue.muted ? 1.3f : 1.8f);

        drawDiamond (tip, cue.muted ? 7.0f : 8.5f, hot.withAlpha (0.96f * alpha), cue.muted ? 1.8f : 2.2f);

        if (cue.muted)
        {
            const auto x = 7.0f + 3.0f * alpha;
            g.drawLine (tip.x - x, tip.y - x, tip.x + x, tip.y + x, 2.2f);
            g.drawLine (tip.x + x, tip.y - x, tip.x - x, tip.y + x, 2.2f);
        }
        else
        {
            drawBracket (target, 18.0f + 6.0f * alpha, hot.withAlpha (0.94f * alpha), 2.2f);
            drawDiamond (target, 5.0f + 2.0f * alpha, colour.withAlpha (0.9f * alpha), 1.4f);
        }
    }

    if (activationRingsVisible)
        paintActivationRings (g, view);

    if (soundingNotesVisible)
        paintSoundingNotes (g, view);

    if (triggerTelemetryVisible)
        paintTriggerTelemetry (g, view);

    if (minimapVisible)
        paintMinimap (g, view);

    if (! mouseInCanvas)
        return;

    if (buildMode == CityToolbar::BuildMode::polygon)
    {
        juce::Path path;
        const auto sides = juce::jlimit (3, 8, defaultSides);
        const auto previewRadius = snappedModuleRadiusForPlacement();
        auto previewCentre = hoverWorld;
        auto previewElevation = city.elevationAt (hoverWorld) + 12.0f;
        auto previewRotation = 0.0f;
        auto mountPlatterId = -1;
        auto mountStandIndex = -1;
        const auto hasMount = findAvailableMount (hoverPointerWorld,
                                                  nowSeconds,
                                                  previewRadius,
                                                  mountPlatterId,
                                                  mountStandIndex);

        if (hasMount)
        {
            if (const auto* platter = city.findPlatter (mountPlatterId))
            {
                const auto clampedStand = juce::jlimit (0, platter->stands - 1, mountStandIndex);
                previewCentre = platter->standPosition (clampedStand, nowSeconds, globalTempoBpm);
                previewElevation = platter->elevation + 12.0f;
                previewRotation = platter->rotation + platter->rotationAt (nowSeconds, globalTempoBpm)
                                + juce::MathConstants<float>::twoPi * static_cast<float> (clampedStand)
                                    / static_cast<float> (juce::jlimit (1, 8, platter->stands));
            }
        }

        const auto elevation = city.elevationAt (hoverWorld) - 14.0f;
        const auto lotPath = makeLotPath (hasMount ? previewCentre : hoverWorld, currentBuildFootprint(), elevation);

        g.setColour ((hasMount ? juce::Colour (0xff60f0b2) : juce::Colour (0xffffcf5f)).withAlpha (0.10f));
        g.fillPath (lotPath);
        g.setColour ((hasMount ? juce::Colour (0xff60f0b2) : juce::Colour (0xffffcf5f)).withAlpha (0.62f));
        g.strokePath (lotPath, juce::PathStrokeType (2.0f));

        for (const auto& platter : city.platters())
        {
            if (distance (hoverPointerWorld, platter.position) > platter.hitRadius() + buildGridSize * 1.2f)
                continue;

            for (int i = 0; i < platter.stands; ++i)
            {
                const auto stand = platter.standPosition (i, nowSeconds, globalTempoBpm);
                const auto standScreen = projectGround (stand, platter.elevation + 18.0f);
                const auto occupied = isMountOccupied (platter.id, i);
                const auto highlighted = hasMount && platter.id == mountPlatterId && i == mountStandIndex;
                const auto r = highlighted ? 14.0f : 8.0f;

                g.setColour ((highlighted ? juce::Colour (0xff60f0b2)
                                           : occupied ? juce::Colour (0xffff6f91)
                                                      : juce::Colour (0xfff3f1dc))
                                .withAlpha (highlighted ? 0.34f : 0.18f));
                g.fillEllipse (standScreen.x - r, standScreen.y - r, r * 2.0f, r * 2.0f);
                g.setColour ((highlighted ? juce::Colour (0xff60f0b2)
                                           : occupied ? juce::Colour (0xffff6f91)
                                                      : juce::Colour (0xfff3f1dc))
                                .withAlpha (highlighted ? 0.95f : 0.58f));
                g.drawEllipse (standScreen.x - r, standScreen.y - r, r * 2.0f, r * 2.0f, highlighted ? 2.4f : 1.4f);

                if (occupied)
                {
                    g.drawLine (standScreen.x - 5.0f, standScreen.y - 5.0f, standScreen.x + 5.0f, standScreen.y + 5.0f, 1.4f);
                    g.drawLine (standScreen.x + 5.0f, standScreen.y - 5.0f, standScreen.x - 5.0f, standScreen.y + 5.0f, 1.4f);
                }
            }
        }

        for (int i = 0; i < sides; ++i)
        {
            const auto angle = -juce::MathConstants<float>::halfPi
                             + previewRotation
                             + juce::MathConstants<float>::twoPi * static_cast<float> (i) / static_cast<float> (sides);
            const auto point = projectGround ({ previewCentre.x + std::cos (angle) * previewRadius,
                                                previewCentre.y + std::sin (angle) * previewRadius },
                                              previewElevation);

            if (i == 0)
                path.startNewSubPath (point);
            else
                path.lineTo (point);
        }

        path.closeSubPath();
        g.setColour ((hasMount ? juce::Colour (0xff60f0b2) : juce::Colour (0xffffcf5f)).withAlpha (0.18f));
        g.fillPath (path);
        g.setColour ((hasMount ? juce::Colour (0xff60f0b2) : juce::Colour (0xffffcf5f)).withAlpha (0.82f));
        g.strokePath (path, juce::PathStrokeType (hasMount ? 3.0f : 2.0f));

        if (hasMount)
        {
            const auto target = projectGround (previewCentre, previewElevation + 10.0f);
            const auto cursor = projectGround (hoverPointerWorld, previewElevation);
            g.setColour (juce::Colour (0xff60f0b2).withAlpha (0.62f));
            g.drawLine ({ cursor, target }, 2.0f);
            g.setColour (juce::Colour (0xff60f0b2).withAlpha (0.95f));
            g.fillEllipse (target.x - 4.0f, target.y - 4.0f, 8.0f, 8.0f);
        }
    }
    else if (buildMode == CityToolbar::BuildMode::platter)
    {
        const auto previewDiameter = snappedPlatterDiameterForPlacement();
        auto previewCentre = hoverWorld;
        auto previewElevation = city.elevationAt (hoverWorld) + 12.0f;
        auto mountPlatterId = -1;
        auto mountStandIndex = -1;
        const auto hasMount = findAvailableMount (hoverPointerWorld,
                                                  nowSeconds,
                                                  previewDiameter * 0.5f,
                                                  mountPlatterId,
                                                  mountStandIndex);

        if (hasMount)
        {
            if (const auto* platter = city.findPlatter (mountPlatterId))
            {
                const auto clampedStand = juce::jlimit (0, platter->stands - 1, mountStandIndex);
                previewCentre = platter->standPosition (clampedStand, nowSeconds, globalTempoBpm);
                previewElevation = platter->elevation + 18.0f;
            }
        }

        const auto lotPath = makeLotPath (hasMount ? previewCentre : hoverWorld,
                                          currentBuildFootprint(),
                                          (hasMount ? previewElevation : city.elevationAt (hoverWorld)) - 14.0f);
        g.setColour ((hasMount ? juce::Colour (0xff60f0b2) : juce::Colour (0xff75d7ff)).withAlpha (0.09f));
        g.fillPath (lotPath);
        g.setColour ((hasMount ? juce::Colour (0xff60f0b2) : juce::Colour (0xff75d7ff)).withAlpha (0.58f));
        g.strokePath (lotPath, juce::PathStrokeType (hasMount ? 3.0f : 2.0f));

        for (const auto& platter : city.platters())
        {
            if (distance (hoverPointerWorld, platter.position) > platter.hitRadius() + buildGridSize * 1.2f)
                continue;

            for (int i = 0; i < platter.stands; ++i)
            {
                const auto stand = platter.standPosition (i, nowSeconds, globalTempoBpm);
                const auto standScreen = projectGround (stand, platter.elevation + 18.0f);
                const auto occupied = isMountOccupied (platter.id, i);
                const auto highlighted = hasMount && platter.id == mountPlatterId && i == mountStandIndex;
                const auto r = highlighted ? 14.0f : 8.0f;

                g.setColour ((highlighted ? juce::Colour (0xff60f0b2)
                                           : occupied ? juce::Colour (0xffff6f91)
                                                      : juce::Colour (0xfff3f1dc))
                                .withAlpha (highlighted ? 0.34f : 0.18f));
                g.fillEllipse (standScreen.x - r, standScreen.y - r, r * 2.0f, r * 2.0f);
                g.setColour ((highlighted ? juce::Colour (0xff60f0b2)
                                           : occupied ? juce::Colour (0xffff6f91)
                                                      : juce::Colour (0xfff3f1dc))
                                .withAlpha (highlighted ? 0.95f : 0.58f));
                g.drawEllipse (standScreen.x - r, standScreen.y - r, r * 2.0f, r * 2.0f, highlighted ? 2.4f : 1.4f);
            }
        }

        const auto centre = projectGround (previewCentre, previewElevation);
        const auto radius = previewDiameter * 0.5f * view.zoom;
        g.setColour ((hasMount ? juce::Colour (0xff60f0b2) : juce::Colour (0xff75d7ff)).withAlpha (0.12f));
        g.fillEllipse (centre.x - radius, centre.y - radius * 0.58f, radius * 2.0f, radius * 1.16f);
        g.setColour ((hasMount ? juce::Colour (0xff60f0b2) : juce::Colour (0xff75d7ff)).withAlpha (0.76f));
        g.drawEllipse (centre.x - radius, centre.y - radius * 0.58f, radius * 2.0f, radius * 1.16f, hasMount ? 3.0f : 2.0f);

        if (hasMount)
        {
            const auto cursor = projectGround (hoverPointerWorld, previewElevation);
            g.setColour (juce::Colour (0xff60f0b2).withAlpha (0.62f));
            g.drawLine ({ cursor, centre }, 2.0f);
        }
    }
    else if (buildMode == CityToolbar::BuildMode::block)
    {
        const auto half = snappedBlockSizeForPlacement() * 0.5f;
        const auto elevation = city.elevationAt (hoverWorld);
        auto path = makeLotPath (hoverWorld, half * 2.0f, elevation - 14.0f);
        g.setColour (juce::Colour (0xffa8e06f).withAlpha (0.16f));
        g.fillPath (path);
        g.setColour (juce::Colour (0xffa8e06f).withAlpha (0.78f));
        g.strokePath (path, juce::PathStrokeType (2.0f));
    }
    else if (buildMode == CityToolbar::BuildMode::plank)
    {
        auto baseWorld = hoverWorld;
        auto elevation = city.elevationAt (hoverWorld) + 10.0f;
        auto baseRotation = 0.0f;
        auto mountPlatterId = -1;
        auto mountStandIndex = -1;
        const auto hasMount = findAvailableMount (hoverPointerWorld,
                                                  nowSeconds,
                                                  defaultPlankLength * 0.25f,
                                                  mountPlatterId,
                                                  mountStandIndex);

        if (hasMount)
        {
            if (const auto* platter = city.findPlatter (mountPlatterId))
            {
                const auto clampedStand = juce::jlimit (0, platter->stands - 1, mountStandIndex);
                baseWorld = platter->standPosition (clampedStand, nowSeconds, globalTempoBpm);
                elevation = platter->elevation + 10.0f;
                baseRotation = platter->rotation + platter->rotationAt (nowSeconds, globalTempoBpm)
                             + juce::MathConstants<float>::twoPi * static_cast<float> (clampedStand)
                                / static_cast<float> (juce::jlimit (1, 8, platter->stands));
            }
        }

        const auto direction = baseRotation + defaultPlankAngle;
        const auto endWorld = Vec2 { baseWorld.x + std::cos (direction) * defaultPlankLength,
                                     baseWorld.y + std::sin (direction) * defaultPlankLength };
        const auto start = projectGround (baseWorld, elevation);
        const auto end = projectGround (endWorld, elevation);
        g.setColour ((hasMount ? juce::Colour (0xff60f0b2) : juce::Colour (0xffff9f6e)).withAlpha (0.78f));
        g.drawLine ({ start, end }, hasMount ? 6.0f : 5.0f);
        g.setColour (juce::Colour (0xff75d7ff).withAlpha (0.22f));
        g.fillEllipse (end.x - 14.0f, end.y - 14.0f, 28.0f, 28.0f);
        g.setColour (juce::Colour (0xff75d7ff).withAlpha (0.82f));
        g.drawEllipse (end.x - 14.0f, end.y - 14.0f, 28.0f, 28.0f, 2.0f);

        if (hasMount)
        {
            const auto cursor = projectGround (hoverPointerWorld, elevation);
            g.setColour (juce::Colour (0xff60f0b2).withAlpha (0.62f));
            g.drawLine ({ cursor, start }, 2.0f);
            g.fillEllipse (start.x - 5.0f, start.y - 5.0f, 10.0f, 10.0f);
        }
    }
    else if (buildMode == CityToolbar::BuildMode::cable)
    {
        auto start = std::optional<Vec3> {};

        if (cableSourceSwitchId >= 0)
            if (const auto* powerSwitch = city.findPowerSwitch (cableSourceSwitchId))
                start = Vec3 { powerSwitch->position.x, powerSwitch->position.y, powerSwitch->elevation + 20.0f };

        if (! start.has_value() && cableSourcePowerSourceId >= 0)
            if (const auto* source = city.findPowerSource (cableSourcePowerSourceId))
                start = Vec3 { source->position.x, source->position.y, source->elevation + 48.0f };

        if (start.has_value())
        {
            const auto a = view.project (*start);
            const auto b = projectGround (hoverWorld, city.elevationAt (hoverWorld) + 24.0f);
            g.setColour (juce::Colour (0xff60f0b2).withAlpha (0.78f));
            g.drawLine ({ a, b }, 3.0f);
            g.setColour (juce::Colour (0xfffff1a8).withAlpha (0.85f));
            g.fillEllipse (b.x - 5.0f, b.y - 5.0f, 10.0f, 10.0f);
        }
    }

    auto shouldShowTipTargets = [this] (const FoldingModule& module)
    {
        return buildMode == CityToolbar::BuildMode::select
            || (selectedKind == CityObjectKind::module && selectedId == module.id);
    };

    for (const auto& module : city.modules())
    {
        if (! shouldShowTipTargets (module))
            continue;

        const auto flaps = module.flapTriangles (nowSeconds, globalTempoBpm);

        for (size_t i = 0; i < flaps.size(); ++i)
        {
            const auto tipIndex = static_cast<int> (i);
            const auto tip = view.project (flaps[i][2]);
            const auto selected = selectedKind == CityObjectKind::module && selectedId == module.id && selectedTipIndex == tipIndex;
            const auto hovered = hoverTipModuleId == module.id && hoverTipIndex == tipIndex;
            const auto muted = module.tipIsMuted (tipIndex);
            const auto radius = selected ? 16.0f : hovered ? 14.0f : 10.0f;
            const auto colour = selected ? juce::Colour (0xff60f0b2)
                              : hovered ? juce::Colour (0xffffcf5f)
                                        : muted ? juce::Colour (0xffff6f91)
                                                : juce::Colour (0xfff3f1dc);

            g.setColour (colour.withAlpha (selected || hovered ? 0.28f : 0.14f));
            g.fillEllipse (tip.x - radius, tip.y - radius, radius * 2.0f, radius * 2.0f);
            g.setColour (colour.withAlpha (selected || hovered ? 0.96f : 0.62f));
            g.drawEllipse (tip.x - radius, tip.y - radius, radius * 2.0f, radius * 2.0f, selected || hovered ? 2.4f : 1.6f);
            g.drawLine (tip.x - radius * 0.45f, tip.y, tip.x + radius * 0.45f, tip.y, 1.2f);
            g.drawLine (tip.x, tip.y - radius * 0.45f, tip.x, tip.y + radius * 0.45f, 1.2f);

            if (muted)
            {
                g.drawLine (tip.x - 5.0f, tip.y - 5.0f, tip.x + 5.0f, tip.y + 5.0f, 1.6f);
                g.drawLine (tip.x + 5.0f, tip.y - 5.0f, tip.x - 5.0f, tip.y + 5.0f, 1.6f);
            }
        }
    }

    if (selectedKind == CityObjectKind::module && selectedTipIndex >= 0)
    {
        if (const auto* selected = city.findModule (selectedId))
        {
            const auto flaps = selected->flapTriangles (nowSeconds, globalTempoBpm);
            if (selectedTipIndex < static_cast<int> (flaps.size()))
            {
                const auto apex = flaps[static_cast<size_t> (selectedTipIndex)][2];
                const auto tip = view.project (apex);
                const auto pitch = selected->pitchForTip (selectedTipIndex);
                g.setColour (juce::Colour (0xfff3f1dc));
                g.setFont (juce::FontOptions (12.0f, juce::Font::bold));
                g.drawText (pitch <= 0.0f ? juce::String ("X") : juce::String (juce::roundToInt (pitch)),
                            juce::Rectangle<float> { tip.x + 14.0f, tip.y - 18.0f, 42.0f, 20.0f },
                            juce::Justification::centredLeft);
            }
        }
    }
}

void CityComponent::resized()
{
    {
        const juce::ScopedLock lock (modelLock);
        renderWidth = juce::jmax (1, getWidth());
        renderHeight = juce::jmax (1, getHeight());
        projector.centre = getLocalBounds().toFloat().getCentre();
    }

    const auto toolbarWidth = juce::jmin (getWidth() - 28,
                                          selectedKind == CityObjectKind::module ? 500 : 372);
    auto toolbarHeight = 506;

    if (buildMode == CityToolbar::BuildMode::select && selectedKind == CityObjectKind::none)
        toolbarHeight = 270;
    else if (selectedKind == CityObjectKind::module)
        toolbarHeight = 820;
    else if (buildMode == CityToolbar::BuildMode::platter || selectedKind == CityObjectKind::platter)
        toolbarHeight = 566;
    else if (buildMode == CityToolbar::BuildMode::plank || selectedKind == CityObjectKind::plank)
        toolbarHeight = 410;
    else if (selectedKind == CityObjectKind::powerSwitch)
        toolbarHeight = 460;
    else if (buildMode == CityToolbar::BuildMode::block
          || buildMode == CityToolbar::BuildMode::cable
          || selectedKind == CityObjectKind::block
          || selectedKind == CityObjectKind::powerSwitch
          || selectedKind == CityObjectKind::powerSource)
        toolbarHeight = 352;

    toolbar.setVisible (uiVisible);
    toolbar.setBounds (14, 14, toolbarWidth, juce::jmin (getHeight() - 28, toolbarHeight));
}

void CityComponent::mouseDown (const juce::MouseEvent& event)
{
    grabKeyboardFocus();

    if ((event.mods.isLeftButtonDown() || event.mods.isShiftDown())
        && ! event.mods.isRightButtonDown()
        && ! event.mods.isMiddleButtonDown()
        && ! event.mods.isAltDown())
        recordUndoState();

    {
        const juce::ScopedLock lock (modelLock);

        projector.centre = getLocalBounds().toFloat().getCentre();
        lastMouse = event.position;
        hoverPointerWorld = worldForPointer (event.position);
        hoverWorld = snapToBuildGrid (hoverPointerWorld, currentBuildFootprint());
        lastDragWorld = buildMode == CityToolbar::BuildMode::polygon
                     || buildMode == CityToolbar::BuildMode::platter
                     || buildMode == CityToolbar::BuildMode::block
                     || buildMode == CityToolbar::BuildMode::plank
                     || buildMode == CityToolbar::BuildMode::cable ? hoverWorld
                                                                    : worldForPointer (event.position);

        const auto hit = city.hitTest (lastDragWorld, currentTimeSeconds(), globalTempoBpm);
        auto tipModuleId = -1;
        auto tipIndex = -1;
        const auto tipHit = findTipAt (event.position, tipModuleId, tipIndex);
        const auto wantsPan = event.mods.isRightButtonDown()
                           || event.mods.isMiddleButtonDown()
                           || event.mods.isAltDown();

        if (wantsPan)
        {
            dragMode = DragMode::pan;
            return;
        }

        if (event.mods.isLeftButtonDown() && ! event.mods.isShiftDown() && buildMode != CityToolbar::BuildMode::cable)
        {
            if (hitSelectedPhaseHandle (event.position))
            {
                dragMode = DragMode::phaseHandle;
                dragSelectedPhaseHandle (event.position);
                return;
            }

            if (hitSelectedResizeHandle (event.position))
            {
                dragMode = DragMode::resizeHandle;
                dragSelectedResizeHandle (event.position);
                return;
            }
        }

        if (event.mods.isShiftDown())
        {
            if (hit.kind == CityObjectKind::module)
            {
                city.removeModule (hit.id);
                activeCollisions.clear();

                if (selectedKind == CityObjectKind::module && selectedId == hit.id)
                {
                    selectedId = -1;
                    selectedKind = CityObjectKind::none;
                    selectedTipIndex = -1;
                }
            }
            else if (hit.kind == CityObjectKind::platter)
            {
                city.removePlatter (hit.id);
                activeCollisions.clear();

                if (selectedKind == CityObjectKind::platter && selectedId == hit.id)
                {
                    selectedId = -1;
                    selectedKind = CityObjectKind::none;
                }
            }
            else if (hit.kind == CityObjectKind::block)
            {
                city.removeBlock (hit.id);
                activeCollisions.clear();

                if (selectedKind == CityObjectKind::block && selectedId == hit.id)
                {
                    selectedId = -1;
                    selectedKind = CityObjectKind::none;
                }
            }
            else if (hit.kind == CityObjectKind::plank)
            {
                city.removePlank (hit.id);
                activeCollisions.clear();

                if (selectedKind == CityObjectKind::plank && selectedId == hit.id)
                {
                    selectedId = -1;
                    selectedKind = CityObjectKind::none;
                }
            }
            else if (hit.kind == CityObjectKind::powerSwitch)
            {
                city.removePowerSwitch (hit.id);
                activeCollisions.clear();
                activePowerSwitchTouches.clear();

                if (selectedKind == CityObjectKind::powerSwitch && selectedId == hit.id)
                {
                    selectedId = -1;
                    selectedKind = CityObjectKind::none;
                }

                if (cableSourceSwitchId == hit.id)
                    cableSourceSwitchId = -1;
            }
            else if (hit.kind == CityObjectKind::powerSource)
            {
                city.removePowerSource (hit.id);
                activeCollisions.clear();
                activePowerSwitchTouches.clear();

                if (selectedKind == CityObjectKind::powerSource && selectedId == hit.id)
                {
                    selectedId = -1;
                    selectedKind = CityObjectKind::none;
                }

                if (cableSourcePowerSourceId == hit.id)
                    cableSourcePowerSourceId = -1;
            }
        }
        else if (event.mods.isLeftButtonDown())
        {
            if (buildMode == CityToolbar::BuildMode::select)
            {
                if (tipHit)
                {
                    selectedId = tipModuleId;
                    selectedKind = CityObjectKind::module;
                    selectedTipIndex = tipIndex;
                    dragMode = DragMode::none;
                }
                else
                {
                    selectedId = hit.id;
                    selectedKind = hit.kind;
                    selectedTipIndex = -1;
                    dragMode = hit.kind == CityObjectKind::none ? DragMode::none : DragMode::object;
                }
            }
            else if (buildMode == CityToolbar::BuildMode::cable)
            {
                handleCableClick (hit, event.mods.isCommandDown());
            }
            else if (buildMode == CityToolbar::BuildMode::polygon)
            {
                int platterId = -1;
                int standIndex = -1;
                int plankId = -1;
                const auto radius = snappedModuleRadiusForPlacement();
                const auto attachWorld = worldForPointer (event.position);

                if (findAvailableMount (attachWorld, currentTimeSeconds(), radius, platterId, standIndex))
                {
                    auto& placed = city.addModule (attachWorld,
                                                   defaultSides,
                                                   radius,
                                                   defaultFlapDepth,
                                                   defaultRateDivision,
                                                   defaultPhase);
                    placed.attachedPlatterId = platterId;
                    placed.attachedStandIndex = standIndex;

                    if (const auto* platter = city.findPlatter (platterId))
                    {
                        placed.position = platter->standPosition (standIndex, currentTimeSeconds(), globalTempoBpm);
                        placed.elevation = platter->elevation + 12.0f;
                        placed.rotation = platter->rotation + platter->rotationAt (currentTimeSeconds(), globalTempoBpm)
                                         + juce::MathConstants<float>::twoPi * static_cast<float> (standIndex)
                                            / static_cast<float> (juce::jlimit (1, 8, platter->stands));
                    }

                    selectedId = placed.id;
                    selectedKind = CityObjectKind::module;
                    selectedTipIndex = -1;
                    dragMode = DragMode::none;
                    activeCollisions.clear();
                }
                else if (findAvailablePlankSocket (attachWorld, radius, plankId))
                {
                    auto& placed = city.addModule (attachWorld,
                                                   defaultSides,
                                                   radius,
                                                   defaultFlapDepth,
                                                   defaultRateDivision,
                                                   defaultPhase);
                    placed.attachedPlankId = plankId;

                    if (const auto* plank = city.findPlank (plankId))
                    {
                        placed.position = plank->socketPosition();
                        placed.elevation = plank->elevation + 10.0f;
                        placed.rotation = plank->rotation + plank->angle;
                    }

                    selectedId = placed.id;
                    selectedKind = CityObjectKind::module;
                    selectedTipIndex = -1;
                    dragMode = DragMode::none;
                    activeCollisions.clear();
                }
                else if (hit.kind != CityObjectKind::none && hit.kind != CityObjectKind::block)
                {
                    selectedId = hit.id;
                    selectedKind = hit.kind;
                    selectedTipIndex = -1;
                    dragMode = DragMode::object;
                }
                else
                {
                    auto& placed = city.addModule (lastDragWorld,
                                                   defaultSides,
                                                   radius,
                                                   defaultFlapDepth,
                                                   defaultRateDivision,
                                                   defaultPhase);
                    selectedId = placed.id;
                    selectedKind = CityObjectKind::module;
                    selectedTipIndex = -1;
                    dragMode = DragMode::none;
                    activeCollisions.clear();
                }
            }
            else if (buildMode == CityToolbar::BuildMode::platter)
            {
                int platterId = -1;
                int standIndex = -1;
                int plankId = -1;
                const auto diameter = snappedPlatterDiameterForPlacement();
                const auto attachWorld = worldForPointer (event.position);
                const auto hasMount = findAvailableMount (attachWorld, currentTimeSeconds(), diameter * 0.5f, platterId, standIndex);
                const auto hasPlankSocket = ! hasMount && findAvailablePlankSocket (attachWorld, diameter * 0.5f, plankId);

                auto& placed = city.addPlatter (lastDragWorld,
                                                defaultPlatterStands,
                                                diameter,
                                                defaultPlatterRateDivision,
                                                defaultPlatterPhase);

                if (hasMount)
                {
                    placed.attachedPlatterId = platterId;
                    placed.attachedStandIndex = standIndex;

                    if (const auto* parent = city.findPlatter (platterId))
                    {
                        placed.position = parent->standPosition (standIndex, currentTimeSeconds(), globalTempoBpm);
                        placed.elevation = parent->elevation + 18.0f;
                        placed.rotation = parent->rotation + parent->rotationAt (currentTimeSeconds(), globalTempoBpm)
                                         + juce::MathConstants<float>::twoPi * static_cast<float> (standIndex)
                                            / static_cast<float> (juce::jlimit (1, 8, parent->stands));
                    }
                }
                else if (hasPlankSocket)
                {
                    placed.attachedPlankId = plankId;

                    if (const auto* plank = city.findPlank (plankId))
                    {
                        placed.position = plank->socketPosition();
                        placed.elevation = plank->elevation + 16.0f;
                        placed.rotation = plank->rotation + plank->angle;
                    }
                }

                selectedId = placed.id;
                selectedKind = CityObjectKind::platter;
                selectedTipIndex = -1;
                dragMode = DragMode::none;
                activeCollisions.clear();
            }
            else if (buildMode == CityToolbar::BuildMode::plank)
            {
                int platterId = -1;
                int standIndex = -1;
                const auto length = defaultPlankLength;
                const auto attachWorld = worldForPointer (event.position);

                if (findAvailableMount (attachWorld, currentTimeSeconds(), length * 0.25f, platterId, standIndex))
                {
                    auto& plank = city.addPlank (attachWorld, length, defaultPlankAngle);
                    plank.attachedPlatterId = platterId;
                    plank.attachedStandIndex = standIndex;

                    if (const auto* platter = city.findPlatter (platterId))
                    {
                        plank.position = platter->standPosition (standIndex, currentTimeSeconds(), globalTempoBpm);
                        plank.elevation = platter->elevation + 10.0f;
                        plank.rotation = platter->rotation + platter->rotationAt (currentTimeSeconds(), globalTempoBpm)
                                       + juce::MathConstants<float>::twoPi * static_cast<float> (standIndex)
                                            / static_cast<float> (juce::jlimit (1, 8, platter->stands));
                    }

                    selectedId = plank.id;
                    selectedKind = CityObjectKind::plank;
                    selectedTipIndex = -1;
                    dragMode = DragMode::none;
                    activeCollisions.clear();
                }
                else
                {
                    auto& plank = city.addPlank (lastDragWorld, length, defaultPlankAngle);
                    selectedId = plank.id;
                    selectedKind = CityObjectKind::plank;
                    selectedTipIndex = -1;
                    dragMode = DragMode::none;
                    activeCollisions.clear();
                }
            }
            else if (buildMode == CityToolbar::BuildMode::block)
            {
                if (auto* existing = city.findTopBlockAt (lastDragWorld))
                {
                    existing->levels = juce::jlimit (1, 16, existing->levels + 1);
                    selectedId = existing->id;
                    selectedKind = CityObjectKind::block;
                    selectedTipIndex = -1;
                }
                else
                {
                    auto& placed = city.addBlock (lastDragWorld, snappedBlockSizeForPlacement(), defaultBlockLevels);
                    selectedId = placed.id;
                    selectedKind = CityObjectKind::block;
                    selectedTipIndex = -1;
                }

                dragMode = DragMode::none;
                activeCollisions.clear();
            }
            else if (hit.kind != CityObjectKind::none && hit.kind != CityObjectKind::block)
            {
                selectedId = hit.id;
                selectedKind = hit.kind;
                selectedTipIndex = -1;
                dragMode = DragMode::object;
            }
            else
            {
                dragMode = DragMode::none;
                activeCollisions.clear();
            }
        }
    }

    syncToolbar();
    resized();
    repaint();
}

void CityComponent::mouseMove (const juce::MouseEvent& event)
{
    {
        const juce::ScopedLock lock (modelLock);
        mouseInCanvas = true;
        hoverPointerWorld = worldForPointer (event.position);
        hoverWorld = snapToBuildGrid (hoverPointerWorld, currentBuildFootprint());
        hoverTipModuleId = -1;
        hoverTipIndex = -1;
        findTipAt (event.position, hoverTipModuleId, hoverTipIndex);
    }

    repaint();
}

void CityComponent::mouseEnter (const juce::MouseEvent& event)
{
    mouseMove (event);
}

void CityComponent::mouseExit (const juce::MouseEvent&)
{
    {
        const juce::ScopedLock lock (modelLock);
        mouseInCanvas = false;
        hoverTipModuleId = -1;
        hoverTipIndex = -1;
    }

    repaint();
}

void CityComponent::mouseDrag (const juce::MouseEvent& event)
{
    {
        const juce::ScopedLock lock (modelLock);
        hoverPointerWorld = worldForPointer (event.position);
        hoverWorld = snapToBuildGrid (hoverPointerWorld, currentBuildFootprint());

        if (dragMode == DragMode::pan)
        {
            const auto delta = event.position - lastMouse;
            projector.pan += delta;
            lastMouse = event.position;
            return;
        }

        if (dragMode == DragMode::object)
        {
            const auto world = projector.unprojectToElevation (event.position, selectedElevation());
            const auto delta = world - lastDragWorld;

            if (selectedKind == CityObjectKind::module)
            {
                if (auto* selected = city.findModule (selectedId))
                {
                    selected->attachedPlatterId = -1;
                    selected->attachedStandIndex = -1;
                    selected->attachedPlankId = -1;
                    selected->position += delta;
                    selected->elevation = city.elevationAt (selected->position);
                }
            }
            else if (selectedKind == CityObjectKind::platter)
            {
                if (auto* selected = city.findPlatter (selectedId))
                {
                    selected->attachedPlatterId = -1;
                    selected->attachedStandIndex = -1;
                    selected->attachedPlankId = -1;
                    selected->position += delta;
                    selected->elevation = city.elevationAt (selected->position);
                }
            }
            else if (selectedKind == CityObjectKind::block)
            {
                if (auto* selected = city.findBlock (selectedId))
                    selected->position += delta;
            }
            else if (selectedKind == CityObjectKind::plank)
            {
                if (auto* selected = city.findPlank (selectedId))
                {
                    selected->attachedPlatterId = -1;
                    selected->attachedStandIndex = -1;
                    selected->position += delta;
                    selected->elevation = city.elevationAt (selected->position) + 10.0f;
                }
            }
            else if (selectedKind == CityObjectKind::powerSwitch)
            {
                if (auto* selected = city.findPowerSwitch (selectedId))
                {
                    selected->position += delta;
                    selected->elevation = city.elevationAt (selected->position);
                }
            }
            else if (selectedKind == CityObjectKind::powerSource)
            {
                if (auto* selected = city.findPowerSource (selectedId))
                {
                    selected->position += delta;
                    selected->elevation = city.elevationAt (selected->position);
                }
            }

            lastDragWorld = world;
            activeCollisions.clear();
        }
        else if (dragMode == DragMode::resizeHandle)
        {
            dragSelectedResizeHandle (event.position);
        }
        else if (dragMode == DragMode::phaseHandle)
        {
            dragSelectedPhaseHandle (event.position);
        }
        else if (dragMode == DragMode::cableWire)
        {
            lastMouse = event.position;
        }
    }

    openGLContext.triggerRepaint();
    repaint();
}

void CityComponent::mouseUp (const juce::MouseEvent& event)
{
    if (dragMode == DragMode::cableWire)
    {
        const juce::ScopedLock lock (modelLock);
        const auto releaseWorld = worldForPointer (event.position);
        completeCableDrag (city.hitTest (releaseWorld, currentTimeSeconds(), globalTempoBpm));
    }

    dragMode = DragMode::none;
    syncToolbar();
    repaint();
}

void CityComponent::mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    const auto factor = std::pow (1.18f, wheel.deltaY * 8.0f);
    zoomAt (event.position, factor);
}

bool CityComponent::keyPressed (const juce::KeyPress& key)
{
    const auto text = key.getTextCharacter();
    const auto lowerText = static_cast<char> (std::tolower (static_cast<unsigned char> (text)));

    if (key.getModifiers().isCommandDown())
    {
        if (lowerText == 'z')
            return key.getModifiers().isShiftDown() ? redo() : undo();

        if (lowerText == 'c')
            return copySelectionToClipboard();

        if (lowerText == 'v')
            return pasteSelectionFromClipboard();
    }

    if (key == juce::KeyPress::tabKey)
    {
        toggleViewMode();
        return true;
    }

    if (lowerText == 'o')
    {
        setTriggerTelemetryVisible (! triggerTelemetryVisible);
        return true;
    }

    if (lowerText == 'r')
    {
        setActivationRingsVisible (! activationRingsVisible);
        return true;
    }

    if (lowerText == 'w' || lowerText == 'a' || lowerText == 's' || lowerText == 'd')
    {
        const auto step = 58.0f;

        {
            const juce::ScopedLock lock (modelLock);

            if (lowerText == 'w')
                projector.pan.y -= step;
            else if (lowerText == 's')
                projector.pan.y += step;
            else if (lowerText == 'a')
                projector.pan.x -= step;
            else if (lowerText == 'd')
                projector.pan.x += step;
        }

        repaint();
        return true;
    }

    if (text >= '1' && text <= '6')
    {
        setSelectedSides (static_cast<int> (text - '1') + 3);
        return true;
    }

    if (text == '[' || text == ']')
    {
        adjustSelectedOrDefaultRadius (text == '[' ? -4.0f : 4.0f);
        return true;
    }

    if (text == '{' || text == '}')
    {
        adjustSelectedOrDefaultFlapDepth (text == '{' ? -3.0f : 3.0f);
        return true;
    }

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (deleteSelectedObject())
            return true;
    }

    if (key == juce::KeyPress::escapeKey)
    {
        {
            const juce::ScopedLock lock (modelLock);
            selectedId = -1;
            selectedKind = CityObjectKind::none;
            selectedTipIndex = -1;
        }

        syncToolbar();
        repaint();
        return true;
    }

    return false;
}

void CityComponent::timerCallback()
{
    const auto now = juce::Time::getMillisecondCounterHiRes() * 0.001;
    const auto delta = juce::jlimit (0.0, 0.1, now - lastFrameTimeSeconds);
    lastFrameTimeSeconds = now;

    {
        const juce::ScopedLock lock (modelLock);

        if (transportPlaying)
            transportTimeSeconds += delta;
        else
            return;

        const auto frameTimeSeconds = transportTimeSeconds;

        updateAttachedStructures (frameTimeSeconds);

        for (auto& module : city.modules())
        {
            module.flash = juce::jmax (0.0f, module.flash - static_cast<float> (delta * 4.4));
            module.soundingPitchFlash = juce::jmax (0.0f, module.soundingPitchFlash - static_cast<float> (delta * 3.2));
        }

        for (auto& platter : city.platters())
        {
            platter.flash = juce::jmax (0.0f, platter.flash - static_cast<float> (delta * 4.0));

            for (auto& flash : platter.standFlashes)
                flash = juce::jmax (0.0f, flash - static_cast<float> (delta * 4.2));
        }

        for (auto& block : city.blocks())
            block.flash = juce::jmax (0.0f, block.flash - static_cast<float> (delta * 4.0));

        for (auto& cue : tipTriggerCues)
            cue.age += static_cast<float> (delta);

        tipTriggerCues.erase (std::remove_if (tipTriggerCues.begin(),
                                              tipTriggerCues.end(),
                                              [] (const CityRenderState::TipTriggerCue& cue)
                                              {
                                                  const auto life = cue.contactOnly ? (cue.tipTip ? 0.28f : 0.22f)
                                                                                   : (cue.tipTip ? 0.42f : 0.30f);
                                                  return cue.age >= life;
                                              }),
                              tipTriggerCues.end());

        for (auto& powerSwitch : city.powerSwitches())
        {
            powerSwitch.flash = juce::jmax (0.0f, powerSwitch.flash - static_cast<float> (delta * 5.0));
            powerSwitch.pulse = juce::jmax (0.0f, powerSwitch.pulse - static_cast<float> (delta * 0.72));

            if (! powerSwitch.powered && powerSwitch.restoreAtSeconds >= 0.0 && frameTimeSeconds >= powerSwitch.restoreAtSeconds)
            {
                powerSwitch.powered = true;
                powerSwitch.restoreAtSeconds = -1.0;
                powerSwitch.flash = 1.0f;
                announcePowerChange (powerSwitch, 8, 0.88f, frameTimeSeconds);
            }
        }

        for (auto& source : city.powerSources())
        {
            source.flash = juce::jmax (0.0f, source.flash - static_cast<float> (delta * 4.6));

            if (! source.powered && source.restoreAtSeconds >= 0.0 && frameTimeSeconds >= source.restoreAtSeconds)
            {
                source.powered = true;
                source.restoreAtSeconds = -1.0;
                source.flash = 1.0f;
            }
        }

        if (soundTriggerResetRequested.exchange (false, std::memory_order_acq_rel))
        {
            activeCollisions.clear();
            activePowerSwitchTouches.clear();
            activeTipContacts.clear();
            tipContactReleaseTimes.clear();
            lastCollisionSoundTimeSeconds = -1.0;
        }

        updatePowerSwitches (frameTimeSeconds);
        updateCollisions (frameTimeSeconds);
    }

    repaint();
}

double CityComponent::currentTimeSeconds() const noexcept
{
    return transportTimeSeconds;
}

void CityComponent::updateAttachedStructures (double timeSeconds)
{
    for (int pass = 0; pass < 8; ++pass)
    {
        auto changed = false;

        for (auto& platter : city.platters())
        {
            if (platter.attachedPlatterId < 0 || platter.attachedStandIndex < 0)
                continue;

            if (const auto* parent = city.findPlatter (platter.attachedPlatterId))
            {
                const auto standIndex = juce::jlimit (0, parent->stands - 1, platter.attachedStandIndex);
                platter.attachedStandIndex = standIndex;
                const auto angle = parent->rotation + parent->rotationAt (timeSeconds, globalTempoBpm)
                                 + juce::MathConstants<float>::twoPi * static_cast<float> (standIndex)
                                    / static_cast<float> (juce::jlimit (1, 8, parent->stands));
                const auto nextPosition = parent->standPosition (standIndex, timeSeconds, globalTempoBpm);
                const auto nextElevation = parent->elevation + 18.0f;
                const auto nextRotation = angle;
                changed = changed
                       || distance (platter.position, nextPosition) > 0.001f
                       || std::abs (platter.elevation - nextElevation) > 0.001f
                       || std::abs (platter.rotation - nextRotation) > 0.001f;
                platter.position = nextPosition;
                platter.elevation = nextElevation;
                platter.rotation = nextRotation;
            }
            else
            {
                platter.attachedPlatterId = -1;
                platter.attachedStandIndex = -1;
                platter.elevation = city.elevationAt (platter.position);
            }
        }

        if (! changed)
            break;
    }

    for (auto& plank : city.planks())
    {
        if (plank.attachedPlatterId < 0 || plank.attachedStandIndex < 0)
            continue;

        if (const auto* parent = city.findPlatter (plank.attachedPlatterId))
        {
            const auto standIndex = juce::jlimit (0, parent->stands - 1, plank.attachedStandIndex);
            plank.attachedStandIndex = standIndex;
            const auto angle = parent->rotation + parent->rotationAt (timeSeconds, globalTempoBpm)
                             + juce::MathConstants<float>::twoPi * static_cast<float> (standIndex)
                                / static_cast<float> (juce::jlimit (1, 8, parent->stands));
            plank.position = parent->standPosition (standIndex, timeSeconds, globalTempoBpm);
            plank.elevation = parent->elevation + 10.0f;
            plank.rotation = angle;
        }
        else
        {
            plank.attachedPlatterId = -1;
            plank.attachedStandIndex = -1;
            plank.elevation = city.elevationAt (plank.position) + 10.0f;
        }
    }

    for (auto& module : city.modules())
    {
        if (module.attachedPlankId >= 0)
        {
            if (const auto* plank = city.findPlank (module.attachedPlankId))
            {
                module.position = plank->socketPosition();
                module.elevation = plank->elevation + 10.0f;
                module.rotation = plank->rotation + plank->angle;
                continue;
            }

            module.attachedPlankId = -1;
            module.elevation = city.elevationAt (module.position);
        }

        if (module.attachedPlatterId < 0 || module.attachedStandIndex < 0)
            continue;

        if (const auto* platter = city.findPlatter (module.attachedPlatterId))
        {
            const auto standIndex = juce::jlimit (0, platter->stands - 1, module.attachedStandIndex);
            module.attachedStandIndex = standIndex;
            const auto angle = platter->rotation + platter->rotationAt (timeSeconds, globalTempoBpm)
                             + juce::MathConstants<float>::twoPi * static_cast<float> (standIndex)
                                / static_cast<float> (juce::jlimit (1, 8, platter->stands));
            module.position = platter->standPosition (standIndex, timeSeconds, globalTempoBpm);
            module.elevation = platter->elevation + 12.0f;
            module.rotation = angle;
        }
        else
        {
            module.attachedPlatterId = -1;
            module.attachedStandIndex = -1;
            module.elevation = city.elevationAt (module.position);
        }
    }

    for (auto& platter : city.platters())
    {
        if (platter.attachedPlankId < 0)
            continue;

        if (const auto* plank = city.findPlank (platter.attachedPlankId))
        {
            platter.position = plank->socketPosition();
            platter.elevation = plank->elevation + 16.0f;
            platter.rotation = plank->rotation + plank->angle;
        }
        else
        {
            platter.attachedPlankId = -1;
            platter.elevation = city.elevationAt (platter.position);
        }
    }
}

bool CityComponent::findAvailableMount (Vec2 worldPosition,
                                        double timeSeconds,
                                        float radius,
                                        int& platterId,
                                        int& standIndex) const
{
    auto bestDistance = std::numeric_limits<float>::max();
    auto found = false;

    for (const auto& platter : city.platters())
    {
        for (int i = 0; i < platter.stands; ++i)
        {
            auto occupied = false;
            for (const auto& module : city.modules())
            {
                if (module.attachedPlatterId == platter.id
                 && juce::jlimit (0, platter.stands - 1, module.attachedStandIndex) == i)
                {
                    occupied = true;
                    break;
                }
            }

            for (const auto& childPlatter : city.platters())
            {
                if (childPlatter.attachedPlatterId == platter.id
                 && juce::jlimit (0, platter.stands - 1, childPlatter.attachedStandIndex) == i)
                {
                    occupied = true;
                    break;
                }
            }

            for (const auto& plank : city.planks())
            {
                if (plank.attachedPlatterId == platter.id
                 && juce::jlimit (0, platter.stands - 1, plank.attachedStandIndex) == i)
                {
                    occupied = true;
                    break;
                }
            }

            if (occupied)
                continue;

            const auto stand = platter.standPosition (i, timeSeconds, globalTempoBpm);
            const auto d = distance (worldPosition, stand);
            const auto snapRadius = juce::jmax (platter.mountRadius() * 1.8f, juce::jmin (radius, buildGridSize * 0.72f));

            if (d <= snapRadius && d < bestDistance)
            {
                bestDistance = d;
                platterId = platter.id;
                standIndex = i;
                found = true;
            }
        }
    }

    return found;
}

bool CityComponent::findAvailablePlankSocket (Vec2 worldPosition, float radius, int& plankId) const
{
    auto bestDistance = std::numeric_limits<float>::max();
    auto found = false;

    for (const auto& plank : city.planks())
    {
        auto occupied = false;

        for (const auto& module : city.modules())
        {
            if (module.attachedPlankId == plank.id)
            {
                occupied = true;
                break;
            }
        }

        if (! occupied)
        {
            for (const auto& platter : city.platters())
            {
                if (platter.attachedPlankId == plank.id)
                {
                    occupied = true;
                    break;
                }
            }
        }

        if (occupied)
            continue;

        const auto socket = plank.socketPosition();
        const auto d = distance (worldPosition, socket);
        const auto snapRadius = juce::jmax (plank.socketRadius() * 1.8f, juce::jmin (radius, buildGridSize * 0.72f));

        if (d <= snapRadius && d < bestDistance)
        {
            bestDistance = d;
            plankId = plank.id;
            found = true;
        }
    }

    return found;
}

bool CityComponent::findTipAt (juce::Point<float> screenPoint, int& moduleId, int& tipIndex) const
{
    auto view = projector;
    view.centre = getLocalBounds().toFloat().getCentre();

    auto bestDistance = 26.0f;
    auto found = false;

    for (const auto& module : city.modules())
    {
        const auto centre = view.project ({ module.position.x, module.position.y, module.elevation });
        const auto screenRadius = polygonTipReach (module.sides, module.radius, module.flapDepth) * view.zoom + 32.0f;

        if (centre.getDistanceFrom (screenPoint) > screenRadius)
            continue;

        const auto timeSeconds = module.powered ? currentTimeSeconds() : 0.0;
        const auto flaps = module.flapTriangles (timeSeconds, globalTempoBpm);

        for (size_t i = 0; i < flaps.size(); ++i)
        {
            const auto tip = view.project (flaps[i][2]);
            const auto d = tip.getDistanceFrom (screenPoint);

            if (d < bestDistance)
            {
                bestDistance = d;
                moduleId = module.id;
                tipIndex = static_cast<int> (i);
                found = true;
            }
        }
    }

    return found;
}

void CityComponent::updateCollisions (double timeSeconds)
{
    if (! soundTriggersArmed.load (std::memory_order_acquire))
    {
        activeCollisions.clear();
        activeTipContacts.clear();
        tipContactReleaseTimes.clear();
        lastCollisionSoundTimeSeconds = -1.0;
        return;
    }

    struct CollisionProxy
    {
        CityObjectKind kind = CityObjectKind::none;
        int id = -1;
        FoldingModule module;
        float collisionRadius = 0.0f;
    };

    auto keyFor = [] (const CollisionProxy& proxy)
    {
        return proxy.id;
    };

    auto flashProxy = [this] (const CollisionProxy& proxy)
    {
        if (proxy.kind == CityObjectKind::module)
        {
            if (auto* module = city.findModule (proxy.id))
                module->flash = juce::jmax (module->flash, 1.0f);
        }
    };

    std::set<std::pair<int, int>> currentCollisions;
    std::set<std::pair<int, int>> currentTipContacts;
    std::vector<CollisionProxy> proxies;
    proxies.reserve (city.modules().size());

    auto tipKey = [] (const CollisionProxy& proxy, size_t tipIndex)
    {
        return proxy.id * 32 + static_cast<int> (tipIndex);
    };

    auto bodyKey = [] (const CollisionProxy& proxy)
    {
        return proxy.id * 32 + 31;
    };

    auto closestPointOnSegment = [] (Vec3 point, Vec3 a, Vec3 b)
    {
        const auto ab = Vec2 { b.x - a.x, b.y - a.y };
        const auto ap = Vec2 { point.x - a.x, point.y - a.y };
        const auto denom = ab.x * ab.x + ab.y * ab.y;
        const auto t = denom > 0.0001f ? juce::jlimit (0.0f, 1.0f, (ap.x * ab.x + ap.y * ab.y) / denom) : 0.0f;

        return Vec3 { a.x + (b.x - a.x) * t,
                      a.y + (b.y - a.y) * t,
                      a.z + (b.z - a.z) * t };
    };

    auto pointInTriangle = [] (Vec3 point, const std::array<Vec3, 3>& triangle, float& interpolatedZ)
    {
        const auto ax = triangle[0].x;
        const auto ay = triangle[0].y;
        const auto bx = triangle[1].x;
        const auto by = triangle[1].y;
        const auto cx = triangle[2].x;
        const auto cy = triangle[2].y;
        const auto denom = (by - cy) * (ax - cx) + (cx - bx) * (ay - cy);

        if (std::abs (denom) < 0.0001f)
            return false;

        const auto w0 = ((by - cy) * (point.x - cx) + (cx - bx) * (point.y - cy)) / denom;
        const auto w1 = ((cy - ay) * (point.x - cx) + (ax - cx) * (point.y - cy)) / denom;
        const auto w2 = 1.0f - w0 - w1;
        constexpr auto tolerance = -0.015f;

        if (w0 < tolerance || w1 < tolerance || w2 < tolerance)
            return false;

        interpolatedZ = triangle[0].z * w0 + triangle[1].z * w1 + triangle[2].z * w2;
        return true;
    };

    auto findTipSurfaceContact = [&closestPointOnSegment, &pointInTriangle] (Vec3 tip,
                                                                            const FoldingModule& module,
                                                                            const std::vector<std::array<Vec3, 3>>& flaps,
                                                                            float xyTolerance,
                                                                            float zTolerance,
                                                                            Vec3& target)
    {
        auto bestScore = std::numeric_limits<float>::max();
        auto found = false;

        auto considerTarget = [&] (Vec3 candidate)
        {
            const auto xyDistance = distance ({ tip.x, tip.y }, { candidate.x, candidate.y });
            const auto zDistance = std::abs (tip.z - candidate.z);

            if (xyDistance > xyTolerance || zDistance > zTolerance)
                return;

            const auto score = xyDistance + zDistance * 0.38f;

            if (score < bestScore)
            {
                bestScore = score;
                target = candidate;
                found = true;
            }
        };

        const auto floor = module.floorVertices();

        if (floor.size() >= 3)
        {
            for (size_t i = 1; i + 1 < floor.size(); ++i)
            {
                std::array<Vec3, 3> floorTriangle { floor[0], floor[i], floor[i + 1] };
                auto z = module.elevation;

                if (pointInTriangle (tip, floorTriangle, z))
                    considerTarget ({ tip.x, tip.y, z });
            }

            for (size_t i = 0; i < floor.size(); ++i)
                considerTarget (closestPointOnSegment (tip, floor[i], floor[(i + 1) % floor.size()]));
        }

        for (const auto& flap : flaps)
        {
            auto z = 0.0f;

            if (pointInTriangle (tip, flap, z))
                considerTarget ({ tip.x, tip.y, z });

            considerTarget (closestPointOnSegment (tip, flap[0], flap[1]));
            considerTarget (closestPointOnSegment (tip, flap[1], flap[2]));
            considerTarget (closestPointOnSegment (tip, flap[2], flap[0]));
        }

        return found;
    };

    for (const auto& module : city.modules())
        if (city.isPowered (CityObjectKind::module, module.id, module.position))
            proxies.push_back ({ CityObjectKind::module,
                                 module.id,
                                 module,
                                 module.collisionRadiusAt (timeSeconds, globalTempoBpm) });

    std::sort (proxies.begin(), proxies.end(), [] (const CollisionProxy& a, const CollisionProxy& b)
    {
        return a.module.position.x - a.collisionRadius < b.module.position.x - b.collisionRadius;
    });

    for (size_t i = 0; i < proxies.size(); ++i)
    {
        const auto& a = proxies[i];
        const auto aMaxX = a.module.position.x + a.collisionRadius;

        for (size_t j = i + 1; j < proxies.size(); ++j)
        {
            const auto& b = proxies[j];

            if (b.module.position.x - b.collisionRadius > aMaxX)
                break;

            if (std::abs (a.module.elevation - b.module.elevation) > 54.0f)
                continue;

            const auto collisionDistance = a.collisionRadius + b.collisionRadius;
            const auto dx = a.module.position.x - b.module.position.x;
            const auto dy = a.module.position.y - b.module.position.y;

            if (std::abs (dx) > collisionDistance || std::abs (dy) > collisionDistance)
                continue;

            if (dx * dx + dy * dy <= collisionDistance * collisionDistance)
            {
                const auto pair = sortedPair (keyFor (a), keyFor (b));
                currentCollisions.insert (pair);

                flashProxy (a);
                flashProxy (b);

                const auto flapsA = a.module.flapTriangles (timeSeconds, globalTempoBpm);
                const auto flapsB = b.module.flapTriangles (timeSeconds, globalTempoBpm);
                const auto tipContactDistance = juce::jlimit (18.0f,
                                                              42.0f,
                                                              (a.module.flapDepth + b.module.flapDepth) * 0.28f);
                const auto tipContactZTolerance = juce::jlimit (24.0f,
                                                                 64.0f,
                                                                 (a.module.flapDepth + b.module.flapDepth) * 0.42f);
                auto anyTipSurfaceContact = false;
                auto audibleTipContact = false;

                auto handleTipContact = [&] (const CollisionProxy& source,
                                             size_t tipIndex,
                                             Vec3 tip,
                                             const CollisionProxy& targetProxy,
                                             Vec3 target,
                                             std::pair<int, int> tipPair)
                {
                    anyTipSurfaceContact = true;

                    const auto inserted = currentTipContacts.insert (tipPair);

                    if (! inserted.second)
                        return;

                    auto& random = juce::Random::getSystemRandom();
                    const auto probabilityHit = source.module.shouldPlayTip (static_cast<int> (tipIndex), random);
                    const auto tipPitch = source.module.randomPitchForTip (static_cast<int> (tipIndex), random);
                    const auto muted = tipPitch <= 0.0f || ! probabilityHit;
                    const auto alreadyActive = activeTipContacts.contains (tipPair);

                    addTipContactCue (source.module, tip, targetProxy.module, target, muted, alreadyActive);

                    if (alreadyActive || muted)
                        return;

                    audibleTipContact = true;

                    if (auto* module = city.findModule (source.module.id))
                    {
                        module->soundingPitch = tipPitch;
                        module->soundingPitchFlash = 1.0f;
                        module->flash = juce::jmax (module->flash, 1.0f);
                    }

                    if (auto* module = city.findModule (targetProxy.module.id))
                        module->flash = juce::jmax (module->flash, 0.72f);

                    triggerCitySound (SonicEventType::tipTrigger,
                                      source.module.sides,
                                      targetProxy.module.sides,
                                      source.module.foldAt (timeSeconds, globalTempoBpm),
                                      targetProxy.module.foldAt (timeSeconds, globalTempoBpm),
                                      tipPitch,
                                      source.module.soundLanguageForTip (static_cast<int> (tipIndex)),
                                      source.module.soundProgramForTip (static_cast<int> (tipIndex)),
                                      static_cast<int> (tipIndex));
                };

                for (size_t tipA = 0; tipA < flapsA.size(); ++tipA)
                {
                    const auto apexA = flapsA[tipA][2];
                    Vec3 target {};

                    if (! findTipSurfaceContact (apexA, b.module, flapsB, tipContactDistance, tipContactZTolerance, target))
                        continue;

                    const auto tipPair = sortedPair (tipKey (a, tipA), bodyKey (b));
                    handleTipContact (a, tipA, apexA, b, target, tipPair);
                }

                for (size_t tipB = 0; tipB < flapsB.size(); ++tipB)
                {
                    const auto apexB = flapsB[tipB][2];
                    Vec3 target {};

                    if (! findTipSurfaceContact (apexB, a.module, flapsA, tipContactDistance, tipContactZTolerance, target))
                        continue;

                    const auto tipPair = sortedPair (tipKey (b, tipB), bodyKey (a));
                    handleTipContact (b, tipB, apexB, a, target, tipPair);
                }

                if (! anyTipSurfaceContact
                 && ! audibleTipContact
                 && ! activeCollisions.contains (pair)
                 && (lastCollisionSoundTimeSeconds < 0.0 || timeSeconds - lastCollisionSoundTimeSeconds > 0.095))
                {
                    lastCollisionSoundTimeSeconds = timeSeconds;
                    triggerCitySound (SonicEventType::collision,
                                      a.module.sides,
                                      b.module.sides,
                                      a.module.foldAt (timeSeconds, globalTempoBpm),
                                      b.module.foldAt (timeSeconds, globalTempoBpm));
                }
            }
        }
    }

    const auto minBreakSeconds = (60.0 / static_cast<double> (juce::jmax (1.0f, globalTempoBpm))) / 16.0;
    auto gatedTipContacts = currentTipContacts;

    for (const auto& contact : activeTipContacts)
    {
        if (currentTipContacts.contains (contact))
        {
            tipContactReleaseTimes.erase (contact);
            continue;
        }

        auto release = tipContactReleaseTimes.find (contact);

        if (release == tipContactReleaseTimes.end())
            release = tipContactReleaseTimes.insert ({ contact, timeSeconds }).first;

        if (timeSeconds - release->second < minBreakSeconds)
            gatedTipContacts.insert (contact);
        else
            tipContactReleaseTimes.erase (contact);
    }

    for (const auto& contact : currentTipContacts)
    {
        const auto release = tipContactReleaseTimes.find (contact);

        if (release != tipContactReleaseTimes.end() && timeSeconds - release->second >= minBreakSeconds)
            tipContactReleaseTimes.erase (release);
    }

    activeCollisions = std::move (currentCollisions);
    activeTipContacts = std::move (gatedTipContacts);
}

void CityComponent::updatePowerSwitches (double timeSeconds)
{
    if (! soundTriggersArmed.load (std::memory_order_acquire))
    {
        activePowerSwitchTouches.clear();
        return;
    }

    if (city.powerSwitches().empty())
    {
        activePowerSwitchTouches.clear();
        return;
    }

    struct SwitchProxy
    {
        int key = 0;
        FoldingModule module;
    };

    std::vector<SwitchProxy> proxies;
    proxies.reserve (city.modules().size());

    for (const auto& module : city.modules())
        if (city.isPowered (CityObjectKind::module, module.id, module.position))
            proxies.push_back ({ module.id, module });

    std::set<std::pair<int, int>> currentTouches;

    auto hasOtherEnergizedSwitch = [this] (int switchId)
    {
        for (const auto& other : city.powerSwitches())
            if (other.id != switchId && city.isSwitchEnergized (other))
                return true;

        return false;
    };

    for (const auto& proxy : proxies)
    {
        const auto flaps = proxy.module.flapTriangles (timeSeconds, globalTempoBpm);

        for (size_t flapIndex = 0; flapIndex < flaps.size(); ++flapIndex)
        {
            const auto& flap = flaps[flapIndex];
            const auto apex = flap[2];
            const auto apexPosition = Vec2 { apex.x, apex.y };

            for (auto& powerSwitch : city.powerSwitches())
            {
                if (std::abs (apex.z - powerSwitch.elevation) > 96.0f)
                    continue;

                const auto dx = apexPosition.x - powerSwitch.position.x;
                const auto dy = apexPosition.y - powerSwitch.position.y;

                if (std::abs (dx) > powerSwitch.triggerRadius || std::abs (dy) > powerSwitch.triggerRadius)
                    continue;

                if (squaredDistance (apexPosition, powerSwitch.position) > powerSwitch.triggerRadius * powerSwitch.triggerRadius)
                    continue;

                powerSwitch.flash = juce::jmax (powerSwitch.flash, 0.78f);

                const auto touch = std::make_pair (proxy.key, powerSwitch.id);
                const auto insertResult = currentTouches.insert (touch);

                if (! insertResult.second)
                    continue;

                auto& random = juce::Random::getSystemRandom();
                const auto tipPitch = proxy.module.randomPitchForTip (static_cast<int> (flapIndex), random);
                const auto probabilityHit = proxy.module.shouldPlayTip (static_cast<int> (flapIndex), random);
                addTipTriggerCue (proxy.module, apex, powerSwitch, tipPitch <= 0.0f || ! probabilityHit, true);

                if (! activePowerSwitchTouches.contains (touch))
                {
                    if (tipPitch <= 0.0f || ! probabilityHit)
                    {
                        addTipTriggerCue (proxy.module, apex, powerSwitch, true);
                        continue;
                    }

                    auto triggered = false;

                    if (powerSwitch.activationMode == PowerSwitchActivationMode::tipToggle)
                    {
                        if (city.isSwitchEnergized (powerSwitch) && ! hasOtherEnergizedSwitch (powerSwitch.id))
                        {
                            powerSwitch.flash = 0.55f;
                            continue;
                        }

                        powerSwitch.powered = ! powerSwitch.powered;
                        powerSwitch.restoreAtSeconds = -1.0;
                        triggered = true;
                    }
                    else if (powerSwitch.powered)
                    {
                        if (city.isSwitchEnergized (powerSwitch) && ! hasOtherEnergizedSwitch (powerSwitch.id))
                        {
                            powerSwitch.flash = 0.55f;
                            continue;
                        }

                        powerSwitch.powered = false;
                        powerSwitch.restoreAtSeconds = timeSeconds + static_cast<double> (powerSwitch.offDurationSeconds);
                        triggered = true;
                    }
                    else if (powerSwitch.retriggerPolicy == PowerSwitchRetriggerPolicy::turnOnWhileOff)
                    {
                        powerSwitch.powered = true;
                        powerSwitch.restoreAtSeconds = -1.0;
                        triggered = true;
                    }
                    else
                    {
                        powerSwitch.flash = 0.55f;
                        continue;
                    }

                    powerSwitch.flash = 1.0f;
                    activeCollisions.clear();

                    if (triggered)
                    {
                        if (auto* module = city.findModule (proxy.module.id))
                        {
                            module->soundingPitch = tipPitch;
                            module->soundingPitchFlash = 1.0f;
                            module->flash = juce::jmax (module->flash, 1.0f);
                        }

                        addTipTriggerCue (proxy.module, apex, powerSwitch);
                        triggerCitySound (SonicEventType::tipTrigger,
                                          proxy.module.sides,
                                          powerSwitch.powered ? 8 : 3,
                                          proxy.module.foldAt (timeSeconds, globalTempoBpm),
                                          powerSwitch.powered ? 1.0f : 0.0f,
                                          tipPitch,
                                          proxy.module.soundLanguageForTip (static_cast<int> (flapIndex)),
                                          proxy.module.soundProgramForTip (static_cast<int> (flapIndex)),
                                          static_cast<int> (flapIndex));
                    }

                    announcePowerChange (powerSwitch,
                                          proxy.module.sides,
                                          proxy.module.foldAt (timeSeconds, globalTempoBpm),
                                          timeSeconds,
                                          tipPitch);
                }
            }
        }
    }

    activePowerSwitchTouches = std::move (currentTouches);
}

void CityComponent::announcePowerChange (PowerSwitch& powerSwitch, int sourceSides, float sourceFold, double timeSeconds, float pitchOverride)
{
    juce::ignoreUnused (timeSeconds);

    powerSwitch.flash = juce::jmax (powerSwitch.flash, 1.0f);
    powerSwitch.pulse = 1.0f;
    flashConnectedTargets (powerSwitch.id, powerSwitch.powered ? 0.95f : 0.55f);

    triggerCitySound (powerSwitch.powered ? SonicEventType::switchOn : SonicEventType::switchOff,
                      sourceSides,
                      powerSwitch.powered ? 8 : 3,
                      sourceFold,
                      powerSwitch.powered ? 1.0f : 0.0f,
                      pitchOverride);

    triggerCitySound (SonicEventType::cablePulse,
                      sourceSides,
                      powerSwitch.powered ? 7 : 4,
                      sourceFold,
                      powerSwitch.powered ? 0.84f : 0.22f,
                      pitchOverride);
}

void CityComponent::flashConnectedTargets (int switchId, float amount)
{
    for (const auto& cable : city.powerCables())
    {
        if (cable.switchId != switchId)
            continue;

        if (cable.targetKind == CityObjectKind::module)
        {
            if (auto* module = city.findModule (cable.targetId))
                module->flash = juce::jmax (module->flash, amount);
        }
        else if (cable.targetKind == CityObjectKind::platter)
        {
            if (auto* platter = city.findPlatter (cable.targetId))
            {
                platter->flash = juce::jmax (platter->flash, amount);

                for (auto& standFlash : platter->standFlashes)
                    standFlash = juce::jmax (standFlash, amount * 0.82f);
            }
        }
        else if (cable.targetKind == CityObjectKind::block)
        {
            if (auto* block = city.findBlock (cable.targetId))
                block->flash = juce::jmax (block->flash, amount * 0.72f);
        }
        else if (cable.targetKind == CityObjectKind::plank)
        {
            if (auto* plank = city.findPlank (cable.targetId))
                plank->flash = juce::jmax (plank->flash, amount * 0.72f);
        }
    }
}

void CityComponent::addTipTriggerCue (const FoldingModule& module, Vec3 tip, const PowerSwitch& powerSwitch, bool muted, bool contactOnly)
{
    tipTriggerCues.push_back ({ tip,
                                { powerSwitch.position.x, powerSwitch.position.y, powerSwitch.elevation + 16.0f },
                                module.sides,
                                0.0f,
                                muted,
                                contactOnly });

    if (tipTriggerCues.size() > 18)
        tipTriggerCues.erase (tipTriggerCues.begin(),
                              tipTriggerCues.begin() + static_cast<std::ptrdiff_t> (tipTriggerCues.size() - 18));

    for (auto& placedModule : city.modules())
    {
        if (placedModule.id == module.id && distance (placedModule.position, module.position) < 1.0f)
        {
            placedModule.flash = juce::jmax (placedModule.flash, 1.0f);

            if (placedModule.attachedPlatterId >= 0 && placedModule.attachedStandIndex >= 0)
            {
                if (auto* platter = city.findPlatter (placedModule.attachedPlatterId))
                {
                    platter->flash = juce::jmax (platter->flash, 0.72f);
                    platter->standFlashes[static_cast<size_t> (juce::jlimit (0, 7, placedModule.attachedStandIndex))] = 1.0f;
                }
            }

            return;
        }
    }
}

void CityComponent::addTipContactCue (const FoldingModule& a, Vec3 tipA, const FoldingModule& b, Vec3 tipB, bool muted, bool contactOnly)
{
    tipTriggerCues.push_back ({ tipA,
                                tipB,
                                juce::jlimit (3, 8, (a.sides + b.sides) / 2),
                                0.0f,
                                muted,
                                contactOnly,
                                true });

    if (tipTriggerCues.size() > 30)
        tipTriggerCues.erase (tipTriggerCues.begin(),
                              tipTriggerCues.begin() + static_cast<std::ptrdiff_t> (tipTriggerCues.size() - 30));

    for (auto& module : city.modules())
    {
        if (module.id == a.id || module.id == b.id)
            module.flash = juce::jmax (module.flash, muted ? 0.62f : 0.92f);

        if ((module.id == a.id || module.id == b.id) && module.attachedPlatterId >= 0 && module.attachedStandIndex >= 0)
        {
            if (auto* platter = city.findPlatter (module.attachedPlatterId))
            {
                platter->flash = juce::jmax (platter->flash, muted ? 0.38f : 0.62f);
                platter->standFlashes[static_cast<size_t> (juce::jlimit (0, 7, module.attachedStandIndex))] = muted ? 0.62f : 0.92f;
            }
        }
    }
}

void CityComponent::paintSoundingNotes (juce::Graphics& g, const IsoProjector& view)
{
    for (const auto& module : city.modules())
    {
        if (module.soundingPitch <= 0.0f || module.soundingPitchFlash <= 0.01f)
            continue;

        const auto centre = view.project ({ module.position.x, module.position.y, module.elevation + 12.0f });
        const auto alpha = juce::jlimit (0.0f, 1.0f, module.soundingPitchFlash);
        const auto label = noteNameForMidi (module.soundingPitch);
        const auto selected = selectedKind == CityObjectKind::module && selectedId == module.id;
        const auto bounds = juce::Rectangle<float> (centre.x - 24.0f, centre.y - 12.0f, 48.0f, 24.0f);
        const auto base = selected ? juce::Colour (0xffffd766) : juce::Colour (0xff35e7ff);

        g.setColour (juce::Colours::black.withAlpha (0.22f * alpha));
        g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f), 7.0f);
        g.setColour (juce::Colour (0xee071411).withAlpha (0.72f * alpha));
        g.fillRoundedRectangle (bounds, 7.0f);
        g.setColour (base.withAlpha (0.74f * alpha));
        g.drawRoundedRectangle (bounds.reduced (0.75f), 7.0f, selected ? 1.8f : 1.2f);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.setColour (juce::Colours::white.withAlpha (0.96f * alpha));
        g.drawText (label, bounds, juce::Justification::centred);
    }
}

void CityComponent::paintActivationRings (juce::Graphics& g, const IsoProjector& view)
{
    for (const auto& module : city.modules())
    {
        const auto powered = city.isPowered (CityObjectKind::module, module.id, module.position);
        const auto selected = selectedKind == CityObjectKind::module && selectedId == module.id;
        const auto reach = polygonTipReach (module.sides, module.radius, module.flapDepth);
        const auto z = module.elevation + 3.0f;
        const auto centre = view.project ({ module.position.x, module.position.y, z });
        const auto screenRadius = reach * view.zoom * (view.viewMode == CityViewMode::topDown ? 1.0f : 1.15f) + 64.0f;

        if (centre.x < -screenRadius
         || centre.y < -screenRadius
         || centre.x > static_cast<float> (getWidth()) + screenRadius
         || centre.y > static_cast<float> (getHeight()) + screenRadius)
            continue;

        const auto segments = juce::jlimit (24, 72, juce::roundToInt (reach * view.zoom * 0.62f));
        const auto colour = selected ? juce::Colour (0xffffd766)
                          : powered ? juce::Colour (0xff32c9b1)
                                    : juce::Colour (0xff8b948f);

        juce::Path ring;

        for (int i = 0; i <= segments; ++i)
        {
            const auto angle = juce::MathConstants<float>::twoPi * static_cast<float> (i) / static_cast<float> (segments);
            const auto point = view.project ({ module.position.x + std::cos (angle) * reach,
                                               module.position.y + std::sin (angle) * reach,
                                               z });

            if (i == 0)
                ring.startNewSubPath (point);
            else
                ring.lineTo (point);
        }

        const auto alpha = powered ? 0.42f : 0.18f;
        g.setColour (colour.withAlpha (alpha * 0.18f));
        g.strokePath (ring, juce::PathStrokeType (selected ? 8.0f : 6.0f));
        g.setColour (colour.withAlpha (alpha));
        g.strokePath (ring, juce::PathStrokeType (selected ? 2.4f : 1.8f));

        const auto label = juce::String (juce::roundToInt (reach));
        g.setColour (juce::Colour (0xeef8fbf1).withAlpha (selected ? 0.86f : 0.66f));
        g.fillRoundedRectangle ({ centre.x + 8.0f, centre.y - 18.0f, 54.0f, 18.0f }, 5.0f);
        g.setColour (colour.withAlpha (selected ? 0.96f : 0.72f));
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (label, juce::Rectangle<float> { centre.x + 8.0f, centre.y - 18.0f, 54.0f, 18.0f }, juce::Justification::centred);
    }
}

void CityComponent::paintTriggerTelemetry (juce::Graphics& g, const IsoProjector& view)
{
    struct TelemetryRow
    {
        Vec3 tip;
        Vec3 target;
        int moduleId = -1;
        int tipIndex = -1;
        int switchId = -1;
        float xyDistance = 0.0f;
        float zDistance = 0.0f;
        float triggerRadius = 0.0f;
        float margin = 0.0f;
        float fold = 0.0f;
        float pitch = 0.0f;
        bool contacting = false;
        bool muted = false;
        bool powered = false;
    };

    std::vector<TelemetryRow> rows;
    rows.reserve (city.modules().size() * city.powerSwitches().size());

    const auto timeSeconds = currentTimeSeconds();

    for (const auto& module : city.modules())
    {
        if (! city.isPowered (CityObjectKind::module, module.id, module.position))
            continue;

        const auto flaps = module.flapTriangles (timeSeconds, globalTempoBpm);
        const auto fold = module.foldAt (timeSeconds, globalTempoBpm);

        for (size_t tip = 0; tip < flaps.size(); ++tip)
        {
            const auto apex = flaps[tip][2];
            const auto apex2 = Vec2 { apex.x, apex.y };

            for (const auto& powerSwitch : city.powerSwitches())
            {
                const auto xy = distance (apex2, powerSwitch.position);
                const auto z = std::abs (apex.z - powerSwitch.elevation);
                const auto margin = xy - powerSwitch.triggerRadius;

                if (margin > 220.0f && rows.size() > 24)
                    continue;

                rows.push_back ({ apex,
                                  { powerSwitch.position.x, powerSwitch.position.y, powerSwitch.elevation + 16.0f },
                                  module.id,
                                  static_cast<int> (tip),
                                  powerSwitch.id,
                                  xy,
                                  z,
                                  powerSwitch.triggerRadius,
                                  margin,
                                  fold,
                                  module.pitchForTip (static_cast<int> (tip)),
                                  xy <= powerSwitch.triggerRadius && z <= 96.0f,
                                  module.tipIsMuted (static_cast<int> (tip)),
                                  city.isSwitchEnergized (powerSwitch) });
            }
        }
    }

    std::stable_sort (rows.begin(), rows.end(), [] (const TelemetryRow& a, const TelemetryRow& b)
    {
        if (a.contacting != b.contacting)
            return a.contacting;

        return std::abs (a.margin) < std::abs (b.margin);
    });

    if (rows.size() > 12)
        rows.resize (12);

    for (const auto& row : rows)
    {
        const auto tip = view.project (row.tip);
        const auto target = view.project (row.target);
        const auto colour = row.contacting ? juce::Colour (0xff32c9b1)
                          : row.margin < 28.0f ? juce::Colour (0xffffd766)
                                                : juce::Colour (0xff75d7ff);

        g.setColour (colour.withAlpha (row.contacting ? 0.76f : 0.34f));
        g.drawLine ({ tip, target }, row.contacting ? 3.0f : 1.5f);
        g.setColour (colour.withAlpha (row.contacting ? 0.96f : 0.62f));
        g.drawEllipse (tip.x - 5.0f, tip.y - 5.0f, 10.0f, 10.0f, row.contacting ? 2.0f : 1.2f);
        g.drawEllipse (target.x - 7.0f, target.y - 7.0f, 14.0f, 14.0f, row.contacting ? 2.0f : 1.2f);
    }

    const auto panelWidth = 372;
    const auto rowHeight = 20;
    const auto panelHeight = 58 + static_cast<int> (rows.size()) * rowHeight;
    auto panel = juce::Rectangle<int> (getWidth() - panelWidth - 18, 18, panelWidth, panelHeight)
                    .getIntersection (getLocalBounds().reduced (12));

    if (panel.isEmpty())
        return;

    g.setColour (juce::Colours::black.withAlpha (0.16f));
    g.fillRoundedRectangle (panel.toFloat().translated (0.0f, 4.0f), 14.0f);
    g.setColour (juce::Colour (0xeef8fbf1));
    g.fillRoundedRectangle (panel.toFloat(), 14.0f);
    g.setColour (juce::Colour (0x9932c9b1));
    g.drawRoundedRectangle (panel.toFloat(), 14.0f, 1.2f);

    auto content = panel.reduced (14, 10);
    g.setColour (juce::Colour (0xff263832));
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText ("trigger telemetry", content.removeFromTop (20), juce::Justification::centredLeft);

    g.setFont (juce::FontOptions (12.0f));
    g.setColour (juce::Colour (0xff65766f));
    g.drawText ("tip -> switch   dist / radius   z   fold   note", content.removeFromTop (20), juce::Justification::centredLeft);
    content.removeFromTop (2);

    if (rows.empty())
    {
        g.setColour (juce::Colour (0xff65766f));
        g.drawText ("no powered polygon tips near trigger zones", content.removeFromTop (rowHeight), juce::Justification::centredLeft);
        return;
    }

    g.setFont (juce::FontOptions (12.0f, juce::Font::bold));

    for (const auto& row : rows)
    {
        auto line = content.removeFromTop (rowHeight);
        const auto statusColour = row.contacting ? juce::Colour (0xff32c9b1)
                                : row.margin < 0.0f ? juce::Colour (0xffffd766)
                                                     : juce::Colour (0xff75d7ff);
        g.setColour (statusColour.withAlpha (0.18f));
        g.fillRoundedRectangle (line.toFloat().reduced (0.0f, 2.0f), 5.0f);

        g.setColour (statusColour);
        g.fillEllipse (static_cast<float> (line.getX()) + 2.0f,
                       static_cast<float> (line.getY()) + 6.0f,
                       8.0f,
                       8.0f);

        const auto noteText = row.muted ? juce::String ("X") : juce::String (juce::roundToInt (row.pitch));
        const auto text = juce::String ("M") + juce::String (row.moduleId)
                        + "." + juce::String (row.tipIndex + 1)
                        + " -> S" + juce::String (row.switchId)
                        + "   " + juce::String (row.xyDistance, 1)
                        + " / " + juce::String (row.triggerRadius, 1)
                        + "   z " + juce::String (row.zDistance, 1)
                        + "   f " + juce::String (row.fold, 2)
                        + "   " + noteText
                        + (row.contacting ? "   HIT" : "")
                        + (row.powered ? "   on" : "   off");

        g.setColour (juce::Colour (0xff263832));
        g.drawText (text, line.withTrimmedLeft (16), juce::Justification::centredLeft);
    }
}

void CityComponent::paintMinimap (juce::Graphics& g, const IsoProjector& view)
{
    const auto parent = getLocalBounds().toFloat().reduced (18.0f);
    const auto size = juce::jlimit (132.0f, 210.0f, juce::jmin (parent.getWidth(), parent.getHeight()) * 0.18f);
    auto bounds = juce::Rectangle<float> { parent.getRight() - size, parent.getY(), size, size };

    if (uiVisible && toolbar.isVisible() && bounds.intersects (toolbar.getBounds().toFloat()))
        bounds.translate (0.0f, static_cast<float> (toolbar.getBottom()) + 14.0f - bounds.getY());

    bounds = bounds.getIntersection (parent);
    if (bounds.getWidth() < 96.0f || bounds.getHeight() < 96.0f)
        return;

    const auto panel = colourWireframeMode ? juce::Colour (0xd904070c)
                                           : juce::Colour (0xeef8fbf1);
    const auto line = colourWireframeMode ? juce::Colour (0xcc23f7ff)
                                          : juce::Colour (0x88263832);
    const auto quiet = colourWireframeMode ? juce::Colour (0xff8cefe5)
                                           : juce::Colour (0xff65766f);

    auto minX = std::numeric_limits<float>::max();
    auto minY = std::numeric_limits<float>::max();
    auto maxX = std::numeric_limits<float>::lowest();
    auto maxY = std::numeric_limits<float>::lowest();
    auto hasPoint = false;

    auto includePoint = [&] (Vec2 point, float radius = 0.0f)
    {
        hasPoint = true;
        minX = juce::jmin (minX, point.x - radius);
        minY = juce::jmin (minY, point.y - radius);
        maxX = juce::jmax (maxX, point.x + radius);
        maxY = juce::jmax (maxY, point.y + radius);
    };

    for (const auto& module : city.modules())
        includePoint (module.position, module.radius + module.flapDepth);

    for (const auto& platter : city.platters())
        includePoint (platter.position, platter.hitRadius());

    for (const auto& block : city.blocks())
        includePoint (block.position, block.halfSize());

    for (const auto& plank : city.planks())
    {
        includePoint (plank.position, 18.0f);
        includePoint (plank.socketPosition(), plank.socketRadius());
    }

    for (const auto& powerSwitch : city.powerSwitches())
        includePoint (powerSwitch.position, powerSwitch.areaRadius);

    for (const auto& source : city.powerSources())
        includePoint (source.position, source.radius);

    const auto topLeft = view.unprojectToGround ({ 0.0f, 0.0f });
    const auto topRight = view.unprojectToGround ({ static_cast<float> (getWidth()), 0.0f });
    const auto bottomRight = view.unprojectToGround ({ static_cast<float> (getWidth()), static_cast<float> (getHeight()) });
    const auto bottomLeft = view.unprojectToGround ({ 0.0f, static_cast<float> (getHeight()) });
    includePoint (topLeft);
    includePoint (topRight);
    includePoint (bottomRight);
    includePoint (bottomLeft);

    if (! hasPoint)
        return;

    const auto pad = juce::jmax (buildGridSize * 1.2f, juce::jmax (maxX - minX, maxY - minY) * 0.10f);
    minX -= pad;
    minY -= pad;
    maxX += pad;
    maxY += pad;

    const auto worldWidth = juce::jmax (1.0f, maxX - minX);
    const auto worldHeight = juce::jmax (1.0f, maxY - minY);
    const auto content = bounds.reduced (12.0f);
    const auto scale = juce::jmin (content.getWidth() / worldWidth, content.getHeight() / worldHeight);
    const auto offset = juce::Point<float> { content.getCentreX() - (minX + maxX) * 0.5f * scale,
                                             content.getCentreY() - (minY + maxY) * 0.5f * scale };

    auto mapPoint = [&] (Vec2 point)
    {
        return juce::Point<float> { point.x * scale + offset.x, point.y * scale + offset.y };
    };

    auto selected = [this] (CityObjectKind kind, int id)
    {
        return selectedKind == kind && selectedId == id;
    };

    g.setColour (juce::Colours::black.withAlpha (colourWireframeMode ? 0.36f : 0.12f));
    g.fillRoundedRectangle (bounds.translated (0.0f, 3.0f), 14.0f);
    g.setColour (panel);
    g.fillRoundedRectangle (bounds, 14.0f);
    g.setColour (line.withAlpha (0.72f));
    g.drawRoundedRectangle (bounds, 14.0f, 1.2f);

    for (int i = 1; i < 4; ++i)
    {
        const auto x = content.getX() + content.getWidth() * static_cast<float> (i) / 4.0f;
        const auto y = content.getY() + content.getHeight() * static_cast<float> (i) / 4.0f;
        g.setColour (line.withAlpha (0.10f));
        g.drawVerticalLine (juce::roundToInt (x), content.getY(), content.getBottom());
        g.drawHorizontalLine (juce::roundToInt (y), content.getX(), content.getRight());
    }

    juce::Path viewPath;
    const Vec2 viewport[] = { topLeft, topRight, bottomRight, bottomLeft };
    for (int i = 0; i < 4; ++i)
    {
        const auto point = mapPoint (viewport[i]);
        if (i == 0)
            viewPath.startNewSubPath (point);
        else
            viewPath.lineTo (point);
    }
    viewPath.closeSubPath();
    g.setColour ((colourWireframeMode ? juce::Colour (0xff23f7ff) : juce::Colour (0xff32c9b1)).withAlpha (0.12f));
    g.fillPath (viewPath);
    g.setColour ((colourWireframeMode ? juce::Colour (0xff23f7ff) : juce::Colour (0xff32c9b1)).withAlpha (0.82f));
    g.strokePath (viewPath, juce::PathStrokeType (1.6f));

    auto drawDot = [&] (Vec2 position, float radius, juce::Colour colour, bool isSelected)
    {
        const auto point = mapPoint (position);
        const auto r = juce::jlimit (2.5f, 8.0f, radius * scale);
        g.setColour (colour.withAlpha (isSelected ? 0.34f : 0.18f));
        g.fillEllipse (point.x - r * 1.8f, point.y - r * 1.8f, r * 3.6f, r * 3.6f);
        g.setColour (colour.withAlpha (isSelected ? 0.98f : 0.78f));
        g.fillEllipse (point.x - r, point.y - r, r * 2.0f, r * 2.0f);
        if (isSelected)
        {
            g.setColour (juce::Colours::white.withAlpha (colourWireframeMode ? 0.86f : 0.72f));
            g.drawEllipse (point.x - r - 2.0f, point.y - r - 2.0f, (r + 2.0f) * 2.0f, (r + 2.0f) * 2.0f, 1.4f);
        }
    };

    auto drawLine = [&] (Vec2 a, Vec2 b, juce::Colour colour, float thickness)
    {
        g.setColour (colour.withAlpha (0.72f));
        g.drawLine ({ mapPoint (a), mapPoint (b) }, thickness);
    };

    for (const auto& block : city.blocks())
    {
        const auto centre = mapPoint (block.position);
        const auto half = juce::jmax (2.5f, block.halfSize() * scale);
        const auto isSelected = selected (CityObjectKind::block, block.id);
        g.setColour ((block.powered ? juce::Colour (0xffa8e06f) : quiet).withAlpha (isSelected ? 0.38f : 0.22f));
        g.fillRect (juce::Rectangle<float> { centre.x - half, centre.y - half, half * 2.0f, half * 2.0f });
        g.setColour ((isSelected ? juce::Colours::white : line).withAlpha (0.78f));
        g.drawRect (juce::Rectangle<float> { centre.x - half, centre.y - half, half * 2.0f, half * 2.0f }, isSelected ? 2.0f : 1.0f);
    }

    for (const auto& plank : city.planks())
    {
        const auto isSelected = selected (CityObjectKind::plank, plank.id);
        drawLine (plank.position, plank.socketPosition(), isSelected ? juce::Colour (0xffffffff) : juce::Colour (0xffff9f6e), isSelected ? 2.6f : 1.8f);
    }

    for (const auto& platter : city.platters())
        drawDot (platter.position, platter.hitRadius() * 0.18f, platter.powered ? juce::Colour (0xff75d7ff) : quiet, selected (CityObjectKind::platter, platter.id));

    for (const auto& module : city.modules())
        drawDot (module.position, module.radius * 0.25f, module.powered ? juce::Colour (0xffffcf5f) : quiet, selected (CityObjectKind::module, module.id));

    for (const auto& powerSwitch : city.powerSwitches())
        drawDot (powerSwitch.position, powerSwitch.triggerRadius * 0.28f, powerSwitch.powered ? juce::Colour (0xff60f0b2) : juce::Colour (0xffff6f91), selected (CityObjectKind::powerSwitch, powerSwitch.id));

    for (const auto& source : city.powerSources())
        drawDot (source.position, source.radius * 0.24f, source.powered ? juce::Colour (0xfffff1a8) : quiet, selected (CityObjectKind::powerSource, source.id));

    g.setColour (quiet.withAlpha (0.82f));
    g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
    g.drawText ("map", bounds.reduced (10.0f).removeFromTop (14.0f), juce::Justification::centredLeft);
}

void CityComponent::triggerCitySound (SonicEventType type,
                                      int sidesA,
                                      int sidesB,
                                      float foldA,
                                      float foldB,
                                      float pitchOverride,
                                      TipSoundLanguage language,
                                      const juce::String& program,
                                      int tipIndex)
{
    if (onCitySound)
        onCitySound (type, sidesA, sidesB, foldA, foldB, pitchOverride, language, program, tipIndex);
}

void CityComponent::configureToolbar()
{
    addAndMakeVisible (toolbar);

    toolbar.onBuildModeChanged = [this] (CityToolbar::BuildMode mode)
    {
        grabKeyboardFocus();
        setBuildMode (mode);
    };

    toolbar.onSidesChanged = [this] (int sides)
    {
        grabKeyboardFocus();
        setSelectedSides (sides);
    };

    toolbar.onRadiusChanged = [this] (float radius)
    {
        grabKeyboardFocus();
        setSelectedOrDefaultRadius (moduleRadiusForGridSquares (radius, currentFlapDepth()));
    };

    toolbar.onFlapDepthChanged = [this] (float flapDepth)
    {
        grabKeyboardFocus();
        setSelectedOrDefaultFlapDepth (flapDepth);
    };

    toolbar.onZoomChanged = [this] (float zoom)
    {
        grabKeyboardFocus();
        setZoom (zoom);
    };

    toolbar.onRotationChanged = [this] (float rotationDegrees)
    {
        grabKeyboardFocus();
        setSelectedRotationDegrees (rotationDegrees);
    };

    toolbar.onTipPitchChanged = [this] (float midiNote)
    {
        grabKeyboardFocus();

        if (midiNote < 0.0f && selectedTipIndex >= 0)
            setSelectedTipRandom (selectedTipIndex, true);
        else
            setSelectedTipPitch (midiNote);
    };

    toolbar.onTipPitchValueChanged = [this] (int tipIndex, float midiNote)
    {
        grabKeyboardFocus();
        setSelectedTipPitch (tipIndex, midiNote);
    };

    toolbar.onTipPitchRandomChanged = [this] (int tipIndex, bool random)
    {
        grabKeyboardFocus();
        setSelectedTipRandom (tipIndex, random);
    };

    toolbar.onTipPitchRangeChanged = [this] (int tipIndex, float low, float high)
    {
        grabKeyboardFocus();
        setSelectedTipRandomRange (tipIndex, low, high);
    };

    toolbar.onTipProbabilityChanged = [this] (int tipIndex, float probability)
    {
        grabKeyboardFocus();
        setSelectedTipProbability (tipIndex, probability);
    };

    toolbar.onTipSoundLanguageChanged = [this] (int tipIndex, TipSoundLanguage language)
    {
        grabKeyboardFocus();
        setSelectedTipSoundLanguage (tipIndex, language);
    };

    toolbar.onTipSoundProgramChanged = [this] (int tipIndex, const juce::String& program)
    {
        setSelectedTipSoundProgram (tipIndex, program);
    };

    toolbar.onRateDivisionChanged = [this] (float rateDivision)
    {
        grabKeyboardFocus();
        setSelectedOrDefaultRateDivision (rateDivision);
    };

    toolbar.onPhaseChanged = [this] (float phaseDegrees)
    {
        grabKeyboardFocus();
        setSelectedOrDefaultPhaseDegrees (phaseDegrees);
    };

    toolbar.onTempoChanged = [this] (float bpm)
    {
        grabKeyboardFocus();
        setGlobalTempo (bpm);
    };

    toolbar.onPlayRequested = [this]
    {
        grabKeyboardFocus();
        playTransport();
    };

    toolbar.onPauseRequested = [this]
    {
        grabKeyboardFocus();
        pauseTransport();
    };

    toolbar.onStopRequested = [this]
    {
        grabKeyboardFocus();
        stopTransport();
    };

    toolbar.onPlatterStandsChanged = [this] (int stands)
    {
        grabKeyboardFocus();
        setSelectedOrDefaultPlatterStands (stands);
    };

    toolbar.onPlatterDiameterChanged = [this] (float diameter)
    {
        grabKeyboardFocus();
        if (selectedKind == CityObjectKind::plank || buildMode == CityToolbar::BuildMode::plank)
            setSelectedOrDefaultPlankLength (lengthForGridSquares (diameter, 24));
        else
            setSelectedOrDefaultPlatterDiameter (lengthForGridSquares (diameter, 64));
    };

    toolbar.onBlockLevelsChanged = [this] (int levels)
    {
        grabKeyboardFocus();
        setSelectedOrDefaultBlockLevels (levels);
    };

    toolbar.onBlockSizeChanged = [this] (float size)
    {
        grabKeyboardFocus();
        setSelectedOrDefaultBlockSize (lengthForGridSquares (size, 24));
    };

    toolbar.onSwitchActivationModeChanged = [this] (PowerSwitchActivationMode mode)
    {
        grabKeyboardFocus();
        setSelectedSwitchActivationMode (mode);
    };

    toolbar.onSwitchOffDurationChanged = [this] (float seconds)
    {
        grabKeyboardFocus();
        setSelectedSwitchOffDuration (seconds);
    };

    toolbar.onSwitchRetriggerPolicyChanged = [this] (PowerSwitchRetriggerPolicy policy)
    {
        grabKeyboardFocus();
        setSelectedSwitchRetriggerPolicy (policy);
    };

    toolbar.onDeleteRequested = [this]
    {
        grabKeyboardFocus();
        deleteSelectedObject();
    };

    toolbar.onClearRequested = [this]
    {
        grabKeyboardFocus();
        clearModules();
    };
}

void CityComponent::syncToolbar()
{
    int sides = 6;
    auto radius = 54.0f;
    auto flapDepth = 32.0f;
    auto zoom = 1.0f;
    auto rotationDegrees = 0.0f;
    auto showRotationControl = false;
    auto rateDivision = 1.0f;
    auto phaseDegrees = 0.0f;
    auto controlsMode = buildMode;
    auto platterStands = defaultPlatterStands;
    auto platterDiameter = defaultPlatterDiameter;
    auto blockLevels = defaultBlockLevels;
    auto blockSize = defaultBlockSize;
    auto tipSelected = false;
    auto tipPitch = 60.0f;
    auto polygonSelected = false;
    std::array<float, 8> tipPitches {};
    std::array<bool, 8> tipRandom {};
    std::array<float, 8> tipRandomLow {};
    std::array<float, 8> tipRandomHigh {};
    std::array<float, 8> tipProbabilities {};
    std::array<TipSoundLanguage, 8> tipSoundLanguages {};
    std::array<juce::String, 8> tipSoundPrograms {};
    auto tempoBpm = globalTempoBpm;
    auto playing = transportPlaying;
    auto switchSelected = false;
    auto switchActivationMode = PowerSwitchActivationMode::tipToggle;
    auto switchOffDuration = 8.0f;
    auto switchRetriggerPolicy = PowerSwitchRetriggerPolicy::ignoreWhileOff;

    {
        const juce::ScopedLock lock (modelLock);
        sides = defaultSides;
        radius = defaultRadius;
        flapDepth = defaultFlapDepth;
        rateDivision = buildMode == CityToolbar::BuildMode::platter ? defaultPlatterRateDivision : defaultRateDivision;
        phaseDegrees = juce::radiansToDegrees (buildMode == CityToolbar::BuildMode::platter ? defaultPlatterPhase : defaultPhase);
        controlsMode = selectedKind == CityObjectKind::module ? CityToolbar::BuildMode::polygon
                     : selectedKind == CityObjectKind::platter ? CityToolbar::BuildMode::platter
                     : selectedKind == CityObjectKind::block ? CityToolbar::BuildMode::block
                     : selectedKind == CityObjectKind::plank ? CityToolbar::BuildMode::plank
                     : selectedKind == CityObjectKind::powerSwitch ? CityToolbar::BuildMode::cable
                     : selectedKind == CityObjectKind::powerSource ? CityToolbar::BuildMode::cable
                     : buildMode;

        if (selectedKind == CityObjectKind::module)
        {
            if (const auto* selected = city.findModule (selectedId))
            {
                sides = selected->sides;
                radius = selected->radius;
                flapDepth = selected->flapDepth;
                rateDivision = selected->rateDivision;
                phaseDegrees = juce::radiansToDegrees (selected->phase);
                rotationDegrees = juce::radiansToDegrees (selected->rotation);
                showRotationControl = true;
                polygonSelected = true;
                tipPitches = selected->tipPitches;
                tipRandom = selected->tipPitchRandom;
                tipRandomLow = selected->tipPitchRandomLow;
                tipRandomHigh = selected->tipPitchRandomHigh;
                tipProbabilities = selected->tipProbabilities;
                tipSoundLanguages = selected->tipSoundLanguages;
                tipSoundPrograms = selected->tipSoundPrograms;
                if (selectedTipIndex >= 0 && selectedTipIndex < selected->sides)
                {
                    tipSelected = true;
                    tipPitch = selected->pitchForTip (selectedTipIndex);
                }
            }
        }
        else if (selectedKind == CityObjectKind::platter)
        {
            if (const auto* selected = city.findPlatter (selectedId))
            {
                rateDivision = selected->rateDivision;
                phaseDegrees = juce::radiansToDegrees (selected->phase);
                rotationDegrees = juce::radiansToDegrees (selected->rotation);
                showRotationControl = true;
                platterStands = selected->stands;
                platterDiameter = selected->diameter;
            }
        }
        else if (selectedKind == CityObjectKind::block)
        {
            if (const auto* selected = city.findBlock (selectedId))
            {
                blockLevels = selected->levels;
                blockSize = selected->size;
            }
        }
        else if (selectedKind == CityObjectKind::plank)
        {
            if (const auto* selected = city.findPlank (selectedId))
            {
                platterDiameter = selected->length;
                rotationDegrees = juce::radiansToDegrees (selected->angle);
                showRotationControl = true;
            }
        }
        else if (selectedKind == CityObjectKind::powerSwitch)
        {
            if (const auto* selected = city.findPowerSwitch (selectedId))
            {
                switchSelected = true;
                switchActivationMode = selected->activationMode;
                switchOffDuration = selected->offDurationSeconds;
                switchRetriggerPolicy = selected->retriggerPolicy;
            }
        }

        zoom = projector.zoom;
        tempoBpm = globalTempoBpm;
        playing = transportPlaying;
    }

    radius = gridSquaresForLength ((radius + flapDepth) * 2.0f);
    platterDiameter = gridSquaresForLength (platterDiameter);
    blockSize = gridSquaresForLength (blockSize);

    toolbar.setValues (sides,
                       radius,
                       flapDepth,
                       zoom,
                       rotationDegrees,
                       showRotationControl,
                       rateDivision,
                       phaseDegrees,
                       buildMode,
                       controlsMode,
                       platterStands,
                       platterDiameter,
                       blockLevels,
                       blockSize,
                       tipSelected,
                       tipPitch,
                       polygonSelected,
                       tipPitches,
                       tipRandom,
                       tipRandomLow,
                       tipRandomHigh,
                       tipProbabilities,
                       tipSoundLanguages,
                       tipSoundPrograms,
                       selectedTipIndex,
                       mode2ProgramEditing,
                       tempoBpm,
                       playing,
                       switchSelected,
                       switchActivationMode,
                       switchOffDuration,
                       switchRetriggerPolicy,
                       colourWireframeMode);
}

CityRenderState CityComponent::createRenderState() const
{
    const juce::ScopedLock lock (modelLock);

    CityRenderState state;
    state.modules = city.modules();
    state.platters = city.platters();
    state.blocks = city.blocks();
    state.planks = city.planks();
    state.powerSwitches = city.powerSwitches();
    for (auto& powerSwitch : state.powerSwitches)
        powerSwitch.powered = city.isSwitchEnergized (powerSwitch);
    state.powerSources = city.powerSources();
    state.powerFeedCables = city.powerFeedCables();
    state.powerCables = city.powerCables();
    state.tipTriggerCues = tipTriggerCues;
    for (auto& module : state.modules)
        module.powered = city.isPowered (CityObjectKind::module, module.id, module.position);
    for (auto& platter : state.platters)
        platter.powered = city.isPowered (CityObjectKind::platter, platter.id, platter.position);
    for (auto& block : state.blocks)
        block.powered = city.isPowered (CityObjectKind::block, block.id, block.position);
    for (auto& plank : state.planks)
        plank.powered = city.isPowered (CityObjectKind::plank, plank.id, plank.position);
    state.pan = projector.pan;
    state.zoom = projector.zoom;
    state.viewMode = projector.viewMode;
    state.colourWireframeMode = colourWireframeMode;
    state.width = renderWidth;
    state.height = renderHeight;
    state.selectedModuleId = selectedKind == CityObjectKind::module ? selectedId : -1;
    state.selectedPlatterId = selectedKind == CityObjectKind::platter ? selectedId : -1;
    state.selectedBlockId = selectedKind == CityObjectKind::block ? selectedId : -1;
    state.selectedPlankId = selectedKind == CityObjectKind::plank ? selectedId : -1;
    state.selectedPowerSwitchId = selectedKind == CityObjectKind::powerSwitch ? selectedId : cableSourceSwitchId;
    state.selectedPowerSourceId = selectedKind == CityObjectKind::powerSource ? selectedId : cableSourcePowerSourceId;
    state.timeSeconds = currentTimeSeconds();
    state.globalTempoBpm = globalTempoBpm;
    return state;
}

void CityComponent::setSelectedSides (int sides)
{
    recordUndoState();

    {
        const juce::ScopedLock lock (modelLock);
        defaultSides = juce::jlimit (3, 8, sides);

        if (selectedKind == CityObjectKind::module)
        {
            if (auto* selected = city.findModule (selectedId))
            {
                selected->sides = defaultSides;
                if (selectedTipIndex >= selected->sides)
                    selectedTipIndex = -1;
            }
        }
        activeCollisions.clear();
    }

    syncToolbar();
    resized();
    repaint();
}

void CityComponent::adjustSelectedOrDefaultRadius (float delta)
{
    if (selectedKind == CityObjectKind::platter || (selectedKind == CityObjectKind::none && buildMode == CityToolbar::BuildMode::platter))
    {
        setSelectedOrDefaultPlatterDiameter (currentPlatterDiameter() + delta * 4.0f);
        return;
    }

    setSelectedOrDefaultRadius (currentRadius() + delta);
}

void CityComponent::adjustSelectedOrDefaultFlapDepth (float delta)
{
    setSelectedOrDefaultFlapDepth (currentFlapDepth() + delta);
}

void CityComponent::setSelectedOrDefaultRadius (float radius)
{
    recordUndoState();

    {
        const juce::ScopedLock lock (modelLock);
        defaultRadius = juce::jlimit (24.0f, 1300.0f, radius);

        if (selectedKind == CityObjectKind::module)
        {
            if (auto* selected = city.findModule (selectedId))
                selected->radius = defaultRadius;
        }
        activeCollisions.clear();
    }

    syncToolbar();
    repaint();
}

void CityComponent::setSelectedOrDefaultFlapDepth (float flapDepth)
{
    recordUndoState();

    {
        const juce::ScopedLock lock (modelLock);
        defaultFlapDepth = juce::jlimit (8.0f, 110.0f, flapDepth);

        if (selectedKind == CityObjectKind::module)
        {
            if (auto* selected = city.findModule (selectedId))
                selected->flapDepth = defaultFlapDepth;
        }
        activeCollisions.clear();
    }

    syncToolbar();
    repaint();
}

void CityComponent::setSelectedOrDefaultRateDivision (float rateDivision)
{
    recordUndoState();

    {
        const juce::ScopedLock lock (modelLock);

        if (selectedKind == CityObjectKind::platter)
        {
            const auto clampedRate = juce::jlimit (FairgroundPlatter::minRotationsPerBar,
                                                  FairgroundPlatter::maxRotationsPerBar,
                                                  rateDivision);
            defaultPlatterRateDivision = clampedRate;

            if (auto* selected = city.findPlatter (selectedId))
                selected->rateDivision = clampedRate;
        }
        else if (selectedKind == CityObjectKind::module)
        {
            const auto clampedRate = juce::jlimit (FoldingModule::minRateDivision, FoldingModule::maxRateDivision, rateDivision);
            defaultRateDivision = clampedRate;

            if (auto* selected = city.findModule (selectedId))
                selected->rateDivision = clampedRate;
        }
        else if (buildMode == CityToolbar::BuildMode::platter)
        {
            const auto clampedRate = juce::jlimit (FairgroundPlatter::minRotationsPerBar,
                                                  FairgroundPlatter::maxRotationsPerBar,
                                                  rateDivision);
            defaultPlatterRateDivision = clampedRate;
        }
        else
        {
            const auto clampedRate = juce::jlimit (FoldingModule::minRateDivision, FoldingModule::maxRateDivision, rateDivision);
            defaultRateDivision = clampedRate;
        }

        activeCollisions.clear();
    }

    syncToolbar();
    repaint();
}

void CityComponent::setSelectedOrDefaultPhaseDegrees (float phaseDegrees)
{
    recordUndoState();

    {
        const juce::ScopedLock lock (modelLock);
        const auto wrappedDegrees = std::fmod (phaseDegrees, 360.0f);
        const auto phase = juce::degreesToRadians (wrappedDegrees < 0.0f ? wrappedDegrees + 360.0f : wrappedDegrees);

        if (selectedKind == CityObjectKind::platter)
        {
            defaultPlatterPhase = phase;

            if (auto* selected = city.findPlatter (selectedId))
                selected->phase = phase;
        }
        else if (selectedKind == CityObjectKind::module)
        {
            defaultPhase = phase;

            if (auto* selected = city.findModule (selectedId))
                selected->phase = phase;
        }
        else if (buildMode == CityToolbar::BuildMode::platter)
        {
            defaultPlatterPhase = phase;
        }
        else
        {
            defaultPhase = phase;
        }

        activeCollisions.clear();
    }

    syncToolbar();
    repaint();
}

void CityComponent::setSelectedRotationDegrees (float rotationDegrees)
{
    recordUndoState();

    {
        const juce::ScopedLock lock (modelLock);
        const auto wrappedDegrees = std::fmod (rotationDegrees, 360.0f);
        const auto rotation = juce::degreesToRadians (wrappedDegrees < 0.0f ? wrappedDegrees + 360.0f : wrappedDegrees);

        if (selectedKind == CityObjectKind::module)
        {
            if (auto* selected = city.findModule (selectedId))
                selected->rotation = rotation;
        }
        else if (selectedKind == CityObjectKind::platter)
        {
            if (auto* selected = city.findPlatter (selectedId))
                selected->rotation = rotation;
        }
        else if (selectedKind == CityObjectKind::plank)
        {
            defaultPlankAngle = rotation;

            if (auto* selected = city.findPlank (selectedId))
                selected->angle = rotation;
        }
        else if (buildMode == CityToolbar::BuildMode::plank)
        {
            defaultPlankAngle = rotation;
        }

        activeCollisions.clear();
    }

    syncToolbar();
    repaint();
}

void CityComponent::setSelectedTipPitch (float midiNote)
{
    setSelectedTipPitch (selectedTipIndex, midiNote);
}

void CityComponent::setSelectedTipPitch (int tipIndex, float midiNote)
{
    recordUndoState();

    {
        const juce::ScopedLock lock (modelLock);

        if (selectedKind == CityObjectKind::module && tipIndex >= 0)
        {
            if (auto* selected = city.findModule (selectedId))
            {
                const auto index = static_cast<size_t> (juce::jlimit (0, 7, tipIndex));
                selected->tipPitches[index] = midiNote <= 0.0f ? 0.0f : juce::jlimit (36.0f, 96.0f, midiNote);
                selected->tipPitchRandom[index] = false;

                if (! selected->tipPitchRandom[index])
                {
                    selected->tipPitchRandomLow[index] = selected->tipPitches[index];
                    selected->tipPitchRandomHigh[index] = selected->tipPitches[index];
                }
            }
        }
    }

    syncToolbar();
    repaint();
}

void CityComponent::setSelectedTipRandom (int tipIndex, bool random)
{
    recordUndoState();

    {
        const juce::ScopedLock lock (modelLock);

        if (selectedKind == CityObjectKind::module && tipIndex >= 0)
        {
            if (auto* selected = city.findModule (selectedId))
            {
                const auto index = static_cast<size_t> (juce::jlimit (0, 7, tipIndex));
                selected->tipPitchRandom[index] = random;

                const auto rangeIsCollapsed = std::abs (selected->tipPitchRandomHigh[index] - selected->tipPitchRandomLow[index]) < 0.5f;

                if (random
                 && (selected->tipPitchRandomHigh[index] <= 0.0f || rangeIsCollapsed)
                 && selected->tipPitches[index] > 0.0f)
                {
                    selected->tipPitchRandomLow[index] = selected->tipPitches[index];
                    selected->tipPitchRandomHigh[index] = juce::jmin (96.0f, selected->tipPitches[index] + 7.0f);
                }
            }
        }
    }

    syncToolbar();
    repaint();
}

void CityComponent::setSelectedTipRandomRange (int tipIndex, float low, float high)
{
    recordUndoState();

    {
        const juce::ScopedLock lock (modelLock);

        if (selectedKind == CityObjectKind::module && tipIndex >= 0)
        {
            if (auto* selected = city.findModule (selectedId))
            {
                const auto index = static_cast<size_t> (juce::jlimit (0, 7, tipIndex));
                selected->tipPitchRandomLow[index] = juce::jlimit (0.0f, 96.0f, juce::jmin (low, high));
                selected->tipPitchRandomHigh[index] = juce::jlimit (0.0f, 96.0f, juce::jmax (low, high));
                selected->tipPitchRandom[index] = true;
            }
        }
    }

    syncToolbar();
    repaint();
}

void CityComponent::setSelectedTipProbability (int tipIndex, float probability)
{
    recordUndoState();

    {
        const juce::ScopedLock lock (modelLock);

        if (selectedKind == CityObjectKind::module && tipIndex >= 0)
        {
            if (auto* selected = city.findModule (selectedId))
            {
                const auto index = static_cast<size_t> (juce::jlimit (0, 7, tipIndex));
                selected->tipProbabilities[index] = juce::jlimit (0.0f, 1.0f, probability);
            }
        }
    }

    syncToolbar();
    repaint();
}

void CityComponent::setSelectedTipSoundLanguage (int tipIndex, TipSoundLanguage language)
{
    recordUndoState();

    {
        const juce::ScopedLock lock (modelLock);

        if (selectedKind == CityObjectKind::module && tipIndex >= 0)
        {
            if (auto* selected = city.findModule (selectedId))
            {
                const auto index = static_cast<size_t> (juce::jlimit (0, 7, tipIndex));
                const auto oldLanguage = selected->tipSoundLanguages[index];
                const auto currentProgram = selected->tipSoundPrograms[index].trim();
                const auto oldDefault = FoldingModule::defaultTipSoundProgram (oldLanguage,
                                                                               static_cast<int> (index),
                                                                               selected->sides).trim();
                selected->tipSoundLanguages[index] = language;

                if (currentProgram.isEmpty() || currentProgram == oldDefault)
                    selected->tipSoundPrograms[index] = FoldingModule::defaultTipSoundProgram (language,
                                                                                               static_cast<int> (index),
                                                                                               selected->sides);
            }
        }
    }

    syncToolbar();
    repaint();
}

void CityComponent::setSelectedTipSoundProgram (int tipIndex, const juce::String& program)
{
    auto shouldUpdate = false;

    {
        const juce::ScopedLock lock (modelLock);

        if (selectedKind == CityObjectKind::module && tipIndex >= 0)
            if (auto* selected = city.findModule (selectedId))
            {
                const auto index = static_cast<size_t> (juce::jlimit (0, 7, tipIndex));
                shouldUpdate = selected->tipSoundPrograms[index] != program;
            }
    }

    if (! shouldUpdate)
        return;

    recordUndoState();

    {
        const juce::ScopedLock lock (modelLock);

        if (selectedKind == CityObjectKind::module && tipIndex >= 0)
            if (auto* selected = city.findModule (selectedId))
                selected->tipSoundPrograms[static_cast<size_t> (juce::jlimit (0, 7, tipIndex))] = program;
    }

    syncToolbar();
    repaint();
}

void CityComponent::setBuildMode (CityToolbar::BuildMode mode)
{
    {
        const juce::ScopedLock lock (modelLock);
        buildMode = mode;
        selectedTipIndex = -1;

        if (buildMode != CityToolbar::BuildMode::cable)
        {
            cableSourceSwitchId = -1;
            cableSourcePowerSourceId = -1;
        }

        if (selectedKind == CityObjectKind::none)
            activeCollisions.clear();
    }

    syncToolbar();
    repaint();
}

void CityComponent::setSelectedOrDefaultPlatterStands (int stands)
{
    recordUndoState();

    {
        const juce::ScopedLock lock (modelLock);
        defaultPlatterStands = juce::jlimit (1, 8, stands);

        if (selectedKind == CityObjectKind::platter)
            if (auto* selected = city.findPlatter (selectedId))
                selected->stands = defaultPlatterStands;

        activeCollisions.clear();
    }

    syncToolbar();
    repaint();
}

void CityComponent::setSelectedOrDefaultPlatterDiameter (float diameter)
{
    recordUndoState();

    {
        const juce::ScopedLock lock (modelLock);
        defaultPlatterDiameter = juce::jlimit (120.0f, 6200.0f, diameter);

        if (selectedKind == CityObjectKind::platter)
            if (auto* selected = city.findPlatter (selectedId))
                selected->diameter = defaultPlatterDiameter;

        activeCollisions.clear();
    }

    syncToolbar();
    repaint();
}

void CityComponent::setSelectedOrDefaultBlockLevels (int levels)
{
    recordUndoState();

    {
        const juce::ScopedLock lock (modelLock);
        defaultBlockLevels = juce::jlimit (1, 16, levels);

        if (selectedKind == CityObjectKind::block)
            if (auto* selected = city.findBlock (selectedId))
                selected->levels = defaultBlockLevels;

        activeCollisions.clear();
    }

    syncToolbar();
    repaint();
}

void CityComponent::setSelectedOrDefaultBlockSize (float size)
{
    recordUndoState();

    {
        const juce::ScopedLock lock (modelLock);
        defaultBlockSize = juce::jlimit (48.0f, 2304.0f, size);

        if (selectedKind == CityObjectKind::block)
            if (auto* selected = city.findBlock (selectedId))
                selected->size = defaultBlockSize;

        activeCollisions.clear();
    }

    syncToolbar();
    repaint();
}

void CityComponent::setSelectedOrDefaultPlankLength (float length)
{
    recordUndoState();

    {
        const juce::ScopedLock lock (modelLock);
        defaultPlankLength = juce::jlimit (96.0f, 2304.0f, length);

        if (selectedKind == CityObjectKind::plank)
            if (auto* selected = city.findPlank (selectedId))
                selected->length = defaultPlankLength;

        activeCollisions.clear();
    }

    syncToolbar();
    repaint();
}

void CityComponent::setSelectedSwitchActivationMode (PowerSwitchActivationMode mode)
{
    recordUndoState();

    {
        const juce::ScopedLock lock (modelLock);

        if (selectedKind == CityObjectKind::powerSwitch)
            if (auto* selected = city.findPowerSwitch (selectedId))
            {
                selected->activationMode = mode;

                if (mode == PowerSwitchActivationMode::tipToggle)
                    selected->restoreAtSeconds = -1.0;
            }
    }

    syncToolbar();
    repaint();
}

void CityComponent::setSelectedSwitchOffDuration (float seconds)
{
    recordUndoState();

    {
        const juce::ScopedLock lock (modelLock);

        if (selectedKind == CityObjectKind::powerSwitch)
            if (auto* selected = city.findPowerSwitch (selectedId))
                selected->offDurationSeconds = juce::jlimit (1.0f, 32.0f, seconds);
    }

    syncToolbar();
    repaint();
}

void CityComponent::setSelectedSwitchRetriggerPolicy (PowerSwitchRetriggerPolicy policy)
{
    recordUndoState();

    {
        const juce::ScopedLock lock (modelLock);

        if (selectedKind == CityObjectKind::powerSwitch)
            if (auto* selected = city.findPowerSwitch (selectedId))
                selected->retriggerPolicy = policy;
    }

    syncToolbar();
    repaint();
}

void CityComponent::setGlobalTempo (float bpm)
{
    recordUndoState();

    {
        const juce::ScopedLock lock (modelLock);
        globalTempoBpm = juce::jlimit (40.0f, 240.0f, bpm);
        activeCollisions.clear();
        activePowerSwitchTouches.clear();
    }

    syncToolbar();
    repaint();
}

float CityComponent::currentGlobalTempo() const
{
    const juce::ScopedLock lock (modelLock);
    return globalTempoBpm;
}

double CityComponent::currentTransportTimeSeconds() const
{
    const juce::ScopedLock lock (modelLock);
    return transportTimeSeconds;
}

bool CityComponent::isTransportPlaying() const
{
    const juce::ScopedLock lock (modelLock);
    return transportPlaying;
}

void CityComponent::playTransport()
{
    {
        const juce::ScopedLock lock (modelLock);
        transportPlaying = true;
        lastFrameTimeSeconds = juce::Time::getMillisecondCounterHiRes() * 0.001;
    }

    syncToolbar();
    repaint();
}

void CityComponent::pauseTransport()
{
    {
        const juce::ScopedLock lock (modelLock);
        transportPlaying = false;
    }

    syncToolbar();
    repaint();
}

void CityComponent::stopTransport()
{
    {
        const juce::ScopedLock lock (modelLock);
        transportPlaying = false;
        transportTimeSeconds = 0.0;
        activeCollisions.clear();
        activePowerSwitchTouches.clear();
        activeTipContacts.clear();
        tipContactReleaseTimes.clear();
        tipTriggerCues.clear();

        for (auto& module : city.modules())
            module.flash = 0.0f;
        for (auto& platter : city.platters())
        {
            platter.flash = 0.0f;
            for (auto& flash : platter.standFlashes)
                flash = 0.0f;
        }
        for (auto& block : city.blocks())
            block.flash = 0.0f;
        for (auto& plank : city.planks())
            plank.flash = 0.0f;
        for (auto& powerSwitch : city.powerSwitches())
        {
            powerSwitch.flash = 0.0f;
            powerSwitch.pulse = 0.0f;
        }
        for (auto& source : city.powerSources())
            source.flash = 0.0f;
    }

    syncToolbar();
    openGLContext.triggerRepaint();
    repaint();
}

void CityComponent::setZoom (float zoom)
{
    {
        const juce::ScopedLock lock (modelLock);
        projector.zoom = juce::jlimit (0.28f, 3.2f, zoom);
    }

    syncToolbar();
    repaint();
}

void CityComponent::toggleViewMode()
{
    setViewMode (projector.viewMode == CityViewMode::isometric ? CityViewMode::topDown
                                                               : CityViewMode::isometric);
}

CityViewMode CityComponent::currentViewMode() const
{
    const juce::ScopedLock lock (modelLock);
    return projector.viewMode;
}

void CityComponent::setViewMode (CityViewMode mode)
{
    {
        const juce::ScopedLock lock (modelLock);
        if (projector.viewMode == mode)
            return;

        projector.centre = getLocalBounds().toFloat().getCentre();
        const auto centreBefore = projector.unprojectToGround (projector.centre);
        projector.viewMode = mode;
        const auto projectedAfter = projector.project ({ centreBefore.x, centreBefore.y, 0.0f });
        projector.pan += projector.centre - projectedAfter;
    }

    syncToolbar();
    openGLContext.triggerRepaint();
    repaint();
}

bool CityComponent::isTriggerTelemetryVisible() const noexcept
{
    return triggerTelemetryVisible;
}

void CityComponent::setTriggerTelemetryVisible (bool shouldBeVisible)
{
    if (triggerTelemetryVisible == shouldBeVisible)
        return;

    triggerTelemetryVisible = shouldBeVisible;
    repaint();
}

bool CityComponent::areActivationRingsVisible() const noexcept
{
    return activationRingsVisible;
}

void CityComponent::setActivationRingsVisible (bool shouldBeVisible)
{
    if (activationRingsVisible == shouldBeVisible)
        return;

    activationRingsVisible = shouldBeVisible;
    repaint();
}

bool CityComponent::areSoundingNotesVisible() const noexcept
{
    return soundingNotesVisible;
}

void CityComponent::setSoundingNotesVisible (bool shouldBeVisible)
{
    if (soundingNotesVisible == shouldBeVisible)
        return;

    soundingNotesVisible = shouldBeVisible;
    repaint();
}

bool CityComponent::isColourWireframeModeEnabled() const noexcept
{
    return colourWireframeMode;
}

void CityComponent::setColourWireframeModeEnabled (bool shouldBeEnabled)
{
    if (colourWireframeMode == shouldBeEnabled)
        return;

    colourWireframeMode = shouldBeEnabled;
    syncToolbar();
    openGLContext.triggerRepaint();
    repaint();
}

bool CityComponent::isUiVisible() const noexcept
{
    return uiVisible;
}

void CityComponent::setUiVisible (bool shouldBeVisible)
{
    if (uiVisible == shouldBeVisible)
        return;

    uiVisible = shouldBeVisible;
    toolbar.setVisible (uiVisible);
    resized();
    grabKeyboardFocus();
    repaint();
}

bool CityComponent::isMinimapVisible() const noexcept
{
    return minimapVisible;
}

void CityComponent::setMinimapVisible (bool shouldBeVisible)
{
    if (minimapVisible == shouldBeVisible)
        return;

    minimapVisible = shouldBeVisible;
    repaint();
}

void CityComponent::setMode2ProgramEditingEnabled (bool shouldBeEnabled)
{
    if (mode2ProgramEditing == shouldBeEnabled)
        return;

    mode2ProgramEditing = shouldBeEnabled;
    syncToolbar();
}

void CityComponent::armSoundTriggers() noexcept
{
    soundTriggersArmed.store (true, std::memory_order_release);
    soundTriggerResetRequested.store (true, std::memory_order_release);
}

void CityComponent::handleCableClick (CityHit hit, bool createPowerSource)
{
    if (hit.kind == CityObjectKind::powerSource)
    {
        cableSourcePowerSourceId = hit.id;
        cableSourceSwitchId = -1;
        selectedId = hit.id;
        selectedKind = CityObjectKind::powerSource;
        dragMode = DragMode::cableWire;
        return;
    }

    if (hit.kind == CityObjectKind::powerSwitch)
    {
        if (cableSourcePowerSourceId >= 0)
        {
            city.connectPowerFeed (cableSourcePowerSourceId, hit.id);
            activeCollisions.clear();
            activePowerSwitchTouches.clear();
        }

        cableSourceSwitchId = hit.id;
        cableSourcePowerSourceId = -1;
        selectedId = hit.id;
        selectedKind = CityObjectKind::powerSwitch;
        dragMode = DragMode::cableWire;
        return;
    }

    const auto isCableTarget = hit.kind == CityObjectKind::module
                            || hit.kind == CityObjectKind::platter
                            || hit.kind == CityObjectKind::block
                            || hit.kind == CityObjectKind::plank;

    if (! isCableTarget)
    {
        if (hit.kind == CityObjectKind::none)
        {
            if (createPowerSource)
            {
                auto& source = city.addPowerSource (lastDragWorld, true, 54.0f);
                cableSourcePowerSourceId = source.id;
                cableSourceSwitchId = -1;
                selectedId = source.id;
                selectedKind = CityObjectKind::powerSource;
                dragMode = DragMode::cableWire;
            }
            else
            {
                auto& powerSwitch = city.addPowerSwitch (lastDragWorld, 230.0f, true, 40.0f);
                cableSourceSwitchId = powerSwitch.id;
                cableSourcePowerSourceId = -1;
                selectedId = powerSwitch.id;
                selectedKind = CityObjectKind::powerSwitch;
                dragMode = DragMode::cableWire;
            }
        }

        return;
    }

    if (cableSourceSwitchId < 0 && selectedKind == CityObjectKind::powerSwitch)
        cableSourceSwitchId = selectedId;

    if (cableSourceSwitchId >= 0)
    {
        city.connectPowerCable (cableSourceSwitchId, hit.kind, hit.id);
        activeCollisions.clear();
    }

    cableSourcePowerSourceId = -1;
    selectedId = hit.id;
    selectedKind = hit.kind;
    dragMode = DragMode::none;
}

void CityComponent::completeCableDrag (CityHit hit)
{
    if (hit.kind == CityObjectKind::powerSwitch && cableSourcePowerSourceId >= 0)
    {
        city.connectPowerFeed (cableSourcePowerSourceId, hit.id);
        cableSourceSwitchId = hit.id;
        cableSourcePowerSourceId = -1;
        selectedId = hit.id;
        selectedKind = CityObjectKind::powerSwitch;
        activeCollisions.clear();
        activePowerSwitchTouches.clear();
        return;
    }

    const auto isCableTarget = hit.kind == CityObjectKind::module
                            || hit.kind == CityObjectKind::platter
                            || hit.kind == CityObjectKind::block
                            || hit.kind == CityObjectKind::plank;

    if (isCableTarget && cableSourceSwitchId >= 0)
    {
        city.connectPowerCable (cableSourceSwitchId, hit.kind, hit.id);
        selectedId = hit.id;
        selectedKind = hit.kind;
        cableSourcePowerSourceId = -1;
        activeCollisions.clear();
    }
}

bool CityComponent::deleteSelectedObject()
{
    {
        const juce::ScopedLock lock (modelLock);
        if (selectedKind == CityObjectKind::none || selectedId < 0)
            return false;
    }

    recordUndoState();

    {
        const juce::ScopedLock lock (modelLock);

        if (selectedKind == CityObjectKind::module)
            city.removeModule (selectedId);
        else if (selectedKind == CityObjectKind::platter)
            city.removePlatter (selectedId);
        else if (selectedKind == CityObjectKind::block)
            city.removeBlock (selectedId);
        else if (selectedKind == CityObjectKind::plank)
            city.removePlank (selectedId);
        else if (selectedKind == CityObjectKind::powerSwitch)
            city.removePowerSwitch (selectedId);
        else if (selectedKind == CityObjectKind::powerSource)
            city.removePowerSource (selectedId);

        if (selectedKind == CityObjectKind::powerSwitch && cableSourceSwitchId == selectedId)
            cableSourceSwitchId = -1;
        if (selectedKind == CityObjectKind::powerSource && cableSourcePowerSourceId == selectedId)
            cableSourcePowerSourceId = -1;
        selectedId = -1;
        selectedKind = CityObjectKind::none;
        selectedTipIndex = -1;
        activeCollisions.clear();
        activePowerSwitchTouches.clear();
    }

    syncToolbar();
    resized();
    repaint();
    return true;
}

juce::var CityComponent::createState() const
{
    const juce::ScopedLock lock (modelLock);

    auto root = juce::DynamicObject::Ptr (new juce::DynamicObject());
    root->setProperty ("format", "FoldingPolygonCity");
    root->setProperty ("version", 1);
    root->setProperty ("tempo", globalTempoBpm);
    root->setProperty ("zoom", projector.zoom);
    root->setProperty ("panX", projector.pan.x);
    root->setProperty ("panY", projector.pan.y);
    root->setProperty ("viewMode", static_cast<int> (projector.viewMode));
    root->setProperty ("colourWireframeMode", colourWireframeMode);
    root->setProperty ("uiVisible", uiVisible);
    root->setProperty ("minimapVisible", minimapVisible);

    juce::Array<juce::var> modules;
    for (const auto& module : city.modules())
    {
        auto item = juce::DynamicObject::Ptr (new juce::DynamicObject());
        item->setProperty ("id", module.id);
        item->setProperty ("position", objectWithPosition (module.position));
        item->setProperty ("sides", module.sides);
        item->setProperty ("radius", module.radius);
        item->setProperty ("flapDepth", module.flapDepth);
        item->setProperty ("elevation", module.elevation);
        item->setProperty ("rateDivision", module.rateDivision);
        item->setProperty ("phase", module.phase);
        item->setProperty ("rotation", module.rotation);
        item->setProperty ("attachedPlatterId", module.attachedPlatterId);
        item->setProperty ("attachedStandIndex", module.attachedStandIndex);
        item->setProperty ("attachedPlankId", module.attachedPlankId);
        juce::Array<juce::var> tipPitches;
        for (const auto pitch : module.tipPitches)
            tipPitches.add (pitch);
        item->setProperty ("tipPitches", tipPitches);
        juce::Array<juce::var> tipPitchRandom;
        juce::Array<juce::var> tipPitchRandomLow;
        juce::Array<juce::var> tipPitchRandomHigh;
        juce::Array<juce::var> tipProbabilities;
        juce::Array<juce::var> tipSoundLanguages;
        juce::Array<juce::var> tipSoundPrograms;
        for (size_t i = 0; i < module.tipPitches.size(); ++i)
        {
            tipPitchRandom.add (module.tipPitchRandom[i]);
            tipPitchRandomLow.add (module.tipPitchRandomLow[i]);
            tipPitchRandomHigh.add (module.tipPitchRandomHigh[i]);
            tipProbabilities.add (module.tipProbabilities[i]);
            tipSoundLanguages.add (static_cast<int> (module.tipSoundLanguages[i]));
            tipSoundPrograms.add (module.tipSoundPrograms[i]);
        }
        item->setProperty ("tipPitchRandom", tipPitchRandom);
        item->setProperty ("tipPitchRandomLow", tipPitchRandomLow);
        item->setProperty ("tipPitchRandomHigh", tipPitchRandomHigh);
        item->setProperty ("tipProbabilities", tipProbabilities);
        item->setProperty ("tipSoundLanguages", tipSoundLanguages);
        item->setProperty ("tipSoundPrograms", tipSoundPrograms);
        item->setProperty ("powered", module.powered);
        modules.add (juce::var (item.get()));
    }
    root->setProperty ("modules", modules);

    juce::Array<juce::var> platters;
    for (const auto& platter : city.platters())
    {
        auto item = juce::DynamicObject::Ptr (new juce::DynamicObject());
        item->setProperty ("id", platter.id);
        item->setProperty ("position", objectWithPosition (platter.position));
        item->setProperty ("stands", platter.stands);
        item->setProperty ("diameter", platter.diameter);
        item->setProperty ("elevation", platter.elevation);
        item->setProperty ("rateDivision", platter.rateDivision);
        item->setProperty ("phase", platter.phase);
        item->setProperty ("rotation", platter.rotation);
        item->setProperty ("attachedPlatterId", platter.attachedPlatterId);
        item->setProperty ("attachedStandIndex", platter.attachedStandIndex);
        item->setProperty ("attachedPlankId", platter.attachedPlankId);
        item->setProperty ("powered", platter.powered);
        platters.add (juce::var (item.get()));
    }
    root->setProperty ("platters", platters);

    juce::Array<juce::var> blocks;
    for (const auto& block : city.blocks())
    {
        auto item = juce::DynamicObject::Ptr (new juce::DynamicObject());
        item->setProperty ("id", block.id);
        item->setProperty ("position", objectWithPosition (block.position));
        item->setProperty ("size", block.size);
        item->setProperty ("levels", block.levels);
        item->setProperty ("levelHeight", block.levelHeight);
        item->setProperty ("powered", block.powered);
        blocks.add (juce::var (item.get()));
    }
    root->setProperty ("blocks", blocks);

    juce::Array<juce::var> planks;
    for (const auto& plank : city.planks())
    {
        auto item = juce::DynamicObject::Ptr (new juce::DynamicObject());
        item->setProperty ("id", plank.id);
        item->setProperty ("position", objectWithPosition (plank.position));
        item->setProperty ("elevation", plank.elevation);
        item->setProperty ("length", plank.length);
        item->setProperty ("angle", plank.angle);
        item->setProperty ("rotation", plank.rotation);
        item->setProperty ("attachedPlatterId", plank.attachedPlatterId);
        item->setProperty ("attachedStandIndex", plank.attachedStandIndex);
        item->setProperty ("powered", plank.powered);
        planks.add (juce::var (item.get()));
    }
    root->setProperty ("planks", planks);

    juce::Array<juce::var> switches;
    for (const auto& powerSwitch : city.powerSwitches())
    {
        auto item = juce::DynamicObject::Ptr (new juce::DynamicObject());
        item->setProperty ("id", powerSwitch.id);
        item->setProperty ("position", objectWithPosition (powerSwitch.position));
        item->setProperty ("elevation", powerSwitch.elevation);
        item->setProperty ("triggerRadius", powerSwitch.triggerRadius);
        item->setProperty ("areaRadius", powerSwitch.areaRadius);
        item->setProperty ("powered", powerSwitch.powered);
        item->setProperty ("offDurationSeconds", powerSwitch.offDurationSeconds);
        item->setProperty ("activationMode", static_cast<int> (powerSwitch.activationMode));
        item->setProperty ("retriggerPolicy", static_cast<int> (powerSwitch.retriggerPolicy));
        switches.add (juce::var (item.get()));
    }
    root->setProperty ("switches", switches);

    juce::Array<juce::var> sources;
    for (const auto& source : city.powerSources())
    {
        auto item = juce::DynamicObject::Ptr (new juce::DynamicObject());
        item->setProperty ("id", source.id);
        item->setProperty ("position", objectWithPosition (source.position));
        item->setProperty ("elevation", source.elevation);
        item->setProperty ("radius", source.radius);
        item->setProperty ("powered", source.powered);
        sources.add (juce::var (item.get()));
    }
    root->setProperty ("sources", sources);

    juce::Array<juce::var> feedCables;
    for (const auto& cable : city.powerFeedCables())
    {
        auto item = juce::DynamicObject::Ptr (new juce::DynamicObject());
        item->setProperty ("sourceId", cable.sourceId);
        item->setProperty ("switchId", cable.switchId);
        feedCables.add (juce::var (item.get()));
    }
    root->setProperty ("feedCables", feedCables);

    juce::Array<juce::var> powerCables;
    for (const auto& cable : city.powerCables())
    {
        auto item = juce::DynamicObject::Ptr (new juce::DynamicObject());
        item->setProperty ("switchId", cable.switchId);
        item->setProperty ("targetKind", static_cast<int> (cable.targetKind));
        item->setProperty ("targetId", cable.targetId);
        powerCables.add (juce::var (item.get()));
    }
    root->setProperty ("powerCables", powerCables);

    return juce::var (root.get());
}

bool CityComponent::loadState (const juce::var& state)
{
    const auto* root = state.getDynamicObject();
    if (root == nullptr || root->getProperty ("format").toString() != "FoldingPolygonCity")
        return false;

    {
        const juce::ScopedLock lock (modelLock);
        city.clear();

        globalTempoBpm = juce::jlimit (40.0f, 240.0f, floatProperty (*root, "tempo", globalTempoBpm));
        projector.zoom = juce::jlimit (0.28f, 3.2f, floatProperty (*root, "zoom", projector.zoom));
        projector.pan = { floatProperty (*root, "panX", projector.pan.x),
                          floatProperty (*root, "panY", projector.pan.y) };
        projector.viewMode = intProperty (*root, "viewMode", 0) == static_cast<int> (CityViewMode::topDown)
                                 ? CityViewMode::topDown
                                 : CityViewMode::isometric;
        colourWireframeMode = boolProperty (*root, "colourWireframeMode", colourWireframeMode);
        uiVisible = boolProperty (*root, "uiVisible", uiVisible);
        minimapVisible = boolProperty (*root, "minimapVisible", minimapVisible);

        if (const auto* blocks = root->getProperty ("blocks").getArray())
        {
            for (const auto& blockValue : *blocks)
            {
                if (const auto* object = blockValue.getDynamicObject())
                {
                    StackBlock block;
                    block.id = intProperty (*object, "id", 0);
                    if (const auto* position = object->getProperty ("position").getDynamicObject())
                        block.position = positionFromObject (*position);
                    block.size = juce::jlimit (48.0f, 2304.0f, floatProperty (*object, "size", block.size));
                    block.levels = juce::jlimit (1, 16, intProperty (*object, "levels", block.levels));
                    block.levelHeight = juce::jlimit (20.0f, 180.0f, floatProperty (*object, "levelHeight", block.levelHeight));
                    block.powered = boolProperty (*object, "powered", block.powered);
                    city.blocks().push_back (block);
                }
            }
        }

        if (const auto* modules = root->getProperty ("modules").getArray())
        {
            for (const auto& moduleValue : *modules)
            {
                if (const auto* object = moduleValue.getDynamicObject())
                {
                    FoldingModule module;
                    module.id = intProperty (*object, "id", 0);
                    if (const auto* position = object->getProperty ("position").getDynamicObject())
                        module.position = positionFromObject (*position);
                    module.sides = juce::jlimit (3, 8, intProperty (*object, "sides", module.sides));
                    module.radius = juce::jlimit (24.0f, 1300.0f, floatProperty (*object, "radius", module.radius));
                    module.flapDepth = juce::jlimit (8.0f, 110.0f, floatProperty (*object, "flapDepth", module.flapDepth));
                    module.elevation = floatProperty (*object, "elevation", city.elevationAt (module.position));
                    module.rateDivision = juce::jlimit (FoldingModule::minRateDivision, FoldingModule::maxRateDivision, floatProperty (*object, "rateDivision", module.rateDivision));
                    module.phase = wrapRadians (floatProperty (*object, "phase", module.phase));
                    module.rotation = wrapRadians (floatProperty (*object, "rotation", module.rotation));
                    module.attachedPlatterId = intProperty (*object, "attachedPlatterId", module.attachedPlatterId);
                    module.attachedStandIndex = intProperty (*object, "attachedStandIndex", module.attachedStandIndex);
                    module.attachedPlankId = intProperty (*object, "attachedPlankId", module.attachedPlankId);
                    module.initialiseTipPitches();
                    if (const auto* tipPitches = object->getProperty ("tipPitches").getArray())
                    {
                        const auto count = juce::jmin (static_cast<int> (module.tipPitches.size()), tipPitches->size());
                        for (int i = 0; i < count; ++i)
                            module.tipPitches[static_cast<size_t> (i)] = static_cast<float> ((double) tipPitches->getReference (i));
                    }
                    if (const auto* randomValues = object->getProperty ("tipPitchRandom").getArray())
                    {
                        const auto count = juce::jmin (static_cast<int> (module.tipPitchRandom.size()), randomValues->size());
                        for (int i = 0; i < count; ++i)
                            module.tipPitchRandom[static_cast<size_t> (i)] = static_cast<bool> ((bool) randomValues->getReference (i));
                    }
                    if (const auto* lows = object->getProperty ("tipPitchRandomLow").getArray())
                    {
                        const auto count = juce::jmin (static_cast<int> (module.tipPitchRandomLow.size()), lows->size());
                        for (int i = 0; i < count; ++i)
                            module.tipPitchRandomLow[static_cast<size_t> (i)] = juce::jlimit (0.0f, 96.0f, static_cast<float> ((double) lows->getReference (i)));
                    }
                    if (const auto* highs = object->getProperty ("tipPitchRandomHigh").getArray())
                    {
                        const auto count = juce::jmin (static_cast<int> (module.tipPitchRandomHigh.size()), highs->size());
                        for (int i = 0; i < count; ++i)
                            module.tipPitchRandomHigh[static_cast<size_t> (i)] = juce::jlimit (0.0f, 96.0f, static_cast<float> ((double) highs->getReference (i)));
                    }
                    if (const auto* probabilities = object->getProperty ("tipProbabilities").getArray())
                    {
                        const auto count = juce::jmin (static_cast<int> (module.tipProbabilities.size()), probabilities->size());
                        for (int i = 0; i < count; ++i)
                            module.tipProbabilities[static_cast<size_t> (i)] = juce::jlimit (0.0f, 1.0f, static_cast<float> ((double) probabilities->getReference (i)));
                    }
                    if (const auto* languages = object->getProperty ("tipSoundLanguages").getArray())
                    {
                        const auto count = juce::jmin (static_cast<int> (module.tipSoundLanguages.size()), languages->size());
                        for (int i = 0; i < count; ++i)
                            module.tipSoundLanguages[static_cast<size_t> (i)] = static_cast<int> (languages->getReference (i)) == static_cast<int> (TipSoundLanguage::chuck)
                                                                                  ? TipSoundLanguage::chuck
                                                                                  : TipSoundLanguage::superCollider;
                    }
                    if (const auto* programs = object->getProperty ("tipSoundPrograms").getArray())
                    {
                        const auto count = juce::jmin (static_cast<int> (module.tipSoundPrograms.size()), programs->size());
                        for (int i = 0; i < count; ++i)
                            module.tipSoundPrograms[static_cast<size_t> (i)] = programs->getReference (i).toString();
                    }
                    module.powered = boolProperty (*object, "powered", module.powered);
                    city.modules().push_back (module);
                }
            }
        }

        if (const auto* platters = root->getProperty ("platters").getArray())
        {
            for (const auto& platterValue : *platters)
            {
                if (const auto* object = platterValue.getDynamicObject())
                {
                    FairgroundPlatter platter;
                    platter.id = intProperty (*object, "id", 0);
                    if (const auto* position = object->getProperty ("position").getDynamicObject())
                        platter.position = positionFromObject (*position);
                    platter.stands = juce::jlimit (1, 8, intProperty (*object, "stands", platter.stands));
                    platter.diameter = juce::jlimit (120.0f, 6200.0f, floatProperty (*object, "diameter", platter.diameter));
                    platter.elevation = floatProperty (*object, "elevation", city.elevationAt (platter.position));
                    platter.rateDivision = juce::jlimit (FairgroundPlatter::minRotationsPerBar,
                                                         FairgroundPlatter::maxRotationsPerBar,
                                                         floatProperty (*object, "rateDivision", platter.rateDivision));
                    platter.phase = wrapRadians (floatProperty (*object, "phase", platter.phase));
                    platter.rotation = wrapRadians (floatProperty (*object, "rotation", platter.rotation));
                    platter.attachedPlatterId = intProperty (*object, "attachedPlatterId", platter.attachedPlatterId);
                    platter.attachedStandIndex = intProperty (*object, "attachedStandIndex", platter.attachedStandIndex);
                    platter.attachedPlankId = intProperty (*object, "attachedPlankId", platter.attachedPlankId);
                    platter.powered = boolProperty (*object, "powered", platter.powered);
                    city.platters().push_back (platter);
                }
            }
        }

        if (const auto* planks = root->getProperty ("planks").getArray())
        {
            for (const auto& plankValue : *planks)
            {
                if (const auto* object = plankValue.getDynamicObject())
                {
                    Plank plank;
                    plank.id = intProperty (*object, "id", 0);
                    if (const auto* position = object->getProperty ("position").getDynamicObject())
                        plank.position = positionFromObject (*position);
                    plank.elevation = floatProperty (*object, "elevation", city.elevationAt (plank.position) + 10.0f);
                    plank.length = juce::jlimit (96.0f, 2304.0f, floatProperty (*object, "length", plank.length));
                    plank.angle = wrapRadians (floatProperty (*object, "angle", plank.angle));
                    plank.rotation = wrapRadians (floatProperty (*object, "rotation", plank.rotation));
                    plank.attachedPlatterId = intProperty (*object, "attachedPlatterId", plank.attachedPlatterId);
                    plank.attachedStandIndex = intProperty (*object, "attachedStandIndex", plank.attachedStandIndex);
                    plank.powered = boolProperty (*object, "powered", plank.powered);
                    city.planks().push_back (plank);
                }
            }
        }

        if (const auto* switches = root->getProperty ("switches").getArray())
        {
            for (const auto& switchValue : *switches)
            {
                if (const auto* object = switchValue.getDynamicObject())
                {
                    PowerSwitch powerSwitch;
                    powerSwitch.id = intProperty (*object, "id", 0);
                    if (const auto* position = object->getProperty ("position").getDynamicObject())
                        powerSwitch.position = positionFromObject (*position);
                    powerSwitch.elevation = floatProperty (*object, "elevation", city.elevationAt (powerSwitch.position));
                    powerSwitch.triggerRadius = juce::jlimit (18.0f, 70.0f, floatProperty (*object, "triggerRadius", powerSwitch.triggerRadius));
                    powerSwitch.areaRadius = juce::jlimit (110.0f, 520.0f, floatProperty (*object, "areaRadius", powerSwitch.areaRadius));
                    powerSwitch.powered = boolProperty (*object, "powered", powerSwitch.powered);
                    powerSwitch.offDurationSeconds = juce::jlimit (1.0f, 32.0f, floatProperty (*object, "offDurationSeconds", powerSwitch.offDurationSeconds));
                    powerSwitch.activationMode = intProperty (*object, "activationMode", 0) == 1
                                                     ? PowerSwitchActivationMode::timedOff
                                                     : PowerSwitchActivationMode::tipToggle;
                    powerSwitch.retriggerPolicy = intProperty (*object, "retriggerPolicy", 0) == 1
                                                      ? PowerSwitchRetriggerPolicy::turnOnWhileOff
                                                      : PowerSwitchRetriggerPolicy::ignoreWhileOff;
                    city.powerSwitches().push_back (powerSwitch);
                }
            }
        }

        if (const auto* sources = root->getProperty ("sources").getArray())
        {
            for (const auto& sourceValue : *sources)
            {
                if (const auto* object = sourceValue.getDynamicObject())
                {
                    PowerSource source;
                    source.id = intProperty (*object, "id", 0);
                    if (const auto* position = object->getProperty ("position").getDynamicObject())
                        source.position = positionFromObject (*position);
                    source.elevation = floatProperty (*object, "elevation", city.elevationAt (source.position));
                    source.radius = juce::jlimit (28.0f, 86.0f, floatProperty (*object, "radius", source.radius));
                    source.powered = boolProperty (*object, "powered", source.powered);
                    city.powerSources().push_back (source);
                }
            }
        }

        if (const auto* feedCables = root->getProperty ("feedCables").getArray())
        {
            for (const auto& cableValue : *feedCables)
            {
                if (const auto* object = cableValue.getDynamicObject())
                    city.powerFeedCables().push_back ({ intProperty (*object, "sourceId", -1),
                                                        intProperty (*object, "switchId", -1) });
            }
        }

        if (const auto* powerCables = root->getProperty ("powerCables").getArray())
        {
            for (const auto& cableValue : *powerCables)
            {
                if (const auto* object = cableValue.getDynamicObject())
                {
                    const auto kind = objectKindFromInt (intProperty (*object, "targetKind", 0));
                    if (kind != CityObjectKind::none)
                        city.powerCables().push_back ({ intProperty (*object, "switchId", -1),
                                                        kind,
                                                        intProperty (*object, "targetId", -1) });
                }
            }
        }

        city.resetNextIdsFromContents();
        selectedId = -1;
        selectedKind = CityObjectKind::none;
        cableSourceSwitchId = -1;
        cableSourcePowerSourceId = -1;
        activeCollisions.clear();
        activePowerSwitchTouches.clear();
        activeTipContacts.clear();
        tipContactReleaseTimes.clear();
        tipTriggerCues.clear();
    }

    syncToolbar();
    resized();
    repaint();
    return true;
}

void CityComponent::recordUndoState()
{
    const auto snapshot = juce::JSON::toString (createState(), false);

    if (! undoStack.empty() && undoStack.back() == snapshot)
        return;

    undoStack.push_back (snapshot);
    redoStack.clear();

    constexpr auto maxUndoStates = 64;
    if (undoStack.size() > maxUndoStates)
        undoStack.erase (undoStack.begin(),
                         undoStack.begin() + static_cast<std::ptrdiff_t> (undoStack.size() - maxUndoStates));
}

bool CityComponent::undo()
{
    if (undoStack.empty())
        return false;

    const auto current = juce::JSON::toString (createState(), false);
    auto previous = undoStack.back();
    undoStack.pop_back();

    if (previous == current && ! undoStack.empty())
    {
        previous = undoStack.back();
        undoStack.pop_back();
    }

    redoStack.push_back (current);

    const auto parsed = juce::JSON::parse (previous);
    return ! parsed.isVoid() && loadState (parsed);
}

bool CityComponent::redo()
{
    if (redoStack.empty())
        return false;

    const auto current = juce::JSON::toString (createState(), false);
    const auto next = redoStack.back();
    redoStack.pop_back();
    undoStack.push_back (current);

    const auto parsed = juce::JSON::parse (next);
    return ! parsed.isVoid() && loadState (parsed);
}

bool CityComponent::canUndo() const
{
    return ! undoStack.empty();
}

bool CityComponent::canRedo() const
{
    return ! redoStack.empty();
}

bool CityComponent::canCopySelection() const
{
    const juce::ScopedLock lock (modelLock);
    return selectedKind != CityObjectKind::none && selectedId >= 0;
}

bool CityComponent::canPasteSelection() const
{
    const auto parsed = juce::JSON::parse (juce::SystemClipboard::getTextFromClipboard());
    const auto* root = parsed.getDynamicObject();
    return root != nullptr && root->getProperty ("format").toString() == "FoldingPolygonCitySelection";
}

bool CityComponent::copySelectionToClipboard() const
{
    const juce::ScopedLock lock (modelLock);

    if (selectedKind == CityObjectKind::none || selectedId < 0)
        return false;

    auto root = juce::DynamicObject::Ptr (new juce::DynamicObject());
    root->setProperty ("format", "FoldingPolygonCitySelection");
    root->setProperty ("version", 1);
    root->setProperty ("kind", static_cast<int> (selectedKind));

    auto item = juce::DynamicObject::Ptr (new juce::DynamicObject());

    if (selectedKind == CityObjectKind::module)
    {
        const auto* module = city.findModule (selectedId);
        if (module == nullptr)
            return false;

        item->setProperty ("position", objectWithPosition (module->position));
        item->setProperty ("sides", module->sides);
        item->setProperty ("radius", module->radius);
        item->setProperty ("flapDepth", module->flapDepth);
        item->setProperty ("rateDivision", module->rateDivision);
        item->setProperty ("phase", module->phase);
        item->setProperty ("rotation", module->rotation);
        item->setProperty ("powered", module->powered);

        juce::Array<juce::var> tipPitches;
        for (const auto pitch : module->tipPitches)
            tipPitches.add (pitch);
        item->setProperty ("tipPitches", tipPitches);
        juce::Array<juce::var> tipPitchRandom;
        juce::Array<juce::var> tipPitchRandomLow;
        juce::Array<juce::var> tipPitchRandomHigh;
        juce::Array<juce::var> tipProbabilities;
        juce::Array<juce::var> tipSoundLanguages;
        juce::Array<juce::var> tipSoundPrograms;
        for (size_t i = 0; i < module->tipPitches.size(); ++i)
        {
            tipPitchRandom.add (module->tipPitchRandom[i]);
            tipPitchRandomLow.add (module->tipPitchRandomLow[i]);
            tipPitchRandomHigh.add (module->tipPitchRandomHigh[i]);
            tipProbabilities.add (module->tipProbabilities[i]);
            tipSoundLanguages.add (static_cast<int> (module->tipSoundLanguages[i]));
            tipSoundPrograms.add (module->tipSoundPrograms[i]);
        }
        item->setProperty ("tipPitchRandom", tipPitchRandom);
        item->setProperty ("tipPitchRandomLow", tipPitchRandomLow);
        item->setProperty ("tipPitchRandomHigh", tipPitchRandomHigh);
        item->setProperty ("tipProbabilities", tipProbabilities);
        item->setProperty ("tipSoundLanguages", tipSoundLanguages);
        item->setProperty ("tipSoundPrograms", tipSoundPrograms);
    }
    else if (selectedKind == CityObjectKind::platter)
    {
        const auto* platter = city.findPlatter (selectedId);
        if (platter == nullptr)
            return false;

        item->setProperty ("position", objectWithPosition (platter->position));
        item->setProperty ("stands", platter->stands);
        item->setProperty ("diameter", platter->diameter);
        item->setProperty ("rateDivision", platter->rateDivision);
        item->setProperty ("phase", platter->phase);
        item->setProperty ("rotation", platter->rotation);
        item->setProperty ("powered", platter->powered);
    }
    else if (selectedKind == CityObjectKind::block)
    {
        const auto* block = city.findBlock (selectedId);
        if (block == nullptr)
            return false;

        item->setProperty ("position", objectWithPosition (block->position));
        item->setProperty ("size", block->size);
        item->setProperty ("levels", block->levels);
        item->setProperty ("levelHeight", block->levelHeight);
        item->setProperty ("powered", block->powered);
    }
    else if (selectedKind == CityObjectKind::plank)
    {
        const auto* plank = city.findPlank (selectedId);
        if (plank == nullptr)
            return false;

        item->setProperty ("position", objectWithPosition (plank->position));
        item->setProperty ("length", plank->length);
        item->setProperty ("angle", plank->angle);
        item->setProperty ("rotation", plank->rotation);
        item->setProperty ("powered", plank->powered);
    }
    else if (selectedKind == CityObjectKind::powerSwitch)
    {
        const auto* powerSwitch = city.findPowerSwitch (selectedId);
        if (powerSwitch == nullptr)
            return false;

        item->setProperty ("position", objectWithPosition (powerSwitch->position));
        item->setProperty ("areaRadius", powerSwitch->areaRadius);
        item->setProperty ("triggerRadius", powerSwitch->triggerRadius);
        item->setProperty ("powered", powerSwitch->powered);
        item->setProperty ("offDurationSeconds", powerSwitch->offDurationSeconds);
        item->setProperty ("activationMode", static_cast<int> (powerSwitch->activationMode));
        item->setProperty ("retriggerPolicy", static_cast<int> (powerSwitch->retriggerPolicy));
    }
    else if (selectedKind == CityObjectKind::powerSource)
    {
        const auto* source = city.findPowerSource (selectedId);
        if (source == nullptr)
            return false;

        item->setProperty ("position", objectWithPosition (source->position));
        item->setProperty ("radius", source->radius);
        item->setProperty ("powered", source->powered);
    }

    root->setProperty ("item", juce::var (item.get()));
    juce::SystemClipboard::copyTextToClipboard (juce::JSON::toString (juce::var (root.get()), false));
    return true;
}

bool CityComponent::pasteSelectionFromClipboard()
{
    const auto parsed = juce::JSON::parse (juce::SystemClipboard::getTextFromClipboard());
    const auto* root = parsed.getDynamicObject();

    if (root == nullptr || root->getProperty ("format").toString() != "FoldingPolygonCitySelection")
        return false;

    const auto* item = root->getProperty ("item").getDynamicObject();
    if (item == nullptr)
        return false;

    const auto kind = objectKindFromInt (intProperty (*root, "kind", static_cast<int> (CityObjectKind::none)));
    if (kind == CityObjectKind::none)
        return false;

    recordUndoState();

    {
        const juce::ScopedLock lock (modelLock);
        auto position = Vec2 {};
        if (const auto* savedPosition = item->getProperty ("position").getDynamicObject())
            position = positionFromObject (*savedPosition);
        position = snapToBuildGrid (position + Vec2 { buildGridSize, buildGridSize });

        selectedKind = kind;
        selectedTipIndex = -1;

        if (kind == CityObjectKind::module)
        {
            auto& module = city.addModule (position,
                                           intProperty (*item, "sides", defaultSides),
                                           floatProperty (*item, "radius", defaultRadius),
                                           floatProperty (*item, "flapDepth", defaultFlapDepth),
                                           floatProperty (*item, "rateDivision", defaultRateDivision),
                                           floatProperty (*item, "phase", defaultPhase));
            module.rotation = floatProperty (*item, "rotation", module.rotation);
            module.powered = boolProperty (*item, "powered", module.powered);
            module.attachedPlatterId = -1;
            module.attachedStandIndex = -1;

            if (const auto* tipPitches = item->getProperty ("tipPitches").getArray())
            {
                const auto count = juce::jmin (static_cast<int> (module.tipPitches.size()), tipPitches->size());
                for (int i = 0; i < count; ++i)
                    module.tipPitches[static_cast<size_t> (i)] = static_cast<float> ((double) tipPitches->getReference (i));
            }
            if (const auto* randomValues = item->getProperty ("tipPitchRandom").getArray())
            {
                const auto count = juce::jmin (static_cast<int> (module.tipPitchRandom.size()), randomValues->size());
                for (int i = 0; i < count; ++i)
                    module.tipPitchRandom[static_cast<size_t> (i)] = static_cast<bool> ((bool) randomValues->getReference (i));
            }
            if (const auto* lows = item->getProperty ("tipPitchRandomLow").getArray())
            {
                const auto count = juce::jmin (static_cast<int> (module.tipPitchRandomLow.size()), lows->size());
                for (int i = 0; i < count; ++i)
                    module.tipPitchRandomLow[static_cast<size_t> (i)] = juce::jlimit (0.0f, 96.0f, static_cast<float> ((double) lows->getReference (i)));
            }
            if (const auto* highs = item->getProperty ("tipPitchRandomHigh").getArray())
            {
                const auto count = juce::jmin (static_cast<int> (module.tipPitchRandomHigh.size()), highs->size());
                for (int i = 0; i < count; ++i)
                    module.tipPitchRandomHigh[static_cast<size_t> (i)] = juce::jlimit (0.0f, 96.0f, static_cast<float> ((double) highs->getReference (i)));
            }
            if (const auto* probabilities = item->getProperty ("tipProbabilities").getArray())
            {
                const auto count = juce::jmin (static_cast<int> (module.tipProbabilities.size()), probabilities->size());
                for (int i = 0; i < count; ++i)
                    module.tipProbabilities[static_cast<size_t> (i)] = juce::jlimit (0.0f, 1.0f, static_cast<float> ((double) probabilities->getReference (i)));
            }
            if (const auto* languages = item->getProperty ("tipSoundLanguages").getArray())
            {
                const auto count = juce::jmin (static_cast<int> (module.tipSoundLanguages.size()), languages->size());
                for (int i = 0; i < count; ++i)
                    module.tipSoundLanguages[static_cast<size_t> (i)] = static_cast<int> (languages->getReference (i)) == static_cast<int> (TipSoundLanguage::chuck)
                                                                          ? TipSoundLanguage::chuck
                                                                          : TipSoundLanguage::superCollider;
            }
            if (const auto* programs = item->getProperty ("tipSoundPrograms").getArray())
            {
                const auto count = juce::jmin (static_cast<int> (module.tipSoundPrograms.size()), programs->size());
                for (int i = 0; i < count; ++i)
                    module.tipSoundPrograms[static_cast<size_t> (i)] = programs->getReference (i).toString();
            }

            selectedId = module.id;
            buildMode = CityToolbar::BuildMode::select;
        }
        else if (kind == CityObjectKind::platter)
        {
            auto& platter = city.addPlatter (position,
                                             intProperty (*item, "stands", defaultPlatterStands),
                                             floatProperty (*item, "diameter", defaultPlatterDiameter),
                                             floatProperty (*item, "rateDivision", defaultPlatterRateDivision),
                                             floatProperty (*item, "phase", defaultPlatterPhase));
            platter.rotation = floatProperty (*item, "rotation", platter.rotation);
            platter.powered = boolProperty (*item, "powered", platter.powered);
            selectedId = platter.id;
            buildMode = CityToolbar::BuildMode::select;
        }
        else if (kind == CityObjectKind::block)
        {
            auto& block = city.addBlock (position,
                                         floatProperty (*item, "size", defaultBlockSize),
                                         intProperty (*item, "levels", defaultBlockLevels));
            block.levelHeight = floatProperty (*item, "levelHeight", block.levelHeight);
            block.powered = boolProperty (*item, "powered", block.powered);
            selectedId = block.id;
            buildMode = CityToolbar::BuildMode::select;
        }
        else if (kind == CityObjectKind::plank)
        {
            auto& plank = city.addPlank (position,
                                         floatProperty (*item, "length", defaultPlankLength),
                                         floatProperty (*item, "angle", defaultPlankAngle));
            plank.rotation = floatProperty (*item, "rotation", plank.rotation);
            plank.powered = boolProperty (*item, "powered", plank.powered);
            selectedId = plank.id;
            buildMode = CityToolbar::BuildMode::select;
        }
        else if (kind == CityObjectKind::powerSwitch)
        {
            auto& powerSwitch = city.addPowerSwitch (position,
                                                     floatProperty (*item, "areaRadius", 230.0f),
                                                     boolProperty (*item, "powered", true),
                                                     floatProperty (*item, "triggerRadius", 40.0f));
            powerSwitch.offDurationSeconds = floatProperty (*item, "offDurationSeconds", powerSwitch.offDurationSeconds);
            powerSwitch.activationMode = intProperty (*item, "activationMode", 0) == 1
                                             ? PowerSwitchActivationMode::timedOff
                                             : PowerSwitchActivationMode::tipToggle;
            powerSwitch.retriggerPolicy = intProperty (*item, "retriggerPolicy", 0) == 1
                                              ? PowerSwitchRetriggerPolicy::turnOnWhileOff
                                              : PowerSwitchRetriggerPolicy::ignoreWhileOff;
            selectedId = powerSwitch.id;
            buildMode = CityToolbar::BuildMode::select;
        }
        else if (kind == CityObjectKind::powerSource)
        {
            auto& source = city.addPowerSource (position,
                                                boolProperty (*item, "powered", true),
                                                floatProperty (*item, "radius", 54.0f));
            selectedId = source.id;
            buildMode = CityToolbar::BuildMode::select;
        }

        cableSourceSwitchId = -1;
        cableSourcePowerSourceId = -1;
        activeCollisions.clear();
        activePowerSwitchTouches.clear();
        activeTipContacts.clear();
        tipContactReleaseTimes.clear();
    }

    syncToolbar();
    resized();
    repaint();
    return true;
}

void CityComponent::startNewCity()
{
    {
        const juce::ScopedLock lock (modelLock);
        city.clear();
        projector.zoom = 1.0f;
        projector.pan = {};
        projector.viewMode = CityViewMode::isometric;
        buildMode = CityToolbar::BuildMode::select;
        selectedId = -1;
        selectedKind = CityObjectKind::none;
        cableSourceSwitchId = -1;
        cableSourcePowerSourceId = -1;
        activeCollisions.clear();
        activePowerSwitchTouches.clear();
        activeTipContacts.clear();
        tipContactReleaseTimes.clear();
        tipTriggerCues.clear();
    }

    syncToolbar();
    resized();
    repaint();
}

void CityComponent::clearModules()
{
    recordUndoState();

    {
        const juce::ScopedLock lock (modelLock);
        city.clear();
        cableSourceSwitchId = -1;
        cableSourcePowerSourceId = -1;
        selectedId = -1;
        selectedKind = CityObjectKind::none;
        activeCollisions.clear();
        activePowerSwitchTouches.clear();
        activeTipContacts.clear();
        tipContactReleaseTimes.clear();
        tipTriggerCues.clear();
    }

    syncToolbar();
    repaint();
}

void CityComponent::zoomAt (juce::Point<float> screenPoint, float factor)
{
    {
        const juce::ScopedLock lock (modelLock);
        const auto before = projector.unprojectToGround (screenPoint);
        projector.zoom = juce::jlimit (0.28f, 3.2f, projector.zoom * factor);
        const auto after = projector.project ({ before.x, before.y, 0.0f });
        projector.pan += screenPoint - after;
    }

    syncToolbar();
    repaint();
}

Vec2 CityComponent::worldForPointer (juce::Point<float> screenPoint) const
{
    auto bestWorld = projector.unprojectToGround (screenPoint);
    auto bestElevation = 0.0f;

    for (const auto& block : city.blocks())
    {
        const auto candidate = projector.unprojectToElevation (screenPoint, block.topElevation());

        if (block.contains (candidate) && block.topElevation() >= bestElevation)
        {
            bestWorld = candidate;
            bestElevation = block.topElevation();
        }
    }

    return bestWorld;
}

Vec2 CityComponent::snappedWorldForPointer (juce::Point<float> screenPoint) const
{
    return snapToBuildGrid (worldForPointer (screenPoint), currentBuildFootprint());
}

Vec2 CityComponent::snapToBuildGrid (Vec2 world) const noexcept
{
    return snapToBuildGrid (world, buildGridSize);
}

Vec2 CityComponent::snapToBuildGrid (Vec2 world, float footprint) const noexcept
{
    const auto cells = juce::jmax (1, juce::roundToInt (footprint / buildGridSize));
    const auto offset = (cells % 2) == 0 ? 0.0f : buildGridSize * 0.5f;

    return {
        offset + std::round ((world.x - offset) / buildGridSize) * buildGridSize,
        offset + std::round ((world.y - offset) / buildGridSize) * buildGridSize
    };
}

float CityComponent::currentBuildFootprint() const noexcept
{
    if (buildMode == CityToolbar::BuildMode::platter)
        return snappedPlatterDiameterForPlacement();

    if (buildMode == CityToolbar::BuildMode::block)
        return snappedBlockSizeForPlacement();

    if (buildMode == CityToolbar::BuildMode::plank)
        return buildGridSize;

    if (buildMode == CityToolbar::BuildMode::polygon)
        return snapLengthToBuildGrid ((defaultRadius + defaultFlapDepth) * 2.0f, 1);

    return buildGridSize;
}

float CityComponent::snappedModuleRadiusForPlacement() const noexcept
{
    const auto footprint = snapLengthToBuildGrid ((defaultRadius + defaultFlapDepth) * 2.0f, 1);
    return juce::jlimit (24.0f, 1300.0f, footprint * 0.5f - defaultFlapDepth);
}

float CityComponent::snappedPlatterDiameterForPlacement() const noexcept
{
    return juce::jlimit (120.0f, 6200.0f, snapLengthToBuildGrid (defaultPlatterDiameter, 1));
}

float CityComponent::snappedBlockSizeForPlacement() const noexcept
{
    return juce::jlimit (48.0f, 2304.0f, snapLengthToBuildGrid (defaultBlockSize, 1));
}

float CityComponent::selectedElevation() const
{
    if (selectedKind == CityObjectKind::module)
        if (const auto* selected = city.findModule (selectedId))
            return selected->elevation;

    if (selectedKind == CityObjectKind::platter)
        if (const auto* selected = city.findPlatter (selectedId))
            return selected->elevation;

    if (selectedKind == CityObjectKind::block)
        if (const auto* selected = city.findBlock (selectedId))
            return selected->topElevation();

    if (selectedKind == CityObjectKind::plank)
        if (const auto* selected = city.findPlank (selectedId))
            return selected->elevation;

    if (selectedKind == CityObjectKind::powerSwitch)
        if (const auto* selected = city.findPowerSwitch (selectedId))
            return selected->elevation;

    if (selectedKind == CityObjectKind::powerSource)
        if (const auto* selected = city.findPowerSource (selectedId))
            return selected->elevation;

    return city.elevationAt (lastDragWorld);
}

bool CityComponent::hitSelectedResizeHandle (juce::Point<float> screenPoint) const
{
    const juce::ScopedLock lock (modelLock);

    auto centre = Vec2 {};
    auto radius = 0.0f;
    auto elevation = 0.0f;

    if (selectedKind == CityObjectKind::module)
    {
        const auto* selected = city.findModule (selectedId);
        if (selected == nullptr)
            return false;

        centre = selected->position;
        radius = polygonTipReach (selected->sides, selected->radius, selected->flapDepth);
        elevation = selected->elevation;
    }
    else if (selectedKind == CityObjectKind::platter)
    {
        const auto* selected = city.findPlatter (selectedId);
        if (selected == nullptr)
            return false;

        centre = selected->position;
        radius = selected->diameter * 0.5f;
        elevation = selected->elevation;
    }
    else
    {
        return false;
    }

    const auto world = projector.unprojectToElevation (screenPoint, elevation);
    const auto grabBand = juce::jmax (12.0f, 18.0f / juce::jmax (0.1f, projector.zoom));
    return std::abs (distance (world, centre) - radius) <= grabBand;
}

bool CityComponent::hitSelectedPhaseHandle (juce::Point<float> screenPoint) const
{
    const juce::ScopedLock lock (modelLock);

    auto centre = Vec2 {};
    auto radius = 0.0f;
    auto elevation = 0.0f;
    auto phase = 0.0f;

    if (selectedKind == CityObjectKind::module)
    {
        const auto* selected = city.findModule (selectedId);
        if (selected == nullptr)
            return false;

        centre = selected->position;
        radius = polygonTipReach (selected->sides, selected->radius, selected->flapDepth);
        elevation = selected->elevation;
        phase = selected->phase;
    }
    else if (selectedKind == CityObjectKind::platter)
    {
        const auto* selected = city.findPlatter (selectedId);
        if (selected == nullptr)
            return false;

        centre = selected->position;
        radius = selected->diameter * 0.5f;
        elevation = selected->elevation;
        phase = selected->phase;
    }
    else
    {
        return false;
    }

    const auto handle = projector.project ({ centre.x + std::cos (phase) * radius,
                                             centre.y + std::sin (phase) * radius,
                                             elevation + 28.0f });
    return handle.getDistanceFrom (screenPoint) <= 24.0f;
}

void CityComponent::dragSelectedResizeHandle (juce::Point<float> screenPoint)
{
    auto newRadius = 0.0f;
    auto newDiameter = 0.0f;

    {
        const juce::ScopedLock lock (modelLock);

        if (selectedKind == CityObjectKind::module)
        {
            const auto* selected = city.findModule (selectedId);
            if (selected == nullptr)
                return;

            const auto world = projector.unprojectToElevation (screenPoint, selected->elevation);
            newRadius = polygonRadiusForTipReach (selected->sides,
                                                  distance (world, selected->position),
                                                  selected->flapDepth);
        }
        else if (selectedKind == CityObjectKind::platter)
        {
            const auto* selected = city.findPlatter (selectedId);
            if (selected == nullptr)
                return;

            const auto world = projector.unprojectToElevation (screenPoint, selected->elevation);
            newDiameter = distance (world, selected->position) * 2.0f;
        }
        else
        {
            return;
        }
    }

    if (selectedKind == CityObjectKind::platter)
        setSelectedOrDefaultPlatterDiameter (newDiameter);
    else
        setSelectedOrDefaultRadius (newRadius);
}

void CityComponent::dragSelectedPhaseHandle (juce::Point<float> screenPoint)
{
    auto degrees = 0.0f;

    {
        const juce::ScopedLock lock (modelLock);

        auto centre = Vec2 {};
        auto elevation = 0.0f;

        if (selectedKind == CityObjectKind::module)
        {
            const auto* selected = city.findModule (selectedId);
            if (selected == nullptr)
                return;

            centre = selected->position;
            elevation = selected->elevation;
        }
        else if (selectedKind == CityObjectKind::platter)
        {
            const auto* selected = city.findPlatter (selectedId);
            if (selected == nullptr)
                return;

            centre = selected->position;
            elevation = selected->elevation;
        }
        else
        {
            return;
        }

        const auto world = projector.unprojectToElevation (screenPoint, elevation);
        const auto angle = std::atan2 (world.y - centre.y, world.x - centre.x);
        degrees = juce::radiansToDegrees (angle < 0.0f ? angle + juce::MathConstants<float>::twoPi : angle);
    }

    setSelectedOrDefaultPhaseDegrees (degrees);
}

float CityComponent::currentRadius() const
{
    const juce::ScopedLock lock (modelLock);

    if (selectedKind == CityObjectKind::module)
        if (const auto* selected = city.findModule (selectedId))
            return selected->radius;

    return defaultRadius;
}

float CityComponent::currentFlapDepth() const
{
    const juce::ScopedLock lock (modelLock);

    if (selectedKind == CityObjectKind::module)
        if (const auto* selected = city.findModule (selectedId))
            return selected->flapDepth;

    return defaultFlapDepth;
}

float CityComponent::currentRateDivision() const
{
    const juce::ScopedLock lock (modelLock);

    if (selectedKind == CityObjectKind::module)
        if (const auto* selected = city.findModule (selectedId))
            return selected->rateDivision;

    if (selectedKind == CityObjectKind::platter)
        if (const auto* selected = city.findPlatter (selectedId))
            return selected->rateDivision;

    if (buildMode == CityToolbar::BuildMode::platter)
        return defaultPlatterRateDivision;

    return defaultRateDivision;
}

float CityComponent::currentPhaseDegrees() const
{
    const juce::ScopedLock lock (modelLock);

    if (selectedKind == CityObjectKind::module)
        if (const auto* selected = city.findModule (selectedId))
            return juce::radiansToDegrees (selected->phase);

    if (selectedKind == CityObjectKind::platter)
        if (const auto* selected = city.findPlatter (selectedId))
            return juce::radiansToDegrees (selected->phase);

    if (buildMode == CityToolbar::BuildMode::platter)
        return juce::radiansToDegrees (defaultPlatterPhase);

    return juce::radiansToDegrees (defaultPhase);
}

int CityComponent::currentPlatterStands() const
{
    const juce::ScopedLock lock (modelLock);

    if (selectedKind == CityObjectKind::platter)
        if (const auto* selected = city.findPlatter (selectedId))
            return selected->stands;

    return defaultPlatterStands;
}

float CityComponent::currentPlatterDiameter() const
{
    const juce::ScopedLock lock (modelLock);

    if (selectedKind == CityObjectKind::platter)
        if (const auto* selected = city.findPlatter (selectedId))
            return selected->diameter;

    return defaultPlatterDiameter;
}

int CityComponent::currentBlockLevels() const
{
    const juce::ScopedLock lock (modelLock);

    if (selectedKind == CityObjectKind::block)
        if (const auto* selected = city.findBlock (selectedId))
            return selected->levels;

    return defaultBlockLevels;
}

float CityComponent::currentBlockSize() const
{
    const juce::ScopedLock lock (modelLock);

    if (selectedKind == CityObjectKind::block)
        if (const auto* selected = city.findBlock (selectedId))
            return selected->size;

    return defaultBlockSize;
}

float CityComponent::currentPlankLength() const
{
    const juce::ScopedLock lock (modelLock);

    if (selectedKind == CityObjectKind::plank)
        if (const auto* selected = city.findPlank (selectedId))
            return selected->length;

    return defaultPlankLength;
}

int CityComponent::currentSides() const
{
    const juce::ScopedLock lock (modelLock);

    if (selectedKind == CityObjectKind::module)
        if (const auto* selected = city.findModule (selectedId))
            return selected->sides;

    return defaultSides;
}

std::pair<int, int> CityComponent::sortedPair (int a, int b) noexcept
{
    return a < b ? std::make_pair (a, b) : std::make_pair (b, a);
}
