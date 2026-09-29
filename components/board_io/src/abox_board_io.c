#include "abox_board_io.h"

#include <string.h>

#define ABOX_BOARD_IO_DEBOUNCE_MS 20U

static int valid_output(ABoxBoardIoOutput output)
{
    return (unsigned)output < ABOX_BOARD_IO_OUTPUT_COUNT;
}

static int valid_led(ABoxBoardIoLed led)
{
    return (unsigned)led < ABOX_BOARD_IO_LED_COUNT;
}

static uint32_t enter(ABoxBoardIo *io)
{
    return io->port->enter_critical(io->port->context);
}

static void leave(ABoxBoardIo *io, uint32_t key)
{
    io->port->exit_critical(io->port->context, key);
}

ABoxBoardIoResult ABoxBoardIo_Init(ABoxBoardIo *io, const ABoxBoardIoPort *port,
                                   uint8_t allowed_outputs)
{
    unsigned i;
    uint8_t level;
    if (!io) return ABOX_BOARD_IO_IO_FAILED;
    memset(io, 0, sizeof(*io));
    if (!port || !port->prepare || !port->write_output ||
        !port->read_output || !port->read_input || !port->write_led ||
        !port->enter_critical || !port->exit_critical ||
        (allowed_outputs & (uint8_t)~0x0fU) != 0U) return ABOX_BOARD_IO_IO_FAILED;
    io->port = port;
    if (!port->prepare(port->context)) return ABOX_BOARD_IO_IO_FAILED;
    for (i = 0U; i < ABOX_BOARD_IO_OUTPUT_COUNT; ++i) {
        if (!port->read_output(port->context, (ABoxBoardIoOutput)i, &level) || level)
            return ABOX_BOARD_IO_READBACK_FAILED;
    }
    io->allowed_outputs = allowed_outputs;
    io->ready = 1U;
    return ABOX_BOARD_IO_OK;
}

ABoxBoardIoResult ABoxBoardIo_OutputSet(ABoxBoardIo *io, ABoxBoardIoOutput output,
                                        uint8_t on)
{
    uint32_t key;
    uint8_t level = 0U;
    ABoxBoardIoResult result;
    if (!valid_output(output)) return ABOX_BOARD_IO_INVALID_CHANNEL;
    if (!io || !io->ready) return ABOX_BOARD_IO_NOT_READY;
    key = enter(io);
    if (on && !(io->allowed_outputs & (1U << output))) result = ABOX_BOARD_IO_DENIED;
    else if (!io->port->write_output(io->port->context, output, !!on)) {
        if (on) (void)io->port->write_output(io->port->context, output, 0U);
        result = ABOX_BOARD_IO_IO_FAILED;
    }
    else if (!io->port->read_output(io->port->context, output, &level) || level != !!on) {
        /* A failed assertion of an enabled output must not leave it driven. */
        if (on) (void)io->port->write_output(io->port->context, output, 0U);
        result = ABOX_BOARD_IO_READBACK_FAILED;
    }
    else result = ABOX_BOARD_IO_OK;
    leave(io, key);
    return result;
}

ABoxBoardIoResult ABoxBoardIo_OutputGet(ABoxBoardIo *io, ABoxBoardIoOutput output,
                                        uint8_t *on)
{
    uint32_t key;
    int ok;
    if (!valid_output(output) || !on) return ABOX_BOARD_IO_INVALID_CHANNEL;
    if (!io || !io->ready) return ABOX_BOARD_IO_NOT_READY;
    key = enter(io);
    ok = io->port->read_output(io->port->context, output, on);
    leave(io, key);
    return ok ? ABOX_BOARD_IO_OK : ABOX_BOARD_IO_READBACK_FAILED;
}

ABoxBoardIoResult ABoxBoardIo_InputsGet(ABoxBoardIo *io, ABoxBoardIoInputs *inputs)
{
    uint32_t key;
    if (!inputs) return ABOX_BOARD_IO_IO_FAILED;
    if (!io || !io->ready) return ABOX_BOARD_IO_NOT_READY;
    key = enter(io);
    *inputs = io->inputs;
    leave(io, key);
    return ABOX_BOARD_IO_OK;
}

ABoxBoardIoResult ABoxBoardIo_LedSet(ABoxBoardIo *io, ABoxBoardIoLed led,
                                     ABoxBoardIoLedMode mode, uint32_t period_ms,
                                     uint32_t now_ms)
{
    uint32_t key;
    uint8_t level;
    if (!valid_led(led)) return ABOX_BOARD_IO_INVALID_CHANNEL;
    if (mode > ABOX_BOARD_IO_LED_BLINK ||
        (mode == ABOX_BOARD_IO_LED_BLINK && (period_ms < 2U || period_ms > 0x7ffffffeU)))
        return ABOX_BOARD_IO_IO_FAILED;
    if (!io || !io->ready) return ABOX_BOARD_IO_NOT_READY;
    level = mode == ABOX_BOARD_IO_LED_OFF ? 0U : 1U;
    key = enter(io);
    if (!io->port->write_led(io->port->context, led, level)) {
        leave(io, key);
        return ABOX_BOARD_IO_IO_FAILED;
    }
    io->led_mode[led] = (uint8_t)mode;
    io->led_level[led] = level;
    io->led_changed_at_ms[led] = now_ms;
    io->led_half_period_ms[led] = period_ms / 2U;
    leave(io, key);
    return ABOX_BOARD_IO_OK;
}

void ABoxBoardIo_Poll(ABoxBoardIo *io, uint32_t now_ms)
{
    unsigned i;
    uint8_t level;
    uint8_t bit;
    uint32_t key;
    if (!io || !io->ready) return;
    key = enter(io);
    io->inputs.sampled_at_ms = now_ms;
    for (i = 0U; i < ABOX_BOARD_IO_INPUT_COUNT; ++i) {
        bit = (uint8_t)(1U << i);
        if (!io->port->read_input(io->port->context, (ABoxBoardIoInput)i, &level)) {
            io->inputs.valid_mask &= (uint8_t)~bit;
            io->observed_mask &= (uint8_t)~bit;
            io->inputs.raw_mask &= (uint8_t)~bit;
            continue;
        }
        if (level) io->inputs.raw_mask |= bit;
        else io->inputs.raw_mask &= (uint8_t)~bit;
        if (!(io->observed_mask & bit) || !!(io->candidate_mask & bit) != !!level) {
            io->observed_mask |= bit;
            io->changed_at_ms[i] = now_ms;
            if (level) io->candidate_mask |= bit;
            else io->candidate_mask &= (uint8_t)~bit;
        } else if ((uint32_t)(now_ms - io->changed_at_ms[i]) >= ABOX_BOARD_IO_DEBOUNCE_MS) {
            if (level) io->inputs.stable_mask |= bit;
            else io->inputs.stable_mask &= (uint8_t)~bit;
            io->inputs.valid_mask |= bit;
        }
    }
    for (i = 0U; i < ABOX_BOARD_IO_LED_COUNT; ++i) {
        if (io->led_mode[i] == ABOX_BOARD_IO_LED_BLINK &&
            (uint32_t)(now_ms - io->led_changed_at_ms[i]) >= io->led_half_period_ms[i]) {
            level = (uint8_t)!io->led_level[i];
            if (io->port->write_led(io->port->context, (ABoxBoardIoLed)i, level)) {
                io->led_level[i] = level;
                io->led_changed_at_ms[i] = now_ms;
            }
        }
    }
    leave(io, key);
}
