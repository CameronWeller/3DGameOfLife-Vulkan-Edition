# Docker command for C++ Vulkan/HIP Engine

param(
    [switch]$Verbose,
    [switch]$NoBuild,
    [switch]$Push,
    [string]$Tag = "latest"
)

$ErrorActionPreference = "Stop"

# Get script directory and project root
$scriptDir = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$projectRoot = Split-Path -Parent $scriptDir

# Import common functions
. "$scriptDir\common\functions.ps1"

Write-InfoOutput "Docker build process..."

# Check if Docker is installed
if (-not (Test-Command "docker")) {
    Write-ErrorOutput "Docker is not installed. Please install Docker Desktop from: https://www.docker.com/products/docker-desktop"
    exit 1
}

# Check if Docker is running
try {
    $dockerInfo = docker info 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "Docker daemon is not running"
    }
} catch {
    Write-ErrorOutput "Docker is not running. Please start Docker Desktop."
    exit 1
}

# Build the project first (unless skipped)
if (-not $NoBuild) {
    Write-InfoOutput "Building project before creating Docker image..."
    & "$scriptDir\build.ps1" -BuildConfig Release -Verbose:$Verbose
    if ($LASTEXITCODE -ne 0) {
        Write-ErrorOutput "Project build failed. Cannot create Docker image."
        exit 1
    }
}

# Check if Dockerfile exists
$dockerFile = Join-Path $projectRoot "Dockerfile"
if (-not (Test-Path $dockerFile)) {
    Write-ErrorOutput "Dockerfile not found at: $dockerFile"
    exit 1
}

# Check if docker-compose.yml exists
$dockerComposeFile = Join-Path $projectRoot "docker-compose.yml"
$useCompose = Test-Path $dockerComposeFile

# Set image name
$imageName = "cpp-vulkan-hip-engine"
$fullImageName = "${imageName}:${Tag}"

# Build Docker image
Write-InfoOutput "Building Docker image: $fullImageName"
Push-Location $projectRoot
try {
    if ($useCompose) {
        Write-VerboseOutput "Using docker-compose..."
        $composeArgs = @("build")
        if ($Verbose) {
            $composeArgs += "--progress=plain"
        }
        
        $output = & docker-compose $composeArgs 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-ErrorOutput "Docker build failed:"
            Write-ErrorOutput $output
            exit 1
        }
    } else {
        Write-VerboseOutput "Using docker build..."
        $buildArgs = @(
            "build",
            "-t", $fullImageName,
            "-f", $dockerFile
        )
        
        if ($Verbose) {
            $buildArgs += "--progress=plain"
        }
        
        $buildArgs += "."
        
        $output = & docker $buildArgs 2>&1
        if ($LASTEXITCODE -ne 0) {
            Write-ErrorOutput "Docker build failed:"
            Write-ErrorOutput $output
            exit 1
        }
    }
} finally {
    Pop-Location
}

Write-SuccessOutput "Docker image built successfully: $fullImageName"

# Show image info
$imageInfo = docker images $imageName --format "table {{.Repository}}\t{{.Tag}}\t{{.Size}}\t{{.CreatedAt}}"
Write-InfoOutput "Image details:"
Write-Host $imageInfo

# Push to registry if requested
if ($Push) {
    Write-InfoOutput "Pushing image to registry..."
    $output = & docker push $fullImageName 2>&1
    if ($LASTEXITCODE -ne 0) {
        Write-ErrorOutput "Failed to push image:"
        Write-ErrorOutput $output
        exit 1
    }
    Write-SuccessOutput "Image pushed successfully!"
}

# Provide run instructions
Write-Host ""
Write-InfoOutput "To run the Docker container:"
Write-Host "  docker run -it --rm $fullImageName" -ForegroundColor Yellow

if ($useCompose) {
    Write-Host ""
    Write-InfoOutput "Or using docker-compose:"
    Write-Host "  docker-compose up" -ForegroundColor Yellow
} 