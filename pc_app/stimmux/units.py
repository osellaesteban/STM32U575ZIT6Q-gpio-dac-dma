"""Unit conversions and configuration checks (mirror of the firmware rules)."""
from __future__ import annotations

from .model import CHANNELS, MAX_SLOTS, ChannelSetup, DS8RSettings, StimConfig, Timing

AMP_MAX = 1000                  # firmware amplitude: per mil of the DS8R full scale
DS8R_PW_MIN_US = 50
DS8R_PW_MAX_US = 2000
DS8R_MIN_TRIG_INTERVAL_US = 1000
MIN_EVENT_GAP_US = 5
MIN_IDLE_US = 100


def ma_to_permille(ma: float, ds8r: DS8RSettings) -> int:
    if ds8r.range_ma <= 0 or ma <= 0:
        return 0
    return max(0, min(AMP_MAX, int(round(ma / ds8r.range_ma * AMP_MAX))))


def permille_to_ma(permille: float, ds8r: DS8RSettings) -> float:
    return permille * ds8r.range_ma / AMP_MAX


def counts_to_ma(counts: float, baseline: float, ma_per_count: float) -> float | None:
    """Measured current from ADC counts, None if not calibrated."""
    if ma_per_count <= 0:
        return None
    return (counts - baseline) * ma_per_count


def channel_max_ma(channel: ChannelSetup, ds8r: DS8RSettings) -> float:
    """Highest amplitude the application lets a channel reach."""
    if not channel.enabled:
        return 0.0
    return max(0.0, min(channel.safety_limit_ma, ds8r.range_ma, ds8r.current_limit_ma))


def check_timing(timing: Timing, ds8r: DS8RSettings) -> list[str]:
    """Same feasibility rules as stValidate() in the firmware."""
    errors = []
    seq = timing.sequence
    if not seq or len(seq) > MAX_SLOTS:
        errors.append(f"La secuencia debe tener entre 1 y {MAX_SLOTS} slots.")
        return errors
    if any(c < 0 or c >= CHANNELS for c in seq):
        errors.append(f"La secuencia solo puede contener canales 0..{CHANNELS - 1}.")
    pw = ds8r.pulse_width_us
    if not DS8R_PW_MIN_US <= pw <= DS8R_PW_MAX_US:
        errors.append(f"El ancho de pulso del DS8R debe estar entre {DS8R_PW_MIN_US} y {DS8R_PW_MAX_US} µs.")
    if min(timing.settle_us, timing.trigger_us, timing.tail_us) < MIN_EVENT_GAP_US:
        errors.append(f"Settle, trigger y tail deben ser de al menos {MIN_EVENT_GAP_US} µs.")
    trig_off = timing.settle_us + timing.trigger_us
    flip = timing.settle_us + timing.ds8r_delay_us + pw // 2
    gate_off = timing.settle_us + timing.ds8r_delay_us + pw + timing.tail_us
    if flip < trig_off + MIN_EVENT_GAP_US:
        errors.append("El trigger es demasiado largo respecto de la mitad del pulso (modo bifásico).")
    spacing = timing.period_us // len(seq)
    if spacing < gate_off + MIN_IDLE_US:
        errors.append(f"Cada slot dura {spacing} µs y el pulso necesita {gate_off + MIN_IDLE_US} µs: "
                      "aumentá el período o acortá la secuencia.")
    if spacing < DS8R_MIN_TRIG_INTERVAL_US:
        errors.append(f"Cada slot dura {spacing} µs: el DS8R admite como máximo un disparo por ms.")
    if not 0 < timing.dac_full_scale <= 4095:
        errors.append("DACFS debe estar entre 1 y 4095.")
    return errors


def check_channels(config: StimConfig) -> list[str]:
    errors = []
    ds8r = config.ds8r
    for ch in config.channels:
        if not ch.enabled:
            continue
        name = f"Canal {ch.index} ({ch.label})"
        if ch.safety_limit_ma <= 0:
            errors.append(f"{name}: falta el límite de seguridad.")
        elif ch.safety_limit_ma > ds8r.range_ma:
            errors.append(f"{name}: el límite supera el rango del DS8R ({ds8r.range_ma} mA).")
        elif ch.working_level_ma > ch.safety_limit_ma:
            errors.append(f"{name}: el nivel de trabajo supera el límite de seguridad.")
        if ch.polarity not in ("POS", "NEG", "ALT", "BIPH"):
            errors.append(f"{name}: polaridad inválida.")
    return errors
