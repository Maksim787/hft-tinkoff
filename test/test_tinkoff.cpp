#include <config.h>
#include <constants.h>

#include "investapiclient.h"
#include "marketdatastreamservice.h"
#include "ordersstreamservice.h"

void marketStreamCallBack(ServiceReply reply) {
    std::cout << reply.ptr()->DebugString() << std::endl;
}

int main() {
    auto config = read_config();
    InvestApiClient client(endpoint, config["token"].as<std::string>());

    //get references to MarketDataStream and OrdersStream services
    auto marketdata = std::dynamic_pointer_cast<MarketDataStream>(client.service("marketdatastream"));
    auto orders = std::dynamic_pointer_cast<OrdersStream>(client.service("ordersstream"));

    //subscribe to British American Tobacco and Visa Inc. prices
    marketdata->SubscribeLastPriceAsync({"BBG000BWPXQ8", "BBG00844BD08"}, marketStreamCallBack);

    //subscribe to Bashneft (BANE) and Moscow Exchange (MOEX) shares transactions
    marketdata->SubscribeTradesAsync({"BBG004S68758", "BBG004730JJ5"}, marketStreamCallBack);

    //subscribe to your transactions
    orders->TradesStreamAsync({""}, marketStreamCallBack);

    return 0;
}
