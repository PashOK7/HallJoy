import copy
import unittest
from check_keyboard_sheet_presentation import audit_presentation, palette, WHITE, DARK


def fixture():
    rows = [{'values': []}]
    for brand, status, repeat in [('Brand', 'Not investigated', False),
                                   ('Brand', 'Implemented; awaiting hardware testing', True),
                                   ('Other', 'Supported', False)]:
        bg, fg = palette(status)
        values = []
        for text, background, foreground in [(brand, WHITE, WHITE if repeat else DARK),
                                             ('Model', bg, fg), (status, bg, fg)]:
            fmt = dict(backgroundColorStyle=dict(rgbColor=background),
                       textFormat=dict(foregroundColorStyle=dict(rgbColor=foreground)))
            values.append(dict(formattedValue=text, userEnteredFormat=copy.deepcopy(fmt),
                               effectiveFormat=copy.deepcopy(fmt)))
        rows.append(dict(values=values))
    return {'sheets': [dict(properties=dict(title='Main', sheetId=0), data=[dict(rowData=rows)])]}


class PresentationTests(unittest.TestCase):
    def test_correct_conditional_result_does_not_hide_broken_base(self):
        src = fixture()
        src['sheets'][0]['data'][0]['rowData'][2]['values'][0]['userEnteredFormat']['textFormat']['foregroundColorStyle']['rgbColor'] = DARK
        requests, issues = audit_presentation(src)
        self.assertEqual(len(requests), 1)
        self.assertEqual(issues, ['userEnteredFormat:3:1'])

    def test_wrong_effective_color_still_fails(self):
        src = fixture()
        src['sheets'][0]['data'][0]['rowData'][2]['values'][0]['effectiveFormat']['textFormat']['foregroundColorStyle']['rgbColor'] = DARK
        requests, issues = audit_presentation(src)
        self.assertFalse(requests)
        self.assertEqual(issues, ['effectiveFormat:3:1'])

    def test_first_brand_visible_and_status_colors(self):
        self.assertEqual(audit_presentation(fixture()), ([], []))
        src = fixture()
        src['sheets'][0]['data'][0]['rowData'][1]['values'][0]['userEnteredFormat']['textFormat']['foregroundColorStyle']['rgbColor'] = WHITE
        self.assertTrue(audit_presentation(src)[1])
        with self.assertRaises(ValueError): palette('unrecognized')


if __name__ == '__main__': unittest.main()
