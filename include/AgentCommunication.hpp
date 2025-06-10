#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <chrono>

namespace UXMirror {

    /**
     * @brief IS001: Agent Communication Framework Implementation
     * Implements lockfree SPSC queue and message serialization
     */

    // Message types for agent communication
    enum class MessageType : uint32_t {
        // System messages
        AGENT_DISCOVERY = 0x1000,
        AGENT_HEARTBEAT = 0x1001,
        AGENT_SHUTDOWN = 0x1002,
        
        // Performance data
        PERFORMANCE_METRICS = 0x2000,
        GPU_UTILIZATION = 0x2001,
        MEMORY_STATS = 0x2002,
        
        // Simulation data
        SIMULATION_STATE = 0x3000,
        CELL_UPDATE = 0x3001,
        PATTERN_DATA = 0x3002,
        
        // UX data
        UX_METRICS = 0x4000,
        INTERACTION_EVENT = 0x4001,
        HEATMAP_UPDATE = 0x4002,
        
        // Control messages
        OPTIMIZATION_HINT = 0x5000,
        CONFIGURATION_UPDATE = 0x5001,
        ERROR_REPORT = 0x5002
    };

    // Agent identifiers
    enum class AgentID : uint32_t {
        SYSTEM_ARCHITECT = 1,
        SIMULATION_ENGINEER = 2,
        UX_INTELLIGENCE = 3,
        INTEGRATION_SPECIALIST = 4,
        UNKNOWN = 0xFFFFFFFF
    };

    // Message priority levels
    enum class MessagePriority : uint8_t {
        LOW = 0,
        NORMAL = 1,
        HIGH = 2,
        CRITICAL = 3
    };

    /**
     * @brief Binary message format for high-performance communication
     * Unblocks message_queue_synchronization bottleneck
     */
    struct alignas(64) Message {
        // Header (32 bytes)
        struct {
            uint64_t timestamp;
            uint64_t messageId;
            MessageType type;
            AgentID sender;
            AgentID recipient;
            MessagePriority priority;
            uint16_t payloadSize;
            uint8_t flags;
            uint8_t reserved[5]; // Padding to 32 bytes
        } header;
        
        // Payload (up to 4KB)
        static constexpr size_t MAX_PAYLOAD_SIZE = 4096 - 32;
        alignas(64) uint8_t payload[MAX_PAYLOAD_SIZE];
        
        Message() { memset(this, 0, sizeof(Message)); }
        
        // Utility methods
        bool isValid() const;
        size_t totalSize() const { return 32 + header.payloadSize; }
        void calculateChecksum();
        bool verifyChecksum() const;
    };
    // Adjust for Windows alignment differences
    static_assert(sizeof(Message) >= 4096, "Message must be at least 4KB");

    /**
     * @brief Lockfree Single Producer Single Consumer queue
     * High-performance message passing between agents
     */
    template<size_t Size>
    class LockfreeSPSCQueue {
    private:
        static constexpr size_t QUEUE_SIZE = Size;
        static constexpr size_t QUEUE_MASK = Size - 1;
        static_assert((Size & QUEUE_MASK) == 0, "Queue size must be power of 2");
        
        alignas(64) std::atomic<uint64_t> writeIndex{0};
        alignas(64) std::atomic<uint64_t> readIndex{0};
        alignas(64) Message messages[QUEUE_SIZE];
        
    public:
        bool tryEnqueue(const Message& message) {
            const uint64_t currentWrite = writeIndex.load(std::memory_order_relaxed);
            const uint64_t nextWrite = currentWrite + 1;
            
            if (nextWrite - readIndex.load(std::memory_order_acquire) >= QUEUE_SIZE) {
                return false; // Queue full
            }
            
            messages[currentWrite & QUEUE_MASK] = message;
            writeIndex.store(nextWrite, std::memory_order_release);
            return true;
        }
        
        bool tryDequeue(Message& message) {
            const uint64_t currentRead = readIndex.load(std::memory_order_relaxed);
            
            if (currentRead == writeIndex.load(std::memory_order_acquire)) {
                return false; // Queue empty
            }
            
            message = messages[currentRead & QUEUE_MASK];
            readIndex.store(currentRead + 1, std::memory_order_release);
            return true;
        }
        
        size_t size() const {
            return writeIndex.load(std::memory_order_acquire) - readIndex.load(std::memory_order_acquire);
        }
        
        bool empty() const { return size() == 0; }
        bool full() const { return size() >= QUEUE_SIZE; }
    };

    /**
     * @brief Agent discovery and registration protocol
     */
    struct AgentInfo {
        AgentID id;
        std::string name;
        std::string version;
        std::vector<MessageType> supportedMessages;
        std::chrono::steady_clock::time_point lastHeartbeat;
        bool isActive;
        
        // Performance stats
        uint64_t messagesSent;
        uint64_t messagesReceived;
        double averageProcessingTime;
        
        AgentInfo() : id(AgentID::UNKNOWN), isActive(false), 
                     messagesSent(0), messagesReceived(0), averageProcessingTime(0.0) {}
    };

    /**
     * @brief Message handler callback type
     */
    using MessageHandler = std::function<bool(const Message&)>;

    /**
     * @brief Main agent communication framework
     * Manages message routing, agent discovery, and performance monitoring
     */
    class AgentCommunicationFramework {
    public:
        static constexpr size_t QUEUE_SIZE = 1024; // Must be power of 2
        static constexpr uint32_t HEARTBEAT_INTERVAL_MS = 1000;
        static constexpr uint32_t AGENT_TIMEOUT_MS = 5000;
        
        AgentCommunicationFramework();
        ~AgentCommunicationFramework();
        
        // IS001: Core framework methods
        bool initialize();
        void cleanup();
        bool isRunning() const { return running; }
        
        // Agent management
        bool registerAgent(AgentID id, const std::string& name, const std::string& version);
        bool unregisterAgent(AgentID id);
        std::vector<AgentInfo> getActiveAgents() const;
        bool isAgentActive(AgentID id) const;
        
        // Message handling
        bool sendMessage(const Message& message);
        bool sendMessage(AgentID recipient, MessageType type, const void* payload, size_t payloadSize);
        bool broadcastMessage(MessageType type, const void* payload, size_t payloadSize);
        
        // Message registration and callbacks
        void registerMessageHandler(MessageType type, MessageHandler handler);
        void unregisterMessageHandler(MessageType type);
        
        // Priority message handling
        bool sendPriorityMessage(const Message& message);
        void processPriorityMessages();
        
        // Performance monitoring
        struct CommunicationMetrics {
            std::atomic<uint64_t> totalMessagesSent{0};
            std::atomic<uint64_t> totalMessagesReceived{0};
            std::atomic<uint64_t> messagesDropped{0};
            std::atomic<double> averageLatency{0.0};
            std::atomic<uint64_t> queueUtilization{0};
            std::atomic<uint32_t> activeConnections{0};
        };
        
        const CommunicationMetrics& getMetrics() const { return metrics; }
        void resetMetrics();
        
        // Discovery protocol
        void startDiscovery();
        void stopDiscovery();
        bool discoverAgent(AgentID id, uint32_t timeoutMs = 5000);
        
        // Message serialization utilities
        template<typename T>
        bool serializePayload(const T& data, Message& message);
        
        template<typename T>
        bool deserializePayload(const Message& message, T& data);

    private:
        // Agent registry
        mutable std::mutex agentMutex;
        std::unordered_map<AgentID, AgentInfo> agents;
        AgentID localAgentId;
        
        // Message queues (per agent)
        std::unordered_map<AgentID, std::unique_ptr<LockfreeSPSCQueue<QUEUE_SIZE>>> inboxes;
        std::unordered_map<AgentID, std::unique_ptr<LockfreeSPSCQueue<QUEUE_SIZE>>> outboxes;
        
        // Priority message handling
        std::mutex priorityMutex;
        std::vector<Message> priorityMessages;
        
        // Message handlers
        std::mutex handlerMutex;
        std::unordered_map<MessageType, MessageHandler> messageHandlers;
        
        // Threading
        std::atomic<bool> running{false};
        std::thread messageProcessorThread;
        std::thread heartbeatThread;
        std::thread discoveryThread;
        
        // Performance tracking
        CommunicationMetrics metrics;
        std::atomic<uint64_t> nextMessageId{1};
        
        // Internal processing methods
        void messageProcessorFunction();
        void heartbeatFunction();
        void discoveryFunction();
        
        void processIncomingMessages();
        void processOutgoingMessages();
        void processHeartbeats();
        
        // Message utilities
        uint64_t generateMessageId();
        uint64_t getCurrentTimestamp() const;
        bool validateMessage(const Message& message) const;
        
        // Agent lifecycle
        void sendHeartbeat();
        void checkAgentTimeouts();
        void handleAgentDisconnection(AgentID id);
        
        // Error handling
        void handleCommunicationError(const std::string& error);
        void logCommunicationStats();
    };

    /**
     * @brief RAII message builder for easy message construction
     */
    class MessageBuilder {
    public:
        MessageBuilder(AgentID sender, AgentID recipient, MessageType type);
        
        MessageBuilder& setPriority(MessagePriority priority);
        MessageBuilder& setPayload(const void* data, size_t size);
        
        template<typename T>
        MessageBuilder& setPayload(const T& data);
        
        Message build();
        
    private:
        Message message;
    };

    /**
     * @brief Agent base class for easy integration
     */
    class Agent {
    public:
        Agent(AgentID id, const std::string& name);
        virtual ~Agent();
        
        // Lifecycle
        bool start();
        void stop();
        bool isRunning() const { return running; }
        
        // Message handling (to be overridden)
        virtual bool handleMessage(const Message& message) = 0;
        virtual void onAgentConnected(AgentID id) {}
        virtual void onAgentDisconnected(AgentID id) {}
        
    protected:
        // Utility methods for derived classes
        bool sendMessage(AgentID recipient, MessageType type, const void* payload, size_t size);
        bool broadcastMessage(MessageType type, const void* payload, size_t size);
        
        template<typename T>
        bool sendTypedMessage(AgentID recipient, MessageType type, const T& data);
        
        AgentID getAgentId() const { return agentId; }
        const std::string& getName() const { return name; }
        
    private:
        AgentID agentId;
        std::string name;
        std::atomic<bool> running{false};
        std::shared_ptr<AgentCommunicationFramework> framework;
        
        // Message processing
        void messageLoop();
        std::thread messageThread;
    };

} // namespace UXMirror 