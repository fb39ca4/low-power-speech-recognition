#pragma once

#include <modm/processing/protothread.hpp>
#include <modm/processing/timer/timeout.hpp>
#include <common/board.hpp>

#undef	MODM_LOG_LEVEL
#define	MODM_LOG_LEVEL modm::log::INFO

class BlinkThread : public modm::pt::Protothread
{
public:
    inline bool
    run() {
        PT_BEGIN();
        while (true)
        {
            timeout.restart(50);
            Led::set();
            PT_WAIT_UNTIL(timeout.isExpired());

            timeout.restart(950);
            Led::reset();
            PT_WAIT_UNTIL(timeout.isExpired());
        }
        PT_END();
    }
private:
    modm::ShortTimeout timeout;
};