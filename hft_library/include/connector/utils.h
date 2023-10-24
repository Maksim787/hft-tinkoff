#pragma once

#include <investapiclient.h>

class Instrument {
    const double PRECISION = 1e-9;
public:
    const std::string figi;
    const int lot_size;
    const double px_step;

    Instrument(const std::string& figi, int lot_size, double px_step);

    Instrument(const Instrument& instrument) = default;

    int DoublePxToInt(double px) const;

    int QtyToLots(int qty) const;
};

enum class Direction {
    Buy = 1,
    Sell = -1
};

std::ostream& operator<<(std::ostream& os, const Direction& direction);

void CheckReply(const ServiceReply& reply);

double QuotationToDouble(const Quotation& quotation);

double MoneyValueToDouble(const MoneyValue& money_value);