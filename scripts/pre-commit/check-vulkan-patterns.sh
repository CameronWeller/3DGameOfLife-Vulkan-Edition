#!/bin/bash
# Check for proper Vulkan API usage patterns

set -e

exit_code=0

echo "🌋 Checking Vulkan API usage patterns..."

for file in "$@"; do
    if [[ -f "$file" ]]; then
        echo "  Checking: $file"
        
        # Check for proper error handling
        if grep -q "vk[A-Z]" "$file"; then
            echo "  ℹ️  Vulkan API calls detected in: $file"
            
            # Check for error handling patterns
            if grep -q "vk[A-Z].*(" "$file" && ! grep -q "VK_SUCCESS\|vkGetResult\|VkResult" "$file"; then
                echo "  ⚠️  Vulkan API calls without apparent error checking in: $file"
                echo "     Consider checking return values for VK_SUCCESS"
            fi
        fi
        
        # Check for memory management patterns
        vulkan_alloc_functions=("vkAllocateMemory" "vkCreateBuffer" "vkCreateImage")
        vulkan_free_functions=("vkFreeMemory" "vkDestroyBuffer" "vkDestroyImage")
        
        for alloc_func in "${vulkan_alloc_functions[@]}"; do
            if grep -q "$alloc_func" "$file"; then
                echo "  ℹ️  Memory allocation detected: $alloc_func"
                
                # Check if corresponding free function exists
                case "$alloc_func" in
                    "vkAllocateMemory")
                        if ! grep -q "vkFreeMemory" "$file"; then
                            echo "  ⚠️  Found $alloc_func but no vkFreeMemory - check for memory leaks"
                        fi
                        ;;
                    "vkCreateBuffer")
                        if ! grep -q "vkDestroyBuffer" "$file"; then
                            echo "  ⚠️  Found $alloc_func but no vkDestroyBuffer - check for resource leaks"
                        fi
                        ;;
                    "vkCreateImage")
                        if ! grep -q "vkDestroyImage" "$file"; then
                            echo "  ⚠️  Found $alloc_func but no vkDestroyImage - check for resource leaks"
                        fi
                        ;;
                esac
            fi
        done
        
        # Check for deprecated or problematic patterns
        if grep -q "vkMapMemory.*VK_WHOLE_SIZE" "$file"; then
            echo "  ⚠️  Using VK_WHOLE_SIZE with vkMapMemory - ensure proper usage"
        fi
        
        # Check for synchronization
        if grep -q "vkQueueSubmit\|vkCmdDraw\|vkCmdDispatch" "$file"; then
            if ! grep -q "vkQueueWaitIdle\|vkDeviceWaitIdle\|VkSemaphore\|VkFence" "$file"; then
                echo "  ⚠️  Command submission without apparent synchronization in: $file"
                echo "     Consider using proper synchronization (semaphores, fences, barriers)"
            fi
        fi
        
        # Check for validation layer usage (good practice)
        if grep -q "VK_LAYER_KHRONOS_validation\|VK_LAYER_LUNARG" "$file"; then
            echo "  ✅ Validation layers detected - good for debugging"
        fi
        
        # Check for extension handling
        if grep -q "vkGetInstanceExtensionProperties\|vkGetDeviceExtensionProperties" "$file"; then
            echo "  ✅ Extension enumeration detected - good practice"
        fi
        
        # Check for proper queue family handling
        if grep -q "vkGetPhysicalDeviceQueueFamilyProperties" "$file"; then
            echo "  ✅ Queue family enumeration detected - good practice"
        fi
        
        # Check for deprecated functions
        deprecated_vulkan=("vkCreateInfo" "vkAllocateCommandBuffers.*VK_COMMAND_BUFFER_LEVEL_PRIMARY.*1[^0-9]")
        for pattern in "${deprecated_vulkan[@]}"; do
            if grep -qE "$pattern" "$file"; then
                echo "  ⚠️  Potentially problematic pattern found: $pattern"
                echo "     Review for best practices"
            fi
        done
        
        # Check for compute shader patterns
        if grep -q "VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT\|vkCmdDispatch" "$file"; then
            echo "  ℹ️  Compute shader usage detected"
            
            if ! grep -q "VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT.*VK_ACCESS_SHADER" "$file"; then
                echo "  ⚠️  Compute pipeline without proper memory barriers - check synchronization"
            fi
        fi
        
        echo "  ✅ Vulkan pattern check completed for: $file"
    fi
done

if [[ $exit_code -eq 0 ]]; then
    echo "✅ Vulkan API pattern check completed."
fi

exit $exit_code 