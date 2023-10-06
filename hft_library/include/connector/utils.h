#pragma once

#include <investapiclient.h>

class Instrument {
public:
    const std::string figi;
    const int lot_size;
    const double px_step;
    Instrument(const std::string& figi, int lot_size, double px_step);
    Instrument(const Instrument& instrument) = default;
};

enum class Direction {
    Buy = 1,
    Sell = -1
};

std::ostream& operator<<(std::ostream& os, const Direction& direction);

void CheckReply(const ServiceReply& reply);

double QuotationToDouble(const Quotation& quotation);