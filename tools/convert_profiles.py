# Unified profile conversion script with centralized conversion map and modular transformations

import os
import json
import argparse
from pathlib import Path
import xmltodict
import re
from collections import OrderedDict
import shutil

# --- [All functions from is_basic_array to split_encoder_osd are unchanged] ---
def is_basic_array(arr):
    return all(isinstance(x, (int, float, str, bool, type(None))) for x in arr)

def custom_dump(data, filename):
    def _dump(obj, indent_level=0):
        indent = " " * indent_level
        next_indent = " " * (indent_level + 2)

        if isinstance(obj, dict):
            lines = ["{"]
            items = list(obj.items())
            for i, (k, v) in enumerate(items):
                comma = "," if i < len(items) - 1 else ""
                lines.append(f'{next_indent}"{k}": {_dump(v, indent_level + 2)}{comma}')
            lines.append(indent + "}")
            return "\n".join(lines)

        elif isinstance(obj, list):
            if is_basic_array(obj):
                return json.dumps(obj)
            else:
                lines = ["["]
                for i, item in enumerate(obj):
                    comma = "," if i < len(obj) - 1 else ""
                    lines.append(f"{next_indent}{_dump(item, indent_level + 2)}{comma}")
                lines.append(indent + "]")
                return "\n".join(lines)

        else:
            return json.dumps(obj)

    with open(filename, 'w', encoding='utf-8') as f:
        f.write(_dump(data, 0))
        f.write("\n")

def parse_size(size_str):
    match = re.match(r"\[\s*\d+\s+(\d+)\s*\]", size_str)
    return int(match.group(1)) if match else None

def convert_double(value, size):
    try:
        cleaned_value = value.strip().replace("[", "").replace("]", "")
        numbers = list(map(float, cleaned_value.split()))
        return numbers[0] if size == 1 else numbers
    except ValueError:
        return value

def clean_structure(data, fields_to_skip):
    if isinstance(data, dict):
        cleaned_data = {}
        size_info = None
        value_holder = None

        for key, value in data.items():
            if key.startswith("@"):
                if key == "@size":
                    size_info = parse_size(value)
                continue

            if key in fields_to_skip:
                continue

            cleaned_value = clean_structure(value, fields_to_skip)

            if key == "#text":
                value_holder = cleaned_value
            elif key == "cell":
                return cleaned_value if isinstance(cleaned_value, list) else [cleaned_value]
            else:
                cleaned_data[key] = cleaned_value

        if size_info is not None and value_holder is not None:
            return convert_double(value_holder, size_info)
        if "#text" in cleaned_data and len(cleaned_data) == 1:
            return cleaned_data["#text"]
        return cleaned_data

    elif isinstance(data, list):
        return [clean_structure(item, fields_to_skip) for item in data]
    return data

def verify_utf8(obj, path="root"):
    if isinstance(obj, dict):
        for k, v in obj.items():
            verify_utf8(v, f"{path}.{k}")
    elif isinstance(obj, list):
        for i, v in enumerate(obj):
            verify_utf8(v, f"{path}[{i}]")
    elif isinstance(obj, str):
        try:
            obj.encode("utf-8")
        except UnicodeEncodeError:
            print(f"⚠️ Invalid UTF-8 at {path!r}: {repr(obj)}")

def custom_json_dump(data, file_path):
    verify_utf8(data)
    formatted_json = json.dumps(data, indent=2, ensure_ascii=False)
    formatted_json = re.sub(r"\[\s+([\d.,\s-]+)\s+\]", lambda m: "[" + " ".join(m.group(1).split()) + "]", formatted_json)
    with open(file_path, "w", encoding="utf-8") as file:
        file.write(formatted_json)

def sanitize_utf8(obj):
    if isinstance(obj, dict):
        return {sanitize_utf8(k): sanitize_utf8(v) for k, v in obj.items()}
    elif isinstance(obj, list):
        return [sanitize_utf8(i) for i in obj]
    elif isinstance(obj, str):
        try:
            obj.encode('utf-8', errors='strict')
            return obj
        except UnicodeEncodeError:
            return obj.encode('utf-8', errors='replace').decode('utf-8')
    else:
        return obj

def convert_xml_to_json_dict(xml_path, fields_to_skip=None):
    if fields_to_skip is None:
        fields_to_skip = set()
    with open(xml_path, "r", encoding="utf-8") as f:
        xml_content = f.read()
    parsed = xmltodict.parse(xml_content)
    sanitized_dict = sanitize_utf8(parsed)
    return clean_structure(sanitized_dict, fields_to_skip)

def parse_sensor_entry_cfg(cfg_path, output_dir, src_root, linux_root, output_root):
    with open(cfg_path, "r") as f:
        lines = f.readlines()

    config = {}
    mode_blocks = {}
    current_mode = None
    mode_value = None

    for line in lines:
        line = line.strip()
        if not line or line.startswith("#") or line.startswith("hdr_enable") or line.startswith("sensor_i2c_bus") or line.startswith("sensor_i2c_addr"):
            continue

        mode_header = re.match(r"\[mode\.(\d+)\]", line)
        if mode_header:
            current_mode = f"mode.{mode_header.group(1)}"
            mode_blocks[current_mode] = {}
            continue

        if "=" in line:
            key, val = [x.strip() for x in line.split("=", 1)]
            val = val.strip('"')

            if current_mode:
                mode_blocks[current_mode][key] = val
            else:
                if re.match(r"^0x[0-9a-fA-F]+$", val):
                    config[key] = val
                else:
                    try:
                        config[key] = int(val, 0)
                    except ValueError:
                        config[key] = val
                if key == "mode":
                    mode_value = int(val, 0)

    result = {"sensor_configuration": config}

    if mode_value is not None:
        mode_key = f"mode.{mode_value}"
        mode_section = mode_blocks.get(mode_key)
        if mode_section and "xml" in mode_section:
            xml_path_linux = mode_section["xml"]
            relative_to_linux_root = Path(xml_path_linux).relative_to(linux_root).as_posix()
            xml_path = src_root / relative_to_linux_root

            if xml_path.exists():
                calib_json = convert_xml_to_json_dict(xml_path)
                calib_file = output_dir / "calib.json"
                custom_json_dump(calib_json, calib_file)
                
                new_calib_linux_path = (Path(linux_root) / output_dir.relative_to(output_root) / calib_file.name).as_posix()
                result["sensor_calibration_file"] = new_calib_linux_path

    return result

def convert_3a_config_to_iqsettings(json_path, flag_key=None, flag_value=None):
    def load_json_ordered(filepath):
        with open(filepath, 'r', encoding='utf-8') as f:
            return json.load(f, object_pairs_hook=OrderedDict)

    first_data = load_json_ordered(json_path)
    result = OrderedDict()

    if "root" not in first_data or not isinstance(first_data["root"], list):
        return result

    for entry in first_data["root"]:
        if "classname" not in entry:
            continue

        classname = entry["classname"]
        structured = OrderedDict((k, v) for k, v in entry.items() if k != "classname")

        if "tables" in structured and isinstance(structured["tables"], list) and flag_key:
            selected = None
            for table in structured["tables"]:
                if flag_key in table and table[flag_key] == flag_value:
                    selected = OrderedDict((k, v) for k, v in table.items() if k != flag_key)
                    break
            if selected:
                structured.update(selected)
            del structured["tables"]

        if classname not in result:
            result[classname] = structured

    return result

def build_sensor_config(profile_data, src_root, output_dir, linux_root, output_root, version):
    result = {"version": version}
    if "input_video" in profile_data:
        result["input_video"] = profile_data["input_video"]

    isp = profile_data.get("isp_config_files", {})
    cfg_path = isp.get("sensor_entry")
    if cfg_path and Path(os.path.normpath(cfg_path)).as_posix().startswith(Path(os.path.normpath(linux_root)).as_posix()):
        relative_path = Path(cfg_path).relative_to(linux_root).as_posix()
        full_path = src_root / relative_path
        
        if full_path.exists():
            sensor_data = parse_sensor_entry_cfg(full_path, output_dir, src_root, linux_root, output_root)
            result.update(sensor_data)
    return result, "sensor_config.json"

def build_application_settings(profile_data, src_root, output_dir, linux_root, output_root, version):
    application_settings = {"version": version}
    for field in profile_data:
        if profile_data[field]:
            application_settings[field] = profile_data[field]
    return application_settings, "application_settings.json"

def build_stabilizer_settings(profile_data, src_root, output_dir, linux_root, output_root, version):
    stabilizer_settings = {"version": version}
    for field in profile_data:
        if profile_data[field]:
            stabilizer_settings[field] = profile_data[field]
    return stabilizer_settings, "stabilizer_settings.json"

def build_iqsettings(profile_data, src_root, output_dir, linux_root, output_root, version):
    iq_data = {"version": version}
    if "grayscale" in profile_data:
        iq_data["grayscale"] = profile_data["grayscale"]

    for field in ["denoise", "hdr", "dewarp"]:
        if field in profile_data:
            iq_data[field] = profile_data[field]

    path = profile_data.get("isp_config_files", {}).get("3a_config_path")
    if path and Path(os.path.normpath(path)).as_posix().startswith(Path(os.path.normpath(linux_root)).as_posix()):
        relative_path = Path(path).relative_to(linux_root).as_posix()
        full_path = src_root / relative_path
        
        if full_path.exists():
            hdr_enabled = iq_data.get("hdr", {}).get("enabled", False)
            algorithms = convert_3a_config_to_iqsettings(full_path, "hdr", hdr_enabled)
            iq_data["automatic_algorithms"] = algorithms
        else:
            print(f"  - ⚠️  WARNING: 3A config path not found: {full_path}")
            
    return iq_data, "iq_settings.json"

def build_new_linux_path(output_file_path, output_root, linux_root):
    relative_path = output_file_path.relative_to(output_root)
    # Use the normalized linux_root for constructing the new path
    return (Path(os.path.normpath(linux_root)) / relative_path).as_posix()

def split_encoder_osd(fields, src_root, output_dir, linux_root, output_root, version):
    streams = fields.get("encoded_output_streams", [])
    result = []

    for stream in streams:
        stream_id = stream.get("stream_id")
        config_path = stream.get("config_path")
        if not stream_id or not config_path:
            continue
        
        relative_path = Path(config_path).relative_to(linux_root).as_posix()
        full_path = src_root / relative_path

        if not full_path.exists():
            continue

        with open(full_path) as f:
            data = json.load(f)
            
        encoder_data = {"version": version, "encoding": data.get("encoding")}
        osd_data = {"version": version, "osd": data.get("osd")}
        
        if data.get("privacy_mask"):
            masking_data = {"masking": data.get("privacy_mask")}
            mask_file = output_dir / f"masking_{stream_id}.json"
            with open(mask_file, "w") as f:
                json.dump(masking_data, f, indent=2)
            masking_path = build_new_linux_path(mask_file, output_root, linux_root)
        else:
            masking_path = "" 

        encoder_file = output_dir / f"encoder_{stream_id}.json"
        osd_file = output_dir / f"osd_{stream_id}.json"

        if encoder_data["encoding"]:
            custom_dump(encoder_data, encoder_file)
        if osd_data["osd"]:
            custom_dump(osd_data, osd_file)
                
        result.append({
            "stream_id": stream_id,
            "encoding": build_new_linux_path(encoder_file, output_root, linux_root),
            "osd": build_new_linux_path(osd_file, output_root, linux_root),
            "masking": masking_path
        })
    return result
CONVERSION_MAP = {
    "sensor_config": {"fields": ["input_video", "isp_config_files"], "transform": build_sensor_config},
    "application_settings": {"fields": ["application_input_streams", "gmv", "optical_zoom", "digital_zoom", "motion_detection", "roi", "rotation", "flip", "hailort", "application_analytics"], "transform": build_application_settings},
    "stabilizer_settings": {"fields": ["dis", "eis", "gyro"], "transform": build_stabilizer_settings},
    "iq_settings": {"fields": ["isp_config_files", "dewarp", "hdr", "denoise"], "transform": build_iqsettings},
    "encoded_output_streams": {"fields": ["encoded_output_streams"], "transform": split_encoder_osd, "multi_output": True}
}

def process_profile(profile_path, src_root, linux_root, output_root, version, conversion_path_map, medialib_name_map):
    with open(profile_path) as f:
        profile_data = json.load(f)

    grayscale_setting = None
    app_streams = profile_data.get("application_input_streams")
    if isinstance(app_streams, dict) and "grayscale" in app_streams:
        grayscale_setting = app_streams.pop("grayscale")
        if not app_streams:
            profile_data.pop("application_input_streams")

    relative_src_path = profile_path.relative_to(src_root)
    
    final_profile_name = profile_path.stem.replace("_profile", "")
    final_profile_filename = profile_path.name

    try:
        path_parts = list(relative_src_path.parts)
        profiles_idx = path_parts.index("profiles")
        
        if len(path_parts) > profiles_idx + 2:
            mode_name = path_parts[profiles_idx + 1]
            
            if mode_name.lower() == final_profile_name.lower():
                # --- ROBUSTNESS FIX ---
                # Normalize the path before looking it up in the map
                constructed_path = (Path(linux_root) / relative_src_path).as_posix()
                normalized_lookup_path = os.path.normpath(constructed_path)
                
                medialib_base_name = medialib_name_map.get(normalized_lookup_path)
                
                if medialib_base_name:
                    new_distinctive_name = f"{medialib_base_name}_{final_profile_name}"
                    print(f"  - Name conflict found for '{final_profile_name}'. Renaming to '{new_distinctive_name}'.")
                    final_profile_name = new_distinctive_name
                    final_profile_filename = f"{new_distinctive_name}_profile.json"
                else:
                    print(f"  - ⚠️  WARNING: Name conflict for '{final_profile_name}', but its normalized path '{normalized_lookup_path}' was not found in medialib map. Cannot rename.")

            base_parts = path_parts[:profiles_idx + 2] 
            profile_dir = output_root.joinpath(*base_parts, final_profile_name)

        else: 
            base_parts = path_parts[:profiles_idx + 1]
            profile_dir = output_root.joinpath(*base_parts, final_profile_name)

    except ValueError:
        print(f"⚠️  Skipping file - 'profiles' directory not in path: {profile_path}")
        return

    profile_dir.mkdir(parents=True, exist_ok=True)
    stub = {"version": version}

    for key, cfg in CONVERSION_MAP.items():
        fields = cfg.get("fields", [])
        subset = {k: profile_data.get(k) for k in fields if profile_data.get(k) is not None}
        transform = cfg["transform"]

        if key == "iq_settings" and grayscale_setting is not None:
            subset['grayscale'] = grayscale_setting
        if not subset:
            continue
        if cfg.get("multi_output"):
            outputs = transform(subset, src_root, profile_dir, linux_root, output_root, version)
            stub[key] = outputs
        else:
            result = transform(subset, src_root, profile_dir, linux_root, output_root, version)
            if result:
                data, filename = result
                output_file_path = profile_dir / filename
                custom_dump(data, output_file_path)
                stub[key] = build_new_linux_path(output_file_path, output_root, linux_root)
    
    new_profile_path = profile_dir / final_profile_filename
    with open(new_profile_path, "w") as f:
        json.dump(stub, f, indent=2)

    # Normalize the constructed old path for the conversion map key
    old_linux_path_constructed = (Path(linux_root) / relative_src_path).as_posix()
    old_linux_path_normalized = os.path.normpath(old_linux_path_constructed)

    new_linux_path = build_new_linux_path(new_profile_path, output_root, linux_root)
    
    # Store the mapping using the original, un-normalized path from the config file, as that's what process_medialib_file will look for.
    conversion_path_map[old_linux_path_constructed] = new_linux_path 
    print(f"  -> Mapped old '{old_linux_path_constructed}' to new '{new_linux_path}'")

def process_medialib_file(file_path, src_root, output_root, conversion_path_map):
    print(f"Updating medialib config: {file_path.name}")
    try:
        with open(file_path, 'r') as f:
            data = json.load(f)

        if "profiles" not in data or not isinstance(data["profiles"], list):
            print(f"  -> Skipping, no 'profiles' list found.")
            return

        for profile in data["profiles"]:
            old_path = profile.get("config_file")
            # --- ROBUSTNESS FIX ---
            # Check both the original and a normalized version of the path in the conversion map.
            # This handles cases where the path in the JSON might be slightly different from what the script constructed.
            normalized_old_path = os.path.normpath(old_path) if old_path else None
            
            if old_path in conversion_path_map:
                profile["config_file"] = conversion_path_map[old_path]
                print(f"  - Updated '{os.path.basename(old_path)}' to new location.")
            elif normalized_old_path in conversion_path_map:
                profile["config_file"] = conversion_path_map[normalized_old_path]
                print(f"  - Updated '{os.path.basename(old_path)}' (via normalized path) to new location.")
            elif old_path:
                print(f"  - ⚠️  WARNING: Path '{old_path}' not found in conversion map. It was not updated.")

        relative_path = file_path.relative_to(src_root)
        output_path = output_root / relative_path
        output_path.parent.mkdir(parents=True, exist_ok=True)
        with open(output_path, 'w') as f:
            json.dump(data, f, indent=4)
            
    except json.JSONDecodeError:
        print(f"  - ❌ ERROR: Could not parse JSON from {file_path}. Copying file as-is.")
        relative_path = file_path.relative_to(src_root)
        output_path = output_root / relative_path
        output_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(file_path, output_path)
    except Exception as e:
        print(f"  - ❌ ERROR: An unexpected error occurred while processing {file_path}: {e}")
def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--source_root", required=True)
    parser.add_argument("--linux_root", required=True)
    parser.add_argument("--output_root", default="cfg_new")
    parser.add_argument("--version", default="1.1.0")
    args = parser.parse_args()

    src_root = Path(args.source_root).resolve()
    output_root = Path(args.output_root).resolve()

    # --- ROBUSTNESS FIX ---
    # Normalize the linux_root input immediately to ensure consistency.
    linux_root_normalized = os.path.normpath(args.linux_root)
    print(f"--- Using normalized linux_root: '{linux_root_normalized}' ---")

    if output_root.exists():
        print(f"Cleaning output directory: {output_root}")
        shutil.rmtree(output_root)
    output_root.mkdir(parents=True)

    profiles_to_process, medialib_files_to_process, other_files_to_copy = [], [], []

    print("\n--- Pass 1: Discovering files ---")
    profile_source_dirs = set()
    for root, _, files in os.walk(src_root):
        if Path(root).resolve().is_relative_to(output_root): continue
        for file in files:
            if file.endswith("_profile.json"):
                profile_source_dirs.add(Path(root)); break 

    for root, dirs, files in os.walk(src_root):
        current_dir = Path(root)
        if current_dir.resolve().is_relative_to(output_root): continue
        dirs[:] = [d for d in dirs if d not in ['.git', '.vscode']]
        is_part_of_profile_structure = any(current_dir == profile_dir or profile_dir in current_dir.parents for profile_dir in profile_source_dirs)
        for file in files:
            full_path = current_dir / file
            if file.endswith("_profile.json"):
                profiles_to_process.append(full_path)
            elif "medialib_config" in file and file.endswith(".json"):
                medialib_files_to_process.append(full_path)
            elif not is_part_of_profile_structure:
                other_files_to_copy.append(full_path)

    print(f"Found {len(profiles_to_process)} profiles to convert.")
    print(f"Found {len(medialib_files_to_process)} medialib configs to update.")
    print(f"Found {len(other_files_to_copy)} other files to copy.")

    print("\n--- Pre-Pass 2: Building medialib name map ---")
    medialib_name_map = {}
    for medialib_path in medialib_files_to_process:
        try:
            medialib_base_name = medialib_path.stem.replace("_medialib_config", "")
            with open(medialib_path, 'r') as f: data = json.load(f)
            if "profiles" in data and isinstance(data["profiles"], list):
                for profile in data["profiles"]:
                    if "config_file" in profile and profile["config_file"]:
                        # --- ROBUSTNESS FIX ---
                        # Normalize the path from the JSON before using it as a key.
                        normalized_path = os.path.normpath(profile["config_file"])
                        medialib_name_map[normalized_path] = medialib_base_name
        except (json.JSONDecodeError, IOError) as e:
            print(f"  - ⚠️ Could not read or parse {medialib_path}: {e}")
    
    print(f"Built name map with {len(medialib_name_map)} entries for conflict resolution.")

    conversion_path_map = {}
    print("\n--- Pass 2: Converting profiles and building path map ---")
    for profile_path in profiles_to_process:
        print(f"Processing profile: {profile_path.name}")
        # Use the normalized linux_root throughout the script.
        process_profile(profile_path, src_root, linux_root_normalized, output_root, args.version, conversion_path_map, medialib_name_map)

    print("\n--- Pass 3: Updating medialib configuration files ---")
    for medialib_path in medialib_files_to_process:
        process_medialib_file(medialib_path, src_root, output_root, conversion_path_map)

    print("\n--- Final Step: Copying remaining files ---")
    for file_path in other_files_to_copy:
        relative_path = file_path.relative_to(src_root)
        output_path = output_root / relative_path
        output_path.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(file_path, output_path)

    print("\nConversion complete.")

if __name__ == "__main__":
    main()