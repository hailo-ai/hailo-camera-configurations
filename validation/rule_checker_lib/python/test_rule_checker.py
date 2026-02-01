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
                if config_file:
                    # Assuming the path in config_file is relative to /etc/imaging/ on the target,
                    # and we need to construct the full path in the workspace.
                    relative_path = config_file.replace('/etc/imaging/', '')
                    full_path = os.path.join(base_path, relative_path)
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
        "cfg/hailo15h/imx678/kit/4k/medialib_configs",
        "cfg/hailo15h/imx678/kit/4k/medialib_configs/case_studies",
        "cfg/hailo15l/imx678/theia_sl410m/4k/medialib_configs/",
        "cfg/hailo15l/imx678/theia_sl410m/4k/medialib_configs/case_studies",
        "cfg/hailo15l/imx678/kit/4k/medialib_configs",
        "cfg/hailo15l/imx678/kit/4k/medialib_configs/case_studies",
    ]

    medialib_config_paths += [
        "cfg/hailo15h/imx664/theia_sl410m/4mp/medialib_configs/",
        "cfg/hailo15h/imx715/kit/4k/medialib_configs/",
        "cfg/hailo15h/imx715/theia_sl410m/4k/medialib_configs/",
        "cfg/hailo15l/imx675/kit/5mp/medialib_configs/",
        "cfg/hailo15l/imx675/kit/5mp/medialib_configs/case_studies",
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
