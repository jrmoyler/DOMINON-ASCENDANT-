#!/usr/bin/env bash
set -euo pipefail

project_file="${1:-DominionAscendant.uproject}"
editor_cmd="${UNREAL_EDITOR_CMD:-UnrealEditor-Cmd}"

"${editor_cmd}" "${project_file}" -run=DAUIAsset -unattended -nop4
"${editor_cmd}" "${project_file}" -run=DAUIAsset -ValidateOnly -unattended -nop4
