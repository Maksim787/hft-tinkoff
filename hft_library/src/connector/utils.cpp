#include <connector/utils.h>

Instrument::Instrument(const std::string& figi, int lot_size, double px_step)
        :
        figi(figi),
        lot_size(lot_size),
        px_step(px_step) {
    assert(lot_size > 0);
    assert(px_step > 0);
}

int Instrument::DoublePxToInt(double px) const {
    double int_part;
    double floating_part = std::modf(px, &int_part);
    assert(std::abs(floating_part) < PRECISION);
    return static_cast<int>(int_part);
}

int Instrument::QtyToLots(int qty) const {
    assert(qty % lot_size == 0);
    return qty / lot_size;
}

std::ostream& operator<<(std::ostream& os, const Direction& direction) {
    switch (direction) {
        case Direction::Buy:
            os << "Buy";
            break;
        case Direction::Sell:
            os << "Sell";
            break;
        default:
            assert(false);
    }
    return os;
}

void CheckReply(const ServiceReply& reply) {
    // Check for error messages
    if (!reply.GetStatus().ok()) {
        std::cerr << "Error: " << reply.GetErrorMessage() << std::endl;
        throw reply;
    }
}

double QuotationToDouble(const Quotation& quotation) {
    return quotation.units() + quotation.nano() / 1e9;
}

double MoneyValueToDouble(const MoneyValue& money_value) {
    assert(money_value.currency() == "rub" && "Only RUB positions are supported");
    return money_value.units() + money_value.nano() / 1e9;
}