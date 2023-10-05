#include <connector/utils.h>

InstrumentInfo::InstrumentInfo(const std::string& figi, int lot_size, double px_step)
        :
        figi(figi),
        lot_size(lot_size),
        px_step(px_step) {
    assert(lot_size > 0);
    assert(px_step > 0);
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