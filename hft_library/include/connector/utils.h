#pragma once

#include <investapiclient.h>

class InstrumentInfo {
public:
    const std::string figi;
    const int lot_size;
    const double px_step;
    InstrumentInfo(const std::string& figi, int lot_size, double px_step);
    InstrumentInfo(const InstrumentInfo& instrument) = default;
};

enum class Direction {
    Buy = 1,
    Sell = -1
};

void CheckReply(const ServiceReply& reply);

double QuotationToDouble(const Quotation& quotation);