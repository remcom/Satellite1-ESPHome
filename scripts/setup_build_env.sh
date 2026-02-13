#!/bin/bash
set -e

# Configuration
PYTHON_MIN_VERSION="3.11"
PYTHON_MAX_VERSION="3.15"
VENV_DIR=".venv"

# Get script directory to find requirements.txt
# Handle both direct execution and sourcing from different directories
if [[ -n "${BASH_SOURCE[0]}" && "${BASH_SOURCE[0]}" != "${0}" ]]; then
    # Script is being sourced
    SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
else
    # Script is being executed directly or BASH_SOURCE is empty
    SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
fi

# If SCRIPT_DIR doesn't contain setup_build_env.sh, try finding project root from cwd
if [[ ! -f "$SCRIPT_DIR/setup_build_env.sh" ]]; then
    # Fallback: assume we're in the project root or use git to find it
    if [[ -f "scripts/setup_build_env.sh" ]]; then
        SCRIPT_DIR="$(pwd)/scripts"
    elif command -v git &> /dev/null && git rev-parse --show-toplevel &> /dev/null; then
        SCRIPT_DIR="$(git rev-parse --show-toplevel)/scripts"
    fi
fi

PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
REQUIREMENTS_FILE="$PROJECT_ROOT/requirements.txt"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

info() { echo -e "${GREEN}[INFO]${NC} $1"; }
warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
error() { echo -e "${RED}[ERROR]${NC} $1"; exit 1; }

# Check if script is being sourced (required for venv activation to persist)
if [[ "${BASH_SOURCE[0]}" == "${0}" ]]; then
    warn "This script should be sourced, not executed directly."
    warn "Usage: source $0"
fi

# Version comparison helper
version_gte() {
    # Returns 0 if $1 >= $2
    printf '%s\n%s' "$2" "$1" | sort -V -C
}

version_lt() {
    # Returns 0 if $1 < $2
    ! version_gte "$1" "$2"
}

# Find suitable Python (3.11 <= version < 3.14)
find_python() {
    # Try specific versions first (prefer newer)
    for cmd in "python3.14" "python3.13" "python3.12" "python3.11" "python3" "python"; do
        if command -v "$cmd" &> /dev/null; then
            version=$("$cmd" -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')" 2>/dev/null)
            if [[ -n "$version" ]] && version_gte "$version" "$PYTHON_MIN_VERSION" && version_lt "$version" "$PYTHON_MAX_VERSION"; then
                echo "$cmd"
                return 0
            fi
        fi
    done
    return 1
}

PYTHON=$(find_python) || error "Python >=$PYTHON_MIN_VERSION,<$PYTHON_MAX_VERSION not found. Please install Python 3.11, 3.12, 3.13, or 3.14"
info "Using Python: $PYTHON ($($PYTHON --version 2>&1))"

# Change to project root
cd "$PROJECT_ROOT"

# Check if requirements.txt exists
[[ -f "$REQUIREMENTS_FILE" ]] || error "requirements.txt not found at $REQUIREMENTS_FILE"

# Create or verify virtual environment
if [[ ! -d "$VENV_DIR" ]]; then
    info "Creating virtual environment at $VENV_DIR"
    "$PYTHON" -m venv "$VENV_DIR"
elif [[ ! -f "$VENV_DIR/bin/activate" ]]; then
    warn "Virtual environment appears corrupted, recreating..."
    rm -rf "$VENV_DIR"
    "$PYTHON" -m venv "$VENV_DIR"
fi

# Activate virtual environment
info "Activating virtual environment"
source "$VENV_DIR/bin/activate"

# Upgrade pip quietly
info "Upgrading pip"
pip install --upgrade pip --quiet

# Check if requirements need to be installed/updated
REQUIREMENTS_HASH=$(md5sum "$REQUIREMENTS_FILE" 2>/dev/null | cut -d' ' -f1 || md5 -q "$REQUIREMENTS_FILE" 2>/dev/null)
HASH_FILE="$VENV_DIR/.requirements_hash"

if [[ ! -f "$HASH_FILE" ]] || [[ "$(cat "$HASH_FILE" 2>/dev/null)" != "$REQUIREMENTS_HASH" ]]; then
    info "Installing Python dependencies from requirements.txt"
    pip install -r "$REQUIREMENTS_FILE"
    echo "$REQUIREMENTS_HASH" > "$HASH_FILE"
else
    info "Dependencies are up to date (skipping install)"
fi

# Verify ESPHome is available
if command -v esphome &> /dev/null; then
    info "ESPHome version: $(esphome version 2>/dev/null || echo 'unknown')"
else
    error "ESPHome not found after installation"
fi

echo ""
info "Build environment ready!"
info "Run 'esphome compile config/satellite1.yaml' to build"
