#include "rules/encoder_rules.h"
#include "nlohmann/json.hpp"
#include "common.h"
namespace rule_checker
{

    EvalResult eval_lambda_on_all_encoders(const nlohmann::json &data,
                                           const std::function<EvalResult(const nlohmann::json &encoder_data, const std::string &encoder_path)> &eval_lambda,
                                           const std::string &rule_id)
    {
        const std::string encoded_streams_path = "/encoded_output_streams";

        try
        {
            const nlohmann::json::json_pointer ptr(encoded_streams_path);
            if (!data.contains(ptr))
            {
                return EvalResultBuilder::error(rule_id, encoded_streams_path, "Encoded output streams not found.");
            }

            const auto &encoded_streams = data.at(ptr);
            if (!encoded_streams.is_array())
            {
                return EvalResultBuilder::error(rule_id, encoded_streams_path, "Encoded output streams is not an array.");
            }

            for (size_t i = 0; i < encoded_streams.size(); ++i)
            {
                const std::string encoder_path = encoded_streams_path + "/" + std::to_string(i) + "/encoding";
                const auto &encoder_data = encoded_streams.at(i).at("encoding");
                EvalResult res = eval_lambda(encoder_data, encoder_path);
                if (!res.passed)
                {
                    return res;
                }
            }
        }
        catch (const nlohmann::json::exception &e)
        {
            return EvalResultBuilder::error(rule_id, encoded_streams_path, "Failed to parse or access field. Details: " + std::string(e.what()));
        }

        return EvalResultBuilder::success();
    }

    std::string MonitorFramesIntraPicRateMultiplicationRule::id() const
    {
        return "monitor_frames_intra_pic_rate_multiplication";
    }

    std::string MonitorFramesIntraPicRateMultiplicationRule::message() const
    {
        return "When both monitor_frames and intra_pic_rate are specified, monitor_frames must be a multiple of intra_pic_rate.";
    }

    EvalResult MonitorFramesIntraPicRateMultiplicationRule::evaluate(const nlohmann::json &data) const
    {

        return eval_lambda_on_all_encoders(data, [this](const nlohmann::json &encoder_data, const std::string &encoder_path) -> EvalResult
                                           {
        const std::string monitor_frames_path = "/encoding/hailo_encoder/rate_control/monitor_frames";
        const std::string intra_pic_rate_path = "/encoding/hailo_encoder/rate_control/intra_pic_rate";

        int monitor_frames;
        EvalResult result = get_json_value(encoder_data, monitor_frames_path, monitor_frames, id());
        if (!result.passed) {
            EvalResultBuilder::promote_errors_to_info(result);
            return result;
        }

        int intra_pic_rate;
        result = get_json_value(encoder_data, intra_pic_rate_path, intra_pic_rate, id());
        if (!result.passed) {
            EvalResultBuilder::promote_errors_to_info(result);
            return result;
        }

        if (monitor_frames % intra_pic_rate != 0) {
            return EvalResultBuilder::error(id(), encoder_path + "/" + monitor_frames_path,
                                     message() + " monitor_frames: " + std::to_string(monitor_frames) +
                                     ", intra_pic_rate: " + std::to_string(intra_pic_rate));
        }
        return EvalResultBuilder::success(); }, id());
    }

    std::string GopLengthIntraPicRateMultiplicationRule::id() const
    {
        return "gop_length_intra_pic_rate_multiplication";
    }

    std::string GopLengthIntraPicRateMultiplicationRule::message() const
    {
        return "When both gop_length and intra_pic_rate are specified, gop_length must be a multiple of intra_pic_rate.";
    }

    EvalResult GopLengthIntraPicRateMultiplicationRule::evaluate(const nlohmann::json &data) const
    {
        return eval_lambda_on_all_encoders(data, [this](const nlohmann::json &encoder_data, const std::string &encoder_path) -> EvalResult
                                           {
        const std::string gop_length_path = "/encoding/hailo_encoder/rate_control/gop_length";
        const std::string intra_pic_rate_path = "/encoding/hailo_encoder/rate_control/intra_pic_rate";

        int gop_length;
        EvalResult result = get_json_value(encoder_data, gop_length_path, gop_length, id());
        if (!result.passed) {
            EvalResultBuilder::promote_errors_to_info(result);
            return result;
        }

        int intra_pic_rate;
        result = get_json_value(encoder_data, intra_pic_rate_path, intra_pic_rate, id());
        if (!result.passed) {
            EvalResultBuilder::promote_errors_to_info(result);
            return result;
        }

        if (gop_length % intra_pic_rate != 0) {
            return EvalResultBuilder::error(id(), encoder_path + "/" + gop_length_path,
                                     message() + " gop_length: " + std::to_string(gop_length) +
                                     ", intra_pic_rate: " + std::to_string(intra_pic_rate));
        }
        return EvalResultBuilder::success(); }, id());
    }

    std::string RateControlPqHdrConstraintRule::id() const
    {
        return "rate_control_pq_hdr_constraint";
    }

    std::string RateControlPqHdrConstraintRule::message() const
    {
        return "qp_hdr must be within the range [qp_min, qp_max]. If picture_rc is set to 1, qp_hdr may also be -1 (automatic selection).";
    }

    EvalResult RateControlPqHdrConstraintRule::evaluate(const nlohmann::json &data) const
    {
        return eval_lambda_on_all_encoders(data, [this](const nlohmann::json &encoder_data, const std::string &encoder_path) -> EvalResult
                                           {
            const std::string picture_rc_path = "/encoding/hailo_encoder/rate_control/picture_rc";
            const std::string qp_hdr_path = "/encoding/hailo_encoder/rate_control/quantization/qp_hdr";
            const std::string qp_min_path = "/encoding/hailo_encoder/rate_control/quantization/qp_min";
            const std::string qp_max_path = "/encoding/hailo_encoder/rate_control/quantization/qp_max";

            int picture_rc;
            EvalResult result = get_json_value(encoder_data, picture_rc_path, picture_rc, id());
            if (!result.passed)
            {
                EvalResultBuilder::promote_errors_to_info(result);
                return result;
            }

            int qp_hdr;
            result = get_json_value(encoder_data, qp_hdr_path, qp_hdr, id());
            if (!result.passed)
            {
                EvalResultBuilder::promote_errors_to_info(result);
                return result;
            }

            int qp_min, qp_max;
            result = get_json_value(encoder_data, qp_min_path, qp_min, id());
            if (!result.passed)
            {
                EvalResultBuilder::promote_errors_to_info(result);
                return result;
            }

            result = get_json_value(encoder_data, qp_max_path, qp_max, id());
            if (!result.passed)
            {
                EvalResultBuilder::promote_errors_to_info(result);
                return result;
            }

            if (picture_rc == 0)
            {
                if (qp_hdr < qp_min || qp_hdr > qp_max)
                {
                    return EvalResultBuilder::error(id(), encoder_path + qp_hdr_path.substr(encoder_path.length()),
                                                    message() + " (picture_rc: " + std::to_string(picture_rc) +
                                                        ", qp_hdr: " + std::to_string(qp_hdr) +
                                                        ", qp_min: " + std::to_string(qp_min) +
                                                        ", qp_max: " + std::to_string(qp_max) + ")");
                }
            }
            else if (qp_hdr != -1 && (qp_hdr < qp_min || qp_hdr > qp_max))
            {
                return EvalResultBuilder::error(id(), encoder_path + qp_hdr_path.substr(encoder_path.length()),
                                                message() + " (picture_rc: " + std::to_string(picture_rc) +
                                                    ", qp_hdr: " + std::to_string(qp_hdr) +
                                                    ", qp_min: " + std::to_string(qp_min) +
                                                    ", qp_max: " + std::to_string(qp_max) + ")");
            }

        return EvalResultBuilder::success(); }, id());
    }

    std::string ProfileLevelConstraintRule::id() const
    {
        return "profile_level_constraint";
    }

    std::string ProfileLevelConstraintRule::message() const
    {
        return "Profile and level must be either both set or both unset";
    }

    EvalResult ProfileLevelConstraintRule::evaluate(const nlohmann::json &data) const
    {
        return eval_lambda_on_all_encoders(data, [this](const nlohmann::json &encoder_data, const std::string &encoder_path) -> EvalResult
                                           {
        const std::string profile_path = "/encoding/hailo_encoder/config/output_stream/profile";
        const std::string level_path = "/encoding/hailo_encoder/config/output_stream/level";

        const nlohmann::json::json_pointer profile_ptr(profile_path);
        const nlohmann::json::json_pointer level_ptr(level_path);

        bool has_profile = encoder_data.contains(profile_ptr);
        bool has_level = encoder_data.contains(level_ptr);

        if (has_profile != has_level) {
            return EvalResultBuilder::error(id(), encoder_path + profile_path,
                                     message() + " (profile set: " + std::string(has_profile ? "true" : "false") +
                                     ", level set: " + std::string(has_level ? "true" : "false") + ")");
        }

        return EvalResultBuilder::success(); }, id());
    }

    std::string PictureRcConstraintRule::id() const
    {
        return "picture_rc_constraint";
    }

    std::string PictureRcConstraintRule::message() const
    {
        return "picture_rc must be 1 for CBR/CVBR rate control modes";
    }

    EvalResult PictureRcConstraintRule::evaluate(const nlohmann::json &data) const
    {
        return eval_lambda_on_all_encoders(data, [this](const nlohmann::json &encoder_data, const std::string &encoder_path) -> EvalResult
                                           {
        const std::string rate_control_path = "/encoding/hailo_encoder/rate_control/rc_mode";
        const std::string picture_rc_path = "/encoding/hailo_encoder/rate_control/picture_rc";

        std::string rate_control;
        EvalResult result = get_json_value(encoder_data, rate_control_path, rate_control, id());
        if (!result.passed) {
            EvalResultBuilder::promote_errors_to_info(result);
            return result;
        }

        // Only check if picture_rc is present
        const nlohmann::json::json_pointer picture_rc_ptr(picture_rc_path);
        if (!encoder_data.contains(picture_rc_ptr)) {
            return EvalResultBuilder::success();
        }

        int picture_rc;
        result = get_json_value(encoder_data, picture_rc_path, picture_rc, id());
        if (!result.passed) {
            EvalResultBuilder::promote_errors_to_info(result);
            return result;
        }

        if ((rate_control == "CBR" || rate_control == "CVBR") && picture_rc != 1) {
            return EvalResultBuilder::error(id(), encoder_path + picture_rc_path,
                                     message() + " For " + rate_control + ", picture_rc is: " + std::to_string(picture_rc));
        }

        return EvalResultBuilder::success(); }, id());
    }

    std::string TargetBitrateWhenPictureRcRule::id() const
    {
        return "target_bitrate_when_picture_rc";
    }

    std::string TargetBitrateWhenPictureRcRule::message() const
    {
        return "target_bitrate must be set when picture_rc is 1 or when rc_mode is CBR/CVBR";
    }

    EvalResult TargetBitrateWhenPictureRcRule::evaluate(const nlohmann::json &data) const
    {
        return eval_lambda_on_all_encoders(data, [this](const nlohmann::json &encoder_data, const std::string &encoder_path) -> EvalResult
                                           {
        const std::string picture_rc_path = "/encoding/hailo_encoder/rate_control/picture_rc";
        const std::string rc_mode_path = "/encoding/hailo_encoder/rate_control/rc_mode";
        const std::string target_bitrate_path = "/encoding/hailo_encoder/rate_control/bitrate/target_bitrate";

        bool require_target_bitrate = false;

        // Check rc_mode
        std::string rc_mode;
        EvalResult rc_mode_result = get_json_value(encoder_data, rc_mode_path, rc_mode, id());
        if (rc_mode_result.passed && (rc_mode == "CBR" || rc_mode == "CVBR")) {
            require_target_bitrate = true;
        }

        // Check picture_rc
        int picture_rc;
        EvalResult picture_rc_result = get_json_value(encoder_data, picture_rc_path, picture_rc, id());
        if (picture_rc_result.passed && picture_rc == 1) {
            require_target_bitrate = true;
        }

        if (require_target_bitrate) {
            const nlohmann::json::json_pointer target_bitrate_ptr(target_bitrate_path);
            if (!encoder_data.contains(target_bitrate_ptr)) {
                return EvalResultBuilder::error(id(), encoder_path + target_bitrate_path,
                                         message() + " (rc_mode: " + rc_mode +
                                         ", picture_rc: " + (picture_rc_result.passed ? std::to_string(picture_rc) : "unset") + ")");
            }
        }

        return EvalResultBuilder::success(); }, id());
    }

    std::string SmartEncoderEnablementRule::id() const
    {
        return "smart_encoder_enablement";
    }

    std::string SmartEncoderEnablementRule::message() const
    {
        return "smart_encoder can only be enabled when codec is CODEC_TYPE_H264 (AVC) and rate_control.rc_mode is CVBR.";
    }

    EvalResult SmartEncoderEnablementRule::evaluate(const nlohmann::json &data) const
    {
        return eval_lambda_on_all_encoders(data, [this](const nlohmann::json &encoder_data, const std::string &encoder_path) -> EvalResult
                                           {
        const std::string smart_encoder_enabled_path = "/encoding/hailo_encoder/smart_encoder/enabled";
        const std::string rc_mode_path = "/encoding/hailo_encoder/rate_control/rc_mode";
        const std::string codec_path = "/encoding/hailo_encoder/config/output_stream/codec";

        const nlohmann::json::json_pointer smart_encoder_ptr(smart_encoder_enabled_path);
        if (!encoder_data.contains(smart_encoder_ptr)) {
            return EvalResultBuilder::success();
        }

        bool smart_encoder_enabled;
        EvalResult result = get_json_value(encoder_data, smart_encoder_enabled_path, smart_encoder_enabled, id());
        if (!result.passed) {
            EvalResultBuilder::promote_errors_to_info(result);
            return result;
        }

        if (!smart_encoder_enabled) {
            return EvalResultBuilder::success();
        }

        EvalResult collected = EvalResultBuilder::success();

        // Check codec is AVC (H.264)
        std::string codec;
        result = get_json_value(encoder_data, codec_path, codec, id());
        if (!result.passed) {
            EvalResultBuilder::promote_errors_to_info(result);
            return result;
        }

        if (codec != "CODEC_TYPE_H264") {
            EvalResultBuilder::merge(collected,
                EvalResultBuilder::error(id(), encoder_path + smart_encoder_enabled_path,
                                     message() + " (smart_encoder.enabled: true, codec: " + codec + ")"));
        }

        // Check rate control mode is CVBR
        std::string rc_mode;
        result = get_json_value(encoder_data, rc_mode_path, rc_mode, id());
        if (!result.passed) {
            EvalResultBuilder::promote_errors_to_info(result);
            return result;
        }

        if (rc_mode != "CVBR") {
            EvalResultBuilder::merge(collected,
                EvalResultBuilder::error(id(), encoder_path + smart_encoder_enabled_path,
                                     message() + " (smart_encoder.enabled: true, rc_mode: " + rc_mode + ")"));
        }

        return collected; }, id());
    }

    std::string SmartEncoderRoiBoundsRule::id() const
    {
        return "smart_encoder_roi_bounds";
    }

    std::string SmartEncoderRoiBoundsRule::message() const
    {
        return "smart_encoder ROI coordinates must be within [0, 1] and cumulative bounds (x+width, y+height) must not exceed 1.";
    }

    EvalResult SmartEncoderRoiBoundsRule::evaluate(const nlohmann::json &data) const
    {
        return eval_lambda_on_all_encoders(data, [this](const nlohmann::json &encoder_data, const std::string &encoder_path) -> EvalResult
                                           {
        const std::string smart_encoder_rois_path = "/encoding/hailo_encoder/smart_encoder/rois";

        const nlohmann::json::json_pointer rois_ptr(smart_encoder_rois_path);
        if (!encoder_data.contains(rois_ptr)) {
            return EvalResultBuilder::success();
        }

        try {
            const auto &rois = encoder_data.at(rois_ptr);
            if (!rois.is_array()) {
                return EvalResultBuilder::success();
            }

            EvalResult collected = EvalResultBuilder::success();

            for (size_t i = 0; i < rois.size(); ++i) {
                const auto &roi = rois.at(i);
                const std::string roi_path = smart_encoder_rois_path + "/" + std::to_string(i);

                if (!roi.is_object()) {
                    EvalResultBuilder::merge(collected,
                        EvalResultBuilder::error(id(), encoder_path + roi_path,
                                                 "ROI at index " + std::to_string(i) + " is not an object."));
                    continue;
                }

                auto get_field = [&](const std::string &field, double &out) -> bool {
                    if (roi.contains(field) && roi[field].is_number()) {
                        out = roi[field].get<double>();
                        return true;
                    }
                    EvalResultBuilder::merge(collected,
                        EvalResultBuilder::error(id(), encoder_path + roi_path + "/" + field,
                                                 "ROI at index " + std::to_string(i) + " is missing required numeric field '" + field + "'."));
                    return false;
                };

                double x, y, width, height;
                bool valid = true;
                valid &= get_field("x", x);
                valid &= get_field("y", y);
                valid &= get_field("width", width);
                valid &= get_field("height", height);

                if (!valid) {
                    continue;
                }

                // Check that individual components are within [0, 1]
                if (x < 0.0 || x > 1.0 || y < 0.0 || y > 1.0 ||
                    width < 0.0 || width > 1.0 || height < 0.0 || height > 1.0) {
                    EvalResultBuilder::merge(collected,
                        EvalResultBuilder::error(id(), encoder_path + roi_path,
                                             message() + " (x: " + std::to_string(x) +
                                             ", y: " + std::to_string(y) +
                                             ", width: " + std::to_string(width) +
                                             ", height: " + std::to_string(height) +
                                             "; x+width: " + std::to_string(x + width) +
                                             ", y+height: " + std::to_string(y + height) + ")"));
                    continue;
                }

                // Check that cumulative bounds don't exceed 1
                if ((x + width) > 1.0 || (y + height) > 1.0) {
                    EvalResultBuilder::merge(collected,
                        EvalResultBuilder::error(id(), encoder_path + roi_path,
                                             message() + " (x: " + std::to_string(x) +
                                             ", y: " + std::to_string(y) +
                                             ", width: " + std::to_string(width) +
                                             ", height: " + std::to_string(height) +
                                             "; x+width: " + std::to_string(x + width) +
                                             ", y+height: " + std::to_string(y + height) + ")"));
                }
            }

            return collected;
        }
        catch (const nlohmann::json::exception &e) {
            return EvalResultBuilder::error(id(), encoder_path + smart_encoder_rois_path,
                                     "Failed to parse smart_encoder ROIs. Details: " + std::string(e.what()));
        } }, id());
    }

    std::string H264BaselineNoBFramesRule::id() const
    {
        return "h264_baseline_no_bframes";
    }

    std::string H264BaselineNoBFramesRule::message() const
    {
        return "H.264 Baseline profile does not support B-frames. Set gop_size to 1 or use a higher profile (Main/High).";
    }

    EvalResult H264BaselineNoBFramesRule::evaluate(const nlohmann::json &data) const
    {
        return eval_lambda_on_all_encoders(data, [this](const nlohmann::json &encoder_data, const std::string &encoder_path) -> EvalResult
                                           {
        const std::string profile_path = "/encoding/hailo_encoder/config/output_stream/profile";
        const std::string gop_size_path = "/encoding/hailo_encoder/gop_config/gop_size";

        // If profile is not set, nothing to check
        const nlohmann::json::json_pointer profile_ptr(profile_path);
        if (!encoder_data.contains(profile_ptr)) {
            return EvalResultBuilder::success();
        }

        std::string profile;
        EvalResult result = get_json_value(encoder_data, profile_path, profile, id());
        if (!result.passed) {
            EvalResultBuilder::promote_errors_to_info(result);
            return result;
        }

        // Only applies to H.264 Baseline profile
        if (profile != "VCENC_H264_BASE_PROFILE") {
            return EvalResultBuilder::success();
        }

        int gop_size;
        result = get_json_value(encoder_data, gop_size_path, gop_size, id());
        if (!result.passed) {
            EvalResultBuilder::promote_errors_to_info(result);
            return result;
        }

        if (gop_size > 1) {
            return EvalResultBuilder::error(id(), encoder_path + gop_size_path,
                                     message() + " (profile: " + profile +
                                     ", gop_size: " + std::to_string(gop_size) + ")");
        }

        return EvalResultBuilder::success(); }, id());
    }

    std::string ExclusiveEncoderTypeRule::id() const
    {
        return "exclusive_encoder_type";
    }

    std::string ExclusiveEncoderTypeRule::message() const
    {
        return "Each encoding stream must contain exactly one of 'hailo_encoder' or 'jpeg_encoder'.";
    }

    EvalResult ExclusiveEncoderTypeRule::evaluate(const nlohmann::json &data) const
    {
        return eval_lambda_on_all_encoders(data, [this](const nlohmann::json &encoder_data, const std::string &encoder_path) -> EvalResult
                                           {
        const nlohmann::json::json_pointer hailo_ptr("/encoding/hailo_encoder");
        const nlohmann::json::json_pointer jpeg_ptr("/encoding/jpeg_encoder");

        bool has_hailo = encoder_data.contains(hailo_ptr);
        bool has_jpeg = encoder_data.contains(jpeg_ptr);

        if (has_hailo && has_jpeg)
        {
            return EvalResultBuilder::error(id(), encoder_path,
                                     message() + " Found both 'hailo_encoder' and 'jpeg_encoder'.");
        }

        if (!has_hailo && !has_jpeg)
        {
            return EvalResultBuilder::error(id(), encoder_path,
                                     message() + " Found neither 'hailo_encoder' nor 'jpeg_encoder'.");
        }

        return EvalResultBuilder::success(); }, id());
    }

} // namespace rule_checker