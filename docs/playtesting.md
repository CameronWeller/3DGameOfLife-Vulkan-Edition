# Play Testing Guide

## Current Setup

### Local E2E Tests
We have a basic end-to-end play test framework in `tests/e2e/playtest_beta.cpp` that validates:
- Application initialization
- Simulation controls (play/pause)
- Simulation progression
- Speed control
- Pattern loading
- Grid reset
- Performance validation

### Running Play Tests
```bash
# Build the play test
cmake --build build --target playtest_beta

# Run the play test
./build/tests/e2e/playtest_beta
```

## Online Play Testing Platforms

### Recommended Platforms

#### 1. **Lysto** (https://lysto.gg)
- **Best for**: Comprehensive player experience testing
- **Features**:
  - Real-player playtests with AI-refined insights
  - Supports PC, iOS, Android, Steam
  - Combines playtests, surveys, interviews, and gameplay data
- **Pricing**: Contact for pricing
- **Use case**: Full player experience research

#### 2. **Antidote** (https://antidote.gg)
- **Best for**: All-in-one playtesting platform
- **Features**:
  - Actionable insights from real players
  - Supports Windows, Mac, Steam, Android, iOS, browsers
  - Integrated user research tools
- **Pricing**: Contact for pricing
- **Use case**: Comprehensive playtesting and user research

#### 3. **G.Round** (https://developer.gameround.co)
- **Best for**: Public testing and focus groups
- **Features**:
  - Open Public Tests
  - Focus Group Tests
  - Global community of gamers
- **Pricing**: Contact for pricing
- **Use case**: Public beta testing and targeted focus groups

#### 4. **Pretty Good Playtesting** (https://pgpt.dev)
- **Best for**: Indie developers
- **Features**:
  - Tester recruitment management
  - Playtest creation tools
  - Automatic reports highlighting confusion points
  - Feedback analysis
- **Pricing**: Indie-friendly pricing
- **Use case**: Indie game playtesting on a budget

## Setting Up Online Play Testing

### For Steam/PC Builds

1. **Create a Steam Playtest**:
   - Upload beta build to Steam
   - Set up playtest access
   - Collect feedback through Steam's built-in tools

2. **Use Lysto or Antidote**:
   - Upload your build
   - Recruit testers
   - Collect gameplay data and feedback
   - Analyze results

### For Web-Based Testing

1. **Use G.Round**:
   - Create a public test
   - Share link with testers
   - Collect feedback through their platform

2. **Custom Solution**:
   - Host build on web server
   - Use analytics (e.g., Google Analytics)
   - Collect feedback via forms

## Automated E2E Testing

### Current Limitations
- Our current playtest is a mock/simulation
- Real E2E testing requires:
  - Headless rendering (offscreen Vulkan)
  - Input simulation (keyboard/mouse automation)
  - Screenshot comparison for visual regression
  - Performance profiling

### Future Enhancements

1. **Headless Mode**:
   ```cpp
   // Add to main_minimal.cpp
   bool headless = false; // Set via command line
   if (headless) {
       // Run without window, capture frames to files
   }
   ```

2. **Input Recording/Playback**:
   - Record user sessions
   - Playback for regression testing
   - Validate expected outcomes

3. **Visual Regression Testing**:
   - Capture screenshots at key points
   - Compare against baseline
   - Detect rendering regressions

4. **Performance Profiling**:
   - Track FPS during gameplay
   - Monitor memory usage
   - Validate performance targets

## Recommended Workflow

1. **Local E2E Tests** (Current):
   - Run `playtest_beta` before commits
   - Validates core functionality

2. **CI/CD Integration**:
   - Add playtest to GitHub Actions
   - Run on every PR

3. **Periodic Online Testing**:
   - Use Lysto/Antidote for major releases
   - Collect real player feedback
   - Iterate based on insights

4. **Public Beta** (When ready):
   - Use G.Round or Steam Playtest
   - Gather broader feedback
   - Validate with real users

## Next Steps

1. ✅ Basic E2E playtest framework (done)
2. ⏳ Add headless rendering mode
3. ⏳ Integrate with CI/CD
4. ⏳ Set up online playtesting account (Lysto/Antidote)
5. ⏳ Create automated visual regression tests

