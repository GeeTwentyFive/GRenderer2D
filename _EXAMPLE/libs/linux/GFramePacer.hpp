#pragma once

#ifdef _WIN32
        #include <windows.h>
#else
        #include <sys/mman.h>
        #include <sched.h>
        #include <sys/resource.h>
#endif

#include <chrono>

class GFramePacer { private: std::chrono::time_point<std::chrono::steady_clock> _previous_time; public:
        double target_frametime_ms = 1.0;  // Should be a power-of-2 (like 1.0, 2.0, 4.0, 8.0) (to align as fraction/multiple of USB input report rate)

        GFramePacer() {
                _previous_time = std::chrono::steady_clock::now();

                #ifdef _WIN32
                {
                        SetPriorityClass(GetCurrentProcess(), REALTIME_PRIORITY_CLASS);
                }
                #else
                {
                        mlockall(MCL_CURRENT | MCL_FUTURE);  // Lock all virtual memory pages to RAM (no disk swapping)

                        setpriority(PRIO_PROCESS, 0, -20);

                        // Set to stay on same CPU core (unless core 0|1 (same physical core on SMT CPU), then falls back to 2)
                        int core = sched_getcpu();
                        if (core <= 1) core = 2;
                        cpu_set_t mask; CPU_ZERO(&mask);
                        CPU_SET(core, &mask);
                        sched_setaffinity(0, sizeof(cpu_set_t), &mask);
                }
                #endif
        }

        double Wait() {
                while (std::chrono::steady_clock::now() < _previous_time+std::chrono::duration<double, std::milli>(target_frametime_ms)) {}  // busy-loop instead of sleep due to OS sleep being imprecise (especially on Windows) and causing context switching stutter
                auto new_time = std::chrono::steady_clock::now();
                double delta_time = std::chrono::duration<double>(new_time - _previous_time).count();
                _previous_time = new_time;
                return delta_time;
        }
};