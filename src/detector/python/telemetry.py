"""Lossless packet export and conservative display metadata, after CRC validation only."""
import json
import math

# Inherited reference-parser annotations, not independently verified per firmware.
STATE_BITS = {15: "Altitude valid", 14: "GPS valid", 13: "In air",
              12: "Motors on", 11: "UUID set", 10: "Home set",
              9: "Private mode disabled", 8: "Serial valid",
              2: "Z velocity valid", 1: "Y velocity valid"}


def position_status(lat, lon, flag=None):
    if lat is None or lon is None:
        return "Unavailable"
    if not math.isfinite(lat) or not math.isfinite(lon) or not (-90 <= lat <= 90 and -180 <= lon <= 180):
        return "Invalid coordinate range"
    if lat == 0 and lon == 0:
        return "Unavailable (zero pair)"
    if flag is False:
        return "Validity flag unset (tentative mapping)"
    return "Reported; validity unverified"


def packet_telemetry(payload):
    fields = []
    state = payload.get("state_info")
    def flag(bit):
        return None if state is None else bool(state & (1 << bit))
    def add(group, name, value, status="Reported; semantics unverified"):
        fields.append(dict(group=group, name=name,
                           value="Unavailable" if value is None else str(value),
                           status="Unavailable" if value is None else status))
    def pos(name, lat_key, lon_key, validity):
        lat, lon = payload.get(lat_key), payload.get(lon_key)
        status = position_status(lat, lon, validity)
        value = None if lat is None or lon is None else f"{lat:.6f}, {lon:.6f}"
        add("Positions", name, value, status)
        return status
    aircraft_status = pos("Aircraft latitude / longitude", "latitude", "longitude", flag(14))
    pos("App/controller latitude / longitude", "app_lat", "app_lon", None)
    pos("Home latitude / longitude", "latitude_home", "longitude_home", flag(10))
    for name in ("altitude", "height"):
        add("Aircraft", name.title() + " (raw)", payload.get(name + "_raw"))
        value = payload.get(name)
        add("Aircraft", name.title() + " (reference conversion)",
            None if value is None else f"{value:.2f} m",
            "Raw / 3.281; scale and vertical reference unverified")
    for key in ("v_north", "v_east", "v_up"):
        add("Aircraft", key + " (raw)", payload.get(key), "Units, axes and validity unverified")
    add("Aircraft", "d_1_angle (raw)", payload.get("d_1_angle"))
    for key in ("sequence_number", "version", "gps_time", "uuid_len"):
        add("Identity / state", key, payload.get(key),
            "Epoch and units unverified" if key == "gps_time" else "Reported")
    uuid_status = ("Invalid length (>20 bytes)" if not payload.get("uuid_length_valid", True)
                   else "UUID-set flag unset (tentative mapping)" if flag(11) is False
                   else "Reported; text may contain replacement characters")
    add("Identity / state", "UUID", payload.get("uuid") or None, uuid_status)
    add("Identity / state", "UUID bytes (hex)", payload.get("uuid_hex") or None, uuid_status)
    add("Identity / state", "State bitfield", None if state is None else f"0x{state:04X}", "Raw bits preserved")
    for bit, name in STATE_BITS.items():
        add("Identity / state", f"Bit {bit}: {name}",
            None if state is None else "Set" if flag(bit) else "Clear", "Tentative reference mapping")
    known_mask = sum(1 << bit for bit in STATE_BITS)
    add("Identity / state", "Uninterpreted state bits", None if state is None else f"0x{state & (~known_mask & 0xFFFF):04X}", "No inferred meaning")
    return dict(fields=fields, aircraft_position_status=aircraft_status,
                packet_json=json.dumps(payload, indent=2, ensure_ascii=True, allow_nan=False))
