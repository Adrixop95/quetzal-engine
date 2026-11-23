"""Check public descriptor coverage and reproducible rules without private inputs."""

import copy
import csv
import json
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]
DATA = ROOT / "docs" / "data"
STATUSES = {"verified", "guess", "unused"}


def validate_coverage(rules):
    """Require a label and evidence status for every nonzero word of each kind.

    A constant -1 is nonzero too. A zero word in one record still requires a
    label if another record of that kind uses it. All-zero columns may be absent.
    """
    with (DATA / "descriptor_kinds.csv").open(newline="", encoding="utf-8") as source:
        kinds = {row["object_kind"]: row["kind"] for row in csv.DictReader(source)}
    for object_kind, kind in kinds.items():
        layout = rules["descriptor_layouts"][kind]
        records = [obj for obj in rules["types"] if obj["kind"] == object_kind]
        words = {}
        for word in layout["words"]:
            index = word["index"]
            if index in words:
                raise ValueError(f"{kind}: duplicate d{index}")
            words[index] = word
        active = set()
        for obj in records:
            raw = obj["descriptor_raw"]
            if len(raw) != layout["word_count"]:
                raise ValueError(f"{kind}: wrong descriptor width")
            active.update(i for i, value in enumerate(raw) if value != 0)
        for index in active:
            word = words.get(index, {})
            if not re.fullmatch(r"[a-z][a-z0-9_]*", word.get("name", "")):
                raise ValueError(f"{kind} d{index}: missing name")
            if word.get("status") not in STATUSES:
                raise ValueError(f"{kind} d{index}: missing status")
            if word.get("offset") != index * 4 or not word.get("meaning"):
                raise ValueError(f"{kind} d{index}: missing offset or meaning")
            evidence = word.get("evidence", [])
            if word["status"] != "unused" and not evidence:
                raise ValueError(f"{kind} d{index}: missing evidence")
            if any(not re.fullmatch(r"0x[0-9a-f]{6}", addr) for addr in evidence):
                raise ValueError(f"{kind} d{index}: invalid evidence address")
            for obj in records:
                field = obj.get("fields", {}).get(word["name"], {})
                if (field.get("index") != index or
                        field.get("status") != word["status"] or
                        field.get("value") != obj["descriptor_raw"][index]):
                    raise ValueError(f"{kind} d{index}: inconsistent type field")


class DescriptorFieldsTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.rules = json.loads((DATA / "rules.json").read_text())

    def test_all_nonzero_words_have_names_and_statuses(self):
        validate_coverage(self.rules)

    def test_engine_use_matrix_covers_every_word(self):
        matrix = (ROOT / "docs" / "descriptor_use_matrix.md").read_text()
        for family, layout in self.rules["descriptor_layouts"].items():
            section = matrix.split(f"## {family.capitalize()}\n", 1)[1].split("\n## ", 1)[0]
            rows = re.findall(r"^\| d(\d+) \| `([^`]+)`.*? \| (V|G|unused) \| (Yes|No) \|", section, re.M)
            expected = [(str(w["index"]), w["name"],
                         {"verified": "V", "guess": "G", "unused": "unused"}[w["status"]])
                        for w in layout["words"]]
            self.assertEqual([row[:3] for row in rows], expected)
            for index, name, status, used in rows:
                if status != "V" or name == "name_string_pointer":
                    self.assertEqual(used, "No", f"{family} d{index}")

    def test_missing_word_name_status_or_evidence_fails(self):
        for key in ("name", "status", "evidence"):
            with self.subTest(key=key):
                rules = copy.deepcopy(self.rules)
                del rules["descriptor_layouts"]["marine"]["words"][17][key]
                with self.assertRaises(ValueError):
                    validate_coverage(rules)
        rules = copy.deepcopy(self.rules)
        del rules["descriptor_layouts"]["marine"]["words"][17]
        with self.assertRaises(ValueError):
            validate_coverage(rules)

    def test_zero_in_one_type_does_not_hide_used_column(self):
        rules = copy.deepcopy(self.rules)
        self.assertEqual(rules["types"][0]["kind"], "Marine")
        rules["types"][0]["descriptor_raw"][17] = 0
        del rules["descriptor_layouts"]["marine"]["words"][17]["name"]
        with self.assertRaises(ValueError):
            validate_coverage(rules)

    def test_unused_sentinel_needs_status_but_all_zero_may_be_omitted(self):
        rules = copy.deepcopy(self.rules)
        marine = rules["descriptor_layouts"]["marine"]["words"]
        self.assertEqual(marine[8]["status"], "unused")
        del marine[8]["status"]
        with self.assertRaises(ValueError):
            validate_coverage(rules)
        rules = copy.deepcopy(self.rules)
        # Marine hit sound d16 is zero for every type, unlike death sound d17.
        rules["descriptor_layouts"]["marine"]["words"].pop(16)
        validate_coverage(rules)

    def test_known_hp_weapon_and_armor_corrections(self):
        for obj in self.rules["types"]:
            if obj["kind"] in ("Headtruck", "Truck", "Spider"):
                fields = obj["fields"]
                for name in ("weapon_id", "armor_fire", "armor_physical"):
                    self.assertEqual(fields[name]["status"], "verified")
                self.assertEqual(fields["hp_max"]["index"], fields["hp_initial"]["index"])
                self.assertNotEqual(fields["hp_max"]["index"],
                                    fields["secondary_hp_initial"]["index"])
            if obj["kind"] == "Marine":
                self.assertEqual(obj["fields"]["armor_d14"]["index"], 14)
                self.assertEqual(obj["fields"]["armor_d14"]["status"], "verified")
        catapult = self.rules["types"][83]["fields"]
        self.assertEqual(catapult["hp_initial"]["value"], 10000)
        self.assertEqual(catapult["secondary_hp_initial"]["value"], 3000)

    def test_weapon_statuses_preserve_the_evidence_boundary(self):
        for weapon in self.rules["weapons"]:
            if weapon["class"] in (0, 3, 7, 9, 11):
                self.assertTrue(weapon["status"].startswith("verified"))
                self.assertEqual(weapon["class_link_status"], "guess")
                self.assertTrue(weapon["evidence"])
            if weapon["class"] in (0, 3):
                raw = weapon["record_raw"]
                self.assertEqual(weapon["projectile_speed"], raw[1])
                self.assertEqual(weapon["reload_ticks"], raw[2])
            if weapon["class"] == 7:
                self.assertEqual(weapon["required_airborne_flag"], weapon["record_raw"][5])
            if weapon["class"] == 11:
                self.assertTrue(weapon["hit_effect_status"].startswith("open"))

    def test_regeneration_matches_checked_in_json(self):
        with tempfile.TemporaryDirectory() as directory:
            output = Path(directory) / "rules.json"
            subprocess.run([sys.executable, "-I", str(ROOT / "tools/build_rules.py"),
                            "--output", str(output)], check=True, capture_output=True)
            self.assertEqual(output.read_bytes(), (DATA / "rules.json").read_bytes())

    def test_ground_weapon_contracts_survive_descriptor_regeneration(self):
        for weapon in self.rules["weapons"]:
            kind = weapon["class"]
            with self.subTest(weapon=weapon["id"]):
                if kind in (2, 13):
                    self.assertEqual(weapon["armor_helper_duration_ticks"],
                                     weapon["record_raw"][3])
                elif kind == 3:
                    self.assertEqual(weapon["contact_damage_per_tick"],
                                     weapon["record_raw"][4])
                    self.assertEqual(weapon["damage_speed_numerator"], 48)
                    self.assertIn("estimate", weapon["status"])
                elif kind == 6:
                    self.assertEqual(weapon["target_attack_cooldown_add"], 18)
                elif kind == 7:
                    self.assertEqual(weapon["reload_ticks"], weapon["record_raw"][6])
                    self.assertEqual(weapon["range_px"], weapon["record_raw"][3])
                    self.assertEqual(weapon["target_airborne"],
                                     bool(weapon["required_airborne_flag"]))
                    self.assertEqual(weapon["target_extent_px"], 28)
                    self.assertTrue(weapon["inner_damage_bypasses_armor"])
                    self.assertEqual(weapon["mult_e8"], 204 / 256)
                elif kind == 10:
                    self.assertEqual(weapon["attacker_heal_divisor"], 3)
                    self.assertEqual(weapon["mult_organic_f4_or_e8"], 1.25)
                elif kind == 14:
                    self.assertEqual(weapon["id"], 0)
                    self.assertIn("sentinel", weapon["status"])


if __name__ == "__main__":
    unittest.main()
