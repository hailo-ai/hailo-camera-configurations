import rule_checker
import json
import os

def get_profiles_from_medialib_configs(medialib_config_paths, base_path):
    medialib_configs_files = []
    for medialib_config_path in medialib_config_paths:
        full_medialib_path = os.path.join(base_path, medialib_config_path)
        if os.path.isfile(full_medialib_path):
            medialib_configs_files.append(full_medialib_path)
        elif os.path.isdir(full_medialib_path):
            for root, _, files in os.walk(full_medialib_path):
                for file in files:
                    medialib_configs_files.append(os.path.join(root, file))
        else:
            raise FileNotFoundError(f"Medialib path not found: {full_medialib_path}")

    profiles_map = {}
    for medialib_config_path in medialib_configs_files:
        with open(medialib_config_path, 'r') as f:
            medialib_config = json.load(f)
            profile_paths = []
            for profile in medialib_config.get('profiles', []):
                config_file = profile.get('config_file')
                if not config_file:
                    continue
                if os.path.isabs(config_file):
                    # Legacy convention: device-absolute paths under /etc/imaging/ mirror to <base_path>/cfg/.
                    relative_to_etc_imaging = config_file.replace('/etc/imaging/', '', 1)
                    full_path = os.path.join(base_path, relative_to_etc_imaging)
                else:
                    # Relative refs anchor against the medialib_config's own directory,
                    # matching the JsonParser per-file resolution rule in media-library.
                    full_path = os.path.normpath(
                        os.path.join(os.path.dirname(medialib_config_path), config_file))
                profile_paths.append(full_path)
            if profile_paths:
                profiles_map[medialib_config_path] = profile_paths
    return profiles_map

def test_check_profiles_with_default_rules():
    # These are the paths to the medialib configurations we want to test.
    # We will discover all profiles from these configurations and run the rule checker on them.
    medialib_config_paths = [
        "cfg/hailo15h/imx678/theia_sl410m/4k/medialib_configs/",
        "cfg/hailo15h/imx678/theia_sl410m/4k/medialib_configs/case_studies",
        "cfg/hailo15l/imx678/theia_sl410m/4k/medialib_configs/",
        "cfg/hailo15l/imx678/theia_sl410m/4k/medialib_configs/case_studies",
    ]

    medialib_config_paths += [
        "cfg/hailo15h/imx664/theia_sl410m/4mp/medialib_configs/",
        "cfg/hailo15h/imx715/theia_sl410m/4k/medialib_configs/",
        "cfg/hailo15l/imx675/theia_sl410m/5mp/medialib_configs/",
        "cfg/hailo15l/imx675/theia_sl410m/5mp/medialib_configs/case_studies",
    ]

    base_path = os.getenv("HAILO_CONFIG_BASE_PATH")
    if not base_path:
        assert False, "Please set the HAILO_CONFIG_BASE_PATH environment variable to the root of the configurations tree."
    base_path = os.path.abspath(base_path)

    rules_ids_blacklist = []
    # Get a mapping of medialib configs to their contained profiles.
    medialib_and_profile_paths = get_profiles_from_medialib_configs(medialib_config_paths, base_path)
    
    # A list to accumulate all errors found across all profiles.
    all_errors = []
    # Track total profiles tested
    total_profiles_tested = 0
    
    # Iterate over all medialib configs and their profiles.
    for medialib_config, profile_paths in medialib_and_profile_paths.items():
        print(f"Testing medialib config: {medialib_config}")
        print(f"Found {len(profile_paths)} profiles in this medialib config")
        # Iterate over all profiles in the current medialib config.
        for profile_path in profile_paths:
            total_profiles_tested += 1
            print(f"Testing profile: {profile_path}")
            
            # Use validate_rules_on_profile to flatten and check the profile
            result = rule_checker.validate_rules_on_profile(profile_path, base_path)

            assert isinstance(result, rule_checker.EvalResult)
            
            # If the check failed, we add the errors to our list.
            if not result.passed:
                error_details = {
                    "profile": profile_path,
                    "stage": "validation",
                    "violations": []
                }
                for v in result.error_violations:
                    if v.rule_id not in rules_ids_blacklist:
                        error_details["violations"].append(str(v))
                if len(error_details["violations"]) > 0:
                    all_errors.append(error_details)

            assert isinstance(result.error_violations, list)
            assert isinstance(result.info_violations, list)
            assert all(isinstance(v, rule_checker.Violation) for v in result.all_violations())

    # After checking all profiles, if we found any errors, we format them and fail the test.
    if all_errors:
        error_messages = ["Found errors in the following profiles:"]
        for error in all_errors:
            error_messages.append(f"\nProfile: {error['profile']} (Stage: {error['stage']})")
            for violation in error['violations']:
                error_messages.append(f"  - {violation}")
        summary = f"\nSummary:\n- Total profiles tested: {total_profiles_tested}\n- Total profiles with errors: {len(all_errors)}"
        print(summary)
        error_messages.append(summary)
        assert not all_errors, "\n".join(error_messages)


def test_relative_config_file_path_resolves_to_medialib_parent_dir(tmp_path):
    """A medialib_config with a relative `config_file` must resolve against the
    medialib_config's own parent directory, not against HAILO_CONFIG_BASE_PATH.
    Uses a synthetic tempdir layout so no fixture leaks into the production
    configurations_tree."""
    medialib_dir = tmp_path / "medialib_configs"
    profiles_dir = tmp_path / "profiles"
    medialib_dir.mkdir()
    profiles_dir.mkdir()

    target_profile = profiles_dir / "synthetic_profile.json"
    target_profile.write_text("{}")

    medialib_config = medialib_dir / "synthetic_medialib_config.json"
    medialib_config.write_text(json.dumps({
        "profiles": [
            {"name": "Daylight", "config_file": "../profiles/synthetic_profile.json"},
        ],
    }))

    profiles_map = get_profiles_from_medialib_configs([str(medialib_config)], str(tmp_path))

    expected = os.path.normpath(str(target_profile))
    resolved = next(iter(profiles_map.values()))
    assert resolved == [expected], (
        f"relative config_file did not resolve against the medialib_config's parent.\n"
        f"  expected: {expected}\n"
        f"  got:      {resolved}")


def test_absolute_config_file_path_keeps_legacy_mapping(tmp_path):
    """Counterpart to the relative-path test: absolute `/etc/imaging/...` config_file
    values must still be mapped to <base_path>/<rest>, preserving the legacy behaviour."""
    medialib_dir = tmp_path / "medialib_configs"
    medialib_dir.mkdir()

    medialib_config = medialib_dir / "synthetic_medialib_config.json"
    medialib_config.write_text(json.dumps({
        "profiles": [
            {"name": "Daylight",
             "config_file": "/etc/imaging/cfg/hailo15h/.../daylight_profile.json"},
        ],
    }))

    profiles_map = get_profiles_from_medialib_configs([str(medialib_config)], str(tmp_path))

    expected = os.path.join(str(tmp_path), "cfg/hailo15h/.../daylight_profile.json")
    resolved = next(iter(profiles_map.values()))
    assert resolved == [expected], (
        f"absolute config_file no longer maps to <base_path>/<rest>.\n"
        f"  expected: {expected}\n"
        f"  got:      {resolved}")
