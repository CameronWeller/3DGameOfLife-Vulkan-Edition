# WSL-specific configuration for NixOS
# This file handles WSL integration and Windows interoperability

{ config, lib, pkgs, ... }:

{
  # WSL configuration
  wsl = {
    enable = true;
    defaultUser = "vulkan-dev";
    startMenuLaunchers = true;
    
    # WSL integration settings
    interop = {
      enabled = true;
      appendWindowsPath = false;
    };
    
    # WSL networking
    networking = {
      enable = true;
      generateHosts = true;
      generateResolvConf = true;
    };
    
    # WSL automount settings
    automount = {
      enabled = true;
      mountFsTab = false;
      options = [
        "metadata"
        "uid=1000"
        "gid=1000"
        "umask=22"
        "fmask=11"
      ];
    };
  };

  # Environment variables for WSL
  environment.variables = {
    # WSL-specific environment variables
    WSL_DISTRO_NAME = "NixOS-Vulkan";
    WSL_INTEROP = "/run/WSL/1_interop";
    
    # Display settings for WSL
    DISPLAY = ":0";
    WAYLAND_DISPLAY = "wayland-0";
    
    # WSL path configuration
    PATH = lib.mkForce "/run/current-system/sw/bin:/home/vulkan-dev/.nix-profile/bin:/home/vulkan-dev/.local/bin:$PATH";
    
    # WSL home directory
    HOME = "/home/vulkan-dev";
    
    # WSL user
    USER = "vulkan-dev";
    
    # WSL shell
    SHELL = "${pkgs.zsh}/bin/zsh";
    
    # WSL terminal
    TERM = "xterm-256color";
    
    # WSL locale
    LANG = "en_US.UTF-8";
    LC_ALL = "en_US.UTF-8";
    
    # WSL timezone
    TZ = "UTC";
    
    # WSL editor
    EDITOR = "${pkgs.vim}/bin/vim";
    VISUAL = "${pkgs.vim}/bin/vim";
    
    # WSL pager
    PAGER = "${pkgs.less}/bin/less";
    
    # WSL man pages
    MANPATH = "${pkgs.man-db}/share/man";
    
    # WSL info pages
    INFOPATH = "${pkgs.texinfo}/share/info";
    
    # WSL XDG directories
    XDG_DATA_HOME = "/home/vulkan-dev/.local/share";
    XDG_CONFIG_HOME = "/home/vulkan-dev/.config";
    XDG_CACHE_HOME = "/home/vulkan-dev/.cache";
    XDG_RUNTIME_DIR = "/run/user/1000";
    
    # WSL development environment
    CC = "${pkgs.gcc}/bin/gcc";
    CXX = "${pkgs.gcc}/bin/g++";
    AR = "${pkgs.binutils}/bin/ar";
    LD = "${pkgs.binutils}/bin/ld";
    NM = "${pkgs.binutils}/bin/nm";
    OBJCOPY = "${pkgs.binutils}/bin/objcopy";
    OBJDUMP = "${pkgs.binutils}/bin/objdump";
    RANLIB = "${pkgs.binutils}/bin/ranlib";
    READELF = "${pkgs.binutils}/bin/readelf";
    STRIP = "${pkgs.binutils}/bin/strip";
    
    # WSL CMake configuration
    CMAKE_BUILD_TYPE = "Debug";
    CMAKE_GENERATOR = "Ninja";
    CMAKE_EXPORT_COMPILE_COMMANDS = "ON";
    
    # WSL Vulkan configuration
    VULKAN_SDK = "${pkgs.vulkan-headers}";
    VK_LAYER_PATH = "${pkgs.vulkan-validation-layers}/share/vulkan/explicit_layer.d";
    VK_ICD_FILENAMES = "${pkgs.mesa.drivers}/share/vulkan/icd.d/intel_icd.x86_64.json:${pkgs.mesa.drivers}/share/vulkan/icd.d/radeon_icd.x86_64.json";
    
    # WSL OpenGL configuration
    LIBGL_DRIVERS_PATH = "${pkgs.mesa.drivers}/lib/dri";
    MESA_GL_VERSION_OVERRIDE = "4.5";
    MESA_GLSL_VERSION_OVERRIDE = "450";
    
    # WSL audio configuration
    ALSA_CONFIG_DIR = "${pkgs.alsa-lib}/share/alsa";
    PULSE_COOKIE = "/home/vulkan-dev/.config/pulse/cookie";
    
    # WSL Python configuration
    PYTHONPATH = "${pkgs.python3}/lib/python3.11/site-packages";
    PYTHONUNBUFFERED = "1";
    
    # WSL Git configuration
    GIT_AUTHOR_NAME = "Vulkan Developer";
    GIT_AUTHOR_EMAIL = "vulkan-dev@nixos-wsl.local";
    GIT_COMMITTER_NAME = "Vulkan Developer";
    GIT_COMMITTER_EMAIL = "vulkan-dev@nixos-wsl.local";
    
    # WSL development tools
    GDB = "${pkgs.gdb}/bin/gdb";
    VALGRIND = "${pkgs.valgrind}/bin/valgrind";
    CLANG_TIDY = "${pkgs.clang-tools}/bin/clang-tidy";
    CPPCHECK = "${pkgs.cppcheck}/bin/cppcheck";
    
    # WSL documentation
    DOXYGEN = "${pkgs.doxygen}/bin/doxygen";
    GRAPHVIZ_DOT = "${pkgs.graphviz}/bin/dot";
  };

  # WSL-specific services
  services = {
    # WSL systemd services
    systemd = {
      # Enable systemd in WSL
      enable = true;
      
      # WSL systemd configuration
      user = {
        services = {
          # WSL user services
          "wsl-startup" = {
            description = "WSL Startup Service";
            wantedBy = [ "default.target" ];
            serviceConfig = {
              Type = "oneshot";
              ExecStart = "${pkgs.bash}/bin/bash -c 'echo WSL startup complete'";
              RemainAfterExit = true;
            };
          };
        };
      };
    };
    
    # WSL networking services
    networkd = {
      enable = true;
    };
    
    # WSL time synchronization
    timesyncd = {
      enable = true;
    };
    
    # WSL logind
    logind = {
      enable = true;
      lidSwitch = "ignore";
      lidSwitchDocked = "ignore";
      lidSwitchExternalPower = "ignore";
    };
    
    # WSL udev
    udev = {
      enable = true;
      packages = with pkgs; [
        # WSL udev packages
        udev
        eudev
      ];
    };
  };

  # WSL-specific programs
  programs = {
    # WSL bash configuration
    bash = {
      enableCompletion = true;
      enableLsColors = true;
      interactiveShellInit = ''
        # WSL bash initialization
        export PS1='\[\033[01;32m\]\u@\h\[\033[00m\]:\[\033[01;34m\]\w\[\033[00m\]\$ '
        
        # WSL aliases
        alias ll='ls -la'
        alias la='ls -A'
        alias l='ls -CF'
        alias ..='cd ..'
        alias ...='cd ../..'
        alias ....='cd ../../..'
        
        # WSL functions
        wsl() {
          case "$1" in
            "start")
              echo "Starting WSL services..."
              sudo systemctl start wsl-startup
              ;;
            "stop")
              echo "Stopping WSL services..."
              sudo systemctl stop wsl-startup
              ;;
            "restart")
              echo "Restarting WSL services..."
              sudo systemctl restart wsl-startup
              ;;
            "status")
              echo "WSL status:"
              systemctl status wsl-startup
              ;;
            *)
              echo "Usage: wsl {start|stop|restart|status}"
              ;;
          esac
        }
      '';
    };
    
    # WSL zsh configuration
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
          "wsl"
        ];
        customPkgs = with pkgs; [
          zsh-syntax-highlighting
          zsh-autosuggestions
          zsh-completions
        ];
      };
      interactiveShellInit = ''
        # WSL zsh initialization
        export PS1='%F{green}%n@%m%f:%F{blue}%~%f$ '
        
        # WSL aliases
        alias ll='ls -la'
        alias la='ls -A'
        alias l='ls -CF'
        alias ..='cd ..'
        alias ...='cd ../..'
        alias ....='cd ../../..'
        
        # WSL functions
        wsl() {
          case "$1" in
            "start")
              echo "Starting WSL services..."
              sudo systemctl start wsl-startup
              ;;
            "stop")
              echo "Stopping WSL services..."
              sudo systemctl stop wsl-startup
              ;;
            "restart")
              echo "Restarting WSL services..."
              sudo systemctl restart wsl-startup
              ;;
            "status")
              echo "WSL status:"
              systemctl status wsl-startup
              ;;
            *)
              echo "Usage: wsl {start|stop|restart|status}"
              ;;
          esac
        }
      '';
    };
  };

  # WSL-specific security
  security = {
    # WSL sudo configuration
    sudo = {
      enable = true;
      wheelNeedsPassword = false;
      extraRules = [
        {
          groups = [ "wheel" ];
          commands = [
            {
              command = "ALL";
              options = [ "NOPASSWD" ];
            }
          ];
        }
      ];
    };
    
    # WSL audit configuration
    auditd = {
      enable = true;
      rules = [
        "-w /etc/passwd -p wa -k identity"
        "-w /etc/group -p wa -k identity"
        "-w /etc/shadow -p wa -k identity"
        "-w /etc/sudoers -p wa -k scope"
        "-w /var/log/auth.log -p wa -k authentication"
      ];
    };
  };

  # WSL-specific networking
  networking = {
    # WSL network configuration
    networkmanager = {
      enable = true;
      wifi.backend = "wpa_supplicant";
    };
    
    # WSL firewall (disabled in WSL)
    firewall = {
      enable = false;
    };
    
    # WSL hosts configuration
    hosts = {
      "127.0.0.1" = [ "localhost" "nixos-wsl" ];
      "::1" = [ "localhost" "nixos-wsl" ];
    };
    
    # WSL DNS configuration
    nameservers = [
      "8.8.8.8"
      "8.8.4.4"
      "1.1.1.1"
      "1.0.0.1"
    ];
  };
} 