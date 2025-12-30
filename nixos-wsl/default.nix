{ config, lib, pkgs, ... }:

{
  imports = [
    ./hardware-configuration.nix
    ./wsl-configuration.nix
  ];

  # Enable flakes
  nix.settings.experimental-features = [ "nix-command" "flakes" ];

  # System configuration
  system.stateVersion = "23.11";

  # Enable WSL
  wsl.enable = true;
  wsl.defaultUser = "vulkan-dev";

  # User configuration
  users.users.vulkan-dev = {
    isNormalUser = true;
    extraGroups = [ "wheel" "video" "audio" ];
    shell = pkgs.zsh;
    initialPassword = "vulkan123";
  };

  # Development environment
  environment.systemPackages = with pkgs; [
    # Core development tools
    git
    cmake
    ninja
    gcc
    clang
    clang-tools
    pkg-config
    meson
    
    # Vulkan development
    vulkan-loader
    vulkan-tools
    vulkan-headers
    vulkan-validation-layers
    glslang
    shaderc
    
    # Graphics libraries
    glfw
    glm
    imgui
    
    # Utilities
    spdlog
    nlohmann_json
    benchmark
    gtest
    
    # Build tools
    make
    automake
    autoconf
    libtool
    
    # Development utilities
    vim
    nano
    htop
    tree
    wget
    curl
    
    # Shell and terminal
    zsh
    oh-my-zsh
    tmux
    
    # Version control
    git-crypt
    git-lfs
    
    # Code quality
    clang-tidy
    cppcheck
    include-what-you-use
    
    # Performance tools
    perf-tools
    valgrind
    gdb
    
    # Documentation
    doxygen
    graphviz
    
    # Python for scripts
    python3
    python3Packages.pip
  ];

  # Enable services
  services = {
    # SSH for remote development
    openssh.enable = true;
    
    # Development services
    dbus.enable = true;
    
    # Audio support
    pipewire.enable = true;
    pipewire.alsa.enable = true;
    pipewire.pulse.enable = true;
  };

  # Hardware acceleration
  hardware = {
    opengl = {
      enable = true;
      driSupport = true;
      driSupport32Bit = true;
    };
    
    # Enable audio
    pulseaudio.enable = false; # Disabled in favor of pipewire
  };

  # Environment variables
  environment.variables = {
    VULKAN_SDK = "${pkgs.vulkan-headers}";
    VK_LAYER_PATH = "${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d";
    VK_ICD_FILENAMES = "${pkgs.mesa.drivers}/share/vulkan/icd.d/intel_icd.x86_64.json:${pkgs.mesa.drivers}/share/vulkan/icd.d/radeon_icd.x86_64.json";
    
    # Development environment
    CC = "gcc";
    CXX = "g++";
    CMAKE_BUILD_TYPE = "Debug";
    
    # Python
    PYTHONPATH = "${pkgs.python3}/lib/python3.11/site-packages";
  };

  # Shell configuration
  programs = {
    zsh = {
      enable = true;
      enableCompletion = true;
      enableAutosuggestions = true;
      syntaxHighlighting.enable = true;
      ohMyZsh = {
        enable = true;
        theme = "agnoster";
        plugins = [
          "git"
          "docker"
          "cmake"
          "ninja"
          "vulkan"
        ];
      };
    };
    
    # Enable bash for compatibility
    bash.enableCompletion = true;
    
    # Development tools
    tmux.enable = true;
    vim.defaultEditor = true;
  };

  # Security settings
  security = {
    # Allow wheel group to use sudo
    sudo.wheelNeedsPassword = false;
    
    # Enable audit
    auditd.enable = true;
  };

  # Networking
  networking = {
    firewall.enable = false; # WSL doesn't need firewall
    networkmanager.enable = true;
  };

  # System settings
  boot = {
    # WSL doesn't need bootloader
    loader.grub.enable = false;
    
    # Kernel parameters
    kernelParams = [
      "console=ttyS0"
      "panic=1"
      "boot.panic_on_fail"
    ];
  };

  # File systems
  fileSystems = {
    "/" = {
      device = "/dev/sda1";
      fsType = "ext4";
    };
  };

  # Swap
  swapDevices = [ ];
} 