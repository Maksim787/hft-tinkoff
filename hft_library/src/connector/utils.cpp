#include <connector/utils.h>

Instrument::Instrument(const std::string& figi, int lot_size, double px_step)
        :
        figi(figi),
        lot_size(lot_size),
        px_step(px_step) {
    assert(lot_size > 0);
    assert(px_step > 0);
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
    if (!reply.GetStatus().ok()) {
        std::cerr << "Error: " << reply.GetErrorMessage() << std::endl;
        throw reply;
    }
}

double QuotationToDouble(const Quotation& quotation) {
    return quotation.units() + quotation.nano() / 1e9;
}