# Local Execution Guide for AI Agents

This guide outlines methods to run AI agents locally on hardware using open-source tools, as researched for the Pre-Cleaner and Project Coordinator agent.

## Method 1: Ollama with OpenAI Swarm

### Overview
Use Ollama to run open-source models locally and Swarm to create agents without relying on external APIs.

### Setup Steps
1. Install Swarm: `pip install git+https://github.com/openai/swarm.git`
2. Import dependencies: `from openai import OpenAI; from swarm import Swarm, Agent`
3. Pull a model (e.g., Llama3.2:1b): `ollama pull llama3.2:1b`
4. Create Swarm client: `client = OpenAI(base_url='http://localhost:11434/v1', api_key='ollama'); swarm = Swarm(client=client)`
5. Define an agent: `agent = Agent(name='Example', instructions='Your instructions', model='llama3.2:1b')`
6. Run the agent with messages.

### Benefits
- Runs entirely locally.
- Supports tool usage with open-source models.

## Method 2: Teeny AI Agent

### Overview
A minimalist Python implementation for building agents with tools like web search and code execution.

### Setup Steps
1. Clone the repository: `git clone https://github.com/beverm2391/teenyagent`
2. Install dependencies (e.g., anthropic, aiohttp, etc.).
3. Define tools and agent with a model like Claude.
4. Run the agent loop.

### Benefits
- Small codebase (~few hundred lines).
- Easy to extend with custom tools.

## Method 3: AgencyOS

### Overview
Golang-based environment for high-performance agents with caching and vector storage.

### Setup Steps
1. Clone: `git clone https://github.com/d0rc/agent-os`
2. Configure `config.yaml` with database and tool settings.
3. Build and run the server: `go build; ./agent-os`
4. Define agents in YAML and interact via API.

### Benefits
- Production-ready with load balancing and caching.
- Supports Monte-Carlo search and real-world grounding.

## Method 4: Agent-OS by SmartComputer-AI

### Overview
Python framework for autonomous agents with a Git-inspired data layer.

### Setup Steps
1. Clone: `git clone https://github.com/smartcomputer-ai/agent-os`
2. Install: `poetry install`
3. Run demo agent: `poetry run aos -d agents/jetpack/ push; poetry run aos -d agents/jetpack/ run`
4. Access web interface at http://127.0.0.1:5000

### Benefits
- Local-first with persistent data.
- Supports self-coding agents.

## Autonomous Restart Method

To enable autonomous restarting at user request, use a simple PowerShell script:

```powershell
# scripts/agent_restart.ps1
param([string]$agentName)
Stop-Process -Name $agentName -Force
Start-Process -FilePath 'pwsh.exe' -ArgumentList \"-Command 'poetry run aos -d agents/$agentName/ run'\"
```

Trigger via user command or signal.

## Hardware Recommendations
- CPU: Multi-core for parallel tasks.
- GPU: NVIDIA/AMD for model acceleration (use ROCm or CUDA).
- RAM: 16GB+ for larger models.
- Storage: SSD for fast data access. 