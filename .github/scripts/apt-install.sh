#!/usr/bin/env bash
set -uo pipefail

opts=(-o Acquire::Retries=2 -o Acquire::http::Timeout=20 -o Acquire::https::Timeout=20 -o DPkg::Lock::Timeout=120)
mirrors=/etc/apt/apt-mirrors.txt
arch=$(dpkg --print-architecture 2>/dev/null || echo unknown)

for attempt in 1 2 3; do
    if sudo timeout 300 apt-get "${opts[@]}" update && sudo DEBIAN_FRONTEND=noninteractive timeout 600 apt-get "${opts[@]}" install -y "$@"; then
        exit 0
    fi
    echo "apt failed (attempt ${attempt} of 3)"
    sudo dpkg --configure -a || true
    if [ -f "$mirrors" ] && { [ "$arch" = amd64 ] || [ "$arch" = i386 ]; } && ! grep -q "mirrors.edge.kernel.org" "$mirrors"; then
        echo "adding mirrors.edge.kernel.org to $mirrors as a fallback"
        echo "http://mirrors.edge.kernel.org/ubuntu/" | sudo tee -a "$mirrors" > /dev/null
    fi
    if [ "$attempt" -lt 3 ]; then
        sleep $((attempt * 30))
    fi
done

echo "::error::apt could not install: $*"
exit 1
