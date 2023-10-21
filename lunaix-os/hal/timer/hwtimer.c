#include <hal/hwtimer.h>
#include <lunaix/spike.h>

#include <usr/lunaix/ioctl_defs.h>

struct hwtimer* current_timer;

ticks_t
hwtimer_base_frequency()
{
    assert(current_timer);
    return current_timer->base_freq;
}

ticks_t
hwtimer_current_systicks()
{
    assert(current_timer);
    return current_timer->systicks();
}

ticks_t
hwtimer_to_ticks(u32_t value, int unit)
{
    assert(current_timer);
    // in case system frequency is less than 1000Hz
    if (unit != TIME_MS) {
        return current_timer->running_freq * unit * value;
    }

    ticks_t freq_ms = current_timer->running_freq / 1000;

    return freq_ms * value;
}

static int
__hwtimer_ioctl(struct device* dev, u32_t req, va_list args)
{
    struct hwtimer* hwt = (struct hwtimer*)dev->underlay;
    switch (req) {
        case TIMERIO_GETINFO:
            // TODO
            break;

        default:
            break;
    }
    return 0;
}

void
hwtimer_init(u32_t hertz, void* tick_callback)
{
    struct hwtimer* hwt_ctx = hwtimer_choose();

    hwt_ctx->init(hwt_ctx, hertz, tick_callback);
    hwt_ctx->running_freq = hertz;

    current_timer = hwt_ctx;

    struct device* timerdev = device_allocsys(NULL, hwt_ctx);

    timerdev->ops.exec_cmd = __hwtimer_ioctl;

    device_register(timerdev, &hwt_ctx->class, hwt_ctx->name);
}