void Engine::renderPatternPreview(VkCommandBuffer cmdBuffer, const VoxelData& voxelData, 
                                const glm::mat4& view, const glm::mat4& proj) {
    // Bind the preview pipeline
    vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, previewPipeline_);
    
    // Update uniform buffers
    updatePreviewUniforms(view, proj);
    
    // Bind descriptor sets
    vkCmdBindDescriptorSets(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, previewPipelineLayout_,
                           0, 1, &previewDescriptorSet_, 0, nullptr);
    
    // Bind vertex and index buffers
    VkBuffer vertexBuffers[] = {previewVertexBuffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(cmdBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(cmdBuffer, previewIndexBuffer_, 0, VK_INDEX_TYPE_UINT32);
    
    // Draw voxels
    for (const auto& voxel : voxelData.getVoxels()) {
        glm::mat4 model = glm::translate(glm::mat4(1.0f), voxel.position);
        vkCmdPushConstants(cmdBuffer, previewPipelineLayout_, VK_SHADER_STAGE_VERTEX_BIT,
                          0, sizeof(glm::mat4), &model);
        vkCmdDrawIndexed(cmdBuffer, 36, 1, 0, 0, 0); // 36 indices for a cube
    }
}

bool Engine::saveImageToFile(VkImage image, const std::string& filename) {
    // Create staging buffer
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = 512 * 512 * 4; // RGBA format
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &stagingBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create staging buffer!");
    }
    
    // Allocate memory for staging buffer
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device_, stagingBuffer, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    if (vkAllocateMemory(device_, &allocInfo, nullptr, &stagingBufferMemory) != VK_SUCCESS) {
        vkDestroyBuffer(device_, stagingBuffer, nullptr);
        throw std::runtime_error("Failed to allocate staging buffer memory!");
    }
    
    vkBindBufferMemory(device_, stagingBuffer, stagingBufferMemory, 0);
    
    // Create command buffer for copying
    VkCommandBuffer cmdBuffer = beginSingleTimeCommands();
    
    // Transition image layout for copying
    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    // Copy image to buffer
    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {512, 512, 1};
    
    vkCmdCopyImageToBuffer(cmdBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                          stagingBuffer, 1, &region);
    
    // Transition image layout back
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    
    vkCmdPipelineBarrier(cmdBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);
    
    endSingleTimeCommands(cmdBuffer);
    
    // Map buffer memory and save image
    void* data;
    vkMapMemory(device_, stagingBufferMemory, 0, 512 * 512 * 4, 0, &data);
    
    // Save image using stb_image_write
    stbi_write_png(filename.c_str(), 512, 512, 4, data, 512 * 4);
    
    vkUnmapMemory(device_, stagingBufferMemory);
    
    // Cleanup
    vkDestroyBuffer(device_, stagingBuffer, nullptr);
    vkFreeMemory(device_, stagingBufferMemory, nullptr);
    
    return true;
}

void Engine::updatePreviewUniforms(const glm::mat4& view, const glm::mat4& proj) {
    // Update view and projection matrices
    VkBuffer viewProjBuffer;
    VkDeviceMemory viewProjBufferMemory;
    
    struct ViewProjUBO {
        glm::mat4 view;
        glm::mat4 proj;
    } ubo;
    
    ubo.view = view;
    ubo.proj = proj;
    
    // Create and fill buffer
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = sizeof(ViewProjUBO);
    bufferInfo.usage = VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    
    if (vkCreateBuffer(device_, &bufferInfo, nullptr, &viewProjBuffer) != VK_SUCCESS) {
        throw std::runtime_error("Failed to create view/proj uniform buffer!");
    }
    
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device_, viewProjBuffer, &memRequirements);
    
    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    
    if (vkAllocateMemory(device_, &allocInfo, nullptr, &viewProjBufferMemory) != VK_SUCCESS) {
        vkDestroyBuffer(device_, viewProjBuffer, nullptr);
        throw std::runtime_error("Failed to allocate view/proj uniform buffer memory!");
    }
    
    vkBindBufferMemory(device_, viewProjBuffer, viewProjBufferMemory, 0);
    
    void* data;
    vkMapMemory(device_, viewProjBufferMemory, 0, sizeof(ViewProjUBO), 0, &data);
    memcpy(data, &ubo, sizeof(ViewProjUBO));
    vkUnmapMemory(device_, viewProjBufferMemory);
    
    // Update descriptor set
    VkDescriptorBufferInfo bufferInfo2{};
    bufferInfo2.buffer = viewProjBuffer;
    bufferInfo2.offset = 0;
    bufferInfo2.range = sizeof(ViewProjUBO);
    
    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = previewDescriptorSet_;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.dstArrayElement = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pBufferInfo = &bufferInfo2;
    
    vkUpdateDescriptorSets(device_, 1, &descriptorWrite, 0, nullptr);
    
    // Cleanup
    vkDestroyBuffer(device_, viewProjBuffer, nullptr);
    vkFreeMemory(device_, viewProjBufferMemory, nullptr);
} 