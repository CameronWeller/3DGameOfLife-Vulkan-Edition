# Project Rules and Guidelines

## Code Style

### General Formatting
- Use 4 spaces for indentation (no tabs)
- Maximum line length: 100 characters
- Use Allman style braces (braces on new lines)
- Pointer alignment: Left (`Type* var` not `Type *var`)
- Namespace indentation: None
- Sort includes and using declarations

### Naming Conventions
- Classes: PascalCase
- Functions: camelCase
- Variables: camelCase
- Constants: UPPER_CASE
- Member variables: m_camelCase
- Namespaces: lowercase

### File Organization
- One class per header file
- Header files use .h extension
- Implementation files use .cpp extension
- Include guards use `#pragma once`
- Group includes in the following order:
  1. Project headers
  2. System headers
  3. Third-party headers

## Static Analysis

### Cppcheck Rules
- Enable all checks
- Suppress missingIncludeSystem warnings
- Use C++17 standard
- Check for:
  - Memory leaks
  - Buffer overflows
  - Resource leaks
  - Uninitialized variables
  - Null pointer dereferences

### Clang-Tidy Rules
- Enable modernize-* checks
- Enable performance-* checks
- Enable readability-* checks
- Enable bugprone-* checks
- Enable cppcoreguidelines-* checks

## Build Rules

### Dependencies
- Use vcpkg for dependency management
- All dependencies must be specified in vcpkg.json
- Minimum required versions:
  - Visual Studio 2022
  - CMake 3.14.5+
  - Vulkan SDK 1.3+
  - .NET Framework 4.7.2 (for EasyHook)

### Build Configuration
- Support both Debug and Release builds
- Support x64 and x86 architectures
- Enable all warnings as errors in Release builds
- Enable sanitizers in Debug builds
- Use Ninja as the default generator

## Code Quality

### Documentation
- Document all public APIs
- Use Doxygen-style comments for classes and functions
- Include examples for complex functionality
- Document all configuration options

### Testing
- Unit tests required for all new features
- Integration tests for critical paths
- Performance tests for compute-intensive operations
- Test coverage minimum: 80%

### Error Handling
- Use exceptions for error conditions
- Include error messages in exceptions
- Log all errors with appropriate severity
- Handle resource cleanup in destructors

### Performance
- Profile critical paths
- Use appropriate data structures
- Minimize memory allocations
- Use move semantics where appropriate
- Consider cache locality

## Security

### Memory Safety
- Use smart pointers instead of raw pointers
- Avoid raw memory management
- Use RAII for resource management
- Validate all input data

### Thread Safety
- Document thread safety guarantees
- Use appropriate synchronization primitives
- Avoid global state
- Use thread-local storage when appropriate

## Version Control

### Git Workflow
- Use feature branches
- Squash commits before merging
- Write meaningful commit messages
- Follow conventional commits format

### Code Review
- All changes require review
- Address all review comments
- Run all tests before submitting
- Ensure CI passes

### Change Logging
- Update IOlog.txt with all changes in real-time
- Include timestamp, file modified, and description
- Log all build errors and resolutions
- Track dependency updates and version changes
- Document configuration changes

## Continuous Integration

### Build Pipeline
- Build on all supported platforms
- Run all tests
- Run static analysis
- Generate documentation
- Create release artifacts

### Quality Gates
- All tests must pass
- No new warnings
- No decrease in test coverage
- Documentation must be up to date
- Performance must not regress 