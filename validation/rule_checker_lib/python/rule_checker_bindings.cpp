#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "rule_checker_interface.h"

namespace py = pybind11;

PYBIND11_MODULE(rule_checker, m) {
    m.doc() = "Python bindings for rule_checker library";

    // Bind the Violation struct
    py::class_<rule_checker::Violation>(m, "Violation")
        .def(py::init<>())
        .def_readwrite("rule_id", &rule_checker::Violation::rule_id)
        .def_readwrite("path", &rule_checker::Violation::path)
        .def_readwrite("message", &rule_checker::Violation::message)
        .def_readwrite("rule_location", &rule_checker::Violation::rule_location)
        .def("to_string", &rule_checker::Violation::to_string)
        .def("__repr__", &rule_checker::Violation::to_string)
        .def("__str__", &rule_checker::Violation::to_string);

    // Bind the EvalResult struct
    py::class_<rule_checker::EvalResult>(m, "EvalResult")
        .def(py::init<>())
        .def_readwrite("passed", &rule_checker::EvalResult::passed)
        .def_readwrite("error_violations", &rule_checker::EvalResult::error_violations)
        .def_readwrite("info_violations", &rule_checker::EvalResult::info_violations)
        .def("all_violations", &rule_checker::EvalResult::all_violations,
            "Get all violations (errors + info) combined")
        .def("__repr__", [](const rule_checker::EvalResult& r) {
            return "EvalResult(passed=" + std::string(r.passed ? "True" : "False") + 
                   ", errors=" + std::to_string(r.error_violations.size()) + 
                   ", info=" + std::to_string(r.info_violations.size()) + ")";
        });

    // Bind the factory functions
    m.def("validate_rules", &rule_checker::validate_rules, 
          py::arg("json_content"),
          "Factory function to create, configure, and run a RuleChecker");
    
    m.def("validate_rules_on_profile", &rule_checker::validate_rules_on_profile,
          py::arg("profile_path"),
          py::arg("base_path") = "",
          "Factory function to validate a profile configuration with flattening");
}
