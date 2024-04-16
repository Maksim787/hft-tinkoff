#pragma once

#include <spdlog/spdlog.h>

#include <iostream>

#include "hft_library/third_party/TinkoffInvestSDK/investapiclient.h"

class Instrument {
    constexpr static double PRECISION = 1e-20;

   public:
    const std::string figi;
    const int lot_size;
    const double px_step;

    Instrument(const std::string& figi, int lot_size, double px_step);

    Instrument(const Instrument& instrument) = default;

    // Qty functions
    [[nodiscard]] int QtyToLots(int qty) const;

    // Px functions
    [[nodiscard]] int QuotationToPx(const Quotation& quotation) const;

    [[nodiscard]] int MoneyValueToPx(const MoneyValue& money_value) const;

    [[nodiscard]] std::pair<int, int> PxToQuotation(int px) const;
};

enum class Direction {
    Buy = 1,
    Sell = -1
};

std::ostream& operator<<(std::ostream& os, Direction direction);

const std::map<int, std::string> ERROR_DEFINITION = {
    {30042, "not enough assets for a margin trade"},
    {30059, "cancel order error: %s"},
    {30079, "instrument is not available for trading"}};

template <typename Type>
Type* ParseReply(ServiceReply& reply, std::shared_ptr<spdlog::logger> logger) {
    // Parse reply and check for error messages
    const auto& status = reply.GetStatus();
    if (!status.ok()) {
        // Get error code
        std::string status_erorr_message = status.error_message();  // code
        std::string status_error_details = status.error_details();  // empty
        std::string reply_error_message = reply.GetErrorMessage();  // string description
        // Find the error
        std::string error_definition;
        try {
            auto it = ERROR_DEFINITION.find(std::stoi(status_erorr_message));
            error_definition = it != ERROR_DEFINITION.end() ? it->second : "Unknown Error";
        } catch (const std::invalid_argument& exc) {
            error_definition = "Unknown Error";
        }
        if (error_definition == "Unknown Error") {
            logger->error("status.error_message() = '{}'; status.error_details() = '{}'; reply.GetErrorMessage() = '{}'; error_definition = '{}'", status_erorr_message, status_error_details, reply_error_message, error_definition);
        } else {
            logger->warn("status.error_message() = '{}'; status.error_details() = '{}'; reply.GetErrorMessage() = '{}'; error_definition = '{}'", status_erorr_message, status_error_details, reply_error_message, error_definition);
        }
        throw reply;
    }
    auto response = std::dynamic_pointer_cast<Type>(reply.ptr());
    assert(response);
    // Return value is always non-null
    return response.get();
}

using TimeType = int64_t;

TimeType time_from_protobuf(const google::protobuf::Timestamp& timestamp);

TimeType current_time();
