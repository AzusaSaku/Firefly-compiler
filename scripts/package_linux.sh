#!/usr/bin/env bash
set -euo pipefail

exe=""
output_dir="artifacts"
full=0
llvm_bin=""
licenses_dir="licenses"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --exe)
      exe="$2"
      shift 2
      ;;
    --output-dir)
      output_dir="$2"
      shift 2
      ;;
    --full)
      full=1
      shift
      ;;
    --llvm-bin)
      llvm_bin="$2"
      shift 2
      ;;
    --licenses-dir)
      licenses_dir="$2"
      shift 2
      ;;
    *)
      echo "unknown option: $1" >&2
      exit 1
      ;;
  esac
done

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(cd "$script_dir/.." && pwd)"
cd "$repo_root"

find_firefly() {
  if [[ -n "$exe" ]]; then
    [[ -x "$exe" ]] || { echo "firefly executable not found: $exe" >&2; exit 1; }
    realpath "$exe"
    return
  fi

  for candidate in \
    "build/firefly" \
    "build/Release/firefly" \
    "cmake-build-llvm/firefly" \
    "cmake-build-debug/firefly"; do
    if [[ -x "$candidate" ]]; then
      realpath "$candidate"
      return
    fi
  done

  echo "firefly executable not found; pass --exe path/to/firefly" >&2
  exit 1
}

copy_required_file() {
  local source="$1"
  local destination="$2"
  [[ -f "$source" ]] || { echo "required file not found: $source" >&2; exit 1; }
  cp "$source" "$destination"
}

copy_required_dir() {
  local source="$1"
  local destination="$2"
  [[ -d "$source" ]] || { echo "required directory not found: $source" >&2; exit 1; }
  cp -R "$source" "$destination"
}

copy_public_docs() {
  local destination="$1"
  [[ -d "docs" ]] || { echo "required directory not found: docs" >&2; exit 1; }
  mkdir -p "$destination"
  find docs -maxdepth 1 -type f ! -name 'firefly-0.1.0-development-brief.md' -exec cp {} "$destination" \;
}

copy_license_notices() {
  local destination="$1"
  local source_dir="$2"
  local tools="$3"

  if [[ -d "$source_dir" ]]; then
    copy_required_dir "$source_dir" "$destination"
    return
  fi

  mkdir -p "$destination"
  copy_required_file "LICENSE" "$destination/Firefly-MIT.txt"

  local llvm_prefix=""
  if command -v llvm-config >/dev/null 2>&1; then
    llvm_prefix="$(llvm-config --prefix)"
  else
    llvm_prefix="$(cd "$tools/../.." && pwd)"
  fi

  if [[ -f "$llvm_prefix/share/llvm/copyright" ]]; then
    cp "$llvm_prefix/share/llvm/copyright" "$destination/LLVM-Apache-2.0-LLVM-exception.txt"
  elif [[ -f "/usr/share/doc/llvm/copyright" ]]; then
    cp "/usr/share/doc/llvm/copyright" "$destination/LLVM-Apache-2.0-LLVM-exception.txt"
  else
    echo "LLVM notice not found; pass --licenses-dir for a full package" >&2
    exit 1
  fi

  if [[ -f "/usr/share/doc/zlib1g-dev/copyright" ]]; then
    cp "/usr/share/doc/zlib1g-dev/copyright" "$destination/zlib.txt"
  elif [[ -f "/usr/share/doc/zlib1g/copyright" ]]; then
    cp "/usr/share/doc/zlib1g/copyright" "$destination/zlib.txt"
  else
    echo "zlib notice not found; pass --licenses-dir for a full package" >&2
    exit 1
  fi

  if [[ -f "/usr/share/doc/libzstd-dev/copyright" ]]; then
    cp "/usr/share/doc/libzstd-dev/copyright" "$destination/zstd.txt"
  elif [[ -f "/usr/share/doc/libzstd1/copyright" ]]; then
    cp "/usr/share/doc/libzstd1/copyright" "$destination/zstd.txt"
  else
    echo "zstd notice not found; pass --licenses-dir for a full package" >&2
    exit 1
  fi
}

resolve_llvm_bin() {
  if [[ -n "$llvm_bin" ]]; then
    [[ -d "$llvm_bin" ]] || { echo "LLVM tools directory not found: $llvm_bin" >&2; exit 1; }
    realpath "$llvm_bin"
    return
  fi

  if command -v llvm-config >/dev/null 2>&1; then
    dirname "$(llvm-config --bindir)/llc"
    return
  fi

  echo "LLVM tools directory not found; pass --llvm-bin for a full package" >&2
  exit 1
}

firefly_exe="$(find_firefly)"
version_output="$("$firefly_exe" --version)"
if [[ ! "$version_output" =~ ^firefly[[:space:]]+(.+)$ ]]; then
  echo "could not read Firefly version from $firefly_exe" >&2
  exit 1
fi
version="${BASH_REMATCH[1]}"
kind="lightweight"
if [[ "$full" -eq 1 ]]; then
  kind="full"
fi
package_name="firefly-$version-linux-x64-$kind"

mkdir -p "$output_dir"
output_abs="$(realpath "$output_dir")"
package_root="$output_abs/$package_name"
if [[ -e "$package_root" ]]; then
  case "$(realpath "$package_root")" in
    "$output_abs"/*) rm -rf "$package_root" ;;
    *) echo "refusing to remove package path outside output directory: $package_root" >&2; exit 1 ;;
  esac
fi

mkdir -p "$package_root/bin"
copy_required_file "$firefly_exe" "$package_root/bin/firefly"
copy_required_file "README.md" "$package_root"
copy_required_file "LICENSE" "$package_root"
copy_required_dir "examples" "$package_root/examples"
copy_public_docs "$package_root/docs"

if [[ "$full" -eq 1 ]]; then
  tools="$(resolve_llvm_bin)"
  copy_required_file "$tools/llc" "$package_root/bin/llc"
  if [[ -x "$tools/clang" ]]; then
    copy_required_file "$tools/clang" "$package_root/bin/clang"
  elif command -v clang >/dev/null 2>&1; then
    copy_required_file "$(command -v clang)" "$package_root/bin/clang"
  else
    echo "required file not found: clang" >&2
    exit 1
  fi
  copy_license_notices "$package_root/licenses" "$licenses_dir" "$tools"
fi

archive_path="$output_abs/$package_name.tar.gz"
rm -f "$archive_path"
tar -C "$output_abs" -czf "$archive_path" "$package_name"
rm -rf "$package_root"

echo "wrote $archive_path"
