"""Grouping boundaries: cosmetics may merge; gameplay differences must not."""
import copy
import struct
import unittest
from k1_spawn_menu_catalog import canonical, combat_stats, combat_item, saber_color


class GroupingTests(unittest.TestCase):
    def test_unused_model_strings_do_not_change_blade_color(self):
        raw = bytearray(1024)
        struct.pack_into('<I', raw, 52, 100)
        struct.pack_into('<H', raw, 112, 0x0821)
        raw[280:295] = b'w_lsabrepurp01\0\0'
        raw[700:715] = b'w_lsabreyelo01\0\0'
        self.assertEqual(saber_color(raw), 'Purple')

    def test_equal_display_totals_do_not_hide_different_feats_or_skills(self):
        source = {'HitPoints': 30, 'ClassList': [{'Class': 3, 'ClassLevel': 4}],
                  'FeatList': [{'Feat': 4}], 'SkillList': [{'Rank': 1}, {'Rank': 2}]}
        for key, value in [('FeatList', [{'Feat': 5}]), ('SkillList', [{'Rank': 2}, {'Rank': 1}]),
                           ('HitPoints', 31), ('ClassList', [{'Class': 4, 'ClassLevel': 4}])]:
            other = copy.deepcopy(source);other[key] = value
            self.assertNotEqual(canonical(combat_stats(source)), canonical(combat_stats(other)))

    def test_appearance_and_campaign_identity_do_not_split_stats(self):
        self.assertEqual(combat_stats({'Str': 12, 'Appearance_Type': 40, 'Tag': 'a', 'FactionID': 1}),
                         combat_stats({'Str': 12, 'Appearance_Type': 50, 'Tag': 'b', 'FactionID': 2}))

    def test_only_saber_art_can_change_weapon_identity(self):
        saber = {'BaseItem': 8, 'ModelVariation': 1, 'LocalizedName': 'Red', 'PropertiesList': []}
        blue = dict(saber, ModelVariation=2, LocalizedName='Blue')
        self.assertEqual(combat_item(saber, 16, 'Equip_ItemList'), combat_item(blue, 16, 'Equip_ItemList'))
        enhanced = dict(blue, PropertiesList=[{'PropertyName': 0, 'CostValue': 1}])
        self.assertNotEqual(combat_item(saber, 16, 'Equip_ItemList'), combat_item(enhanced, 16, 'Equip_ItemList'))
        sword = dict(saber, BaseItem=1)
        other = dict(sword, ModelVariation=2)
        self.assertNotEqual(combat_item(sword, 16, 'Equip_ItemList'), combat_item(other, 16, 'Equip_ItemList'))


if __name__ == '__main__':
    unittest.main()
