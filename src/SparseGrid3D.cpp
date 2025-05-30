#include "SparseGrid3D.h"
#include <random>
#include <algorithm>

SparseGrid3D::SparseGrid3D(uint32_t width, uint32_t height, uint32_t depth)
    : width(width), height(height), depth(depth),
      population(0), generation(0),
      currentRules(GameRules::RULE_5766),
      boundaryType(GameRules::BoundaryType::TOROIDAL) {
}

void SparseGrid3D::initialize() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(0.0f, 1.0f);
    
    clear();
    
    // Initialize with random state (20% density)
    for (uint32_t z = 0; z < depth; z++) {
        for (uint32_t y = 0; y < height; y++) {
            for (uint32_t x = 0; x < width; x++) {
                if (dis(gen) < 0.2f) {
                    setCell(x, y, z, true);
                }
            }
        }
    }
}

void SparseGrid3D::update() {
    generation++;
    nextLiveCells.clear();
    neighborCache.clear();
    population = 0;
    
    // First pass: count neighbors for all live cells and their neighbors
    std::unordered_set<uint64_t> cellsToCheck;
    
    // Add all live cells and their neighbors to the check set
    for (uint64_t cell : liveCells) {
        uint32_t x, y, z;
        getCellCoords(cell, x, y, z);
        
        cellsToCheck.insert(cell);
        
        // Add all neighbors
        for (int32_t dz = -1; dz <= 1; dz++) {
            for (int32_t dy = -1; dy <= 1; dy++) {
                for (int32_t dx = -1; dx <= 1; dx++) {
                    if (dx == 0 && dy == 0 && dz == 0) continue;
                    
                    int32_t nx = x + dx;
                    int32_t ny = y + dy;
                    int32_t nz = z + dz;
                    
                    if (isValidPosition(nx, ny, nz) || boundaryType != GameRules::BoundaryType::INFINITE) {
                        cellsToCheck.insert(getCellKey(nx, ny, nz));
                    }
                }
            }
        }
    }
    
    // Second pass: apply rules to all cells that need checking
    for (uint64_t cell : cellsToCheck) {
        uint32_t x, y, z;
        getCellCoords(cell, x, y, z);
        
        bool currentState = liveCells.count(cell) > 0;
        uint32_t neighbors = countNeighbors(x, y, z);
        bool nextState = GameRules::getNextState(currentState, neighbors, currentRules);
        
        if (nextState) {
            nextLiveCells.insert(cell);
            population++;
        }
    }
    
    // Swap states
    std::swap(liveCells, nextLiveCells);
}

void SparseGrid3D::resize(uint32_t newWidth, uint32_t newHeight, uint32_t newDepth) {
    width = newWidth;
    height = newHeight;
    depth = newDepth;
    
    // Clear cells that are now outside the grid
    std::unordered_set<uint64_t> newLiveCells;
    for (uint64_t cell : liveCells) {
        uint32_t x, y, z;
        getCellCoords(cell, x, y, z);
        if (isValidPosition(x, y, z)) {
            newLiveCells.insert(cell);
        }
    }
    liveCells = std::move(newLiveCells);
    
    // Update population count
    population = liveCells.size();
}

void SparseGrid3D::setCell(uint32_t x, uint32_t y, uint32_t z, bool state) {
    if (!isValidPosition(x, y, z)) return;
    
    uint64_t key = getCellKey(x, y, z);
    bool currentState = liveCells.count(key) > 0;
    
    if (currentState != state) {
        if (state) {
            liveCells.insert(key);
            population++;
        } else {
            liveCells.erase(key);
            population--;
        }
    }
}

bool SparseGrid3D::getCell(uint32_t x, uint32_t y, uint32_t z) const {
    if (!isValidPosition(x, y, z)) {
        return getWrappedCell(x, y, z);
    }
    return liveCells.count(getCellKey(x, y, z)) > 0;
}

void SparseGrid3D::clear() {
    liveCells.clear();
    nextLiveCells.clear();
    neighborCache.clear();
    population = 0;
    generation = 0;
}

void SparseGrid3D::setRuleSet(const GameRules::RuleSet& rules) {
    currentRules = rules;
}

void SparseGrid3D::setBoundaryType(GameRules::BoundaryType type) {
    boundaryType = type;
}

uint64_t SparseGrid3D::getCellKey(uint32_t x, uint32_t y, uint32_t z) const {
    return (static_cast<uint64_t>(z) << 40) | (static_cast<uint64_t>(y) << 20) | x;
}

void SparseGrid3D::getCellCoords(uint64_t key, uint32_t& x, uint32_t& y, uint32_t& z) const {
    x = key & 0xFFFFF;
    y = (key >> 20) & 0xFFFFF;
    z = (key >> 40) & 0xFFFFF;
}

bool SparseGrid3D::isValidPosition(uint32_t x, uint32_t y, uint32_t z) const {
    return x < width && y < height && z < depth;
}

uint32_t SparseGrid3D::countNeighbors(uint32_t x, uint32_t y, uint32_t z) const {
    uint64_t key = getCellKey(x, y, z);
    
    // Check cache first
    auto it = neighborCache.counts.find(key);
    if (it != neighborCache.counts.end()) {
        return it->second;
    }
    
    uint32_t count = 0;
    
    // Check all 26 neighbors
    for (int32_t dz = -1; dz <= 1; dz++) {
        for (int32_t dy = -1; dy <= 1; dy++) {
            for (int32_t dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0 && dz == 0) continue;
                
                if (getWrappedCell(x + dx, y + dy, z + dz)) {
                    count++;
                }
            }
        }
    }
    
    // Cache the result
    neighborCache.counts[key] = count;
    
    return count;
}

bool SparseGrid3D::getWrappedCell(int32_t x, int32_t y, int32_t z) const {
    switch (boundaryType) {
        case GameRules::BoundaryType::TOROIDAL:
            // Wrap coordinates
            x = (x + width) % width;
            y = (y + height) % height;
            z = (z + depth) % depth;
            return liveCells.count(getCellKey(x, y, z)) > 0;
            
        case GameRules::BoundaryType::MIRROR:
            // Mirror coordinates
            if (x < 0) x = -x;
            if (y < 0) y = -y;
            if (z < 0) z = -z;
            if (x >= width) x = 2 * width - x - 1;
            if (y >= height) y = 2 * height - y - 1;
            if (z >= depth) z = 2 * depth - z - 1;
            return liveCells.count(getCellKey(x, y, z)) > 0;
            
        case GameRules::BoundaryType::FIXED:
        case GameRules::BoundaryType::INFINITE:
        default:
            return false;
    }
}

size_t SparseGrid3D::getMemoryUsage() const {
    size_t total = 0;
    
    // Size of the sets
    total += liveCells.size() * sizeof(uint64_t);
    total += nextLiveCells.size() * sizeof(uint64_t);
    
    // Size of the neighbor cache
    total += neighborCache.counts.size() * (sizeof(uint64_t) + sizeof(uint32_t));
    
    // Size of the class members
    total += sizeof(SparseGrid3D);
    
    return total;
} 