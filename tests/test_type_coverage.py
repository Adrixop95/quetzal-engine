"""Run all-type simulation coverage against the existing public rules metadata."""

import json
from pathlib import Path
import subprocess
import sys
import tempfile


def main():
    executable, data = sys.argv[1:]
    data = Path(data)
    rules = json.loads((data / "rules.json").read_text(encoding="utf-8"))
    types = rules["types"]
    assert [t["id"] for t in types] == list(range(90)), "type coverage must be exhaustive"
    kinds = ["Marine", "Spider", "Building", "Zeppelin", "Truck", "Bird",
             "Headtruck", "Defence", "Boat", "Cannon", "Johnka", "Camera"]
    # Only V fields with established consumers belong in these expectations.
    fields = {
        "anim_id": "anim_id", "hp_initial": "hp_max",
        "secondary_hp_initial": "secondary_hp", "regen_per_tick": "regen",
        "regen_cap_alt": "regen_alt_threshold", "armor_physical": "physical_armor",
        "armor_auxiliary": "auxiliary_armor", "armor_fire": "blast_multiplier",
        "weapon_id": "weapon_id", "sight_mask_id": "vision_mask",
        "speed_px_per_tick": "speed", "cruise_altitude_px": "cruise_height",
        "turret_anim_id": "turret_anim_id", "turret_variant_index": "turret_variant_index",
        "spawn_sound_list_id": "spawn_sound_list_id", "death_sound_list_id": "death_sound_list_id",
        "engine_sound_list_id": "engine_sound_list_id", "attack_sound_list_id": "attack_sound_list_id",
        "turn_step": "turn_rate", "turret_turn_step": "turn_rate",
    }
    with tempfile.TemporaryDirectory(prefix="aztec-type-coverage-") as temporary:
        expected = Path(temporary) / "expected.txt"
        rows = []
        for t in types:
            values = {"kind": kinds.index(t["kind"]), "price": t["unit_price"],
                      "time": t["build_time"], "dev_level": t["dev_level"]}
            for field, member in fields.items():
                fact = t.get("fields", {}).get(field)
                if fact and fact["status"] == "verified":
                    values[member] = fact["value"] * (256 if member == "speed" else 1)
            rows.extend(f"{t['id']} {member} {value}\n" for member, value in values.items())
        # Check every recipe, including each town level and prerequisite scope.
        for recipe in rules["build_tree"]:
            needs = recipe["prereq_ugi(A=flag 0x8000000)"] or "-"
            rows.append(f"recipe {recipe['race']} {recipe['townhall_type']} "
                        f"{recipe['item_ugi']} {recipe['flagB_construct']} {needs}\n")
        expected.write_text("".join(rows), encoding="utf-8")
        subprocess.run([executable, str(data), str(expected)], check=True, timeout=120)


if __name__ == "__main__":
    main()
