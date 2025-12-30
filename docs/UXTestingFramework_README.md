# UX Testing Framework

A modular, cross-platform framework for automated user experience testing of desktop applications, with specific support for Minecraft testing.

## Features

- **Modular Design**: Separate interfaces for application launching, input simulation, screen capture, and image analysis
- **Cross-Platform Support**: Windows implementation provided, extensible to other platforms
- **Minecraft Integration**: Specialized testing scenarios for Minecraft gameplay
- **Flexible Test Scenarios**: Easy-to-create test sequences with preconditions and postconditions
- **Comprehensive Reporting**: Detailed test results with screenshots and timing information
- **Performance Testing**: Built-in support for performance and compatibility testing

## Architecture

### Core Components

1. **UXTestingFramework**: Main framework class that orchestrates testing
2. **TestScenario**: Individual test scenarios with input events and validation
3. **ApplicationLauncher**: Platform-specific application launching
4. **InputSimulator**: Platform-specific input simulation (mouse, keyboard)
5. **ScreenCapture**: Platform-specific screen capture functionality
6. **ImageAnalyzer**: Image comparison and template matching

### Platform Abstraction

The framework uses interfaces to abstract platform-specific functionality:

```cpp
class ApplicationLauncher {
    virtual bool launch(const AppConfig& config) = 0;
    virtual bool terminate(const std::string& windowTitle) = 0;
    // ...
};
```

## Building

### Prerequisites

- CMake 3.16 or higher
- C++17 compatible compiler
- Windows SDK (for Windows implementation)

### Build Instructions

```bash
mkdir build
cd build
cmake -f ../CMakeLists_UXTesting.txt ..
cmake --build .
```

### Output

- `UXTestingFramework.lib` - Static library
- `UXTestLauncher.exe` - Basic test launcher
- `MinecraftUXTester.exe` - Minecraft-specific tester

## Usage

### Basic Framework Usage

```cpp
#include "UXTestingFramework.h"

// Initialize framework
UXTestingFramework framework;
framework.initialize();

// Create test scenario
auto scenario = std::make_shared<TestScenario>("My Test");
scenario->setAppConfig(appConfig);

// Add input events
InputEvent event;
event.type = InputType::MOUSE_CLICK;
event.x = 100;
event.y = 200;
event.description = "Click button";
scenario->addInputEvent(event);

// Run test
framework.addScenario(scenario);
TestResult result = framework.runScenario("My Test");
```

### Minecraft Testing

```cpp
#include "MinecraftUXTesting.h"

// Configure Minecraft
MinecraftConfig config;
config.launcherPath = "C:\\Program Files (x86)\\Minecraft Launcher\\MinecraftLauncher.exe";
config.username = "testuser";
config.version = "latest";

// Create testing manager
MinecraftUXTestingManager manager;
manager.setMinecraftConfig(config);

// Run tests
manager.createBasicTestSuite();
auto results = manager.runBasicTests();
```

### Command Line Usage

```bash
# Basic framework test
./UXTestLauncher

# Minecraft tests
./MinecraftUXTester basic
./MinecraftUXTester performance
./MinecraftUXTester compatibility
./MinecraftUXTester accessibility
./MinecraftUXTester all
```

## Test Scenarios

### Basic Functionality Tests

- Application launch and shutdown
- Basic UI navigation
- Input validation
- Error handling

### Performance Tests

- Frame rate monitoring
- Memory usage tracking
- Load time measurement
- Stress testing

### Compatibility Tests

- Resolution testing
- Fullscreen/windowed mode
- Mod compatibility
- Version compatibility

### Accessibility Tests

- Keyboard navigation
- Color blindness support
- High contrast mode
- Screen reader compatibility

## Minecraft-Specific Features

### World Management

- Automatic world backup/restore
- Test world creation
- World state validation

### Gameplay Testing

- Movement and controls
- Inventory management
- Crafting system
- Building mechanics
- Combat system

### Performance Monitoring

- FPS tracking
- Memory leak detection
- Chunk loading performance
- Network latency testing

## Configuration

### Application Configuration

```cpp
AppConfig config;
config.executablePath = "path/to/app.exe";
config.windowTitle = "Application Window";
config.launchTimeoutMs = 30000;
config.stabilizationDelayMs = 2000;
```

### Minecraft Configuration

```cpp
MinecraftConfig config;
config.launcherPath = "path/to/launcher.exe";
config.username = "testuser";
config.version = "1.19.2";
config.worldName = "TestWorld";
config.isBedrock = false;
```

## Output and Reporting

### Test Results

- Success/failure status
- Execution time
- Error messages
- Screenshots
- Performance metrics

### Report Formats

- Console output
- Log files
- HTML reports
- JSON data export

## Legal Considerations

### Minecraft Testing

- This framework is designed for legitimate testing purposes
- Respect Minecraft's Terms of Service
- Do not use for cheating or automation in multiplayer
- Consider using test accounts for automated testing

### General Usage

- Only test applications you own or have permission to test
- Respect application terms of service
- Use responsibly and ethically

## Extending the Framework

### Adding New Platforms

1. Implement platform-specific interfaces
2. Create platform-specific launcher
3. Add platform-specific input simulation
4. Implement platform-specific screen capture

### Adding New Test Types

1. Extend TestScenario class
2. Add new input event types
3. Implement validation logic
4. Create test scenario builders

### Adding New Applications

1. Create application-specific configuration
2. Define UI element locations
3. Create application-specific test scenarios
4. Add application-specific validation

## Troubleshooting

### Common Issues

1. **Application not launching**: Check executable path and permissions
2. **Input not working**: Verify window focus and input permissions
3. **Screenshots not capturing**: Check display permissions
4. **Tests timing out**: Increase timeout values or check application responsiveness

### Debug Mode

Enable debug logging:

```cpp
framework.setLogLevel(3); // Maximum verbosity
```

## Contributing

1. Follow the modular design principles
2. Add platform-specific implementations in separate files
3. Include comprehensive error handling
4. Add unit tests for new functionality
5. Update documentation for new features

## License

This framework is provided as-is for educational and testing purposes. Please respect the terms of service of any applications you test with this framework. 