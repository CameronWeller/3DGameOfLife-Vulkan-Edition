#pragma once

#include <volk.h>

namespace gol3d {

// Points binding `binding` of a descriptor set at a whole buffer.
inline void writeBufferDescriptor(VkDevice device, VkDescriptorSet set, uint32_t binding,
                                  VkDescriptorType type, VkBuffer buffer) {
    VkDescriptorBufferInfo bufferInfo{buffer, 0, VK_WHOLE_SIZE};
    VkWriteDescriptorSet write{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
    write.dstSet = set;
    write.dstBinding = binding;
    write.descriptorType = type;
    write.descriptorCount = 1;
    write.pBufferInfo = &bufferInfo;
    vkUpdateDescriptorSets(device, 1, &write, 0, nullptr);
}

} // namespace gol3d
