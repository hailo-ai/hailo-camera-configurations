project('rule_checker', 'cpp',
  version: '1.0.0',
  default_options: [
    'cpp_std=c++17',
    'warning_level=2',
    'werror=false',
  ],
)

# Get option values
build_tests = get_option('build_tests')
build_bindings = get_option('build_bindings')

# Dependencies
nlohmann_json_dep = dependency('nlohmann_json', required: true)
gtest_dep = dependency('gtest', main: true, required: build_tests)

# Python dependencies (optional)
py_mod = import('python')
py = py_mod.find_installation('python3', required: false)
pybind11_dep = dependency('pybind11', required: false)

inc_rules = include_directories([
  'rules'
])

# Include directories
inc = include_directories([
  'include',
  'src',
])

# Source files
rule_checker_sources = [
  'src/config_flattener.cpp',
  'src/config_transformer.cpp',
  'src/eval_result.cpp',
  'src/logger.cpp',
  'src/rule_checker.cpp',
  'src/rule_engine.cpp',
  'src/rule_checker_interface.cpp',
  'src/common.cpp',
]

rules = [
  'rules/profile_rules.cpp',
  'rules/encoder_rules.cpp',
  'rules/iq_rules.cpp',
  'rules/denoise_rules.cpp',
  'rules/rules.cpp',
]

rule_checker_lib = library('rule_checker',
  rule_checker_sources + rules,
  include_directories: [inc, inc_rules],
  dependencies: [nlohmann_json_dep],
  install: true,
  install_dir: get_option('libdir'),
  version: '1.0.0',
)

# Unit tests executable
if build_tests and gtest_dep.found()
    test_sources = [
        'test_infra/unit_tests_for_results_structs.cpp',
        'test_infra/rules/test_rules.cpp',
        'test_infra/unit_tests_infra.cpp'
    ]
    test_inc = include_directories(['test_infra'])

    executable('unit_tests',
        test_sources + rule_checker_sources,
        dependencies: [gtest_dep, nlohmann_json_dep],
        include_directories: [inc, test_inc, inc_rules]
    )

    # Rules correctness validation tests
    rules_validation_sources = [
        'rules/rules_corectness_validation/profile_rules_tests.cpp',
        'rules/rules_corectness_validation/iq_rules_tests.cpp',
        'rules/rules_corectness_validation/encoder_rules_tests.cpp'
    ]
    rules_validation_inc = include_directories(['rules/rules_corectness_validation'])

    executable('rules_validation_tests',
        rules_validation_sources + rule_checker_sources + rules,
        dependencies: [gtest_dep, nlohmann_json_dep],
        include_directories: [inc, test_inc, rules_validation_inc, inc_rules]
    )
endif

# Python bindings
if build_bindings and py.found() and pybind11_dep.found()
  py.extension_module('rule_checker',
    'python/rule_checker_bindings.cpp',
    dependencies: [pybind11_dep, nlohmann_json_dep],
    link_with: rule_checker_lib,
    include_directories: inc,
    install: false,
  )
endif
