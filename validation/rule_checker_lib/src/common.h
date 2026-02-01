#pragma once

#include "eval_result.h"

namespace rule_checker {

class EvalResultBuilder {
public:
    static EvalResult success();
    
    static EvalResult error(const std::string& rule_id, const std::string& path, 
                           const std::string& msg, const std::string& rule_location = "");
    
    static EvalResult info(const std::string& rule_id, const std::string& path, 
                          const std::string& msg, const std::string& rule_location = "");
    
    static void merge(EvalResult& target, const EvalResult& other);
    
    static void promote_errors_to_info(EvalResult& result);
};

} // namespace rule_checker
