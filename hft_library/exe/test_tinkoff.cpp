#include <investapiclient.h>
#include <marketdatastreamservice.h>
#include <ordersstreamservice.h>

#include <config.h>
#include <constants.h>


void StreamCallBack(ServiceReply reply) {
    std::cout << reply.ptr()->DebugString() << std::endl;
}

int main() {
    auto config = read_config();
    std::cout << "Create client" << std::endl;
    InvestApiClient client(ENDPOINT, config["runner"]["token"].as<std::string>());
    std::cout << "Create client: success" << std::endl;

    const std::string figi = config["runner"]["figi"].as<std::string>();
    const std::string account_id = config["user"]["account_id"].as<std::string>();
    {
        auto orders_stream = std::dynamic_pointer_cast<OrdersStream>(client.service("ordersstream"));
        orders_stream->TradesStreamAsync(
                {account_id},
                [](ServiceReply reply) { StreamCallBack(reply); }
        );
    }

//    {   //get references to MarketDataStream and OrdersStream services
//        auto marketdata = std::dynamic_pointer_cast<MarketDataStream>(client.service("marketdatastream"));
//        auto orders = std::dynamic_pointer_cast<OrdersStream>(client.service("ordersstream"));
//
//        //subscribe to Bashneft (BANE) and Moscow Exchange (MOEX) prices
//        marketdata->SubscribeLastPriceAsync({figi}, StreamCallBack);
//
//        //subscribe to Bashneft (BANE) and Moscow Exchange (MOEX) shares transactions
//        marketdata->SubscribeTradesAsync({figi}, StreamCallBack);
//
//        //subscribe to your transactions
//        orders->TradesStreamAsync({""}, StreamCallBack);
//    }
    std::cout << "Reached final point" << std::endl;

    return 0;
}
