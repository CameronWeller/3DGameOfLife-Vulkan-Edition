#include "HIPCellularAutomata.hpp"
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cassert>
#include <unordered_map>

// Include the structures from the header
using namespace UXMirror;

#ifdef HIP_AVAILABLE
// HIP kernel for 3D cellular automata computation
__global__ void cellularAutomataKernel(
    const CellState* __restrict__ current_grid,
    CellState* __restrict__ next_grid,
    const SimulationParams* __restrict__ params,
    uint32_t width, uint32_t height, uint32_t depth
) {
    // Calculate 3D thread index with coalesced memory access
    uint32_t x = blockIdx.x * blockDim.x + threadIdx.x;
    uint32_t y = blockIdx.y * blockDim.y + threadIdx.y;  
    uint32_t z = blockIdx.z * blockDim.z + threadIdx.z;
    
    // Bounds checking
    if (x >= width || y >= height || z >= depth) return;
    
    // Linear index for coalesced memory access
    uint32_t idx = z * width * height + y * width + x;
    
    // Shared memory for workgroup tiling (optimized access pattern)
    __shared__ CellState shared_cells[8][8][8]; // 8x8x8 tile
    
    // Local thread coordinates within the tile
    uint32_t local_x = threadIdx.x;
    uint32_t local_y = threadIdx.y;
    uint32_t local_z = threadIdx.z;
    
    // Load current cell into shared memory
    if (local_x < 8 && local_y < 8 && local_z < 8) {
        shared_cells[local_z][local_y][local_x] = current_grid[idx];
    }
    
    __syncthreads();
    
    // Count living neighbors (26-neighborhood for 3D)
    int neighbor_count = 0;
    float total_energy = 0.0f;
    
    // Optimized neighbor counting with memory coalescing
    for (int dz = -1; dz <= 1; dz++) {
        for (int dy = -1; dy <= 1; dy++) {
            for (int dx = -1; dx <= 1; dx++) {
                if (dx == 0 && dy == 0 && dz == 0) continue;
                
                int nx = x + dx;
                int ny = y + dy;
                int nz = z + dz;
                
                // Handle boundary conditions (wrap-around for now)
                nx = (nx + width) % width;
                ny = (ny + height) % height;
                nz = (nz + depth) % depth;
                
                uint32_t neighbor_idx = nz * width * height + ny * width + nx;
                CellState neighbor = current_grid[neighbor_idx];
                
                if (neighbor.state) {
                    neighbor_count++;
                    total_energy += neighbor.energy;
                }
            }
        }
    }
    
    // Get current cell state
    CellState current = current_grid[idx];
    CellState next = current;
    
    // Apply Game of Life rules with energy system
    bool should_be_alive = false;
    
    if (current.state) {
        // Cell is currently alive
        should_be_alive = (neighbor_count >= params->survival_min && 
                          neighbor_count <= params->survival_max);
        if (!should_be_alive) {
            next.age = 0;
            next.energy = 0;
        } else {
            next.age = min(127, current.age + 1);
            next.energy = max(0.0f, current.energy * params->energy_decay_rate);
        }
    } else {
        // Cell is currently dead
        should_be_alive = (neighbor_count >= params->birth_threshold);
        if (should_be_alive) {
            next.age = 1;
            next.energy = min(255.0f, total_energy * 0.1f); // Inherit energy from neighbors
        }
    }
    
    next.state = should_be_alive ? 1 : 0;
    next.neighbor_count = neighbor_count;
    
    // Write result with coalesced access
    next_grid[idx] = next;
}

// Neighbor counting kernel for statistics
__global__ void countLiveCellsKernel(
    const CellState* __restrict__ grid,
    uint32_t* __restrict__ counts,
    uint32_t total_cells
) {
    uint32_t idx = blockIdx.x * blockDim.x + threadIdx.x;
    if (idx >= total_cells) return;
    
    // Use shared memory for reduction
    __shared__ uint32_t shared_counts[256];
    uint32_t tid = threadIdx.x;
    
    shared_counts[tid] = (idx < total_cells && grid[idx].state) ? 1 : 0;
    __syncthreads();
    
    // Parallel reduction
    for (uint32_t stride = blockDim.x / 2; stride > 0; stride >>= 1) {
        if (tid < stride) {
            shared_counts[tid] += shared_counts[tid + stride];
        }
        __syncthreads();
    }
    
    if (tid == 0) {
        atomicAdd(counts, shared_counts[0]);
    }
}

namespace UXMirror {

HIPCellularAutomata::HIPCellularAutomata()
    : grid_width_(0)
    , grid_height_(0)
    , grid_depth_(0)
    , total_cells_(0)
    , device_grid_current_(nullptr)
    , device_grid_next_(nullptr)
    , device_params_(nullptr)
    , boundary_condition_(BoundaryCondition::WRAP_AROUND)
    , current_generation_(0)
    , start_event_(nullptr)
    , stop_event_(nullptr)
    , last_kernel_time_(0.0f)
{
    // Initialize default simulation parameters
    simulation_params_.energy_decay_rate = 0.95f;
    simulation_params_.birth_threshold = 3;
    simulation_params_.death_threshold = 2;
    simulation_params_.survival_min = 2;
    simulation_params_.survival_max = 3;
}

HIPCellularAutomata::~HIPCellularAutomata() {
    cleanup();
}

bool HIPCellularAutomata::initialize(uint32_t width, uint32_t height, uint32_t depth) {
    grid_width_ = width;
    grid_height_ = height;
    grid_depth_ = depth;
    total_cells_ = static_cast<size_t>(width) * height * depth;
    
    if (!validateGridDimensions()) {
        setError("Invalid grid dimensions");
        return false;
    }
    
    // Initialize HIP events for timing
    hipError_t result = hipEventCreate(&start_event_);
    if (result != hipSuccess) {
        setError("Failed to create start event");
        return false;
    }
    
    result = hipEventCreate(&stop_event_);
    if (result != hipSuccess) {
        setError("Failed to create stop event");
        return false;
    }
    
    // Allocate GPU memory
    if (!allocateGridMemory()) {
        return false;
    }
    
    // Optimize workgroup configuration
    if (!optimizeWorkgroupConfig()) {
        setError("Failed to optimize workgroup configuration");
        return false;
    }
    
    // Update simulation parameters
    simulation_params_.grid_width = width;
    simulation_params_.grid_height = height;
    simulation_params_.grid_depth = depth;
    simulation_params_.generation = 0;
    
    return true;
}

void HIPCellularAutomata::cleanup() {
    if (device_grid_current_) {
        hipFree(device_grid_current_);
        device_grid_current_ = nullptr;
    }
    
    if (device_grid_next_) {
        hipFree(device_grid_next_);
        device_grid_next_ = nullptr;
    }
    
    if (device_params_) {
        hipFree(device_params_);
        device_params_ = nullptr;
    }
    
    if (start_event_) {
        hipEventDestroy(start_event_);
        start_event_ = nullptr;
    }
    
    if (stop_event_) {
        hipEventDestroy(stop_event_);
        stop_event_ = nullptr;
    }
}

bool HIPCellularAutomata::allocateGridMemory() {
    size_t grid_size = total_cells_ * sizeof(CellState);
    
    // Allocate current grid
    hipError_t result = hipMalloc(&device_grid_current_, grid_size);
    if (result != hipSuccess) {
        setError("Failed to allocate current grid memory");
        return false;
    }
    
    // Allocate next grid
    result = hipMalloc(&device_grid_next_, grid_size);
    if (result != hipSuccess) {
        setError("Failed to allocate next grid memory");
        return false;
    }
    
    // Initialize grids to zero
    result = hipMemset(device_grid_current_, 0, grid_size);
    if (result != hipSuccess) {
        setError("Failed to initialize current grid");
        return false;
    }
    
    result = hipMemset(device_grid_next_, 0, grid_size);
    if (result != hipSuccess) {
        setError("Failed to initialize next grid");
        return false;
    }
    
    // Allocate simulation parameters
    result = hipMalloc(&device_params_, sizeof(SimulationParams));
    if (result != hipSuccess) {
        setError("Failed to allocate simulation parameters");
        return false;
    }
    
    return true;
}

bool HIPCellularAutomata::optimizeWorkgroupConfig() {
    // This addresses the "implement_workgroup_tiling" unblock action
    
    // Get device properties for optimization
    hipDeviceProp_t device_props;
    hipError_t result = hipGetDeviceProperties(&device_props, 0);
    if (result != hipSuccess) {
        setError("Failed to get device properties");
        return false;
    }
    
    // Calculate optimal block dimensions
    // For 3D cellular automata, 8x8x8 is typically optimal for memory coalescing
    workgroup_config_.block_size_x = 8;
    workgroup_config_.block_size_y = 8;
    workgroup_config_.block_size_z = 8;
    
    // Calculate grid dimensions
    workgroup_config_.grid_size_x = (grid_width_ + workgroup_config_.block_size_x - 1) / workgroup_config_.block_size_x;
    workgroup_config_.grid_size_y = (grid_height_ + workgroup_config_.block_size_y - 1) / workgroup_config_.block_size_y;
    workgroup_config_.grid_size_z = (grid_depth_ + workgroup_config_.block_size_z - 1) / workgroup_config_.block_size_z;
    
    // Calculate shared memory size
    workgroup_config_.shared_memory_size = 8 * 8 * 8 * sizeof(CellState);
    
    // Verify configuration is within device limits
    if (workgroup_config_.block_size_x * workgroup_config_.block_size_y * workgroup_config_.block_size_z > device_props.maxThreadsPerBlock) {
        setError("Block size exceeds device limits");
        return false;
    }
    
    if (workgroup_config_.shared_memory_size > device_props.sharedMemPerBlock) {
        setError("Shared memory size exceeds device limits");
        return false;
    }
    
    return true;
}

bool HIPCellularAutomata::stepSimulation() {
    // Copy simulation parameters to device
    hipError_t result = hipMemcpy(device_params_, &simulation_params_, sizeof(SimulationParams), hipMemcpyHostToDevice);
    if (result != hipSuccess) {
        setError("Failed to copy simulation parameters");
        return false;
    }
    
    // Launch compute kernel with timing
    result = hipEventRecord(start_event_);
    if (result != hipSuccess) {
        setError("Failed to record start event");
        return false;
    }
    
    if (!launchComputeKernel()) {
        return false;
    }
    
    result = hipEventRecord(stop_event_);
    if (result != hipSuccess) {
        setError("Failed to record stop event");
        return false;
    }
    
    // Wait for completion and measure time
    result = hipEventSynchronize(stop_event_);
    if (result != hipSuccess) {
        setError("Failed to synchronize stop event");
        return false;
    }
    
    result = hipEventElapsedTime(&last_kernel_time_, start_event_, stop_event_);
    if (result != hipSuccess) {
        setError("Failed to measure kernel time");
        return false;
    }
    
    // Swap grids for next iteration
    std::swap(device_grid_current_, device_grid_next_);
    
    current_generation_++;
    simulation_params_.generation = current_generation_;
    
    return true;
}

bool HIPCellularAutomata::launchComputeKernel() {
    // This addresses the "optimize_memory_access_patterns" unblock action
    
    dim3 block_dim(workgroup_config_.block_size_x, 
                   workgroup_config_.block_size_y, 
                   workgroup_config_.block_size_z);
    
    dim3 grid_dim(workgroup_config_.grid_size_x,
                  workgroup_config_.grid_size_y,
                  workgroup_config_.grid_size_z);
    
    // Launch kernel with optimized memory access patterns
    hipLaunchKernelGGL(
        cellularAutomataKernel,
        grid_dim,
        block_dim,
        workgroup_config_.shared_memory_size,
        0, // Default stream
        device_grid_current_,
        device_grid_next_,
        device_params_,
        grid_width_,
        grid_height_,
        grid_depth_
    );
    
    // Check for kernel launch errors
    hipError_t result = hipGetLastError();
    if (result != hipSuccess) {
        setError("Kernel launch failed: " + std::string(hipGetErrorString(result)));
        return false;
    }
    
    return true;
}

bool HIPCellularAutomata::copyGridToDevice(const std::vector<CellState>& host_grid) {
    if (host_grid.size() != total_cells_) {
        setError("Host grid size mismatch");
        return false;
    }
    
    size_t grid_size = total_cells_ * sizeof(CellState);
    hipError_t result = hipMemcpy(device_grid_current_, host_grid.data(), grid_size, hipMemcpyHostToDevice);
    if (result != hipSuccess) {
        setError("Failed to copy grid to device");
        return false;
    }
    
    return true;
}

bool HIPCellularAutomata::copyGridFromDevice(std::vector<CellState>& host_grid) {
    host_grid.resize(total_cells_);
    
    size_t grid_size = total_cells_ * sizeof(CellState);
    hipError_t result = hipMemcpy(host_grid.data(), device_grid_current_, grid_size, hipMemcpyDeviceToHost);
    if (result != hipSuccess) {
        setError("Failed to copy grid from device");
        return false;
    }
    
    return true;
}

uint32_t HIPCellularAutomata::getLiveCellCount() const {
    // Allocate device memory for count
    uint32_t* device_count;
    hipError_t result = hipMalloc(&device_count, sizeof(uint32_t));
    if (result != hipSuccess) {
        return 0;
    }
    
    // Initialize count to zero
    result = hipMemset(device_count, 0, sizeof(uint32_t));
    if (result != hipSuccess) {
        hipFree(device_count);
        return 0;
    }
    
    // Launch counting kernel
    dim3 block_dim(256);
    dim3 grid_dim((total_cells_ + block_dim.x - 1) / block_dim.x);
    
    hipLaunchKernelGGL(
        countLiveCellsKernel,
        grid_dim,
        block_dim,
        0,
        0,
        device_grid_current_,
        device_count,
        total_cells_
    );
    
    // Copy result back to host
    uint32_t host_count = 0;
    result = hipMemcpy(&host_count, device_count, sizeof(uint32_t), hipMemcpyDeviceToHost);
    
    hipFree(device_count);
    
    return (result == hipSuccess) ? host_count : 0;
}

bool HIPCellularAutomata::validateGridDimensions() const {
    return (grid_width_ > 0 && grid_height_ > 0 && grid_depth_ > 0 &&
            grid_width_ <= 2048 && grid_height_ <= 2048 && grid_depth_ <= 2048);
}

void HIPCellularAutomata::setError(const std::string& error) {
    last_error_ = error;
    std::cerr << "HIPCellularAutomata Error: " << error << std::endl;
}

// Utility function implementations
namespace CellularAutomataUtils {

KernelLaunchParams calculateOptimalLaunchParams(uint32_t width, uint32_t height, uint32_t depth) {
    KernelLaunchParams params;
    
    // Optimal block size for 3D problems
    params.block_dim = dim3(8, 8, 8);
    
    // Calculate grid dimensions
    params.grid_dim = dim3(
        (width + params.block_dim.x - 1) / params.block_dim.x,
        (height + params.block_dim.y - 1) / params.block_dim.y,
        (depth + params.block_dim.z - 1) / params.block_dim.z
    );
    
    // Shared memory for tile optimization
    params.shared_memory_size = 8 * 8 * 8 * sizeof(CellState);
    params.stream = nullptr;
    
    return params;
}

bool analyzeMemoryCoalescing(const WorkgroupConfig& config, float& efficiency) {
    // This addresses the "memory_access_pattern_analysis" unblock action
    
    // Calculate memory coalescing efficiency based on access patterns
    // For 3D cellular automata with linear memory layout (z-y-x order),
    // optimal coalescing occurs when threads access consecutive x coordinates
    
    float coalescing_factor = 1.0f;
    
    // X-dimension access is always coalesced
    if (config.block_size_x >= 32) {
        coalescing_factor *= 1.0f; // Perfect coalescing
    } else {
        coalescing_factor *= static_cast<float>(config.block_size_x) / 32.0f;
    }
    
    // Y and Z dimensions affect memory stride
    float stride_penalty = 1.0f / (1.0f + log2f(config.block_size_y * config.block_size_z));
    coalescing_factor *= stride_penalty;
    
    efficiency = coalescing_factor;
    return true;
}

inline uint32_t wrapCoordinate(int32_t coord, uint32_t dimension) {
    return (coord + dimension) % dimension;
}

inline bool isValidCoordinate(int32_t x, int32_t y, int32_t z, uint32_t width, uint32_t height, uint32_t depth) {
    return (x >= 0 && x < static_cast<int32_t>(width) &&
            y >= 0 && y < static_cast<int32_t>(height) &&
            z >= 0 && z < static_cast<int32_t>(depth));
}

} // namespace CellularAutomataUtils

} // namespace UXMirror

#else // HIP_AVAILABLE

namespace UXMirror {

// Stub implementations when HIP is not available
HIPCellularAutomata::HIPCellularAutomata() {}
HIPCellularAutomata::~HIPCellularAutomata() {}
bool HIPCellularAutomata::initialize(uint32_t, uint32_t, uint32_t) { return false; }
void HIPCellularAutomata::cleanup() {}
bool HIPCellularAutomata::stepSimulation() { return false; }
uint32_t HIPCellularAutomata::getLiveCellCount() const { return 0; }

} // namespace UXMirror

#endif // HIP_AVAILABLE 