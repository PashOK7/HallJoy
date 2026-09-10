#!/usr/bin/env sh
# Preserve upstream Linux plugin flavours; place all generated files under build.
set -eu
plugin_root=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repo_root=$(CDPATH= cd -- "$plugin_root/../.." && pwd)
soup_root="$repo_root/.cache/uap/Soup"
if [ ! -d "$soup_root/soup" ]; then
    # Standalone upstream source bundles may still provide this input location.
    soup_root="$plugin_root/Soup"
fi
if [ ! -d "$soup_root/soup" ]; then
    printf "%s\n" "Pinned Soup input is missing; see the plugin build documentation." >&2
    exit 1
fi
work_parent="$repo_root/build/obj/UAP/linux"
output="$repo_root/build/bin/UAP/linux"
mkdir -p "$work_parent" "$output"
# Unique workspace avoids overwriting another build or deleting unrelated files.
work_dir=$(mktemp -d "$work_parent/run-XXXXXX")
for input in "$plugin_root"/*.cpp "$plugin_root"/*.h "$plugin_root"/*.hpp "$plugin_root"/*.sun "$plugin_root"/*.a; do
    if [ -f "$input" ]; then cp "$input" "$work_dir/"; fi
done
cp -R "$soup_root" "$work_dir/Soup"
cd "$work_dir"

sun abiv0
sun abiv1
mkdir -p "$output/universal-analog-plugin"
cp libabiv0.so "$output/universal-analog-plugin/abiv0.so"
cp libabiv1.so "$output/universal-analog-plugin/abiv1.so"

sun abiv0-pluswooting
sun abiv1-pluswooting
mkdir -p "$output/universal-analog-plugin-with-wooting-device-support"
cp libabiv0-pluswooting.so "$output/universal-analog-plugin-with-wooting-device-support/abiv0.so"
cp libabiv1-pluswooting.so "$output/universal-analog-plugin-with-wooting-device-support/abiv1.so"
printf "%s\n" "Plugin outputs: $output"
