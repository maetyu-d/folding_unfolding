#pragma once

#include "FairgroundPlatter.h"
#include "FoldingModule.h"
#include "PowerSwitch.h"
#include "Plank.h"
#include "StackBlock.h"
#include <vector>

enum class CityObjectKind
{
    none,
    module,
    platter,
    block,
    plank,
    powerSwitch,
    powerSource
};

struct CityHit
{
    CityObjectKind kind = CityObjectKind::none;
    int id = -1;
};

struct PowerCable
{
    int switchId = -1;
    CityObjectKind targetKind = CityObjectKind::none;
    int targetId = -1;
};

struct PowerFeedCable
{
    int sourceId = -1;
    int switchId = -1;
};

class CityModel
{
public:
    void clear() noexcept;
    void resetNextIdsFromContents() noexcept;

    FoldingModule& addModule (Vec2 position,
                              int sides,
                              float radius,
                              float flapDepth,
                              float rateDivision = 1.0f,
                              float phase = 0.0f);
    FairgroundPlatter& addPlatter (Vec2 position,
                                   int stands,
                                   float diameter,
                                   float rateDivision,
                                   float phase);
    StackBlock& addBlock (Vec2 position, float size, int levels);
    Plank& addPlank (Vec2 position, float length, float angle);
    PowerSwitch& addPowerSwitch (Vec2 position,
                                 float areaRadius,
                                 bool powered = true,
                                 float triggerRadius = 34.0f);
    PowerSource& addPowerSource (Vec2 position, bool powered = true, float radius = 46.0f);
    void connectPowerFeed (int sourceId, int switchId);
    void connectPowerCable (int switchId, CityObjectKind targetKind, int targetId);
    bool removePowerCable (CityObjectKind targetKind, int targetId);
    void removePowerCablesFor (CityObjectKind targetKind, int targetId);
    bool removeModule (int id);
    bool removePlatter (int id);
    bool removeBlock (int id);
    bool removePlank (int id);
    bool removePowerSwitch (int id);
    bool removePowerSource (int id);

    FoldingModule* findModule (int id);
    const FoldingModule* findModule (int id) const;
    FairgroundPlatter* findPlatter (int id);
    const FairgroundPlatter* findPlatter (int id) const;
    PowerSwitch* findPowerSwitch (int id);
    const PowerSwitch* findPowerSwitch (int id) const;
    PowerSource* findPowerSource (int id);
    const PowerSource* findPowerSource (int id) const;
    StackBlock* findBlock (int id);
    const StackBlock* findBlock (int id) const;
    Plank* findPlank (int id);
    const Plank* findPlank (int id) const;
    StackBlock* findTopBlockAt (Vec2 worldPosition);
    const StackBlock* findTopBlockAt (Vec2 worldPosition) const;
    float elevationAt (Vec2 worldPosition) const;
    bool isPoweredAt (Vec2 worldPosition) const;
    bool isPowered (CityObjectKind kind, int id, Vec2 worldPosition) const;
    bool isSwitchEnergized (const PowerSwitch& powerSwitch) const;

    CityHit hitTest (Vec2 worldPosition,
                     double timeSeconds,
                     float globalTempoBpm = FoldingModule::defaultGlobalTempoBpm) const;

    std::vector<FoldingModule>& modules() noexcept { return placedModules; }
    const std::vector<FoldingModule>& modules() const noexcept { return placedModules; }
    std::vector<FairgroundPlatter>& platters() noexcept { return placedPlatters; }
    const std::vector<FairgroundPlatter>& platters() const noexcept { return placedPlatters; }
    std::vector<StackBlock>& blocks() noexcept { return placedBlocks; }
    const std::vector<StackBlock>& blocks() const noexcept { return placedBlocks; }
    std::vector<Plank>& planks() noexcept { return placedPlanks; }
    const std::vector<Plank>& planks() const noexcept { return placedPlanks; }
    std::vector<PowerSwitch>& powerSwitches() noexcept { return placedPowerSwitches; }
    const std::vector<PowerSwitch>& powerSwitches() const noexcept { return placedPowerSwitches; }
    std::vector<PowerSource>& powerSources() noexcept { return placedPowerSources; }
    const std::vector<PowerSource>& powerSources() const noexcept { return placedPowerSources; }
    std::vector<PowerFeedCable>& powerFeedCables() noexcept { return placedPowerFeedCables; }
    const std::vector<PowerFeedCable>& powerFeedCables() const noexcept { return placedPowerFeedCables; }
    std::vector<PowerCable>& powerCables() noexcept { return placedPowerCables; }
    const std::vector<PowerCable>& powerCables() const noexcept { return placedPowerCables; }

private:
    int nextId = 1;
    int nextPlatterId = 1;
    int nextBlockId = 1;
    int nextPlankId = 1;
    int nextPowerSwitchId = 1;
    int nextPowerSourceId = 1;
    std::vector<FoldingModule> placedModules;
    std::vector<FairgroundPlatter> placedPlatters;
    std::vector<StackBlock> placedBlocks;
    std::vector<Plank> placedPlanks;
    std::vector<PowerSwitch> placedPowerSwitches;
    std::vector<PowerSource> placedPowerSources;
    std::vector<PowerFeedCable> placedPowerFeedCables;
    std::vector<PowerCable> placedPowerCables;
};
