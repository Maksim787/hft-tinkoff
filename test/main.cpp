#include <config.h>
#include <runner.h>

int main() {
    auto config = read_config();
    Runner runner(config);
    runner.Start();

    std::cout << "Main thread Sleep" << std::endl;
    std::this_thread::sleep_until(std::chrono::time_point<std::chrono::system_clock>::max());

    std::cout << "Exit 0" << std::endl;
    return 0;
}
