#include <thread>
#include <chrono>

int heavy_function(int x)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return x;
}