# Static Analysis Setup

This document describes the static analysis tools and configuration used in the 3D Game of Life - Vulkan Edition project.

## Available Tools

The project uses several static analysis tools to ensure code quality:

1. **Clang-Tidy**
   - Modern C++ linter and static analyzer
   - Checks for common programming errors and style issues
   - Configuration in `.cmake/StaticAnalysis.cmake`

2. **Cppcheck**
   - Static analysis tool for C/C++ code
   - Detects various types of bugs and undefined behavior
   - Configuration in `.cmake/StaticAnalysis.cmake`

3. **Include What You Use (IWYU)**
   - Analyzes and optimizes include directives
   - Helps maintain clean and efficient header dependencies
   - Configuration in `.cmake/iwyu.imp`

4. **SonarQube**
   - Comprehensive code quality and security analysis
   - Provides detailed reports and metrics
   - Configuration in `.cmake/StaticAnalysis.cmake`

## Running Static Analysis

### Local Development

To run static analysis locally:

1. Install the required tools:
   ```bash
   # Ubuntu/Debian
   sudo apt-get install clang-tidy cppcheck include-what-you-use

   # Windows (using vcpkg)
   vcpkg install clang-tidy cppcheck include-what-you-use
   ```

2. Configure CMake with static analysis enabled:
   ```bash
   cmake -B build -S . -DENABLE_STATIC_ANALYSIS=ON
   ```

3. Run the analysis:
   ```bash
   # Run all tools
   cmake --build build --target static-analysis

   # Run individual tools
   cmake --build build --target clang-tidy
   cmake --build build --target cppcheck
   cmake --build build --target iwyu
   ```

### CI/CD Pipeline

Static analysis is automatically run in the CI/CD pipeline for:
- Every push to `main` and `develop` branches
- Every pull request targeting `main` or `develop`

The pipeline includes:
1. Build and test with multiple compilers
2. Code quality checks using all static analysis tools
3. SonarQube analysis
4. Docker image build and push
5. Release creation (for tags)

## Configuration Files

### StaticAnalysis.cmake

Main configuration file for static analysis tools. Located in `.cmake/StaticAnalysis.cmake`.

Key features:
- Tool detection and configuration
- Custom analysis targets
- Integration with CMake build system

### iwyu.imp

Include What You Use mapping file. Located in `.cmake/iwyu.imp`.

Defines:
- Header file relationships
- Public/private include rules
- Third-party library mappings

## Best Practices

1. **Regular Analysis**
   - Run static analysis before committing changes
   - Address warnings and errors promptly
   - Keep tools up to date

2. **Configuration**
   - Review and update tool configurations as needed
   - Add new checks when appropriate
   - Suppress false positives carefully

3. **Documentation**
   - Document any suppressions or special cases
   - Keep this guide up to date
   - Share knowledge with team members

## Troubleshooting

### Common Issues

1. **Missing Tools**
   - Ensure all required tools are installed
   - Check PATH environment variable
   - Verify tool versions

2. **False Positives**
   - Review suppression rules
   - Update tool configurations
   - Document necessary suppressions

3. **Performance Issues**
   - Use parallel analysis when possible
   - Exclude third-party code
   - Optimize include paths

### Getting Help

- Check tool documentation
- Review CI/CD logs
- Consult team members
- Open an issue if needed

## Additional Resources

- [Clang-Tidy Documentation](https://clang.llvm.org/extra/clang-tidy/)
- [Cppcheck Manual](http://cppcheck.sourceforge.net/manual.pdf)
- [Include What You Use](https://github.com/include-what-you-use/include-what-you-use)
- [SonarQube Documentation](https://docs.sonarqube.org/) 