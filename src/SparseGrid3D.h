#pragma once

#include <unordered_set>
#include <vector>
#include <array>
#include <cstdint>
#include "GameRules.h"

class SparseGrid3D {
public:
    SparseGrid3D(uint32_t width, uint32_t height, uint32_t depth);
    
    // Grid management
    void initialize();
    void update();
    void resize(uint32_t width, uint32_t height, uint32_t depth);
    
    // Cell operations
    void setCell(uint32_t x, uint32_t y, uint32_t z, bool state);
    bool getCell(uint32_t x, uint32_t y, uint32_t z) const;
    void clear();
    
    // Rule management
    void setRuleSet(const GameRules::RuleSet& rules);
    void setBoundaryType(GameRules::BoundaryType type);
    const GameRules::RuleSet& getCurrentRuleSet() const { return currentRules; }
    GameRules::BoundaryType getBoundaryType() const { return boundaryType; }
    
    // Grid properties
    uint32_t getWidth() const { return width; }
    uint32_t getHeight() const { return height; }
    uint32_t getDepth() const { return depth; }
    uint32_t getTotalCells() const { return width * height * depth; }
    
    // Statistics
    uint32_t getPopulation() const { return population; }
    uint32_t getGeneration() const { return generation; }
    
    // Memory usage
    size_t getMemoryUsage() const;
    
private:
    // Grid dimensions
    uint32_t width;
    uint32_t height;
    uint32_t depth;
    
    // State tracking
    uint32_t population;
    uint32_t generation;
    
    // Sparse storage
    std::unordered_set<uint64_t> liveCells;
    std::unordered_set<uint64_t> nextLiveCells;
    
    // Rules and boundaries
    GameRules::RuleSet currentRules;
    GameRules::BoundaryType boundaryType;
    
    // Helper functions
    uint64_t getCellKey(uint32_t x, uint32_t y, uint32_t z) const;
    void getCellCoords(uint64_t key, uint32_t& x, uint32_t& y, uint32_t& z) const;
    bool isValidPosition(uint32_t x, uint32_t y, uint32_t z) const;
    uint32_t countNeighbors(uint32_t x, uint32_t y, uint32_t z) const;
    bool getWrappedCell(int32_t x, int32_t y, int32_t z) const;
    
    // Optimization: Cache for neighbor counts
    struct NeighborCache {
        std::unordered_map<uint64_t, uint32_t> counts;
        void clear() { counts.clear(); }
    };
    mutable NeighborCache neighborCache;
}; 