#include <config.h>
#include <runner.h>
#include <strategy.h>


class ExampleStrategy : public Strategy {
public:
    explicit ExampleStrategy(Runner& runner) : Strategy(runner) {}

private:
    void OnConnectorsReadiness() override {
        std::cout << "Strategy: all connectors are ready" << std::endl;
    }

    void OnOrderBookUpdate() override {
        m_order_book.Print();
    }

    void OnTradesUpdate() override {
        m_trades.Print();
    }

    void OnOurTrade(const LimitOrder& order, int executed_qty) override {
        std::cout << "Execution: " << executed_qty << "\n";
        order.Print();
        std::cout << "New positions:\n";
        m_positions.Print();
    }
};

int main() {
    auto config = read_config();
    Runner::StrategyGetter strategy_getter = [](Runner& runner) {
        return std::make_shared<ExampleStrategy>(runner);
    };
    Runner runner(config, strategy_getter);
    runner.Start();

    std::cout << "Main thread Sleep" << std::endl;
    std::this_thread::sleep_until(std::chrono::time_point<std::chrono::system_clock>::max());

    std::cout << "Exit 0" << std::endl;
    return 0;
}
