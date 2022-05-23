#include <iostream>
#include <iomanip>
#include <chrono>
#include <cmath>

#include "pool.h"

template <class F>
double ComputeIntegral(F&& function, double from, double to, double step)
{
    auto current = from;
    auto sum = 0.0;

    while (current < to)
    {
        sum += std::invoke(std::forward<F>(function), current) * step;
        current += step;
    }

    return sum;
}

template <class F>
double ComputeIntegralValueOnPool(F&& function, MySimpleThreadPool& pool, double from, double to, double step)
{
    const auto segmentLength = (to - from) / pool.workersCount;
    std::vector<std::future<double>> segmentsComputationResults;

    segmentsComputationResults.reserve(pool.workersCount);

    for (size_t i = 0; i < pool.workersCount; i++)
    {
        const auto start = from + i * segmentLength;
        const auto end = from + (i + 1) * segmentLength;

        segmentsComputationResults.push_back(
            pool.EnqueueUserTask([=] { return ComputeIntegral(std::forward<F>(function), start, end, step); })
        );
    }

    auto sum = 0.0;

    for (auto& computationResult : segmentsComputationResults)
    {
        computationResult.wait();

        sum += computationResult.get();
    }

    return sum;
}

double GetUnitCircleSquareOnH(double h)
{
    return std::atan(1) * 4 * (1 - h * h);
}

const double step = 0.000000005;

double ComputeUnitSphereVolume()
{
    const auto half = ComputeIntegral(GetUnitCircleSquareOnH, 0, 1, step);

    return half + half;
}

double ComputeUnitSphereVolumeInThreadPool(size_t threadsCount)
{
    MySimpleThreadPool pool(threadsCount);

    const auto half = ComputeIntegralValueOnPool(GetUnitCircleSquareOnH, pool, 0, 1, step);

    return half + half;
}

template <class F, class... Args>
void MeasureTime(const char* name, F&& function, Args&&... args)
{
    std::cout << "Measuring " << name << "...";
    const auto start = std::chrono::steady_clock::now();

    auto result = std::invoke(std::forward<F>(function), std::forward<Args>(args)...);

    const auto elapsed = std::chrono::steady_clock::now() - start;

    std::cout << " took " << std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() << "_ms"
        << " with result " << std::fixed << std::setprecision(10) << result << std::endl;
}

void WarmUp(int warmingUps)
{
    for (auto i = 1; i <= warmingUps; i++)
    {
        std::cout << "Warming up stage " << i << " of " << warmingUps << std::endl;

        ComputeUnitSphereVolume();

        MySimpleThreadPool pool(std::thread::hardware_concurrency());

        std::vector<std::future<double>> t;

        for (unsigned i = 0; i < std::thread::hardware_concurrency(); i++)
        {
            t.push_back(pool.EnqueueUserTask(ComputeUnitSphereVolume));
        }

        for (auto& k : t)
        {
            k.wait();
        }
    }
}

int main()
{
    WarmUp(10);

    std::cout << "Volume of unit sphere is " << std::fixed << std::setprecision(10)
        << std::atan(1) * 4 * 4 / 3 << std::endl;

    MeasureTime("compute linear at main thread", ComputeUnitSphereVolume);

    const auto concurrency = std::thread::hardware_concurrency();

    std::cout << "There are " << concurrency << " (logical) threads" << std::endl;

    for (unsigned i = 1; i < 2 * concurrency; i++)
    {
        if (i == concurrency + 1)
        {
            std::cout << "speed limit must have been reached?" << std::endl;
        }

        std::stringstream ss;
        ss << "computing by parts on " << i << " threads";
        MeasureTime(ss.str().c_str(), ComputeUnitSphereVolumeInThreadPool, i);
    }

    return 0;
}
