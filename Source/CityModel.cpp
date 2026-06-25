#include "CityModel.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
constexpr auto structureSurfaceClearance = 2.0f;

float distanceToSegment (Vec2 point, Vec2 a, Vec2 b) noexcept
{
    const auto ab = b - a;
    const auto ap = point - a;
    const auto denom = ab.x * ab.x + ab.y * ab.y;
    const auto t = denom > 0.0001f ? juce::jlimit (0.0f, 1.0f, (ap.x * ab.x + ap.y * ab.y) / denom) : 0.0f;
    return distance (point, { a.x + ab.x * t, a.y + ab.y * t });
}

float squaredDistance (Vec2 a, Vec2 b) noexcept
{
    const auto dx = a.x - b.x;
    const auto dy = a.y - b.y;
    return dx * dx + dy * dy;
}
}

void CityModel::clear() noexcept
{
    placedModules.clear();
    placedPlatters.clear();
    placedBlocks.clear();
    placedPlanks.clear();
    placedPowerSwitches.clear();
    placedPowerSources.clear();
    placedPowerFeedCables.clear();
    placedPowerCables.clear();

    nextId = 1;
    nextPlatterId = 1;
    nextBlockId = 1;
    nextPlankId = 1;
    nextPowerSwitchId = 1;
    nextPowerSourceId = 1;
}

void CityModel::resetNextIdsFromContents() noexcept
{
    auto nextModule = 1;
    auto nextPlatter = 1;
    auto nextBlock = 1;
    auto nextPlank = 1;
    auto nextSwitch = 1;
    auto nextSource = 1;

    for (const auto& module : placedModules)
        nextModule = std::max (nextModule, module.id + 1);
    for (const auto& platter : placedPlatters)
        nextPlatter = std::max (nextPlatter, platter.id + 1);
    for (const auto& block : placedBlocks)
        nextBlock = std::max (nextBlock, block.id + 1);
    for (const auto& plank : placedPlanks)
        nextPlank = std::max (nextPlank, plank.id + 1);
    for (const auto& powerSwitch : placedPowerSwitches)
        nextSwitch = std::max (nextSwitch, powerSwitch.id + 1);
    for (const auto& source : placedPowerSources)
        nextSource = std::max (nextSource, source.id + 1);

    nextId = nextModule;
    nextPlatterId = nextPlatter;
    nextBlockId = nextBlock;
    nextPlankId = nextPlank;
    nextPowerSwitchId = nextSwitch;
    nextPowerSourceId = nextSource;
}

FoldingModule& CityModel::addModule (Vec2 position,
                                     int sides,
                                     float radius,
                                     float flapDepth,
                                     float rateDivision,
                                     float phase)
{
    auto& module = placedModules.emplace_back();
    module.id = nextId++;
    module.position = position;
    module.sides = juce::jlimit (3, 8, sides);
    module.radius = juce::jlimit (24.0f, 1300.0f, radius);
    module.flapDepth = juce::jlimit (8.0f, 110.0f, flapDepth);
    module.elevation = elevationAt (position);
    module.rateDivision = juce::jlimit (FoldingModule::minRateDivision, FoldingModule::maxRateDivision, rateDivision);
    module.phase = std::fmod (phase + juce::MathConstants<float>::twoPi,
                              juce::MathConstants<float>::twoPi);
    module.initialiseTipPitches();

    return module;
}

FairgroundPlatter& CityModel::addPlatter (Vec2 position,
                                          int stands,
                                          float diameter,
                                          float rateDivision,
                                          float phase)
{
    auto& platter = placedPlatters.emplace_back();
    platter.id = nextPlatterId++;
    platter.position = position;
    platter.stands = juce::jlimit (1, 8, stands);
    platter.diameter = juce::jlimit (120.0f, 6200.0f, diameter);
    platter.elevation = elevationAt (position);
    platter.rateDivision = juce::jlimit (FairgroundPlatter::minRotationsPerBar,
                                         FairgroundPlatter::maxRotationsPerBar,
                                         rateDivision);
    platter.phase = std::fmod (phase + juce::MathConstants<float>::twoPi,
                               juce::MathConstants<float>::twoPi);
    return platter;
}

StackBlock& CityModel::addBlock (Vec2 position, float size, int levels)
{
    auto& block = placedBlocks.emplace_back();
    block.id = nextBlockId++;
    block.position = position;
    block.size = juce::jlimit (48.0f, 2304.0f, size);
    block.levels = juce::jlimit (1, 16, levels);
    return block;
}

Plank& CityModel::addPlank (Vec2 position, float length, float angle)
{
    auto& plank = placedPlanks.emplace_back();
    plank.id = nextPlankId++;
    plank.position = position;
    plank.length = juce::jlimit (96.0f, 2304.0f, length);
    plank.angle = std::fmod (angle + juce::MathConstants<float>::twoPi,
                             juce::MathConstants<float>::twoPi);
    plank.elevation = elevationAt (position) + 10.0f;
    return plank;
}

PowerSwitch& CityModel::addPowerSwitch (Vec2 position,
                                        float areaRadius,
                                        bool powered,
                                        float triggerRadius)
{
    auto& powerSwitch = placedPowerSwitches.emplace_back();
    powerSwitch.id = nextPowerSwitchId++;
    powerSwitch.position = position;
    powerSwitch.elevation = elevationAt (position);
    powerSwitch.areaRadius = juce::jlimit (110.0f, 520.0f, areaRadius);
    powerSwitch.triggerRadius = juce::jlimit (18.0f, 70.0f, triggerRadius);
    powerSwitch.powered = powered;
    powerSwitch.restoreAtSeconds = -1.0;
    return powerSwitch;
}

PowerSource& CityModel::addPowerSource (Vec2 position, bool powered, float radius)
{
    auto& source = placedPowerSources.emplace_back();
    source.id = nextPowerSourceId++;
    source.position = position;
    source.elevation = elevationAt (position);
    source.radius = juce::jlimit (28.0f, 86.0f, radius);
    source.powered = powered;
    source.restoreAtSeconds = -1.0;
    return source;
}

void CityModel::connectPowerFeed (int sourceId, int switchId)
{
    if (findPowerSource (sourceId) == nullptr || findPowerSwitch (switchId) == nullptr)
        return;

    auto existing = std::find_if (placedPowerFeedCables.begin(),
                                  placedPowerFeedCables.end(),
                                  [switchId] (const PowerFeedCable& cable)
                                  {
                                      return cable.switchId == switchId;
                                  });

    if (existing != placedPowerFeedCables.end())
    {
        if (existing->sourceId == sourceId)
            placedPowerFeedCables.erase (existing);
        else
            existing->sourceId = sourceId;

        return;
    }

    placedPowerFeedCables.push_back ({ sourceId, switchId });
}

void CityModel::connectPowerCable (int switchId, CityObjectKind targetKind, int targetId)
{
    if (findPowerSwitch (switchId) == nullptr || targetKind == CityObjectKind::none || targetKind == CityObjectKind::powerSwitch)
        return;

    auto existing = std::find_if (placedPowerCables.begin(),
                                  placedPowerCables.end(),
                                  [targetKind, targetId] (const PowerCable& cable)
                                  {
                                      return cable.targetKind == targetKind && cable.targetId == targetId;
                                  });

    if (existing != placedPowerCables.end())
    {
        if (existing->switchId == switchId)
            placedPowerCables.erase (existing);
        else
            existing->switchId = switchId;

        return;
    }

    placedPowerCables.push_back ({ switchId, targetKind, targetId });
}

bool CityModel::removePowerCable (CityObjectKind targetKind, int targetId)
{
    const auto oldSize = placedPowerCables.size();
    removePowerCablesFor (targetKind, targetId);
    return placedPowerCables.size() != oldSize;
}

void CityModel::removePowerCablesFor (CityObjectKind targetKind, int targetId)
{
    placedPowerCables.erase (std::remove_if (placedPowerCables.begin(),
                                             placedPowerCables.end(),
                                             [targetKind, targetId] (const PowerCable& cable)
                                             {
                                                 return (cable.targetKind == targetKind && cable.targetId == targetId)
                                                     || (targetKind == CityObjectKind::powerSwitch && cable.switchId == targetId);
                                             }),
                             placedPowerCables.end());
}

bool CityModel::removeModule (int id)
{
    const auto oldSize = placedModules.size();

    placedModules.erase (std::remove_if (placedModules.begin(),
                                         placedModules.end(),
                                         [id] (const FoldingModule& module) { return module.id == id; }),
                         placedModules.end());

    if (placedModules.size() != oldSize)
        removePowerCablesFor (CityObjectKind::module, id);

    return placedModules.size() != oldSize;
}

bool CityModel::removePlatter (int id)
{
    const auto oldSize = placedPlatters.size();

    placedPlatters.erase (std::remove_if (placedPlatters.begin(),
                                          placedPlatters.end(),
                                          [id] (const FairgroundPlatter& platter) { return platter.id == id; }),
                          placedPlatters.end());

    if (placedPlatters.size() != oldSize)
    {
        removePowerCablesFor (CityObjectKind::platter, id);

        for (auto& module : placedModules)
        {
            if (module.attachedPlatterId == id)
            {
                module.attachedPlatterId = -1;
                module.attachedStandIndex = -1;
                module.elevation = elevationAt (module.position);
            }
        }

        for (auto& platter : placedPlatters)
        {
            if (platter.attachedPlatterId == id)
            {
                platter.attachedPlatterId = -1;
                platter.attachedStandIndex = -1;
                platter.elevation = elevationAt (platter.position);
            }
        }

        for (auto& plank : placedPlanks)
        {
            if (plank.attachedPlatterId == id)
            {
                plank.attachedPlatterId = -1;
                plank.attachedStandIndex = -1;
                plank.elevation = elevationAt (plank.position) + 10.0f;
            }
        }
    }

    return placedPlatters.size() != oldSize;
}

bool CityModel::removeBlock (int id)
{
    const auto oldSize = placedBlocks.size();

    placedBlocks.erase (std::remove_if (placedBlocks.begin(),
                                        placedBlocks.end(),
                                        [id] (const StackBlock& block) { return block.id == id; }),
                        placedBlocks.end());

    if (placedBlocks.size() != oldSize)
        removePowerCablesFor (CityObjectKind::block, id);

    return placedBlocks.size() != oldSize;
}

bool CityModel::removePlank (int id)
{
    const auto oldSize = placedPlanks.size();

    placedPlanks.erase (std::remove_if (placedPlanks.begin(),
                                        placedPlanks.end(),
                                        [id] (const Plank& plank) { return plank.id == id; }),
                        placedPlanks.end());

    if (placedPlanks.size() != oldSize)
    {
        removePowerCablesFor (CityObjectKind::plank, id);

        for (auto& module : placedModules)
        {
            if (module.attachedPlankId == id)
            {
                module.attachedPlankId = -1;
                module.elevation = elevationAt (module.position);
            }
        }

        for (auto& platter : placedPlatters)
        {
            if (platter.attachedPlankId == id)
            {
                platter.attachedPlankId = -1;
                platter.elevation = elevationAt (platter.position);
            }
        }
    }

    return placedPlanks.size() != oldSize;
}

bool CityModel::removePowerSwitch (int id)
{
    const auto oldSize = placedPowerSwitches.size();

    placedPowerSwitches.erase (std::remove_if (placedPowerSwitches.begin(),
                                               placedPowerSwitches.end(),
                                               [id] (const PowerSwitch& powerSwitch) { return powerSwitch.id == id; }),
                               placedPowerSwitches.end());

    if (placedPowerSwitches.size() != oldSize)
    {
        removePowerCablesFor (CityObjectKind::powerSwitch, id);
        placedPowerFeedCables.erase (std::remove_if (placedPowerFeedCables.begin(),
                                                     placedPowerFeedCables.end(),
                                                     [id] (const PowerFeedCable& cable) { return cable.switchId == id; }),
                                     placedPowerFeedCables.end());
    }

    return placedPowerSwitches.size() != oldSize;
}

bool CityModel::removePowerSource (int id)
{
    const auto oldSize = placedPowerSources.size();

    placedPowerSources.erase (std::remove_if (placedPowerSources.begin(),
                                              placedPowerSources.end(),
                                              [id] (const PowerSource& source) { return source.id == id; }),
                              placedPowerSources.end());

    if (placedPowerSources.size() != oldSize)
        placedPowerFeedCables.erase (std::remove_if (placedPowerFeedCables.begin(),
                                                     placedPowerFeedCables.end(),
                                                     [id] (const PowerFeedCable& cable) { return cable.sourceId == id; }),
                                     placedPowerFeedCables.end());

    return placedPowerSources.size() != oldSize;
}

FoldingModule* CityModel::findModule (int id)
{
    const auto found = std::find_if (placedModules.begin(),
                                     placedModules.end(),
                                     [id] (const FoldingModule& module) { return module.id == id; });

    return found != placedModules.end() ? &*found : nullptr;
}

const FoldingModule* CityModel::findModule (int id) const
{
    const auto found = std::find_if (placedModules.begin(),
                                     placedModules.end(),
                                     [id] (const FoldingModule& module) { return module.id == id; });

    return found != placedModules.end() ? &*found : nullptr;
}

FairgroundPlatter* CityModel::findPlatter (int id)
{
    const auto found = std::find_if (placedPlatters.begin(),
                                     placedPlatters.end(),
                                     [id] (const FairgroundPlatter& platter) { return platter.id == id; });

    return found != placedPlatters.end() ? &*found : nullptr;
}

const FairgroundPlatter* CityModel::findPlatter (int id) const
{
    const auto found = std::find_if (placedPlatters.begin(),
                                     placedPlatters.end(),
                                     [id] (const FairgroundPlatter& platter) { return platter.id == id; });

    return found != placedPlatters.end() ? &*found : nullptr;
}

PowerSwitch* CityModel::findPowerSwitch (int id)
{
    const auto found = std::find_if (placedPowerSwitches.begin(),
                                     placedPowerSwitches.end(),
                                     [id] (const PowerSwitch& powerSwitch) { return powerSwitch.id == id; });

    return found != placedPowerSwitches.end() ? &*found : nullptr;
}

const PowerSwitch* CityModel::findPowerSwitch (int id) const
{
    const auto found = std::find_if (placedPowerSwitches.begin(),
                                     placedPowerSwitches.end(),
                                     [id] (const PowerSwitch& powerSwitch) { return powerSwitch.id == id; });

    return found != placedPowerSwitches.end() ? &*found : nullptr;
}

PowerSource* CityModel::findPowerSource (int id)
{
    const auto found = std::find_if (placedPowerSources.begin(),
                                     placedPowerSources.end(),
                                     [id] (const PowerSource& source) { return source.id == id; });

    return found != placedPowerSources.end() ? &*found : nullptr;
}

const PowerSource* CityModel::findPowerSource (int id) const
{
    const auto found = std::find_if (placedPowerSources.begin(),
                                     placedPowerSources.end(),
                                     [id] (const PowerSource& source) { return source.id == id; });

    return found != placedPowerSources.end() ? &*found : nullptr;
}

StackBlock* CityModel::findBlock (int id)
{
    const auto found = std::find_if (placedBlocks.begin(),
                                     placedBlocks.end(),
                                     [id] (const StackBlock& block) { return block.id == id; });

    return found != placedBlocks.end() ? &*found : nullptr;
}

const StackBlock* CityModel::findBlock (int id) const
{
    const auto found = std::find_if (placedBlocks.begin(),
                                     placedBlocks.end(),
                                     [id] (const StackBlock& block) { return block.id == id; });

    return found != placedBlocks.end() ? &*found : nullptr;
}

Plank* CityModel::findPlank (int id)
{
    const auto found = std::find_if (placedPlanks.begin(),
                                     placedPlanks.end(),
                                     [id] (const Plank& plank) { return plank.id == id; });

    return found != placedPlanks.end() ? &*found : nullptr;
}

const Plank* CityModel::findPlank (int id) const
{
    const auto found = std::find_if (placedPlanks.begin(),
                                     placedPlanks.end(),
                                     [id] (const Plank& plank) { return plank.id == id; });

    return found != placedPlanks.end() ? &*found : nullptr;
}

StackBlock* CityModel::findTopBlockAt (Vec2 worldPosition)
{
    StackBlock* best = nullptr;

    for (auto& block : placedBlocks)
        if (block.contains (worldPosition) && (best == nullptr || block.topElevation() > best->topElevation()))
            best = &block;

    return best;
}

const StackBlock* CityModel::findTopBlockAt (Vec2 worldPosition) const
{
    const StackBlock* best = nullptr;

    for (const auto& block : placedBlocks)
        if (block.contains (worldPosition) && (best == nullptr || block.topElevation() > best->topElevation()))
            best = &block;

    return best;
}

float CityModel::elevationAt (Vec2 worldPosition) const
{
    if (const auto* block = findTopBlockAt (worldPosition))
        return block->topElevation() + structureSurfaceClearance;

    return 0.0f;
}

bool CityModel::isPoweredAt (Vec2 worldPosition) const
{
    const PowerSwitch* controller = nullptr;
    auto bestDistanceSquared = std::numeric_limits<float>::max();

    for (const auto& powerSwitch : placedPowerSwitches)
    {
        const auto switchDistanceSquared = squaredDistance (worldPosition, powerSwitch.position);
        const auto areaRadiusSquared = powerSwitch.areaRadius * powerSwitch.areaRadius;

        if (switchDistanceSquared <= areaRadiusSquared && switchDistanceSquared < bestDistanceSquared)
        {
            bestDistanceSquared = switchDistanceSquared;
            controller = &powerSwitch;
        }
    }

    return controller == nullptr || isSwitchEnergized (*controller);
}

bool CityModel::isPowered (CityObjectKind kind, int id, Vec2 worldPosition) const
{
    const auto foundCable = std::find_if (placedPowerCables.begin(),
                                          placedPowerCables.end(),
                                          [kind, id] (const PowerCable& cable)
                                          {
                                              return cable.targetKind == kind && cable.targetId == id;
                                          });

    if (foundCable != placedPowerCables.end())
        if (const auto* powerSwitch = findPowerSwitch (foundCable->switchId))
            return isSwitchEnergized (*powerSwitch);

    return isPoweredAt (worldPosition);
}

bool CityModel::isSwitchEnergized (const PowerSwitch& powerSwitch) const
{
    if (! powerSwitch.powered)
        return false;

    const auto feed = std::find_if (placedPowerFeedCables.begin(),
                                    placedPowerFeedCables.end(),
                                    [&powerSwitch] (const PowerFeedCable& cable)
                                    {
                                        return cable.switchId == powerSwitch.id;
                                    });

    if (feed == placedPowerFeedCables.end())
        return true;

    if (const auto* source = findPowerSource (feed->sourceId))
        return source->powered;

    return false;
}

CityHit CityModel::hitTest (Vec2 worldPosition, double timeSeconds, float globalTempoBpm) const
{
    CityHit bestHit;
    auto bestDistanceSquared = std::numeric_limits<float>::max();

    for (const auto& module : placedModules)
    {
        const auto hitRadius = module.collisionRadiusAt (timeSeconds, globalTempoBpm);
        const auto hitDistanceSquared = squaredDistance (worldPosition, module.position);
        const auto hitRadiusSquared = hitRadius * hitRadius;

        if (hitDistanceSquared <= hitRadiusSquared && hitDistanceSquared < bestDistanceSquared)
        {
            bestDistanceSquared = hitDistanceSquared;
            bestHit = { CityObjectKind::module, module.id };
        }
    }

    for (const auto& platter : placedPlatters)
    {
        const auto centreDistanceSquared = squaredDistance (worldPosition, platter.position);
        const auto hitRadius = platter.hitRadius();
        const auto hitRadiusSquared = hitRadius * hitRadius;

        if (centreDistanceSquared <= hitRadiusSquared && centreDistanceSquared < bestDistanceSquared)
        {
            bestDistanceSquared = centreDistanceSquared;
            bestHit = { CityObjectKind::platter, platter.id };
        }

        const auto mountRadius = platter.mountRadius();
        const auto mountRadiusSquared = mountRadius * mountRadius;

        for (int i = 0; i < platter.stands; ++i)
        {
            const auto stand = platter.standPosition (i, timeSeconds, globalTempoBpm);
            const auto hitDistanceSquared = squaredDistance (worldPosition, stand);

            if (hitDistanceSquared <= mountRadiusSquared && hitDistanceSquared < bestDistanceSquared)
            {
                bestDistanceSquared = hitDistanceSquared;
                bestHit = { CityObjectKind::platter, platter.id };
            }
        }
    }

    for (const auto& plank : placedPlanks)
    {
        const auto socket = plank.socketPosition();
        const auto segmentDistance = distanceToSegment (worldPosition, plank.position, socket);
        const auto hitDistanceSquared = std::min (segmentDistance * segmentDistance,
                                                  squaredDistance (worldPosition, socket));
        const auto socketRadius = plank.socketRadius();
        const auto socketRadiusSquared = socketRadius * socketRadius;

        if (hitDistanceSquared <= socketRadiusSquared && hitDistanceSquared < bestDistanceSquared)
        {
            bestDistanceSquared = hitDistanceSquared;
            bestHit = { CityObjectKind::plank, plank.id };
        }
    }

    for (const auto& powerSwitch : placedPowerSwitches)
    {
        const auto hitDistanceSquared = squaredDistance (worldPosition, powerSwitch.position);
        const auto triggerRadiusSquared = powerSwitch.triggerRadius * powerSwitch.triggerRadius;

        if (hitDistanceSquared <= triggerRadiusSquared && hitDistanceSquared < bestDistanceSquared)
        {
            bestDistanceSquared = hitDistanceSquared;
            bestHit = { CityObjectKind::powerSwitch, powerSwitch.id };
        }
    }

    for (const auto& source : placedPowerSources)
    {
        const auto hitDistanceSquared = squaredDistance (worldPosition, source.position);
        const auto radiusSquared = source.radius * source.radius;

        if (hitDistanceSquared <= radiusSquared && hitDistanceSquared < bestDistanceSquared)
        {
            bestDistanceSquared = hitDistanceSquared;
            bestHit = { CityObjectKind::powerSource, source.id };
        }
    }

    for (const auto& block : placedBlocks)
    {
        const auto hitDistanceSquared = squaredDistance (worldPosition, block.position);

        if (block.contains (worldPosition) && hitDistanceSquared < bestDistanceSquared)
        {
            bestDistanceSquared = hitDistanceSquared;
            bestHit = { CityObjectKind::block, block.id };
        }
    }

    return bestHit;
}
