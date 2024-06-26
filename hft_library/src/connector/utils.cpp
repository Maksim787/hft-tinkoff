#include "connector/utils.h"

Instrument::Instrument(const std::string& figi, int lot_size, double px_step)
    : figi(figi),
      lot_size(lot_size),
      px_step(px_step) {
    assert(lot_size > 0);
    assert(px_step > 0);
}

int DoublePxToInt(double px, double px_step, double precision, bool check_errors = true) {
    // TODO: remove this function
    int int_part = std::round(px / px_step);
    double floating_part = px - int_part * px_step;
    // TODO: grid_trading: /home/maksim/HFT/hft-tinkoff/hft_library/src/connector/utils.cpp:17: int DoublePxToInt(double, double, double, bool): Assertion `std::abs(floating_part) < precision' failed.
    if (check_errors) {
        // assert(std::abs(floating_part) < precision);
        assert(std::abs(floating_part) < 0.02); // TODO: use precision
    }
    return static_cast<int>(int_part + 0.01); // TODO: use precision
}

int Instrument::QtyToLots(int qty) const {
    assert(qty % lot_size == 0);
    assert(qty > 0);
    return qty / lot_size;
}

int Instrument::QuotationToPx(const Quotation& quotation) const {
    // TODO: optimize by introducing precomputed multiplier in constructor
    return DoublePxToInt(quotation.units() + quotation.nano() / 1e9, px_step, PRECISION);
}

int Instrument::MoneyValueToPx(const MoneyValue& money_value) const {
    // TODO: optimize
    assert(money_value.currency() == "rub" && "Only RUB positions are supported");
    return DoublePxToInt(money_value.units() + money_value.nano() / 1e9, px_step, PRECISION, false);
}

std::pair<int, int> Instrument::PxToQuotation(int px) const {
    // TODO: optimize
    assert(px > 0);

    double units;
    double nano = std::modf(px * px_step + PRECISION / 2, &units) * 1e9;

    double tmp;
    double floating_part_of_nano = std::modf(nano, &tmp);
    assert(std::abs(floating_part_of_nano) < PRECISION);

    return {static_cast<int>(units), static_cast<int>(std::round(nano))};
}

std::ostream& operator<<(std::ostream& os, Direction direction) {
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

TimeType time_from_protobuf(const google::protobuf::Timestamp& timestamp) {
    auto seconds = std::chrono::seconds(timestamp.seconds());
    auto nanoseconds = std::chrono::nanoseconds(timestamp.nanos());
    auto duration_since_epoch = std::chrono::system_clock::time_point(seconds + nanoseconds).time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epoch).count();
}

TimeType current_time() {
    auto duration_since_epoch = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::nanoseconds>(duration_since_epoch).count();
}
